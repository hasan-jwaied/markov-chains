#include "mk_test.h"

/*
 * The integer alias table is exact: summing what every column contributes to
 * outcome i must give exactly n * w_i units (out of n * W). Check that.
 */
static void check_exact(const uint64_t *w, uint32_t n)
{
    mk_alias table;
    CHECK_OK(mk_alias_build(&table, w, n));
    uint64_t total = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        total += w[i];
    }
    CHECK_EQ(table.total, total);
    for (uint32_t i = 0; i < n; i++)
    {
        uint64_t units = 0;
        for (uint32_t c = 0; c < n; c++)
        {
            CHECK(table.threshold[c] <= total);
            if (c == i)
            {
                units += table.threshold[c];
            }
            if (table.alias[c] == i && table.threshold[c] < total)
            {
                units += total - table.threshold[c];
            }
        }
        CHECK_EQ(units, w[i] * n);
    }
    mk_alias_free(&table);
}

static void test_exact_tables(void)
{
    const uint64_t one[] = {5};
    const uint64_t even[] = {1, 1, 1, 1};
    const uint64_t skewed[] = {1, 0, 0, 97, 2};
    const uint64_t zipf[] = {60, 30, 20, 15, 12, 10, 8, 7, 6, 6};
    check_exact(one, 1);
    check_exact(even, 4);
    check_exact(skewed, 5);
    check_exact(zipf, 10);

    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 1);
    uint64_t w[300];
    for (int round = 0; round < 20; round++)
    {
        uint32_t n = 1u + mk_rng_below(&rng, 300);
        for (uint32_t i = 0; i < n; i++)
        {
            w[i] = mk_rng_below(&rng, 4) == 0 ? 0u : mk_rng_below(&rng, 1000u);
        }
        w[mk_rng_below(&rng, n)] += 1; /* never all zero */
        check_exact(w, n);
    }
}

static void test_sampling_frequencies(void)
{
    const uint64_t w[] = {1, 2, 3, 4};
    mk_alias table;
    CHECK_OK(mk_alias_build(&table, w, 4));
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 7);
    long counts[4] = {0};
    for (int i = 0; i < 100000; i++)
    {
        counts[mk_alias_sample(&table, &rng)]++;
    }
    for (int i = 0; i < 4; i++)
    {
        CHECK_NEAR((double) counts[i] / 100000.0, (double) w[i] / 10.0, 0.01);
    }
    mk_alias_free(&table);
    mk_alias_free(&table); /* idempotent */
}

static void test_invalid_input(void)
{
    mk_alias table;
    const uint64_t zeros[] = {0, 0};
    const uint64_t huge[] = {UINT64_MAX / 2 + 1, 1}; /* n * w overflows */
    CHECK_STATUS(mk_alias_build(&table, zeros, 0), MK_ERR_INVALID);
    CHECK_STATUS(mk_alias_build(&table, zeros, 2), MK_ERR_INVALID);
    CHECK_STATUS(mk_alias_build(&table, huge, 2), MK_ERR_LIMIT);
    CHECK_STATUS(mk_alias_build(NULL, zeros, 2), MK_ERR_INVALID);
}

void suite_alias(void)
{
    RUN(test_exact_tables);
    RUN(test_sampling_frequencies);
    RUN(test_invalid_input);
}
