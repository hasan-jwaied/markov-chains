#include "markov/chain.h"

#include <math.h>
#include <string.h>
#include "chain_internal.h"
#include "hash.h"
#include "markov/alloc.h"

#define INITIAL_NODES 16u
#define INITIAL_SLOTS 32u
#define INITIAL_EDGES 2u
#define MAX_STATES (UINT32_MAX / 4u) /* keeps slot counts within 32 bits */

/* small helpers */

static bool valid_state(const mk_chain *chain, mk_state state)
{
    return chain != NULL && state < chain->n_nodes;
}

/**
 * Grow an array of `*cap` elements of `elem` bytes so it holds `need`,
 * doubling its capacity. Returns the (possibly moved) array, or NULL with
 * `*status` set; on failure the original array is untouched.
 */
static void *grow(void *array, uint32_t *cap, uint32_t need, size_t elem,
                  uint32_t initial, mk_alloc_tag tag, mk_status *status)
{
    if (need <= *cap)
    {
        return array;
    }
    uint64_t new_cap = *cap ? *cap : initial;
    while (new_cap < need)
    {
        new_cap *= 2u;
    }
    if (new_cap > UINT32_MAX || new_cap > SIZE_MAX / elem)
    {
        *status = MK_ERR_LIMIT;
        return NULL;
    }
    void *grown = mk_realloc(array, (size_t) new_cap * elem, tag);
    if (grown == NULL)
    {
        *status = MK_ERR_NOMEM;
        return NULL;
    }
    *cap = (uint32_t) new_cap;
    return grown;
}

static void drop_alias(mk_alias **alias)
{
    if (*alias)
    {
        mk_alias_free(*alias);
        mk_free(*alias);
        *alias = NULL;
    }
}

/* state index */

static mk_state index_lookup(const mk_chain *chain, const void *data,
                             uint64_t hash)
{
    if (chain->n_slots == 0)
    {
        return MK_NO_STATE;
    }
    uint32_t mask = chain->n_slots - 1u;
    /* Re-mix: plugins only promise equal hashes for equal data. */
    uint32_t i = (uint32_t) (mk_mix64(hash) & mask);
    while (chain->slots[i] != 0)
    {
        mk_state id = chain->slots[i] - 1u;
        const mk_node *node = &chain->nodes[id];
        if (node->hash == hash && chain->vt->compare(node->data, data) == 0)
        {
            return id;
        }
        i = (i + 1u) & mask;
    }
    return MK_NO_STATE;
}

static void index_place(uint32_t *slots, uint32_t n_slots, uint64_t hash,
                        mk_state id)
{
    uint32_t mask = n_slots - 1u;
    uint32_t i = (uint32_t) (mk_mix64(hash) & mask);
    while (slots[i] != 0)
    {
        i = (i + 1u) & mask;
    }
    slots[i] = id + 1u;
}

/** Make room for `count` states at load factor <= 1/2. */
static mk_status index_reserve(mk_chain *chain, uint32_t count)
{
    if ((uint64_t) count * 2u <= chain->n_slots)
    {
        return MK_OK;
    }
    uint64_t n_slots = chain->n_slots ? chain->n_slots : INITIAL_SLOTS;
    while (n_slots < (uint64_t) count * 2u)
    {
        n_slots *= 2u;
    }
    if (n_slots > UINT32_MAX)
    {
        return MK_ERR_LIMIT;
    }
    uint32_t *slots = mk_calloc((size_t) n_slots, sizeof(uint32_t),
                                MK_TAG_INDEX);
    if (slots == NULL)
    {
        return MK_ERR_NOMEM;
    }
    for (uint32_t id = 0; id < chain->n_nodes; id++)
    {
        index_place(slots, (uint32_t) n_slots, chain->nodes[id].hash, id);
    }
    mk_free(chain->slots);
    chain->slots = slots;
    chain->n_slots = (uint32_t) n_slots;
    return MK_OK;
}

/* edge index */

static uint64_t edge_key(mk_state from, mk_state to)
{
    return ((uint64_t) from << 32u) | to;
}

/** Slot holding `key`, or the empty slot where it would go. */
static uint64_t edge_slot(const mk_chain *chain, uint64_t key)
{
    uint64_t mask = chain->n_edge_slots - 1u;
    uint64_t i = mk_mix64(key) & mask;
    while (chain->edge_keys[i] != MK_EDGE_EMPTY && chain->edge_keys[i] != key)
    {
        i = (i + 1u) & mask;
    }
    return i;
}

