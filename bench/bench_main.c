/*
 * Before/after benchmarks: the original assignment engine (linked list,
 * linear search, +1 realloc, linear sampling) against libmarkov (hash index,
 * edge index, geometric growth, alias sampling).
 *
 *   bench [--quick] [corpus files...]
 *
 * Prints Markdown tables. Times are wall-clock medians.
 */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "markov/markov.h"

/* The original Part B engine (legacy/ex3b). */
#include "markov_chain.h"

/* ---- timing ------------------------------------------------------------- */

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

static int compare_doubles(const void *a, const void *b)
{
    double x = *(const double *) a;
    double y = *(const double *) b;
    return (x > y) - (x < y);
}

static double median(double *v, int n)
{
    qsort(v, (size_t) n, sizeof(double), compare_doubles);
    return v[n / 2];
}

/* ---- corpora ------------------------------------------------------------ */

typedef struct corpus
{
    char name[96];
    char *text;
    size_t len;
    size_t words;
} corpus;

static size_t count_words(const char *text, size_t len)
{
    size_t words = 0;
    bool in_word = false;
    for (size_t i = 0; i < len; i++)
    {
        bool delim = text[i] == ' ' || text[i] == '\t' || text[i] == '\r'
                     || text[i] == '\n' || text[i] == '\0';
        words += !delim && !in_word;
        in_word = !delim;
    }
    return words;
}

static bool load_corpus(const char *path, corpus *c)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    c->text = malloc((size_t) size + 1u);
    c->len = c->text ? fread(c->text, 1, (size_t) size, fp) : 0;
    fclose(fp);
    if (c->text == NULL)
    {
        return false;
    }
    c->text[c->len] = '\0';
    const char *base = strrchr(path, '/');
    snprintf(c->name, sizeof(c->name), "%s", base ? base + 1 : path);
    c->words = count_words(c->text, c->len);
    return true;
}

/**
 * Synthetic text with a Zipfian vocabulary (rank r has weight 1/r) and
 * sentences of ~12 words, so vocabulary grows with corpus size the way it
 * does in real text.
 */
static void synthetic_corpus(size_t words, size_t vocab, corpus *c)
{
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, words);
    uint64_t *weights = malloc(sizeof(uint64_t) * vocab);
    for (size_t r = 0; r < vocab; r++)
    {
        weights[r] = (uint64_t) (1e9 / (double) (r + 1u));
    }
    mk_alias zipf;
    mk_alias_build(&zipf, weights, (uint32_t) vocab);
    free(weights);

    c->text = malloc(words * 12u + 1u);
    size_t pos = 0;
    for (size_t i = 0; i < words; i++)
    {
        uint32_t w = mk_alias_sample(&zipf, &rng);
        bool end = mk_rng_below(&rng, 12) == 0 || i + 1 == words;
        pos += (size_t) sprintf(c->text + pos, "w%u%s ", w, end ? "." : "");
    }
    mk_alias_free(&zipf);
    c->len = pos;
    c->words = words;
    snprintf(c->name, sizeof(c->name), "synthetic %zuk words", words / 1000u);
}

/* ---- the original engine ------------------------------------------------ */

static int legacy_comp(void *a, void *b) { return strcmp(a, b); }
static void legacy_free(void *data) { free(data); }
static void legacy_print(void *data) { (void) data; }
static bool legacy_is_last(void *data)
{
    size_t len = strlen(data);
    return len > 0 && ((char *) data)[len - 1u] == '.';
}
static void *legacy_copy(void *data)
{
    size_t len = strlen(data) + 1u;
    char *copy = malloc(len);
    if (copy)
    {
        memcpy(copy, data, len);
    }
    return copy;
}

