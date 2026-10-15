#include <stdlib.h>
#include <string.h>
#include "mk_test.h"

static void test_terminal_rules(void)
{
    const mk_terminal_rule P = MK_TERMINAL_PERIOD;
    const mk_terminal_rule S = MK_TERMINAL_SENTENCE;
    CHECK(mk_text_is_terminal_word("end.", 4, P));
    CHECK(!mk_text_is_terminal_word("end!", 4, P));
    CHECK(!mk_text_is_terminal_word("", 0, P));
    CHECK(mk_text_is_terminal_word("end!", 4, S));
    CHECK(mk_text_is_terminal_word("why?", 4, S));
    CHECK(mk_text_is_terminal_word("said.\"", 6, S));
    CHECK(mk_text_is_terminal_word("(really.)", 9, S));
    CHECK(mk_text_is_terminal_word("done.\xE2\x80\x9D", 8, S)); /* ” */
    CHECK(mk_text_is_terminal_word("it.\xE2\x80\x99", 6, S));   /* ’ */
    CHECK(!mk_text_is_terminal_word("\"", 1, S));
    CHECK(!mk_text_is_terminal_word("Mr", 2, S));
}

static void test_order_one_matches_original(void)
{
    /* The original rules: transitions within a sentence only, every word a
     * state, first-seen order gives the ids. */
    mk_chain *chain = make_text_chain("a b a c.\r\n  b a.", 1);
    CHECK(chain != NULL);
    CHECK_EQ(mk_chain_size(chain), 4); /* a, b, c. and a. ("a." != "a") */
    mk_state a = find_label(chain, "a");
    mk_state b = find_label(chain, "b");
    mk_state c = find_label(chain, "c.");
    mk_state a_dot = find_label(chain, "a.");
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 1);
    CHECK_EQ(mk_chain_out_degree(chain, a), 2);  /* a->b, a->c. */
    CHECK_EQ(mk_chain_out_degree(chain, b), 2);  /* b->a, b->a. */
    CHECK_EQ(mk_chain_out_degree(chain, c), 0);  /* never crosses '.' */
    CHECK_EQ(mk_chain_edges(chain, b)[1].target, a_dot);
    CHECK_EQ(mk_chain_start_count(chain, a), 1);
    CHECK_EQ(mk_chain_start_count(chain, b), 1);
    mk_chain_destroy(&chain);
}

static void test_max_words(void)
{
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_ngram(), &chain));
    mk_text_options opts = mk_text_defaults();
    opts.max_words = 3;
    mk_text_stats stats = {0, 0};
    const char *text = "one two three four five.";
    CHECK_OK(mk_text_build(chain, text, strlen(text), &opts, &stats));
    CHECK_EQ(stats.words, 3);
    CHECK_EQ(mk_chain_size(chain), 3);
    opts.max_words = 0;
    CHECK_OK(mk_text_build(chain, text, strlen(text), &opts, &stats));
    CHECK_EQ(mk_chain_size(chain), 3);
    mk_chain_destroy(&chain);
}

static void test_higher_order(void)
{
    mk_chain *chain = make_text_chain("the cat sat down. the cat ran. hi.", 2);
    CHECK(chain != NULL);
    mk_state the_cat = find_label(chain, "the cat");
    mk_state cat_sat = find_label(chain, "cat sat");
    mk_state cat_ran = find_label(chain, "cat ran.");
    mk_state hi = find_label(chain, "hi.");
    CHECK(the_cat != MK_NO_STATE && cat_sat != MK_NO_STATE);
    CHECK(cat_ran != MK_NO_STATE && hi != MK_NO_STATE);
    CHECK_EQ(mk_chain_start_count(chain, the_cat), 2);
    CHECK_EQ(mk_chain_out_degree(chain, the_cat), 2);
    CHECK(mk_chain_is_terminal(chain, hi)); /* short sentence: one state */
    CHECK_EQ(mk_chain_start_count(chain, hi), 1);
    CHECK_EQ(mk_chain_size(chain), 5);

    const mk_ngram *gram = mk_chain_data(chain, cat_ran);
    CHECK_EQ(gram->order, 2);
    CHECK(strcmp(mk_ngram_word(gram, 0), "cat") == 0);
    CHECK(strcmp(mk_ngram_last_word(gram), "ran.") == 0);

    /* Rendering: all words of the first state, then one per step. */
    mk_state seq[] = {the_cat, cat_ran};
    char buf[64];
    CHECK_EQ(mk_text_render(chain, seq, 2, MK_RENDER_CLEAN, buf, sizeof(buf)),
             12);
    CHECK(strcmp(buf, "the cat ran.") == 0);
    mk_chain_destroy(&chain);
}

