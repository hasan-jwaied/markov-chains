/*
 * The WebAssembly boundary: a flat C ABI over libmarkov for JavaScript.
 *
 * Conventions
 *   - Objects (chains, RNGs, analyses) cross the boundary as opaque pointers,
 *     which in wasm32 are just byte offsets into linear memory. JavaScript
 *     owns them and must free them with the matching *_free function.
 *   - Functions returning a value that can also fail return int32_t: a
 *     non-negative result, or -(mk_status) on error.
 *   - 64-bit counts are returned as doubles (exact up to 2^53), because a
 *     JavaScript number cannot hold a uint64 and BigInt is clumsy.
 *   - Bulk data is never copied: functions hand back pointers to engine
 *     arrays (edges, N matrix, ...) that JS views through HEAP* typed arrays.
 *
 * This file also compiles natively (EXPORT is then empty), so it is checked
 * by the strict native build too.
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/src/chain_internal.h"
#include "markov/markov.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

#define FAIL(status) (-(int32_t) (status))

/* Prototypes (the exports have external linkage by design). */
EXPORT void *mkw_malloc(uint32_t size);
EXPORT void mkw_free(void *ptr);
EXPORT const char *mkw_status_str(int32_t status);
EXPORT void mkw_alloc_stats(double *out);
EXPORT void mkw_heap_trace(int32_t enabled);
EXPORT uint32_t mkw_heap_events(uint32_t *out, uint32_t max_events);
EXPORT uint32_t mkw_heap_events_dropped(void);
EXPORT uint32_t mkw_block_size(const void *ptr);
EXPORT int32_t mkw_block_tag(const void *ptr);
EXPORT mk_chain *mkw_chain_new(int32_t plugin);
EXPORT void mkw_chain_free(mk_chain *chain);
EXPORT int32_t mkw_text_build(mk_chain *chain, const char *text, uint32_t len,
                              int32_t order, double max_words, int32_t rule,
                              double *stats_out);
EXPORT int32_t mkw_snakes_build(mk_chain *chain, int32_t size, int32_t faces,
                                const int32_t *jumps, uint32_t n_jumps);
EXPORT int32_t mkw_snakes_validate(int32_t size, int32_t faces,
                                   const int32_t *jumps, uint32_t n_jumps);
EXPORT const int32_t *mkw_snakes_classic(uint32_t *n_jumps);
EXPORT int32_t mkw_add_transition(mk_chain *chain, uint32_t from, uint32_t to,
                                  uint32_t count);
EXPORT int32_t mkw_intern_word(mk_chain *chain, const char *word);
EXPORT int32_t mkw_intern_words(mk_chain *chain, const char *words,
                                int32_t terminal);
EXPORT uint32_t mkw_chain_size(const mk_chain *chain);
EXPORT double mkw_chain_edge_count(const mk_chain *chain);
EXPORT double mkw_chain_transition_total(const mk_chain *chain);
EXPORT int32_t mkw_state_label(const mk_chain *chain, uint32_t state,
                               char *buf, uint32_t cap);
EXPORT int32_t mkw_state_is_terminal(const mk_chain *chain, uint32_t state);
EXPORT int32_t mkw_state_is_absorbing(const mk_chain *chain, uint32_t state);
EXPORT uint32_t mkw_state_out_degree(const mk_chain *chain, uint32_t state);
EXPORT double mkw_state_out_total(const mk_chain *chain, uint32_t state);
EXPORT uint32_t mkw_state_start_count(const mk_chain *chain, uint32_t state);
EXPORT const mk_edge *mkw_state_edges(const mk_chain *chain, uint32_t state);
EXPORT const void *mkw_state_data(const mk_chain *chain, uint32_t state);
EXPORT mk_rng *mkw_rng_new(int32_t kind, double seed);
EXPORT void mkw_rng_free(mk_rng *rng);
EXPORT int32_t mkw_random_start(mk_chain *chain, mk_rng *rng, int32_t policy);
EXPORT int32_t mkw_next(mk_chain *chain, mk_rng *rng, uint32_t from,
                        int32_t sampler, double temperature);
