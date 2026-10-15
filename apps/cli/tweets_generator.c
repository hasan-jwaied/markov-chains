/*
 * tweets_generator: generate random "tweets" from a text corpus.
 *
 *   tweets_generator [options] <seed> <num_tweets> <corpus> [words_to_read]
 *
 * By default it reproduces the original assignment program exactly (glibc
 * rand(), the original sampling and spacing), which the golden tests check
 * byte for byte. --modern switches to PCG32, O(1) alias sampling, starts
 * drawn from real sentence openings, and clean spacing.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli_common.h"
#include "markov/markov.h"

#define MAX_TWEET_LENGTH 20

static void usage(FILE *out)
{
    fprintf(out,
            "Usage: tweets_generator [--modern] [--order K] [--sentence]\n"
            "                        <seed> <num_tweets> <corpus> "
            "[words_to_read]\n"
            "  --modern    PCG32 + alias sampling + empirical starts\n"
            "  --order K   use the last K words as the state (1-%d)\n"
            "  --sentence  end sentences at . ! ? (default: words ending "
            "in '.')\n",
            MK_NGRAM_MAX_ORDER);
}

int main(int argc, char *argv[])
{
    bool modern = false;
    mk_text_options text = mk_text_defaults();

    int arg = 1;
    for (; arg < argc && strncmp(argv[arg], "--", 2) == 0; arg++)
    {
        unsigned long order;
        if (strcmp(argv[arg], "--modern") == 0)
        {
            modern = true;
        }
        else if (strcmp(argv[arg], "--sentence") == 0)
        {
            text.terminal_rule = MK_TERMINAL_SENTENCE;
        }
        else if (strcmp(argv[arg], "--order") == 0 && arg + 1 < argc
                 && cli_parse_ulong(argv[arg + 1], MK_NGRAM_MAX_ORDER, "order",
                                    &order) && order >= 1)
        {
            text.order = (uint32_t) order;
            arg++;
        }
        else if (strcmp(argv[arg], "--help") == 0)
        {
            usage(stdout);
            return EXIT_SUCCESS;
        }
        else
        {
            usage(stderr);
            return EXIT_FAILURE;
        }
    }

    int positional = argc - arg;
    if (positional != 3 && positional != 4)
    {
        fprintf(stderr, "Usage: invalid number of arguments\n");
        usage(stderr);
        return EXIT_FAILURE;
    }
    unsigned long seed;
    unsigned long num_tweets;
    unsigned long words = 0;
    if (!cli_parse_ulong(argv[arg], UINT_MAX, "seed", &seed)
        || !cli_parse_ulong(argv[arg + 1], INT_MAX, "num_tweets", &num_tweets)
        || (positional == 4
            && !cli_parse_ulong(argv[arg + 3], INT_MAX, "words_to_read",
                                &words)))
    {
        return EXIT_FAILURE;
    }
    text.max_words = positional == 4 ? (int64_t) words : -1;

    char *corpus;
    size_t corpus_len;
    if (!cli_read_file(argv[arg + 2], &corpus, &corpus_len))
    {
        fprintf(stderr, "Error: incorrect file path\n");
        return EXIT_FAILURE;
    }

    mk_chain *chain = NULL;
    mk_status status = mk_chain_create(mk_plugin_ngram(), &chain);
    if (status == MK_OK)
    {
        status = mk_text_build(chain, corpus, corpus_len, &text, NULL);
    }
    free(corpus);

    mk_rng rng;
    mk_rng_seed(&rng, modern ? MK_RNG_PCG32 : MK_RNG_GLIBC, seed);
    mk_generate_opts gen = mk_generate_defaults();
    gen.max_length = MAX_TWEET_LENGTH;
    gen.start_policy = modern ? MK_START_EMPIRICAL : MK_START_COMPAT;
    gen.sampler = modern ? MK_SAMPLE_ALIAS : MK_SAMPLE_LINEAR;
    mk_render_style style = modern ? MK_RENDER_CLEAN : MK_RENDER_COMPAT;

    mk_state states[MAX_TWEET_LENGTH];
    char line[4096];
    for (unsigned long i = 0; status == MK_OK && i < num_tweets; i++)
    {
        size_t n;
        status = mk_chain_generate(chain, &rng, &gen, states,
                                   MAX_TWEET_LENGTH, &n);
        if (status != MK_OK)
        {
            break;
        }
        size_t need = mk_text_render(chain, states, n, style, line,
                                     sizeof(line));
        if (need < sizeof(line))
        {
            printf("Tweet %lu: %s\n", i + 1u, line);
            continue;
        }
        char *big = malloc(need + 1u); /* rare: very long words */
        if (big == NULL)
        {
            status = MK_ERR_NOMEM;
            break;
        }
        mk_text_render(chain, states, n, style, big, need + 1u);
        printf("Tweet %lu: %s\n", i + 1u, big);
        free(big);
    }

    mk_chain_destroy(&chain);
    if (status != MK_OK)
    {
        fprintf(stderr, "Error: %s\n", mk_status_str(status));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
