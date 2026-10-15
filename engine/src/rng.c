#include "markov/rng.h"

#include <stddef.h>

/* PCG32 (PCG-XSH-RR 64/32), after the pcg-c reference implementation */

#define PCG32_MULT 6364136223846793005ULL
#define PCG32_DEFAULT_STREAM 0xda3e39cb94b95bdbULL

static uint32_t pcg32_next(mk_rng *rng)
{
    uint64_t old = rng->u.pcg.state;
    rng->u.pcg.state = old * PCG32_MULT + rng->u.pcg.inc;
    uint32_t xorshifted = (uint32_t) (((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t) (old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

void mk_rng_seed_pcg32(mk_rng *rng, uint64_t initstate, uint64_t initseq)
{
    rng->kind = MK_RNG_PCG32;
    rng->u.pcg.state = 0;
    rng->u.pcg.inc = (initseq << 1u) | 1u;
    (void) pcg32_next(rng);
    rng->u.pcg.state += initstate;
    (void) pcg32_next(rng);
}

/* glibc rand() */

/*
 * glibc's default generator (random_r.c, TYPE_3): a lagged additive
 * Fibonacci generator r[i] = r[i-3] + r[i-31] over 31 words, seeded by a
 * Lehmer LCG and warmed up by discarding 310 outputs. The output drops the
 * lowest bit. Indices replace glibc's fptr/rptr pointers.
 */
#define GLIBC_DEG 31u
#define GLIBC_SEP 3u

static uint32_t glibc_next(mk_rng *rng)
{
    uint32_t *r = rng->u.glibc.r;
    uint32_t f = rng->u.glibc.front;
    uint32_t b = rng->u.glibc.rear;

    r[f] += r[b];
    uint32_t result = r[f] >> 1u;

    if (++f >= GLIBC_DEG)
    {
        f = 0;
        ++b;
    }
    else if (++b >= GLIBC_DEG)
    {
        b = 0;
    }
    rng->u.glibc.front = f;
    rng->u.glibc.rear = b;
    return result;
}

static void glibc_seed(mk_rng *rng, uint32_t seed)
{
    uint32_t *r = rng->u.glibc.r;
    if (seed == 0)
    {
        seed = 1; /* glibc treats a zero seed as 1 */
    }
    r[0] = seed;
    /* r[i] = 16807 * r[i-1] % (2^31 - 1) via Schrage's method, exactly as
     * glibc computes it. glibc keeps `word` in an int32_t, so seeds >= 2^31
     * start out negative (written portably, without a narrowing cast). */
    int64_t word = seed > INT32_MAX ? (int64_t) seed - 4294967296LL
                                    : (int64_t) seed;
    for (uint32_t i = 1; i < GLIBC_DEG; i++)
    {
        int64_t hi = word / 127773;
        int64_t lo = word % 127773;
        word = 16807 * lo - 2836 * hi;
        if (word < 0)
        {
            word += 2147483647;
        }
        r[i] = (uint32_t) word;
    }
    rng->u.glibc.front = GLIBC_SEP;
    rng->u.glibc.rear = 0;
    for (int i = 0; i < 310; i++)
    {
        (void) glibc_next(rng);
    }
}

/* public API */

void mk_rng_seed(mk_rng *rng, mk_rng_kind kind, uint64_t seed)
{
    if (kind == MK_RNG_GLIBC)
    {
        rng->kind = MK_RNG_GLIBC;
        glibc_seed(rng, (uint32_t) seed);
    }
    else
    {
        mk_rng_seed_pcg32(rng, seed, PCG32_DEFAULT_STREAM);
    }
}

uint32_t mk_rng_next(mk_rng *rng)
{
    return rng->kind == MK_RNG_GLIBC ? glibc_next(rng) : pcg32_next(rng);
}

uint32_t mk_rng_below(mk_rng *rng, uint32_t bound)
{
    if (bound <= 1)
    {
        /* Still consume one draw so call sequences stay aligned with the
         * original `rand() % n` code even when n == 1. */
        (void) mk_rng_next(rng);
        return 0;
    }
    if (rng->kind == MK_RNG_GLIBC)
    {
        return glibc_next(rng) % bound; /* biased, but bit-exact */
    }
    /* Reject the first (2^32 mod bound) values so every residue is equally
     * likely. Expected draws < 2. */
    uint32_t threshold = (0u - bound) % bound;
    for (;;)
    {
        uint32_t r = pcg32_next(rng);
        if (r >= threshold)
        {
            return r % bound;
        }
    }
}

static uint64_t next64(mk_rng *rng)
{
    if (rng->kind == MK_RNG_GLIBC)
    {
        /* 31 + 31 + 2 bits */
        uint64_t a = glibc_next(rng);
        uint64_t b = glibc_next(rng);
        uint64_t c = glibc_next(rng);
        return (a << 33u) ^ (b << 2u) ^ (c & 3u);
    }
    uint64_t hi = pcg32_next(rng);
    return (hi << 32u) | pcg32_next(rng);
}

uint64_t mk_rng_below64(mk_rng *rng, uint64_t bound)
{
    if (bound <= UINT32_MAX && rng->kind == MK_RNG_PCG32)
    {
        return mk_rng_below(rng, (uint32_t) bound);
    }
    if (bound <= 1)
    {
        (void) next64(rng);
        return 0;
    }
    uint64_t threshold = (0u - bound) % bound;
    for (;;)
    {
        uint64_t r = next64(rng);
        if (r >= threshold)
        {
            return r % bound;
        }
    }
}

double mk_rng_unit(mk_rng *rng)
{
    return (double) (next64(rng) >> 11u) * (1.0 / 9007199254740992.0);
}