EXPORT int32_t mkw_generate(mk_chain *chain, mk_rng *rng, int32_t start,
                            int32_t policy, int32_t sampler,
                            uint32_t max_length, double temperature,
                            uint32_t *out);
EXPORT int32_t mkw_sample_counts(mk_chain *chain, mk_rng *rng, uint32_t from,
                                 int32_t sampler, double temperature,
                                 uint32_t draws, uint32_t *counts);
EXPORT int32_t mkw_alias_table(const double *weights, uint32_t n,
                               uint32_t *alias_out, double *threshold_out,
                               double *total_out);
EXPORT uint32_t mkw_render(const mk_chain *chain, const uint32_t *states,
                           uint32_t n, int32_t style, char *buf, uint32_t cap);
EXPORT int32_t mkw_dense(const mk_chain *chain, double *out);
EXPORT mk_csr *mkw_csr_new(const mk_chain *chain);
EXPORT void mkw_csr_free(mk_csr *csr);
EXPORT double mkw_power_step(const mk_csr *csr, const double *x, double *y,
                             double damping);
EXPORT int32_t mkw_stationary(const mk_chain *chain, double damping,
                              double tolerance, uint32_t max_iterations,
                              double *pi, double *result_out);
EXPORT mk_absorbing *mkw_absorbing_new(const mk_chain *chain);
EXPORT void mkw_absorbing_free(mk_absorbing *a);
EXPORT int32_t mkw_last_status(void);
EXPORT uint32_t mkw_abs_n_transient(const mk_absorbing *a);
EXPORT uint32_t mkw_abs_n_absorbing(const mk_absorbing *a);
EXPORT const mk_state *mkw_abs_transient(const mk_absorbing *a);
EXPORT const mk_state *mkw_abs_absorbing(const mk_absorbing *a);
EXPORT const double *mkw_abs_fundamental(const mk_absorbing *a);
EXPORT const double *mkw_abs_expected(const mk_absorbing *a);
EXPORT const double *mkw_abs_variance(const mk_absorbing *a);
EXPORT const double *mkw_abs_absorption(const mk_absorbing *a);
EXPORT int32_t mkw_absorption_pmf(const mk_chain *chain, uint32_t start,
                                  uint32_t max_steps, double *pmf);
EXPORT uint32_t mkw_layout(uint32_t *out, uint32_t cap);
EXPORT const mk_node *mkw_chain_nodes(const mk_chain *chain);
EXPORT const mk_vtable *mkw_chain_vtable(const mk_chain *chain);

/* ---- plain memory for JS-owned buffers (not engine memory) -------------- */

void *mkw_malloc(uint32_t size)
{
    return malloc(size ? size : 1u);
}

void mkw_free(void *ptr)
{
    free(ptr);
}

const char *mkw_status_str(int32_t status)
{
    return mk_status_str((mk_status) (status < 0 ? -status : status));
}

/* ---- allocator statistics and the heap event log ------------------------ */

/** out[0..5]: live bytes, live blocks, peak bytes, allocs, frees, failures;
 *  out[6..14]: live bytes per tag; out[15..23]: live blocks per tag. */
void mkw_alloc_stats(double *out)
{
    mk_alloc_stats s;
    mk_alloc_get_stats(&s);
    out[0] = (double) s.live_bytes;
    out[1] = (double) s.live_blocks;
    out[2] = (double) s.peak_bytes;
    out[3] = (double) s.total_allocs;
    out[4] = (double) s.total_frees;
    out[5] = (double) s.failed_allocs;
    for (int t = 0; t < MK_TAG_COUNT; t++)
    {
        out[6 + t] = (double) s.live_bytes_by_tag[t];
        out[6 + MK_TAG_COUNT + t] = (double) s.live_blocks_by_tag[t];
    }
}

