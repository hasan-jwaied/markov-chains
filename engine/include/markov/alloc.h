#ifndef MARKOV_ALLOC_H
#define MARKOV_ALLOC_H

#include <stddef.h>
#include <stdint.h>

/**
 * Tracked allocator.
 *
 * Every byte libmarkov allocates goes through these functions, so the engine
 * can report exactly how much memory it holds (and the web heap inspector can
 * draw it). Each block carries a small header recording its size and tag.
 *
 * The bookkeeping is global and not thread-safe: the engine is designed to be
 * driven from a single thread (one Web Worker in the browser).
 */

/** What a block is used for; drives the per-tag statistics. */
typedef enum mk_alloc_tag
{
    MK_TAG_OTHER = 0,
    MK_TAG_CHAIN,    /* the mk_chain struct itself                  */
    MK_TAG_NODES,    /* the dense array of state nodes              */
    MK_TAG_EDGES,    /* per-state transition arrays                 */
    MK_TAG_INDEX,    /* hash tables (state index, edge index)       */
    MK_TAG_STATE,    /* plugin-owned state payloads (the void *data) */
    MK_TAG_ALIAS,    /* Vose alias tables                           */
    MK_TAG_MATH,     /* matrices and vectors                        */
    MK_TAG_TEXT,     /* tokenizer scratch space                     */
    MK_TAG_COUNT
} mk_alloc_tag;

typedef struct mk_alloc_stats
{
    size_t live_bytes;                  /* user bytes currently allocated */
    size_t live_blocks;                 /* blocks currently allocated     */
    size_t peak_bytes;                  /* high-water mark of live_bytes  */
    uint64_t total_allocs;              /* successful allocations ever    */
    uint64_t total_frees;               /* frees of non-NULL blocks ever  */
    uint64_t failed_allocs;             /* real or injected failures      */
    size_t live_bytes_by_tag[MK_TAG_COUNT];
    size_t live_blocks_by_tag[MK_TAG_COUNT];
} mk_alloc_stats;

typedef enum mk_alloc_event
{
    MK_ALLOC_EVENT_ALLOC,
    MK_ALLOC_EVENT_FREE
} mk_alloc_event;

/**
 * Observer called after every allocation and before every free.
 * `ptr` is the pointer handed to (or by) the caller, not the header.
 */
typedef void (*mk_alloc_hook)(mk_alloc_event event, void *ptr, size_t size,
                              mk_alloc_tag tag, void *user);

void *mk_malloc(size_t size, mk_alloc_tag tag);
void *mk_calloc(size_t count, size_t size, mk_alloc_tag tag);
/** Like realloc(); on failure the original block is left untouched. */
void *mk_realloc(void *ptr, size_t size, mk_alloc_tag tag);
void mk_free(void *ptr);

/** Size the caller asked for when `ptr` was allocated (0 for NULL). */
size_t mk_alloc_size(const void *ptr);

/** Tag `ptr` was allocated with (MK_TAG_OTHER for NULL). */
mk_alloc_tag mk_alloc_tag_of(const void *ptr);

void mk_alloc_get_stats(mk_alloc_stats *out);
const char *mk_alloc_tag_name(mk_alloc_tag tag);
void mk_alloc_set_hook(mk_alloc_hook hook, void *user);

/**
 * Fault injection for tests: let `n` more allocations succeed, then fail
 * every one after that. Pass a negative value to disable (the default).
 */
void mk_alloc_fail_after(long n);

#endif /* MARKOV_ALLOC_H */