static void test_render_styles(void)
{
    mk_chain *chain = make_text_chain("x y z.", 1);
    mk_state seq[] = {0, 1, 2};
    char buf[32];
    mk_text_render(chain, seq, 2, MK_RENDER_COMPAT, buf, sizeof(buf));
    CHECK(strcmp(buf, "x y ") == 0); /* original trailing space */
    mk_text_render(chain, seq, 3, MK_RENDER_COMPAT, buf, sizeof(buf));
    CHECK(strcmp(buf, "x y z.") == 0);
    mk_text_render(chain, seq, 2, MK_RENDER_CLEAN, buf, sizeof(buf));
    CHECK(strcmp(buf, "x y") == 0);
    /* snprintf semantics: truncate, terminate, report the full length */
    CHECK_EQ(mk_text_render(chain, seq, 3, MK_RENDER_CLEAN, buf, 4), 6);
    CHECK(strcmp(buf, "x y") == 0);
    CHECK_EQ(mk_text_render(chain, seq, 3, MK_RENDER_CLEAN, NULL, 0), 6);
    mk_chain_destroy(&chain);
}

static void test_tokenizer_edge_cases(void)
{
    /* NUL is a delimiter; a 5000-byte word stays one word (the legacy
     * fgets bug split it). */
    char text[5100];
    memset(text, 'x', 5000);
    memcpy(text + 5000, " a\0b.", 6);
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_ngram(), &chain));
    mk_text_stats stats = {0, 0};
    CHECK_OK(mk_text_build(chain, text, 5006, NULL, &stats));
    CHECK_EQ(stats.words, 3);
    CHECK_EQ(stats.sentences, 1);
    CHECK_EQ(mk_chain_size(chain), 3);
    CHECK(find_label(chain, "b.") != MK_NO_STATE);
    mk_chain_destroy(&chain);

    CHECK_OK(mk_chain_create(mk_plugin_ngram(), &chain));
    CHECK_OK(mk_text_build(chain, "", 0, NULL, NULL));
    CHECK_OK(mk_text_build(chain, " \t\r\n", 4, NULL, NULL));
    CHECK_EQ(mk_chain_size(chain), 0);
    mk_chain_destroy(&chain);
}

static void test_invalid_arguments(void)
{
    mk_chain *words = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_word(), &words));
    CHECK_STATUS(mk_text_build(words, "a", 1, NULL, NULL), MK_ERR_INVALID);
    mk_chain_destroy(&words);

    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_ngram(), &chain));
    mk_text_options opts = mk_text_defaults();
    opts.order = 0;
    CHECK_STATUS(mk_text_build(chain, "a", 1, &opts, NULL), MK_ERR_INVALID);
    opts.order = MK_NGRAM_MAX_ORDER + 1;
    CHECK_STATUS(mk_text_build(chain, "a", 1, &opts, NULL), MK_ERR_INVALID);
    CHECK_STATUS(mk_text_build(chain, NULL, 1, NULL, NULL), MK_ERR_INVALID);
    mk_chain_destroy(&chain);
}

void suite_text(void)
{
    RUN(test_terminal_rules);
    RUN(test_order_one_matches_original);
    RUN(test_max_words);
    RUN(test_higher_order);
    RUN(test_render_styles);
    RUN(test_tokenizer_edge_cases);
    RUN(test_invalid_arguments);
}