/* A ring buffer of allocation events, drained by the heap inspector. Each
 * event is four words: kind (0 alloc, 1 free), address, size, tag. */
#define HEAP_RING 16384u

static uint32_t g_ring[HEAP_RING * 4u];
static uint32_t g_ring_head;
static uint32_t g_ring_count;
static uint32_t g_ring_dropped;

static void heap_hook(mk_alloc_event event, void *ptr, size_t size,
                      mk_alloc_tag tag, void *user)
{
    (void) user;
    if (g_ring_count == HEAP_RING)
    {
        g_ring_head = (g_ring_head + 1u) % HEAP_RING; /* drop the oldest */
        g_ring_count--;
        g_ring_dropped++;
    }
    uint32_t slot = (g_ring_head + g_ring_count++) % HEAP_RING;
    g_ring[slot * 4u + 0u] = event == MK_ALLOC_EVENT_FREE;
    g_ring[slot * 4u + 1u] = (uint32_t) (uintptr_t) ptr;
    g_ring[slot * 4u + 2u] = (uint32_t) size;
    g_ring[slot * 4u + 3u] = (uint32_t) tag;
}

void mkw_heap_trace(int32_t enabled)
{
    mk_alloc_set_hook(enabled ? heap_hook : NULL, NULL);
    if (!enabled)
    {
        g_ring_head = 0;
        g_ring_count = 0;
        g_ring_dropped = 0;
    }
}

uint32_t mkw_heap_events(uint32_t *out, uint32_t max_events)
{
    uint32_t n = 0;
    while (g_ring_count > 0 && n < max_events)
    {
        memcpy(out + n * 4u, g_ring + g_ring_head * 4u, 4u * sizeof(uint32_t));
        g_ring_head = (g_ring_head + 1u) % HEAP_RING;
        g_ring_count--;
        n++;
    }
    return n;
}

uint32_t mkw_heap_events_dropped(void)
{
    uint32_t dropped = g_ring_dropped;
    g_ring_dropped = 0;
    return dropped;
}

uint32_t mkw_block_size(const void *ptr)
{
    return (uint32_t) mk_alloc_size(ptr);
}

int32_t mkw_block_tag(const void *ptr)
{
    return (int32_t) mk_alloc_tag_of(ptr);
}

/* ---- chains ------------------------------------------------------------- */

static mk_status g_last_status = MK_OK;

int32_t mkw_last_status(void)
{
    return (int32_t) g_last_status;
}

static const mk_vtable *plugin_vtable(int32_t plugin)
{
    switch (plugin)
    {
        case 0: return mk_plugin_word();
        case 1: return mk_plugin_ngram();
        case 2: return mk_plugin_cell();
        default: return NULL;
    }
}

mk_chain *mkw_chain_new(int32_t plugin)
{
    mk_chain *chain = NULL;
    g_last_status = mk_chain_create(plugin_vtable(plugin), &chain);
    return chain;
}

void mkw_chain_free(mk_chain *chain)
{
    mk_chain_destroy(&chain);
}

/** stats_out (optional): [words, sentences]. */
int32_t mkw_text_build(mk_chain *chain, const char *text, uint32_t len,
                       int32_t order, double max_words, int32_t rule,
                       double *stats_out)
{
    mk_text_options opts = mk_text_defaults();
    opts.order = order > 0 ? (uint32_t) order : 0u;
    opts.max_words = max_words < 0 ? -1 : (int64_t) max_words;
    opts.terminal_rule = rule ? MK_TERMINAL_SENTENCE : MK_TERMINAL_PERIOD;
    mk_text_stats stats = {0, 0};
    mk_status status = mk_text_build(chain, text, len, &opts, &stats);
    if (stats_out)
    {
        stats_out[0] = (double) stats.words;
        stats_out[1] = (double) stats.sentences;
    }
    return FAIL(status);
}