static mk_status edge_index_reserve(mk_chain *chain, uint64_t count)
{
    if (count * 2u <= chain->n_edge_slots)
    {
        return MK_OK;
    }
    uint64_t n_slots = chain->n_edge_slots ? chain->n_edge_slots : 64u;
    while (n_slots < count * 2u)
    {
        n_slots *= 2u;
    }
    if (n_slots > SIZE_MAX / sizeof(uint64_t))
    {
        return MK_ERR_LIMIT;
    }
    uint64_t *keys = mk_malloc((size_t) n_slots * sizeof(uint64_t),
                               MK_TAG_INDEX);
    uint32_t *pos = mk_malloc((size_t) n_slots * sizeof(uint32_t),
                              MK_TAG_INDEX);
    if (keys == NULL || pos == NULL)
    {
        mk_free(keys);
        mk_free(pos);
        return MK_ERR_NOMEM;
    }
    memset(keys, 0xff, (size_t) n_slots * sizeof(uint64_t));

    uint64_t *old_keys = chain->edge_keys;
    uint32_t *old_pos = chain->edge_pos;
    uint64_t old_n = chain->n_edge_slots;
    chain->edge_keys = keys;
    chain->edge_pos = pos;
    chain->n_edge_slots = n_slots;
    for (uint64_t i = 0; i < old_n; i++)
    {
        if (old_keys[i] != MK_EDGE_EMPTY)
        {
            uint64_t slot = edge_slot(chain, old_keys[i]);
            keys[slot] = old_keys[i];
            pos[slot] = old_pos[i];
        }
    }
    mk_free(old_keys);
    mk_free(old_pos);
    return MK_OK;
}

/* lifecycle */

mk_generate_opts mk_generate_defaults(void)
{
    mk_generate_opts opts;
    opts.start = MK_NO_STATE;
    opts.start_policy = MK_START_UNIFORM;
    opts.sampler = MK_SAMPLE_ALIAS;
    opts.max_length = 20;
    opts.temperature = 1.0;
    return opts;
}

mk_status mk_chain_create(const mk_vtable *vtable, mk_chain **out)
{
    if (out == NULL)
    {
        return MK_ERR_INVALID;
    }
    *out = NULL;
    if (vtable == NULL || vtable->hash == NULL || vtable->compare == NULL
        || vtable->copy == NULL || vtable->destroy == NULL
        || vtable->is_terminal == NULL)
    {
        return MK_ERR_INVALID;
    }
    mk_chain *chain = mk_calloc(1, sizeof(mk_chain), MK_TAG_CHAIN);
    if (chain == NULL)
    {
        return MK_ERR_NOMEM;
    }
    chain->vt = vtable;
    *out = chain;
    return MK_OK;
}

void mk_chain_destroy(mk_chain **chain_ptr)
{
    if (chain_ptr == NULL || *chain_ptr == NULL)
    {
        return;
    }
    mk_chain *chain = *chain_ptr;
    for (uint32_t i = 0; i < chain->n_nodes; i++)
    {
        mk_node *node = &chain->nodes[i];
        chain->vt->destroy(node->data);
        mk_free(node->edges);
        drop_alias(&node->alias);
    }
    mk_free(chain->nodes);
    mk_free(chain->slots);
    mk_free(chain->edge_keys);
    mk_free(chain->edge_pos);
    mk_free(chain->starters);
    drop_alias(&chain->start_alias);
    mk_free(chain->start_ids);
    mk_free(chain);
    *chain_ptr = NULL;
}

/* building */

mk_state mk_chain_find(const mk_chain *chain, const void *data)
{
    if (chain == NULL || data == NULL)
    {
        return MK_NO_STATE;
    }
    return index_lookup(chain, data, chain->vt->hash(data));
}

