#include <string.h>
#include "mk_test.h"

static mk_chain *word_chain(void)
{
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_word(), &chain));
    return chain;
}

static mk_state intern(mk_chain *chain, const char *word)
{
    mk_state id = MK_NO_STATE;
    CHECK_OK(mk_chain_intern(chain, word, &id, NULL));
    return id;
}

static void test_create_rejects_bad_vtable(void)
{
    mk_vtable broken = *mk_plugin_word();
    broken.compare = NULL;
    mk_chain *chain = (mk_chain *) &broken; /* must be reset to NULL */
    CHECK_STATUS(mk_chain_create(&broken, &chain), MK_ERR_INVALID);
    CHECK(chain == NULL);
    CHECK_STATUS(mk_chain_create(NULL, &chain), MK_ERR_INVALID);
    CHECK_STATUS(mk_chain_create(mk_plugin_word(), NULL), MK_ERR_INVALID);
    mk_chain_destroy(NULL);
    mk_chain_destroy(&chain);
}

static void test_intern_deduplicates(void)
{
    mk_chain *chain = word_chain();
    bool created = false;
    mk_state a;
    CHECK_OK(mk_chain_intern(chain, "alpha", &a, &created));
    CHECK(created);
    char copy[] = "alpha"; /* a different pointer to equal data */
    mk_state again;
    CHECK_OK(mk_chain_intern(chain, copy, &again, &created));
    CHECK(!created);
    CHECK_EQ(again, a);
    mk_state b = intern(chain, "beta.");
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 1);
    CHECK_EQ(mk_chain_size(chain), 2);
    CHECK_EQ(mk_chain_find(chain, "beta."), b);
    CHECK_EQ(mk_chain_find(chain, "gamma"), MK_NO_STATE);
    CHECK(strcmp(mk_chain_data(chain, a), "alpha") == 0);
    CHECK(mk_chain_data(chain, a) != (const void *) copy); /* owned copy */
    CHECK(!mk_chain_is_terminal(chain, a));
    CHECK(mk_chain_is_terminal(chain, b));
    CHECK(mk_chain_data(chain, 99) == NULL);

    char label[4];
    CHECK_EQ(mk_chain_format(chain, a, label, sizeof(label)), 5);
    CHECK(strcmp(label, "alp") == 0);
    mk_chain_destroy(&chain);
    CHECK(chain == NULL);
}

static void test_transitions(void)
{
    mk_chain *chain = word_chain();
    mk_state a = intern(chain, "a");
    mk_state b = intern(chain, "b");
    mk_state c = intern(chain, "c.");
    CHECK_OK(mk_chain_add_transition(chain, a, c, 1));
    CHECK_OK(mk_chain_add_transition(chain, a, b, 2));
    CHECK_OK(mk_chain_add_transition(chain, a, c, 4));

    CHECK_EQ(mk_chain_out_degree(chain, a), 2);
    CHECK_EQ(mk_chain_out_total(chain, a), 7);
    CHECK_EQ(mk_chain_edge_count(chain), 2);
    CHECK_EQ(mk_chain_transition_total(chain), 7);
    const mk_edge *edges = mk_chain_edges(chain, a);
    CHECK(edges != NULL);
    CHECK_EQ(edges[0].target, c); /* insertion order is preserved */
    CHECK_EQ(edges[0].count, 5);
    CHECK_EQ(edges[1].target, b);
    CHECK(mk_chain_edges(chain, b) == NULL);

    CHECK_STATUS(mk_chain_add_transition(chain, a, 42, 1), MK_ERR_INVALID);
    CHECK_STATUS(mk_chain_add_transition(chain, a, b, 0), MK_ERR_INVALID);
    CHECK_STATUS(mk_chain_add_transition(chain, a, b, UINT32_MAX),
                 MK_ERR_LIMIT);
    mk_chain_destroy(&chain);
}

