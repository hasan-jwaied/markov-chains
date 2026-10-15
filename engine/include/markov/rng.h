#ifndef MARKOV_RNG_H
#define MARKOV_RNG_H

#include <stdint.h>

/**
 * Explicit, portable random number generators.
 *
 * The engine never touches the global rand(): every sampling call takes an
 * mk_rng, so runs are reproducible and independent of the C library. That
 * matters in the browser, where Emscripten's libc (musl) implements rand()
 * differently from glibc.
 *
 *  - MK_RNG_PCG32: O'Neill's PCG-XSH-RR 64/32. Fast, statistically strong,
 *    and paired with unbiased bounded sampling.
 *  - MK_RNG_GLIBC: a bit-exact re-implementation of glibc's srand()/rand()
 *    (the TYPE_3 additive feedback generator) together with the original
 *    `rand() % n` reduction. It exists only so the CLIs can reproduce the
 *    output of the original assignment code byte for byte.
 */

typedef enum mk_rng_kind
{
    MK_RNG_PCG32 = 0,
    MK_RNG_GLIBC = 1
} mk_rng_kind;

typedef struct mk_rng
{
    mk_rng_kind kind;
    union
    {
        struct
        {
            uint64_t state;
            uint64_t inc;
        } pcg;
        struct
        {
            uint32_t r[31];
            uint32_t front; /* glibc's fptr, as an index */
            uint32_t rear;  /* glibc's rptr, as an index */
        } glibc;
    } u;
} mk_rng;

/**
 * Seed a generator. For MK_RNG_GLIBC only the low 32 bits are used, exactly
 * like srand(unsigned int).
 */
void mk_rng_seed(mk_rng *rng, mk_rng_kind kind, uint64_t seed);

/** PCG32 with an explicit stream selector (the reference API). */
void mk_rng_seed_pcg32(mk_rng *rng, uint64_t initstate, uint64_t initseq);

/** Next raw output: 32 random bits for PCG32, 31 bits (like rand()) for glibc. */
uint32_t mk_rng_next(mk_rng *rng);

/**
 * Uniform integer in [0, bound), bound > 0.
 * PCG32 rejects the biased tail; glibc mode reproduces `rand() % bound`.
 */
uint32_t mk_rng_below(mk_rng *rng, uint32_t bound);

/** Uniform 64-bit integer in [0, bound), bound > 0 (no modulo bias). */
uint64_t mk_rng_below64(mk_rng *rng, uint64_t bound);

/** Uniform double in [0, 1) with 53 bits of randomness. */
double mk_rng_unit(mk_rng *rng);

#endif /* MARKOV_RNG_H */