mk_status mk_chain_intern(mk_chain *chain, const void *data, mk_state *out,
                          bool *created)
{
    if (created)
    {
        *created = false;
    }
    if (chain == NULL || data == NULL || out == NULL)
    {
        return MK_ERR_INVALID;
    }
    uint64_t hash = chain->vt->hash(data);
    mk_state found = index_lookup(chain, data, hash);
    if (found != MK_NO_STATE)
    {
        *out = found;
        return MK_OK;
    }
    if (chain->n_nodes >= MAX_STATES)
    {
        return MK_ERR_LIMIT;
    }

    /* Allocate everything that can fail before changing any state, so a
     * failed intern leaves the chain exactly as it was. */
    mk_status status = MK_OK;
    mk_node *nodes = grow(chain->nodes, &chain->cap_nodes, chain->n_nodes + 1u,
                          sizeof(mk_node), INITIAL_NODES, MK_TAG_NODES,
                          &status);
    if (nodes == NULL)
    {
        return status;
    }
    chain->nodes = nodes;
    status = index_reserve(chain, chain->n_nodes + 1u);
    if (status != MK_OK)
    {
        return status;
    }
    void *copy = chain->vt->copy(data);
    if (copy == NULL)
    {
        return MK_ERR_NOMEM;
    }

    mk_state id = chain->n_nodes++;
    mk_node *node = &chain->nodes[id];
    memset(node, 0, sizeof(*node));
    node->data = copy;
    node->hash = hash;
    node->terminal = chain->vt->is_terminal(copy) ? 1u : 0u;
    if (!node->terminal)
    {
        chain->n_non_terminal++;
    }
    index_place(chain->slots, chain->n_slots, hash, id);

    *out = id;
    if (created)
    {
        *created = true;
    }
    return MK_OK;
}

mk_status mk_chain_add_transition(mk_chain *chain, mk_state from, mk_state to,
                                  uint32_t count)
{
    if (!valid_state(chain, from) || !valid_state(chain, to) || count == 0)
    {
        return MK_ERR_INVALID;
    }
    mk_node *node = &chain->nodes[from];
    uint64_t key = edge_key(from, to);

    if (chain->n_edge_slots > 0)
    {
        uint64_t slot = edge_slot(chain, key);
        if (chain->edge_keys[slot] == key)
        {
            mk_edge *edge = &node->edges[chain->edge_pos[slot]];
            if (edge->count > UINT32_MAX - count)
            {
                return MK_ERR_LIMIT;
            }
            edge->count += count;
            node->out_total += count;
            chain->total_transitions += count;
            drop_alias(&node->alias);
            return MK_OK;
        }
    }

    /* New edge: reserve every allocation first (all-or-nothing). */
    bool becomes_starter = !node->terminal && !node->is_starter;
    mk_status status = edge_index_reserve(chain, chain->n_edges + 1u);
    if (status != MK_OK)
    {
        return status;
    }
    mk_edge *edges = grow(node->edges, &node->cap_edges, node->n_edges + 1u,
                          sizeof(mk_edge), INITIAL_EDGES, MK_TAG_EDGES,
                          &status);
    if (edges == NULL)
    {
        return status;
    }
    node->edges = edges;
    if (becomes_starter)
    {
        mk_state *starters = grow(chain->starters, &chain->cap_starters,
                                  chain->n_starters + 1u, sizeof(mk_state),
                                  INITIAL_NODES, MK_TAG_NODES, &status);
        if (starters == NULL)
        {
            return status;
        }
        chain->starters = starters;
    }

    uint64_t slot = edge_slot(chain, key);
    chain->edge_keys[slot] = key;
    chain->edge_pos[slot] = node->n_edges;
    node->edges[node->n_edges].target = to;
    node->edges[node->n_edges].count = count;
    node->n_edges++;
    node->out_total += count;
    chain->n_edges++;
    chain->total_transitions += count;
    if (becomes_starter)
    {
        node->is_starter = 1u;
        chain->starters[chain->n_starters++] = from;
    }
    drop_alias(&node->alias);
    return MK_OK;
}

mk_status mk_chain_mark_start(mk_chain *chain, mk_state state)
{
    if (!valid_state(chain, state))
    {
        return MK_ERR_INVALID;
    }
    mk_node *node = &chain->nodes[state];
    if (node->start_count == UINT32_MAX)
    {
        return MK_ERR_LIMIT;
    }
    node->start_count++;
    chain->total_starts++;
    drop_alias(&chain->start_alias);
    return MK_OK;
}

/* inspection */

const mk_vtable *mk_chain_vtable(const mk_chain *chain)
{
    return chain ? chain->vt : NULL;
}

uint32_t mk_chain_size(const mk_chain *chain)
{
    return chain ? chain->n_nodes : 0;
}

uint64_t mk_chain_edge_count(const mk_chain *chain)
{
    return chain ? chain->n_edges : 0;
}

uint64_t mk_chain_transition_total(const mk_chain *chain)
{
    return chain ? chain->total_transitions : 0;
}

const void *mk_chain_data(const mk_chain *chain, mk_state state)
{
    return valid_state(chain, state) ? chain->nodes[state].data : NULL;
}

bool mk_chain_is_terminal(const mk_chain *chain, mk_state state)
{
    return valid_state(chain, state) && chain->nodes[state].terminal;
}

