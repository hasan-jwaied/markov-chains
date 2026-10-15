#ifndef MARKOV_ALIAS_H
#define MARKOV_ALIAS_H

#include <stdint.h>
#include "markov/rng.h"
#include "markov/status.h"

/**
 * Vose's alias method over integer weights: O(n) to build, O(1) per sample.
 *
 * This is an exact, integer-only variant. With weights w_i summing to W, each
 * column i is scaled to s_i = n * w_i and split between itself and an alias so
 * that every column holds exactly W units. Sampling picks a column uniformly
 * and a threshold uniformly in [0, W), so P(i) = w_i / W exactly. There is no
 * floating-point rounding, which makes results identical across platforms
 * (native and WebAssembly) and lets the tests check the tables exactly.
 */
typedef struct mk_alias
{
    uint32_t n;          /* number of outcomes                          */
    uint64_t total;      /* W, the sum of the weights                    */
    uint64_t *threshold; /* keep column i when r < threshold[i]          */
    uint32_t *alias;     /* otherwise return alias[i]                    */
} mk_alias;

/**
 * Build a table from `n` weights. Weights may be zero but must not all be.
 * Requires n * max(w_i) and the weight sum to fit in 64 bits.
 */
mk_status mk_alias_build(mk_alias *table, const uint64_t *weights, uint32_t n);

/** Draw one outcome index in [0, n). */
uint32_t mk_alias_sample(const mk_alias *table, mk_rng *rng);

void mk_alias_free(mk_alias *table);

#endif /* MARKOV_ALIAS_H */