static void test_many_states(void)
{
    /* Exercise index and edge-table growth. */
    mk_chain *chain = word_chain();
    char word[32];
    mk_state prev = MK_NO_STATE;
    for (int i = 0; i < 20000; i++)
    {
        snprintf(word, sizeof(word), "w%d", i);
        mk_state id = intern(chain, word);
        CHECK_EQ(id, (mk_state) i);
        if (prev != MK_NO_STATE)
        {
            CHECK_OK(mk_chain_add_transition(chain, prev, id, 1));
            CHECK_OK(mk_chain_add_transition(chain, id, prev, 1));
        }
        prev = id;
    }
    int found = 0;
    for (int i = 0; i < 20000; i += 7)
    {
        snprintf(word, sizeof(word), "w%d", i);
        found += mk_chain_find(chain, word) == (mk_state) i;
    }
    CHECK_EQ(found, (20000 + 6) / 7);
    CHECK_EQ(mk_chain_edge_count(chain), 2 * 19999);
    mk_chain_destroy(&chain);
}

static void test_start_policies(void)
{
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 3);
    mk_state s;

    /* Every state terminal: the original looped forever; now an error. */
    mk_chain *dots = make_text_chain("a. b. c.", 1);
    CHECK_STATUS(mk_chain_random_start(dots, &rng, MK_START_COMPAT, &s),
                 MK_ERR_NO_START);
    CHECK_STATUS(mk_chain_random_start(dots, &rng, MK_START_UNIFORM, &s),
                 MK_ERR_NO_START);
    mk_chain_destroy(&dots);

    mk_chain *empty = word_chain();
    CHECK_STATUS(mk_chain_random_start(empty, &rng, MK_START_COMPAT, &s),
                 MK_ERR_EMPTY);
    mk_chain_destroy(&empty);

    /* "x" is non-terminal but a dead end (the corpus stops mid-sentence):
     * COMPAT may pick it, UNIFORM never does. */
    mk_chain *chain = make_text_chain("one two. two one. x", 1);
    mk_state x = find_label(chain, "x");
    CHECK(x != MK_NO_STATE);
    int compat_dead_ends = 0;
    for (int i = 0; i < 2000; i++)
    {
        CHECK_OK(mk_chain_random_start(chain, &rng, MK_START_COMPAT, &s));
        CHECK(!mk_chain_is_terminal(chain, s));
        compat_dead_ends += s == x;
        CHECK_OK(mk_chain_random_start(chain, &rng, MK_START_UNIFORM, &s));
        CHECK(s != x && mk_chain_out_degree(chain, s) > 0);
    }
    CHECK(compat_dead_ends > 0);

    /* EMPIRICAL: "one" and "two" each begin one sentence, "x" one more. */
    int counts[8] = {0};
    for (int i = 0; i < 30000; i++)
    {
        CHECK_OK(mk_chain_random_start(chain, &rng, MK_START_EMPIRICAL, &s));
        counts[s]++;
    }
    for (mk_state i = 0; i < mk_chain_size(chain); i++)
    {
        CHECK_NEAR(counts[i] / 30000.0,
                   mk_chain_start_count(chain, i) / 3.0, 0.02);
    }
    mk_chain_destroy(&chain);
}

static void test_samplers_agree_with_counts(void)
{
    mk_chain *chain = word_chain();
    mk_state hub = intern(chain, "hub");
    const uint32_t weights[] = {1, 3, 6, 10};
    mk_state targets[4];
    for (int i = 0; i < 4; i++)
    {
        char name[8];
        snprintf(name, sizeof(name), "t%d.", i);
        targets[i] = intern(chain, name);
        CHECK_OK(mk_chain_add_transition(chain, hub, targets[i], weights[i]));
    }
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 11);
    for (int sampler = MK_SAMPLE_LINEAR; sampler <= MK_SAMPLE_ALIAS; sampler++)
    {
        int counts[5] = {0};
        for (int i = 0; i < 100000; i++)
        {
            mk_state next;
            CHECK_OK(mk_chain_next(chain, &rng, hub, (mk_sampler) sampler,
                                   &next));
            counts[next]++;
        }
        for (int i = 0; i < 4; i++)
        {
            CHECK_NEAR(counts[targets[i]] / 100000.0, weights[i] / 20.0, 0.01);
        }
    }
    /* Adding observations must invalidate the cached alias table. */
    CHECK_OK(mk_chain_add_transition(chain, hub, targets[0], 980));
    int first = 0;
    for (int i = 0; i < 10000; i++)
    {
        mk_state next;
        CHECK_OK(mk_chain_next(chain, &rng, hub, MK_SAMPLE_ALIAS, &next));
        first += next == targets[0];
    }
    CHECK_NEAR(first / 10000.0, 981.0 / 1000.0, 0.01);

    mk_state next;
    CHECK_STATUS(mk_chain_next(chain, &rng, targets[0], MK_SAMPLE_ALIAS, &next),
                 MK_ERR_DEAD_END);
    mk_chain_destroy(&chain);
}