uint32_t mk_chain_out_degree(const mk_chain *chain, mk_state state)
{
    return valid_state(chain, state) ? chain->nodes[state].n_edges : 0;
}

uint64_t mk_chain_out_total(const mk_chain *chain, mk_state state)
{
    return valid_state(chain, state) ? chain->nodes[state].out_total : 0;
}

const mk_edge *mk_chain_edges(const mk_chain *chain, mk_state state)
{
    if (!valid_state(chain, state) || chain->nodes[state].n_edges == 0)
    {
        return NULL;
    }
    return chain->nodes[state].edges;
}

uint32_t mk_chain_start_count(const mk_chain *chain, mk_state state)
{
    return valid_state(chain, state) ? chain->nodes[state].start_count : 0;
}

int mk_chain_format(const mk_chain *chain, mk_state state, char *buf,
                    size_t size)
{
    if (!valid_state(chain, state) || chain->vt->format == NULL)
    {
        if (buf && size > 0)
        {
            buf[0] = '\0';
        }
        return 0;
    }
    return chain->vt->format(chain->nodes[state].data, buf, size);
}

/* sampling */

static mk_status build_start_alias(mk_chain *chain)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < chain->n_nodes; i++)
    {
        n += chain->nodes[i].start_count > 0;
    }
    uint64_t *weights = mk_malloc(sizeof(uint64_t) * n, MK_TAG_ALIAS);
    mk_state *ids = mk_malloc(sizeof(mk_state) * n, MK_TAG_ALIAS);
    mk_alias *alias = mk_malloc(sizeof(mk_alias), MK_TAG_ALIAS);
    mk_status status = MK_ERR_NOMEM;
    if (weights && ids && alias)
    {
        uint32_t k = 0;
        for (uint32_t i = 0; i < chain->n_nodes; i++)
        {
            if (chain->nodes[i].start_count > 0)
            {
                weights[k] = chain->nodes[i].start_count;
                ids[k++] = i;
            }
        }
        status = mk_alias_build(alias, weights, n);
    }
    mk_free(weights);
    if (status != MK_OK)
    {
        mk_free(ids);
        mk_free(alias);
        return status;
    }
    mk_free(chain->start_ids);
    chain->start_ids = ids;
    chain->start_alias = alias;
    return MK_OK;
}

static mk_status uniform_start(const mk_chain *chain, mk_rng *rng,
                               mk_state *out)
{
    if (chain->n_starters == 0)
    {
        return MK_ERR_NO_START;
    }
    *out = chain->starters[mk_rng_below(rng, chain->n_starters)];
    return MK_OK;
}

mk_status mk_chain_random_start(mk_chain *chain, mk_rng *rng,
                                mk_start_policy policy, mk_state *out)
{
    if (chain == NULL || rng == NULL || out == NULL)
    {
        return MK_ERR_INVALID;
    }
    if (chain->n_nodes == 0)
    {
        return MK_ERR_EMPTY;
    }
    switch (policy)
    {
        case MK_START_COMPAT:
            /* Same draws as the original rejection loop; the guard turns
             * its infinite loop into an error. */
            if (chain->n_non_terminal == 0)
            {
                return MK_ERR_NO_START;
            }
            for (;;)
            {
                mk_state id = mk_rng_below(rng, chain->n_nodes);
                if (!chain->nodes[id].terminal)
                {
                    *out = id;
                    return MK_OK;
                }
            }
        case MK_START_EMPIRICAL:
            if (chain->total_starts > 0)
            {
                if (chain->start_alias == NULL)
                {
                    mk_status status = build_start_alias(chain);
                    if (status != MK_OK)
                    {
                        return status;
                    }
                }
                *out = chain->start_ids[mk_alias_sample(chain->start_alias,
                                                        rng)];
                return MK_OK;
            }
            /* No recorded starts (e.g. a hand-built chain): use UNIFORM. */
            return uniform_start(chain, rng, out);
        case MK_START_UNIFORM:
            return uniform_start(chain, rng, out);
    }
    return MK_ERR_INVALID;
}

static mk_status build_node_alias(mk_node *node)
{
    uint64_t *weights = mk_malloc(sizeof(uint64_t) * node->n_edges,
                                  MK_TAG_ALIAS);
    mk_alias *alias = mk_malloc(sizeof(mk_alias), MK_TAG_ALIAS);
    mk_status status = MK_ERR_NOMEM;
    if (weights && alias)
    {
        for (uint32_t i = 0; i < node->n_edges; i++)
        {
            weights[i] = node->edges[i].count;
        }
        status = mk_alias_build(alias, weights, node->n_edges);
    }
    mk_free(weights);
    if (status != MK_OK)
    {
        mk_free(alias);
        return status;
    }
    node->alias = alias;
    return MK_OK;
}