/** jumps: n_jumps (from, to) pairs. */
int32_t mkw_snakes_build(mk_chain *chain, int32_t size, int32_t faces,
                         const int32_t *jumps, uint32_t n_jumps)
{
    mk_board board = {size, faces, n_jumps, (const mk_jump *) (const void *) jumps};
    return FAIL(mk_snakes_build(chain, &board));
}

int32_t mkw_snakes_validate(int32_t size, int32_t faces, const int32_t *jumps,
                            uint32_t n_jumps)
{
    mk_board board = {size, faces, n_jumps, (const mk_jump *) (const void *) jumps};
    return FAIL(mk_snakes_validate(&board));
}

const int32_t *mkw_snakes_classic(uint32_t *n_jumps)
{
    const mk_board *board = mk_snakes_classic_board();
    *n_jumps = board->n_jumps;
    return (const int32_t *) (const void *) board->jumps;
}

int32_t mkw_add_transition(mk_chain *chain, uint32_t from, uint32_t to,
                           uint32_t count)
{
    return FAIL(mk_chain_add_transition(chain, from, to, count));
}

int32_t mkw_intern_word(mk_chain *chain, const char *word)
{
    mk_state id;
    mk_status status = mk_chain_intern(chain, word, &id, NULL);
    return status == MK_OK ? (int32_t) id : FAIL(status);
}

/** Build an n-gram probe from space-separated words into a malloc'd block. */
static mk_ngram *ngram_from_words(const char *words, int32_t terminal)
{
    size_t len = strlen(words);
    mk_ngram *gram = malloc(sizeof(mk_ngram) + len + 1u);
    if (gram == NULL)
    {
        return NULL;
    }
    uint32_t order = 0;
    size_t used = 0;
    for (size_t i = 0; i < len;)
    {
        while (i < len && words[i] == ' ')
        {
            i++;
        }
        size_t start = i;
        while (i < len && words[i] != ' ')
        {
            i++;
        }
        if (i > start)
        {
            memcpy(gram->text + used, words + start, i - start);
            used += i - start;
            gram->text[used++] = '\0';
            order++;
        }
    }
    gram->order = order;
    gram->size = (uint32_t) used;
    gram->terminal = terminal < 0
        ? (order > 0 && mk_text_is_terminal_word(
                            mk_ngram_last_word(gram),
                            strlen(mk_ngram_last_word(gram)),
                            MK_TERMINAL_SENTENCE))
        : terminal != 0;
    return gram;
}

/** Intern a state of the ngram plugin given as space-separated words.
 *  terminal: 1/0, or -1 to infer it from the last word. */
int32_t mkw_intern_words(mk_chain *chain, const char *words, int32_t terminal)
{
    mk_ngram *gram = ngram_from_words(words, terminal);
    if (gram == NULL)
    {
        return FAIL(MK_ERR_NOMEM);
    }
    mk_state id;
    mk_status status = gram->order == 0 ? MK_ERR_INVALID
                                        : mk_chain_intern(chain, gram, &id, NULL);
    free(gram);
    return status == MK_OK ? (int32_t) id : FAIL(status);
}

uint32_t mkw_chain_size(const mk_chain *chain)
{
    return mk_chain_size(chain);
}

double mkw_chain_edge_count(const mk_chain *chain)
{
    return (double) mk_chain_edge_count(chain);
}

double mkw_chain_transition_total(const mk_chain *chain)
{
    return (double) mk_chain_transition_total(chain);
}

int32_t mkw_state_label(const mk_chain *chain, uint32_t state, char *buf,
                        uint32_t cap)
{
    return mk_chain_format(chain, state, buf, cap);
}

int32_t mkw_state_is_terminal(const mk_chain *chain, uint32_t state)
{
    return mk_chain_is_terminal(chain, state);
}

int32_t mkw_state_is_absorbing(const mk_chain *chain, uint32_t state)
{
    return state < mk_chain_size(chain) && mk_math_is_absorbing(chain, state);
}

