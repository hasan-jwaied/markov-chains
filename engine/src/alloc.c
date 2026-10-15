#include "markov/alloc.h"

#include <stdlib.h>
#include <string.h>

#define MK_ALLOC_MAGIC 0x6d6b616cu /* "mkal" */

/*
 * Every block is preceded by a header. The union pads it to the platform's
 * strictest fundamental alignment (C99 has no max_align_t), so the pointer we
 * hand out is as well aligned as one from malloc().
 */
typedef union mk_header
{
    struct
    {
        size_t size;
        uint32_t tag;
        uint32_t magic;
    } h;
    long double align_ld;
    long long align_ll;
    void *align_ptr;
} mk_header;

static mk_alloc_stats g_stats;
static mk_alloc_hook g_hook;
static void *g_hook_user;
static long g_fail_after = -1;

static int should_fail(void)
{
    if (g_fail_after < 0)
    {
        return 0;
    }
    if (g_fail_after == 0)
    {
        return 1;
    }
    g_fail_after--;
    return 0;
}

static void account_alloc(size_t size, mk_alloc_tag tag)
{
    g_stats.live_bytes += size;
    g_stats.live_blocks++;
    g_stats.live_bytes_by_tag[tag] += size;
    g_stats.live_blocks_by_tag[tag]++;
    g_stats.total_allocs++;
    if (g_stats.live_bytes > g_stats.peak_bytes)
    {
        g_stats.peak_bytes = g_stats.live_bytes;
    }
}

static void account_free(size_t size, mk_alloc_tag tag)
{
    g_stats.live_bytes -= size;
    g_stats.live_blocks--;
    g_stats.live_bytes_by_tag[tag] -= size;
    g_stats.live_blocks_by_tag[tag]--;
    g_stats.total_frees++;
}

static mk_header *header_of(void *ptr)
{
    return (mk_header *) ptr - 1;
}

static const mk_header *const_header_of(const void *ptr)
{
    return (const mk_header *) ptr - 1;
}

void *mk_malloc(size_t size, mk_alloc_tag tag)
{
    if ((unsigned) tag >= MK_TAG_COUNT || size > SIZE_MAX - sizeof(mk_header)
        || should_fail())
    {
        g_stats.failed_allocs++;
        return NULL;
    }
    mk_header *block = malloc(sizeof(mk_header) + size);
    if (block == NULL)
    {
        g_stats.failed_allocs++;
        return NULL;
    }
    block->h.size = size;
    block->h.tag = (uint32_t) tag;
    block->h.magic = MK_ALLOC_MAGIC;
    account_alloc(size, tag);

    void *ptr = block + 1;
    if (g_hook)
    {
        g_hook(MK_ALLOC_EVENT_ALLOC, ptr, size, tag, g_hook_user);
    }
    return ptr;
}

void *mk_calloc(size_t count, size_t size, mk_alloc_tag tag)
{
    if (size != 0 && count > SIZE_MAX / size)
    {
        g_stats.failed_allocs++;
        return NULL;
    }
    void *ptr = mk_malloc(count * size, tag);
    if (ptr)
    {
        memset(ptr, 0, count * size);
    }
    return ptr;
}

void *mk_realloc(void *ptr, size_t size, mk_alloc_tag tag)
{
    if (ptr == NULL)
    {
        return mk_malloc(size, tag);
    }
    mk_header *old = header_of(ptr);
    size_t old_size = old->h.size;
    mk_alloc_tag old_tag = (mk_alloc_tag) old->h.tag;

    if ((unsigned) tag >= MK_TAG_COUNT || size > SIZE_MAX - sizeof(mk_header)
        || should_fail())
    {
        g_stats.failed_allocs++;
        return NULL;
    }
    /* Report the old block as freed before realloc may move it, so a hook
     * never sees an address that is live twice. */
    if (g_hook)
    {
        g_hook(MK_ALLOC_EVENT_FREE, ptr, old_size, old_tag, g_hook_user);
    }
    mk_header *block = realloc(old, sizeof(mk_header) + size);
    if (block == NULL)
    {
        g_stats.failed_allocs++;
        if (g_hook)
        {
            g_hook(MK_ALLOC_EVENT_ALLOC, ptr, old_size, old_tag, g_hook_user);
        }
        return NULL;
    }
    account_free(old_size, old_tag);
    block->h.size = size;
    block->h.tag = (uint32_t) tag;
    account_alloc(size, tag);
    /* A resize is one allocation event, not an alloc plus a free. */
    g_stats.total_allocs--;
    g_stats.total_frees--;

    void *out = block + 1;
    if (g_hook)
    {
        g_hook(MK_ALLOC_EVENT_ALLOC, out, size, tag, g_hook_user);
    }
    return out;
}

void mk_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    mk_header *block = header_of(ptr);
    if (block->h.magic != MK_ALLOC_MAGIC)
    {
        abort(); /* not from mk_malloc, or already freed: fail loudly */
    }
    size_t size = block->h.size;
    mk_alloc_tag tag = (mk_alloc_tag) block->h.tag;
    if (g_hook)
    {
        g_hook(MK_ALLOC_EVENT_FREE, ptr, size, tag, g_hook_user);
    }
    account_free(size, tag);
    block->h.magic = 0;
    free(block);
}

size_t mk_alloc_size(const void *ptr)
{
    return ptr ? const_header_of(ptr)->h.size : 0;
}

mk_alloc_tag mk_alloc_tag_of(const void *ptr)
{
    return ptr ? (mk_alloc_tag) const_header_of(ptr)->h.tag : MK_TAG_OTHER;
}

void mk_alloc_get_stats(mk_alloc_stats *out)
{
    if (out)
    {
        *out = g_stats;
    }
}

const char *mk_alloc_tag_name(mk_alloc_tag tag)
{
    static const char *const names[MK_TAG_COUNT] = {
        "other", "chain", "nodes", "edges", "index",
        "state", "alias", "math", "text"
    };
    return (unsigned) tag < MK_TAG_COUNT ? names[tag] : "invalid";
}

void mk_alloc_set_hook(mk_alloc_hook hook, void *user)
{
    g_hook = hook;
    g_hook_user = user;
}

void mk_alloc_fail_after(long n)
{
    g_fail_after = n < 0 ? -1 : n;
}