/** The original fill_database, over an in-memory buffer. */
static MarkovChain *legacy_build(const corpus *c, size_t *states)
{
    MarkovChain *chain = malloc(sizeof(MarkovChain));
    chain->database = calloc(1, sizeof(LinkedList));
    chain->print_func = legacy_print;
    chain->comp_func = legacy_comp;
    chain->free_data = legacy_free;
    chain->copy_func = legacy_copy;
    chain->is_last = legacy_is_last;

    char *buf = malloc(c->len + 1u);
    memcpy(buf, c->text, c->len + 1u);
    Node *prev = NULL;
    for (char *tok = strtok(buf, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n"))
    {
        Node *cur = add_to_database(chain, tok);
        if (prev)
        {
            add_node_to_frequency_list(prev->data, cur->data);
        }
        prev = legacy_is_last(tok) ? NULL : cur;
    }
    free(buf);
    *states = (size_t) chain->database->size;
    return chain;
}

/* ---- benchmarks --------------------------------------------------------- */

typedef struct build_result
{
    double legacy_s;   /* < 0 when skipped */
    double modern_s;
    size_t states;
    uint64_t edges;
    size_t bytes;      /* libmarkov live bytes for the chain */
} build_result;

static mk_chain *modern_build(const corpus *c)
{
    mk_chain *chain = NULL;
    mk_chain_create(mk_plugin_ngram(), &chain);
    mk_text_build(chain, c->text, c->len, NULL, NULL);
    return chain;
}

static build_result bench_build(const corpus *c, bool run_legacy, int reps)
{
    build_result r = {-1.0, 0.0, 0, 0, 0};
    double t[9];
    for (int i = 0; i < reps; i++)
    {
        mk_alloc_stats before;
        mk_alloc_stats after;
        mk_alloc_get_stats(&before);
        double t0 = now_seconds();
        mk_chain *chain = modern_build(c);
        t[i] = now_seconds() - t0;
        mk_alloc_get_stats(&after);
        r.states = mk_chain_size(chain);
        r.edges = mk_chain_edge_count(chain);
        r.bytes = after.live_bytes - before.live_bytes;
        mk_chain_destroy(&chain);
    }
    r.modern_s = median(t, reps);

    if (run_legacy)
    {
        int legacy_reps = c->words > 100000u ? 1 : (reps < 3 ? reps : 3);
        for (int i = 0; i < legacy_reps; i++)
        {
            size_t states;
            double t0 = now_seconds();
            MarkovChain *chain = legacy_build(c, &states);
            t[i] = now_seconds() - t0;
            free_database(&chain);
            if (states != r.states)
            {
                fprintf(stderr, "state count mismatch on %s: %zu vs %zu\n",
                        c->name, states, r.states);
                exit(EXIT_FAILURE);
            }
        }
        r.legacy_s = median(t, legacy_reps);
    }
    return r;
}

static void print_time(double s)
{
    if (s < 0)
    {
        printf(" (skipped) |");
    }
    else if (s < 1e-3)
    {
        printf(" %.0f µs |", s * 1e6);
    }
    else if (s < 1.0)
    {
        printf(" %.1f ms |", s * 1e3);
    }
    else
    {
        printf(" %.2f s |", s);
    }
}

static void bench_sampling(int quick)
{
    const uint32_t degrees[] = {4, 16, 64, 256, 1024, 4096};
    const int draws = quick ? 50000 : 400000;
    printf("\n### Sampling a successor (ns per draw, median of 7 x %d draws)\n\n",
           draws);
    printf("| out-degree | original (linear scan, `rand()`) | linear scan "
           "(PCG32) | Vose alias (PCG32) | speedup |\n");
    printf("|---:|---:|---:|---:|---:|\n");

    for (size_t d = 0; d < sizeof(degrees) / sizeof(degrees[0]); d++)
    {
        uint32_t deg = degrees[d];
        /* Zipfian counts so the scan cannot stop early on average. */
        mk_chain *chain = NULL;
        mk_chain_create(mk_plugin_word(), &chain);
        MarkovChain *legacy = malloc(sizeof(MarkovChain));
        legacy->database = calloc(1, sizeof(LinkedList));
        legacy->comp_func = legacy_comp;
        legacy->copy_func = legacy_copy;
        legacy->free_data = legacy_free;
        legacy->is_last = legacy_is_last;
        legacy->print_func = legacy_print;

        char name[32];
        mk_state hub;
        mk_chain_intern(chain, "hub", &hub, NULL);
        char hub_name[] = "hub";
        Node *legacy_hub = add_to_database(legacy, hub_name);
        for (uint32_t i = 0; i < deg; i++)
        {
            snprintf(name, sizeof(name), "t%u", i);
            mk_state t;
            mk_chain_intern(chain, name, &t, NULL);
            uint32_t count = 1u + 1000u / (deg - i);
            mk_chain_add_transition(chain, hub, t, count);
            Node *lt = add_to_database(legacy, name);
            for (uint32_t k = 0; k < count; k++)
            {
                add_node_to_frequency_list(legacy_hub->data, lt->data);
            }
        }

        /* Median of REPS timed loops per sampler, interleaved so background
         * noise hits all three alike. */
        enum { REPS = 7 };
        double samples[3][REPS];
        mk_rng rng;
        mk_rng_seed(&rng, MK_RNG_PCG32, 1);
        volatile mk_state sink = 0;
        mk_state out = 0;
        mk_chain_next(chain, &rng, hub, MK_SAMPLE_ALIAS, &out); /* build the table */
        srand(1);
        for (int rep = 0; rep < REPS; rep++)
        {
            double t0 = now_seconds();
            for (int i = 0; i < draws; i++)
            {
                sink = (mk_state) (uintptr_t) get_next_random_node(legacy_hub->data);
            }
            samples[0][rep] = (now_seconds() - t0) * 1e9 / draws;
            for (int s = 0; s < 2; s++)
            {
                t0 = now_seconds();
                for (int i = 0; i < draws; i++)
                {
                    mk_chain_next(chain, &rng, hub, (mk_sampler) s, &out);
                    sink = out;
                }
                samples[s + 1][rep] = (now_seconds() - t0) * 1e9 / draws;
            }
        }
        (void) sink;
        double ns[3];
        for (int k = 0; k < 3; k++)
        {
            ns[k] = median(samples[k], REPS);
        }
        printf("| %u | %.1f | %.1f | %.1f | %.1f× |\n", deg, ns[0], ns[1],
               ns[2], ns[0] / ns[2]);
        mk_chain_destroy(&chain);
        free_database(&legacy);
    }
}

int main(int argc, char *argv[])
{
    int quick = 0;
    int sampling_only = 0;
    corpus corpora[32];
    int n = 0;
    for (int i = 1; i < argc && n < 24; i++)
    {
        if (strcmp(argv[i], "--quick") == 0)
        {
            quick = 1;
        }
        else if (strcmp(argv[i], "--sampling-only") == 0)
        {
            sampling_only = 1;
        }
        else if (load_corpus(argv[i], &corpora[n]))
        {
            n++;
        }
        else
        {
            fprintf(stderr, "cannot read %s\n", argv[i]);
        }
    }
    if (sampling_only)
    {
        bench_sampling(quick);
        return EXIT_SUCCESS;
    }
    const size_t sizes[] = {10000, 30000, 100000, 300000, 1000000, 3000000};
    int n_sizes = quick ? 3 : 6;
    for (int i = 0; i < n_sizes; i++)
    {
        synthetic_corpus(sizes[i], 200000, &corpora[n++]);
    }

    printf("### Building the chain (order 1)\n\n");
    printf("| corpus | words | states | transitions | original |"
           " libmarkov | speedup | libmarkov memory |\n");
    printf("|---|---:|---:|---:|---:|---:|---:|---:|\n");
    for (int i = 0; i < n; i++)
    {
        /* The original is O(words x states); past ~300k words it takes
         * minutes, so it is skipped there. */
        bool legacy = corpora[i].words <= 300000u;
        build_result r = bench_build(&corpora[i], legacy, quick ? 3 : 5);
        printf("| %s | %zu | %zu | %llu |", corpora[i].name, corpora[i].words,
               r.states, (unsigned long long) r.edges);
        print_time(r.legacy_s);
        print_time(r.modern_s);
        if (r.legacy_s > 0)
        {
            printf(" **%.0f×** |", r.legacy_s / r.modern_s);
        }
        else
        {
            printf(" — |");
        }
        printf(" %.1f MiB |\n", (double) r.bytes / (1024.0 * 1024.0));
        fflush(stdout);
        free(corpora[i].text);
    }

    bench_sampling(quick);
    return EXIT_SUCCESS;
}