static void test_temperature(void)
{
    mk_chain *chain = word_chain();
    mk_state hub = intern(chain, "hub");
    mk_state rare = intern(chain, "rare.");
    mk_state common = intern(chain, "common.");
    CHECK_OK(mk_chain_add_transition(chain, hub, rare, 1));
    CHECK_OK(mk_chain_add_transition(chain, hub, common, 9));
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 5);

    int cold = 0;
    int hot = 0;
    int unit = 0;
    for (int i = 0; i < 20000; i++)
    {
        mk_state s;
        CHECK_OK(mk_chain_next_tempered(chain, &rng, hub, 0.1, &s));
        cold += s == common;
        CHECK_OK(mk_chain_next_tempered(chain, &rng, hub, 100.0, &s));
        hot += s == common;
        CHECK_OK(mk_chain_next_tempered(chain, &rng, hub, 1.0, &s));
        unit += s == common;
    }
    CHECK(cold > 19990);                       /* 9^10 : 1 */
    CHECK_NEAR(hot / 20000.0, 0.5055, 0.02);   /* 9^0.01 / (1 + 9^0.01) */
    CHECK_NEAR(unit / 20000.0, 0.9, 0.01);
    mk_state s;
    CHECK_STATUS(mk_chain_next_tempered(chain, &rng, hub, 0.0, &s),
                 MK_ERR_INVALID);
    mk_chain_destroy(&chain);
}

static void test_generate(void)
{
    mk_chain *chain = make_text_chain("a b c d e f g h.", 1);
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 1);
    mk_generate_opts opts = mk_generate_defaults();
    mk_state out[32];
    size_t n = 0;

    opts.start = find_label(chain, "a");
    CHECK(opts.start != MK_NO_STATE);
    CHECK_OK(mk_chain_generate(chain, &rng, &opts, out, 32, &n));
    CHECK_EQ(n, 8); /* stops on the terminal "h." */
    CHECK(mk_chain_is_terminal(chain, out[n - 1]));

    opts.max_length = 3;
    CHECK_OK(mk_chain_generate(chain, &rng, &opts, out, 32, &n));
    CHECK_EQ(n, 3);

    CHECK_STATUS(mk_chain_generate(chain, &rng, &opts, out, 2, &n),
                 MK_ERR_INVALID); /* capacity < max_length */
    opts.max_length = 0;
    CHECK_STATUS(mk_chain_generate(chain, &rng, &opts, out, 32, &n),
                 MK_ERR_INVALID);
    mk_chain_destroy(&chain);

    /* A walk that reaches a dead end just stops. */
    mk_chain *cut = make_text_chain("p q r", 1);
    opts = mk_generate_defaults();
    opts.start = 0;
    CHECK_OK(mk_chain_generate(cut, &rng, &opts, out, 32, &n));
    CHECK_EQ(n, 3);
    mk_chain_destroy(&cut);
}

void suite_chain(void)
{
    RUN(test_create_rejects_bad_vtable);
    RUN(test_intern_deduplicates);
    RUN(test_transitions);
    RUN(test_many_states);
    RUN(test_start_policies);
    RUN(test_samplers_agree_with_counts);
    RUN(test_temperature);
    RUN(test_generate);
}
