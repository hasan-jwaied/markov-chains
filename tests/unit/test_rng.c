#include <limits.h>
#include <stdlib.h>
#include "mk_test.h"

/* Reference output of pcg32-demo (pcg32_srandom(42, 54)), from pcg-c. */
static void test_pcg32_reference_vector(void)
{
    static const uint32_t expected[] = {
        0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
        0x83d2f293u, 0xbfa4784bu, 0xcbed606eu
    };
    mk_rng rng;
    mk_rng_seed_pcg32(&rng, 42u, 54u);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        CHECK_EQ(mk_rng_next(&rng), expected[i]);
    }
}

static void test_glibc_matches_libc(void)
{
#ifdef __GLIBC__
    static const unsigned seeds[] = {0u, 1u, 2u, 7u, 42u, 12345u,
                                     2147483647u, 2147483648u, UINT_MAX};
    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++)
    {
        mk_rng rng;
        mk_rng_seed(&rng, MK_RNG_GLIBC, seeds[s]);
        srand(seeds[s]);
        int mismatches = 0;
        for (int i = 0; i < 5000; i++)
        {
            mismatches += (int) mk_rng_next(&rng) != rand();
        }
        CHECK_EQ(mismatches, 0);

        /* ... including the original `rand() % n` reduction */
        mk_rng_seed(&rng, MK_RNG_GLIBC, seeds[s]);
        srand(seeds[s]);
        for (uint32_t n = 1; n < 500; n++)
        {
            mismatches += mk_rng_below(&rng, n) != (uint32_t) (rand() % (int) n);
        }
        CHECK_EQ(mismatches, 0);
    }
#else
    printf("  (skipped: not glibc)\n");
#endif
}

static void test_below_is_uniform(void)
{
    /* Chi-squared over 7 buckets; 6 degrees of freedom, p ~ 1e-6 at 35. */
    enum { BUCKETS = 7, DRAWS = 70000 };
    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, 2026);
    long counts[BUCKETS] = {0};
    for (int i = 0; i < DRAWS; i++)
    {
        uint32_t r = mk_rng_below(&rng, BUCKETS);
        CHECK(r < BUCKETS);
        counts[r]++;
    }
    double chi2 = 0.0;
    double expected = (double) DRAWS / (double) BUCKETS;
    for (int i = 0; i < BUCKETS; i++)
    {
        chi2 += ((double) counts[i] - expected) * ((double) counts[i] - expected) / expected;
    }
    CHECK(chi2 < 35.0);
}

static void test_ranges_and_determinism(void)
{
    mk_rng a;
    mk_rng b;
    mk_rng_seed(&a, MK_RNG_PCG32, 99);
    mk_rng_seed(&b, MK_RNG_PCG32, 99);
    for (int i = 0; i < 1000; i++)
    {
        CHECK_EQ(mk_rng_next(&a), mk_rng_next(&b));
    }
    for (int i = 0; i < 1000; i++)
    {
        double u = mk_rng_unit(&a);
        CHECK(u >= 0.0 && u < 1.0);
        uint64_t big = mk_rng_below64(&a, 10000000000000ULL);
        CHECK(big < 10000000000000ULL);
        CHECK_EQ(mk_rng_below(&a, 1), 0);
    }
    mk_rng g;
    mk_rng_seed(&g, MK_RNG_GLIBC, 5);
    for (int i = 0; i < 1000; i++)
    {
        CHECK(mk_rng_next(&g) <= 0x7fffffffu);
        CHECK(mk_rng_below64(&g, 3) < 3);
    }
}

void suite_rng(void)
{
    RUN(test_pcg32_reference_vector);
    RUN(test_glibc_matches_libc);
    RUN(test_below_is_uniform);
    RUN(test_ranges_and_determinism);
}