uint32_t mkw_state_out_degree(const mk_chain *chain, uint32_t state)
{
    return mk_chain_out_degree(chain, state);
}

double mkw_state_out_total(const mk_chain *chain, uint32_t state)
{
    return (double) mk_chain_out_total(chain, state);
}

uint32_t mkw_state_start_count(const mk_chain *chain, uint32_t state)
{
    return mk_chain_start_count(chain, state);
}

const mk_edge *mkw_state_edges(const mk_chain *chain, uint32_t state)
{
    return mk_chain_edges(chain, state);
}

const void *mkw_state_data(const mk_chain *chain, uint32_t state)
{
    return mk_chain_data(chain, state);
}

/* ---- sampling ----------------------------------------------------------- */

mk_rng *mkw_rng_new(int32_t kind, double seed)
{
    mk_rng *rng = malloc(sizeof(mk_rng));
    if (rng)
    {
        uint64_t s = seed < 0 ? 0u : (uint64_t) seed;
        mk_rng_seed(rng, kind ? MK_RNG_GLIBC : MK_RNG_PCG32, s);
    }
    return rng;
}

void mkw_rng_free(mk_rng *rng)
{
    free(rng);
}

int32_t mkw_random_start(mk_chain *chain, mk_rng *rng, int32_t policy)
{
    mk_state s;
    mk_status status = mk_chain_random_start(chain, rng,
                                             (mk_start_policy) policy, &s);
    return status == MK_OK ? (int32_t) s : FAIL(status);
}

int32_t mkw_next(mk_chain *chain, mk_rng *rng, uint32_t from, int32_t sampler,
                 double temperature)
{
    mk_state s;
    mk_status status = temperature > 0.0 && temperature != 1.0
        ? mk_chain_next_tempered(chain, rng, from, temperature, &s)
        : mk_chain_next(chain, rng, from, (mk_sampler) sampler, &s);
    return status == MK_OK ? (int32_t) s : FAIL(status);
}

/** start < 0: random start with `policy`. Returns the sequence length. */
int32_t mkw_generate(mk_chain *chain, mk_rng *rng, int32_t start,
                     int32_t policy, int32_t sampler, uint32_t max_length,
                     double temperature, uint32_t *out)
{
    mk_generate_opts opts = mk_generate_defaults();
    opts.start = start < 0 ? MK_NO_STATE : (mk_state) start;
    opts.start_policy = (mk_start_policy) policy;
    opts.sampler = (mk_sampler) sampler;
    opts.max_length = max_length;
    opts.temperature = temperature;
    size_t n = 0;
    mk_status status = mk_chain_generate(chain, rng, &opts, out, max_length, &n);
    return status == MK_OK ? (int32_t) n : FAIL(status);
}

/** Draw `draws` successors of `from`; counts[k] counts edge k (insertion
 *  order), so the caller can compare against the exact probabilities. */
int32_t mkw_sample_counts(mk_chain *chain, mk_rng *rng, uint32_t from,
                          int32_t sampler, double temperature, uint32_t draws,
                          uint32_t *counts)
{
    uint32_t degree = mk_chain_out_degree(chain, from);
    const mk_edge *edges = mk_chain_edges(chain, from);
    if (degree == 0)
    {
        return FAIL(MK_ERR_DEAD_END);
    }
    memset(counts, 0, sizeof(uint32_t) * degree);
    for (uint32_t i = 0; i < draws; i++)
    {
        int32_t next = mkw_next(chain, rng, from, sampler, temperature);
        if (next < 0)
        {
            return next;
        }
        for (uint32_t k = 0; k < degree; k++)
        {
            if (edges[k].target == (mk_state) next)
            {
                counts[k]++;
                break;
            }
        }
    }
    return (int32_t) degree;
}