mk_status mk_chain_next(mk_chain *chain, mk_rng *rng, mk_state from,
                        mk_sampler sampler, mk_state *out)
{
    if (!valid_state(chain, from) || rng == NULL || out == NULL)
    {
        return MK_ERR_INVALID;
    }
    mk_node *node = &chain->nodes[from];
    if (node->n_edges == 0)
    {
        return MK_ERR_DEAD_END;
    }

    if (sampler == MK_SAMPLE_ALIAS)
    {
        if (node->alias == NULL)
        {
            mk_status status = build_node_alias(node);
            if (status != MK_OK)
            {
                return status;
            }
        }
        *out = node->edges[mk_alias_sample(node->alias, rng)].target;
        return MK_OK;
    }

    /* The original: r uniform in [0, total), then walk the cumulative sum. */
    uint64_t r = node->out_total <= UINT32_MAX
                     ? mk_rng_below(rng, (uint32_t) node->out_total)
                     : mk_rng_below64(rng, node->out_total);
    uint64_t cumulative = 0;
    for (uint32_t i = 0; i < node->n_edges; i++)
    {
        cumulative += node->edges[i].count;
        if (r < cumulative)
        {
            *out = node->edges[i].target;
            return MK_OK;
        }
    }
    *out = node->edges[node->n_edges - 1u].target; /* unreachable */
    return MK_OK;
}

mk_status mk_chain_next_tempered(const mk_chain *chain, mk_rng *rng,
                                 mk_state from, double temperature,
                                 mk_state *out)
{
    if (!valid_state(chain, from) || rng == NULL || out == NULL
        || !(temperature > 0.0))
    {
        return MK_ERR_INVALID;
    }
    const mk_node *node = &chain->nodes[from];
    if (node->n_edges == 0)
    {
        return MK_ERR_DEAD_END;
    }
    /* w_i = c_i^(1/T), computed in log space relative to the largest count
     * so small temperatures cannot overflow. */
    uint32_t max_count = 0;
    for (uint32_t i = 0; i < node->n_edges; i++)
    {
        if (node->edges[i].count > max_count)
        {
            max_count = node->edges[i].count;
        }
    }
    double log_max = log((double) max_count);
    double sum = 0.0;
    for (uint32_t i = 0; i < node->n_edges; i++)
    {
        sum += exp((log((double) node->edges[i].count) - log_max)
                   / temperature);
    }
    double u = mk_rng_unit(rng) * sum;
    double cumulative = 0.0;
    for (uint32_t i = 0; i < node->n_edges; i++)
    {
        cumulative += exp((log((double) node->edges[i].count) - log_max)
                          / temperature);
        if (u < cumulative)
        {
            *out = node->edges[i].target;
            return MK_OK;
        }
    }
    *out = node->edges[node->n_edges - 1u].target; /* rounding guard */
    return MK_OK;
}

mk_status mk_chain_generate(mk_chain *chain, mk_rng *rng,
                            const mk_generate_opts *opts, mk_state *out,
                            size_t cap, size_t *out_len)
{
    if (chain == NULL || rng == NULL || opts == NULL || out == NULL
        || out_len == NULL || opts->max_length == 0
        || cap < opts->max_length || opts->temperature < 0.0)
    {
        return MK_ERR_INVALID;
    }
    *out_len = 0;

    mk_state current = opts->start;
    if (current == MK_NO_STATE)
    {
        mk_status status = mk_chain_random_start(chain, rng,
                                                 opts->start_policy, &current);
        if (status != MK_OK)
        {
            return status;
        }
    }
    else if (!valid_state(chain, current))
    {
        return MK_ERR_INVALID;
    }

    bool tempered = opts->temperature != 0.0 && opts->temperature != 1.0;
    size_t len = 0;
    out[len++] = current;
    for (uint32_t i = 1; i < opts->max_length; i++)
    {
        if (chain->nodes[current].terminal)
        {
            break;
        }
        mk_state next;
        mk_status status = tempered
            ? mk_chain_next_tempered(chain, rng, current, opts->temperature,
                                     &next)
            : mk_chain_next(chain, rng, current, opts->sampler, &next);
        if (status == MK_ERR_DEAD_END)
        {
            break;
        }
        if (status != MK_OK)
        {
            *out_len = len;
            return status;
        }
        out[len++] = next;
        current = next;
    }
    *out_len = len;
    return MK_OK;
}
