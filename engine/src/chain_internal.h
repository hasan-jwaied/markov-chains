#ifndef MARKOV_SRC_CHAIN_INTERNAL_H
#define MARKOV_SRC_CHAIN_INTERNAL_H

#include "markov/alias.h"
#include "markov/chain.h"

/*
 * Private layout of mk_chain. Exposed only to the engine's own translation
 * units (and the debug/introspection API used by the web heap inspector).
 */

typedef struct mk_node
{
    void *data;            /* plugin payload, owned                       */
    mk_edge *edges;        /* outgoing transitions, insertion order       */
    uint32_t n_edges;
    uint32_t cap_edges;    /* grows geometrically (x2)                    */
    uint64_t out_total;    /* sum of edges[i].count                       */
    uint64_t hash;         /* cached vtable->hash(data)                   */
    mk_alias *alias;       /* lazily built sampler, NULL when stale       */
    uint32_t start_count;  /* times this state began an input sequence    */
    uint8_t terminal;      /* cached vtable->is_terminal(data)            */
    uint8_t is_starter;    /* listed in chain->starters                   */
} mk_node;

struct mk_chain
{
    const mk_vtable *vt;

    mk_node *nodes;        /* dense: state id == index                    */
    uint32_t n_nodes;
    uint32_t cap_nodes;

    /* State index: open addressing, linear probing. Slots hold id + 1, so
     * zero means empty. Capacity is a power of two, load factor <= 1/2. */
    uint32_t *slots;
    uint32_t n_slots;

    /* Edge index: (from << 32 | to) -> position in nodes[from].edges. */
    uint64_t *edge_keys;   /* MK_EDGE_EMPTY marks a free slot             */
    uint32_t *edge_pos;
    uint64_t n_edge_slots;
    uint64_t n_edges;

    uint64_t total_transitions;
    uint32_t n_non_terminal;

    /* States that are non-terminal and have at least one successor: the
     * candidates for MK_START_UNIFORM. Append-only (edges never vanish). */
    mk_state *starters;
    uint32_t n_starters;
    uint32_t cap_starters;

    /* Empirical start distribution (MK_START_EMPIRICAL). */
    uint64_t total_starts;
    mk_alias *start_alias; /* lazily built over states with start_count>0 */
    mk_state *start_ids;   /* column -> state id for start_alias          */
};

#define MK_EDGE_EMPTY UINT64_MAX

#endif /* MARKOV_SRC_CHAIN_INTERNAL_H */