/** Build the exact alias table for integer weights (given as doubles). */
int32_t mkw_alias_table(const double *weights, uint32_t n, uint32_t *alias_out,
                        double *threshold_out, double *total_out)
{
    uint64_t *w = calloc(n ? n : 1u, sizeof(uint64_t));
    if (w == NULL)
    {
        return FAIL(MK_ERR_NOMEM);
    }
    for (uint32_t i = 0; i < n; i++)
    {
        w[i] = weights[i] > 0 ? (uint64_t) weights[i] : 0u;
    }
    mk_alias table;
    mk_status status = mk_alias_build(&table, w, n);
    free(w);
    if (status != MK_OK)
    {
        return FAIL(status);
    }
    for (uint32_t i = 0; i < n; i++)
    {
        alias_out[i] = table.alias[i];
        threshold_out[i] = (double) table.threshold[i];
    }
    *total_out = (double) table.total;
    mk_alias_free(&table);
    return MK_OK;
}

uint32_t mkw_render(const mk_chain *chain, const uint32_t *states, uint32_t n,
                    int32_t style, char *buf, uint32_t cap)
{
    return (uint32_t) mk_text_render(chain, states, n,
                                     style ? MK_RENDER_CLEAN : MK_RENDER_COMPAT,
                                     buf, cap);
}

/* ---- mathematics -------------------------------------------------------- */

int32_t mkw_dense(const mk_chain *chain, double *out)
{
    return FAIL(mk_math_dense(chain, out, mk_chain_size(chain)));
}

mk_csr *mkw_csr_new(const mk_chain *chain)
{
    mk_csr *csr = malloc(sizeof(mk_csr));
    if (csr == NULL)
    {
        g_last_status = MK_ERR_NOMEM;
        return NULL;
    }
    g_last_status = mk_math_csr(chain, csr);
    if (g_last_status != MK_OK)
    {
        free(csr);
        return NULL;
    }
    return csr;
}

void mkw_csr_free(mk_csr *csr)
{
    if (csr)
    {
        mk_csr_free(csr);
        free(csr);
    }
}

double mkw_power_step(const mk_csr *csr, const double *x, double *y,
                      double damping)
{
    return mk_math_power_step(csr, x, y, damping);
}

/** result_out: [iterations, residual, converged]. */
int32_t mkw_stationary(const mk_chain *chain, double damping, double tolerance,
                       uint32_t max_iterations, double *pi, double *result_out)
{
    mk_stationary_opts opts = mk_stationary_defaults();
    opts.damping = damping;
    opts.tolerance = tolerance;
    opts.max_iterations = max_iterations;
    mk_stationary_result r = {0, 0.0, false};
    mk_status status = mk_math_stationary(chain, &opts, pi, &r);
    if (result_out)
    {
        result_out[0] = r.iterations;
        result_out[1] = r.residual;
        result_out[2] = r.converged;
    }
    return FAIL(status);
}

mk_absorbing *mkw_absorbing_new(const mk_chain *chain)
{
    mk_absorbing *a = malloc(sizeof(mk_absorbing));
    if (a == NULL)
    {
        g_last_status = MK_ERR_NOMEM;
        return NULL;
    }
    g_last_status = mk_math_absorbing(chain, a);
    if (g_last_status != MK_OK)
    {
        free(a);
        return NULL;
    }
    return a;
}

void mkw_absorbing_free(mk_absorbing *a)
{
    if (a)
    {
        mk_absorbing_free(a);
        free(a);
    }
}

uint32_t mkw_abs_n_transient(const mk_absorbing *a) { return a->n_transient; }
uint32_t mkw_abs_n_absorbing(const mk_absorbing *a) { return a->n_absorbing; }
const mk_state *mkw_abs_transient(const mk_absorbing *a) { return a->transient; }
const mk_state *mkw_abs_absorbing(const mk_absorbing *a) { return a->absorbing; }
const double *mkw_abs_fundamental(const mk_absorbing *a) { return a->fundamental; }
const double *mkw_abs_expected(const mk_absorbing *a) { return a->expected_steps; }
const double *mkw_abs_variance(const mk_absorbing *a) { return a->variance_steps; }
const double *mkw_abs_absorption(const mk_absorbing *a) { return a->absorption; }

