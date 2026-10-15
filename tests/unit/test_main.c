#include <stdlib.h>
#include <string.h>
#include "mk_test.h"

int mk_test_failures = 0;
int mk_test_checks = 0;

static const char *g_filter;
static int g_tests = 0;
static int g_failed_tests = 0;

void mk_test_run(const char *name, mk_test_fn fn)
{
    if (g_filter && strstr(name, g_filter) == NULL)
    {
        return;
    }
    mk_alloc_stats before;
    mk_alloc_stats after;
    mk_alloc_get_stats(&before);
    int failures = mk_test_failures;

    fn();

    mk_alloc_fail_after(-1);
    mk_alloc_set_hook(NULL, NULL);
    mk_alloc_get_stats(&after);
    if (after.live_blocks != before.live_blocks)
    {
        MK_FAIL_AT(__FILE__, __LINE__, "leak: %zu block(s), %zu byte(s)",
                   after.live_blocks - before.live_blocks,
                   after.live_bytes - before.live_bytes);
    }
    g_tests++;
    if (mk_test_failures != failures)
    {
        g_failed_tests++;
        fprintf(stderr, "FAIL %s\n", name);
    }
}

mk_chain *make_text_chain(const char *text, uint32_t order)
{
    mk_chain *chain = NULL;
    if (mk_chain_create(mk_plugin_ngram(), &chain) != MK_OK)
    {
        return NULL;
    }
    mk_text_options opts = mk_text_defaults();
    opts.order = order;
    if (mk_text_build(chain, text, strlen(text), &opts, NULL) != MK_OK)
    {
        mk_chain_destroy(&chain);
    }
    return chain;
}

mk_state find_label(const mk_chain *chain, const char *label)
{
    char buf[256];
    for (mk_state i = 0; i < mk_chain_size(chain); i++)
    {
        mk_chain_format(chain, i, buf, sizeof(buf));
        if (strcmp(buf, label) == 0)
        {
            return i;
        }
    }
    return MK_NO_STATE;
}

int main(int argc, char *argv[])
{
    g_filter = argc > 1 ? argv[1] : NULL;

    suite_alloc();
    suite_rng();
    suite_alias();
    suite_chain();
    suite_text();
    suite_snakes();
    suite_math();
    suite_faults();

    printf("%d tests, %d checks, %d failed test(s)\n", g_tests,
           mk_test_checks, g_failed_tests);
    return mk_test_failures == 0 && g_tests > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
