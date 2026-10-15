#ifndef MARKOV_CHAIN_H
#define MARKOV_CHAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "markov/rng.h"
#include "markov/status.h"

/**
 * A generic, first-order Markov chain over arbitrary state payloads.
 *
 * States are owned `void *` payloads described by an mk_vtable (the "plugin"):
 * the chain never looks inside them except through the vtable. Each distinct
 * payload is interned once and referred to by a dense integer id (mk_state),
 * which keeps transitions compact (8 bytes each) and makes the chain easy to
 * export as a matrix.
 *
 * Complexity, with V states and E distinct transitions:
 *   - mk_chain_intern / mk_chain_find       O(1) expected (hash index)
 *   - mk_chain_add_transition               O(1) expected (edge index)
 *   - mk_chain_next with MK_SAMPLE_ALIAS    O(1) after an O(deg) table build
 *   - memory                                O(V + E)
 */

typedef uint32_t mk_state;
#define MK_NO_STATE ((mk_state) UINT32_MAX)

/**
 * The plugin interface: a hand-written vtable. This is the C equivalent of an
 * abstract base class (C++) or a trait (Rust); the chain is "generic" because
 * it only ever calls these function pointers.
 */
typedef struct mk_vtable
{
    /** Short identifier, e.g. "ngram" or "cell". */
    const char *name;
    /** Hash of the payload. Equal payloads must hash equally. */
    uint64_t (*hash)(const void *data);
    /** Zero when equal (like strcmp); the sign is not used. */
    int (*compare)(const void *a, const void *b);
    /** Deep copy owned by the chain; allocate with mk_malloc(.., MK_TAG_STATE). */
    void *(*copy)(const void *data);
    /** Release a payload created by copy. */
    void (*destroy)(void *data);
    /** True if a sequence ends at this state (the original is_last). */
    bool (*is_terminal)(const void *data);
    /** snprintf-style: write a label, return the full length it needs. */
    int (*format)(const void *data, char *buf, size_t size);
} mk_vtable;

/** One outgoing transition. `count` is the observed frequency (weight). */
typedef struct mk_edge
{
    mk_state target;
    uint32_t count;
} mk_edge;

typedef struct mk_chain mk_chain;

/** How the first state of a generated sequence is chosen. */
typedef enum mk_start_policy
{
    /**
     * The original algorithm: draw a state uniformly from all states and
     * retry while it is terminal. Consumes the RNG exactly like the original
     * (so output is byte-identical), but fails instead of hanging when no
     * state is non-terminal.
     */
    MK_START_COMPAT = 0,
    /** Uniform over states that are non-terminal AND have a successor. */
    MK_START_UNIFORM,
    /** Weighted by how often each state began a sequence in the input. */
    MK_START_EMPIRICAL
} mk_start_policy;

/** How a successor is drawn from a state's transitions. */
typedef enum mk_sampler
{
    /** Cumulative scan in insertion order, O(deg): the original algorithm. */
    MK_SAMPLE_LINEAR = 0,
    /** Vose alias table, O(1); built lazily and rebuilt when edges change. */
    MK_SAMPLE_ALIAS
} mk_sampler;

typedef struct mk_generate_opts
{
    mk_state start;           /* MK_NO_STATE: choose with start_policy     */
    mk_start_policy start_policy;
    mk_sampler sampler;
    uint32_t max_length;      /* maximum states in the sequence, >= 1      */
    /**
     * Sampling temperature. 1.0 (or 0, meaning "unset") samples the true
     * transition probabilities with `sampler`. Other positive values sample
     * proportionally to count^(1/T): T < 1 is greedier, T > 1 flatter.
     */
    double temperature;
} mk_generate_opts;

/** Defaults: random start (UNIFORM), alias sampling, length 20, T = 1. */
mk_generate_opts mk_generate_defaults(void);

/* ---- lifecycle ---------------------------------------------------------- */

mk_status mk_chain_create(const mk_vtable *vtable, mk_chain **out);
/** Free the chain, every payload and every table; sets *chain to NULL. */
void mk_chain_destroy(mk_chain **chain);

/* ---- building ----------------------------------------------------------- */

/**
 * Return the id of `data`, copying it into the chain if it is new.
 * `*created` (optional) reports whether a new state was added.
 */
mk_status mk_chain_intern(mk_chain *chain, const void *data, mk_state *out,
                          bool *created);

/** Id of `data`, or MK_NO_STATE if it was never interned. */
mk_state mk_chain_find(const mk_chain *chain, const void *data);

/** Add `count` observations of from -> to (creating the edge if needed). */
mk_status mk_chain_add_transition(mk_chain *chain, mk_state from, mk_state to,
                                  uint32_t count);

/** Record that an observed sequence began at `state`. */
mk_status mk_chain_mark_start(mk_chain *chain, mk_state state);

/* ---- inspection --------------------------------------------------------- */

const mk_vtable *mk_chain_vtable(const mk_chain *chain);
uint32_t mk_chain_size(const mk_chain *chain);
/** Number of distinct (from, to) pairs. */
uint64_t mk_chain_edge_count(const mk_chain *chain);
/** Sum of all transition counts. */
uint64_t mk_chain_transition_total(const mk_chain *chain);

const void *mk_chain_data(const mk_chain *chain, mk_state state);
bool mk_chain_is_terminal(const mk_chain *chain, mk_state state);
uint32_t mk_chain_out_degree(const mk_chain *chain, mk_state state);
/** Sum of the counts on the state's outgoing edges. */
uint64_t mk_chain_out_total(const mk_chain *chain, mk_state state);
/** Read-only view of the outgoing edges in insertion order (NULL if none). */
const mk_edge *mk_chain_edges(const mk_chain *chain, mk_state state);
uint32_t mk_chain_start_count(const mk_chain *chain, mk_state state);
/** Format a state's label through the vtable (snprintf semantics). */
int mk_chain_format(const mk_chain *chain, mk_state state, char *buf,
                    size_t size);

/* ---- sampling ----------------------------------------------------------- */

mk_status mk_chain_random_start(mk_chain *chain, mk_rng *rng,
                                mk_start_policy policy, mk_state *out);

/** Draw a successor of `from`. MK_ERR_DEAD_END if it has no transitions. */
mk_status mk_chain_next(mk_chain *chain, mk_rng *rng, mk_state from,
                        mk_sampler sampler, mk_state *out);

/** Draw a successor with probability proportional to count^(1/temperature). */
mk_status mk_chain_next_tempered(const mk_chain *chain, mk_rng *rng,
                                 mk_state from, double temperature,
                                 mk_state *out);

/**
 * Generate one sequence into `out` (capacity `cap` >= opts->max_length).
 * The walk stops after max_length states, after a terminal state, or at a
 * dead end, exactly like the original generate_random_sequence.
 */
mk_status mk_chain_generate(mk_chain *chain, mk_rng *rng,
                            const mk_generate_opts *opts, mk_state *out,
                            size_t cap, size_t *out_len);

#endif /* MARKOV_CHAIN_H */