/** pmf needs max_steps + 2 doubles: the pmf, then P(T > max_steps). */
int32_t mkw_absorption_pmf(const mk_chain *chain, uint32_t start,
                           uint32_t max_steps, double *pmf)
{
    return FAIL(mk_math_absorption_pmf(chain, start, max_steps, pmf,
                                       pmf + max_steps + 1u));
}

/* ---- introspection for the heap inspector ------------------------------- */

#define LAYOUT(type, field) ((uint32_t) offsetof(type, field))

/**
 * Struct sizes and field offsets exactly as this compiler laid them out, so
 * JavaScript can decode raw linear memory without hard-coding anything.
 * Order (keep in sync with web/src/lib/engine/layout.ts):
 */
uint32_t mkw_layout(uint32_t *out, uint32_t cap)
{
    const uint32_t v[] = {
        (uint32_t) sizeof(void *),
        /* mk_chain */
        (uint32_t) sizeof(mk_chain),
        LAYOUT(mk_chain, vt), LAYOUT(mk_chain, nodes),
        LAYOUT(mk_chain, n_nodes), LAYOUT(mk_chain, cap_nodes),
        LAYOUT(mk_chain, slots), LAYOUT(mk_chain, n_slots),
        LAYOUT(mk_chain, edge_keys), LAYOUT(mk_chain, edge_pos),
        LAYOUT(mk_chain, n_edge_slots), LAYOUT(mk_chain, n_edges),
        /* mk_node */
        (uint32_t) sizeof(mk_node),
        LAYOUT(mk_node, data), LAYOUT(mk_node, edges),
        LAYOUT(mk_node, n_edges), LAYOUT(mk_node, cap_edges),
        LAYOUT(mk_node, out_total), LAYOUT(mk_node, hash),
        LAYOUT(mk_node, alias), LAYOUT(mk_node, start_count),
        LAYOUT(mk_node, terminal), LAYOUT(mk_node, is_starter),
        /* mk_edge */
        (uint32_t) sizeof(mk_edge),
        LAYOUT(mk_edge, target), LAYOUT(mk_edge, count),
        /* mk_vtable */
        (uint32_t) sizeof(mk_vtable),
        LAYOUT(mk_vtable, name), LAYOUT(mk_vtable, hash),
        LAYOUT(mk_vtable, compare), LAYOUT(mk_vtable, copy),
        LAYOUT(mk_vtable, destroy), LAYOUT(mk_vtable, is_terminal),
        LAYOUT(mk_vtable, format),
        /* mk_ngram, mk_cell */
        (uint32_t) sizeof(mk_ngram), LAYOUT(mk_ngram, order),
        LAYOUT(mk_ngram, terminal), LAYOUT(mk_ngram, size),
        LAYOUT(mk_ngram, text),
        (uint32_t) sizeof(mk_cell), LAYOUT(mk_cell, number),
        LAYOUT(mk_cell, ladder_to), LAYOUT(mk_cell, snake_to),
        LAYOUT(mk_cell, is_last),
        /* mk_alias */
        (uint32_t) sizeof(mk_alias), LAYOUT(mk_alias, n),
        LAYOUT(mk_alias, total), LAYOUT(mk_alias, threshold),
        LAYOUT(mk_alias, alias),
    };
    uint32_t n = (uint32_t) (sizeof(v) / sizeof(v[0]));
    memcpy(out, v, sizeof(uint32_t) * (n < cap ? n : cap));
    return n;
}

const mk_node *mkw_chain_nodes(const mk_chain *chain)
{
    return chain ? chain->nodes : NULL;
}

const mk_vtable *mkw_chain_vtable(const mk_chain *chain)
{
    return mk_chain_vtable(chain);
}

