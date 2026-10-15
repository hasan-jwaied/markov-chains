#include <string.h>
#include "mk_test.h"

/*
 * Allocation-failure testing: run a pipeline that touches every allocating
 * code path, making the k-th allocation fail for k = 0, 1, 2, ... until the
 * pipeline succeeds. Every run must either succeed or report MK_ERR_NOMEM,
 * and must leave zero live allocations behind (mk_test_run checks the
 * totals; this test checks after every single injected failure).
 */

static const char *CORPUS =
    "the quick brown fox jumps over the lazy dog. the dog sleeps. "
    "a quick brown dog jumps over the fox. the lazy fox sleeps all day. "
    "foxes and dogs are friends. the end.";

static mk_status text_pipeline(void)
{
    mk_chain *chain = NULL;
    mk_status st = mk_chain_create(mk_plugin_ngram(), &chain);
    mk_text_options opts = mk_text_defaults();
    opts.order = 2;
    if (st == MK_OK)
    {
        st = mk_text_build(chain, CORPUS, strlen(CORPUS), &opts, NULL);
    }
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 1);
    mk_generate_opts gen = mk_generate_defaults();
    gen.start_policy = MK_START_EMPIRICAL;
    mk_state out[20];
    size_t n;
    for (int i = 0; st == MK_OK && i < 5; i++)
    {
        st = mk_chain_generate(chain, &rng, &gen, out, 20, &n);
    }
    if (st == MK_OK)
    {
        double pi[64];
        mk_stationary_opts so = mk_stationary_defaults();
        so.damping = 0.85;
        st = mk_chain_size(chain) <= 64 ? mk_math_stationary(chain, &so, pi, NULL)
                                        : MK_ERR_LIMIT;
    }
    mk_chain_destroy(&chain);
    return st;
}

static mk_status snakes_pipeline(void)
{
    mk_chain *chain = NULL;
    mk_status st = mk_chain_create(mk_plugin_cell(), &chain);
    if (st == MK_OK)
    {
        st = mk_snakes_build(chain, mk_snakes_classic_board());
    }
    mk_absorbing a;
    if (st == MK_OK)
    {
        st = mk_math_absorbing(chain, &a);
        if (st == MK_OK)
        {
            mk_absorbing_free(&a);
        }
    }
    double steps[100];
    double pmf[51];
    if (st == MK_OK)
    {
        st = mk_math_expected_steps(chain, steps, 1e-10, 10000);
    }
    if (st == MK_OK)
    {
        st = mk_math_absorption_pmf(chain, 0, 50, pmf, NULL);
    }
    mk_chain_destroy(&chain);
    return st;
}

static void run_with_faults(mk_status (*pipeline)(void), const char *name)
{
    mk_alloc_stats base;
    mk_alloc_get_stats(&base);
    long k = 0;
    for (;; k++)
    {
        mk_alloc_fail_after(k);
        mk_status st = pipeline();
        mk_alloc_fail_after(-1);

        mk_alloc_stats now;
        mk_alloc_get_stats(&now);
        if (now.live_blocks != base.live_blocks)
        {
            MK_FAIL_AT(__FILE__, __LINE__, "%s leaked when allocation %ld "
                       "failed", name, k);
            return;
        }
        if (st == MK_OK)
        {
            break;
        }
        if (st != MK_ERR_NOMEM)
        {
            MK_FAIL_AT(__FILE__, __LINE__, "%s: allocation %ld failed but "
                       "status was '%s'", name, k, mk_status_str(st));
            return;
        }
    }
    CHECK(k > 20); /* the pipeline really allocates; every point was hit */
}

static void test_text_pipeline_faults(void)
{
    run_with_faults(text_pipeline, "text pipeline");
}

static void test_snakes_pipeline_faults(void)
{
    run_with_faults(snakes_pipeline, "snakes pipeline");
}

void suite_faults(void)
{
    RUN(test_text_pipeline_faults);
    RUN(test_snakes_pipeline_faults);
}
