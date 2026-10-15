#include <string.h>
#include "mk_test.h"

static void test_accounting(void)
{
    mk_alloc_stats s0;
    mk_alloc_stats s1;
    mk_alloc_get_stats(&s0);

    char *a = mk_malloc(100, MK_TAG_EDGES);
    char *b = mk_malloc(28, MK_TAG_STATE);
    CHECK(a != NULL && b != NULL);
    mk_alloc_get_stats(&s1);
    CHECK_EQ(s1.live_blocks - s0.live_blocks, 2);
    CHECK_EQ(s1.live_bytes - s0.live_bytes, 128);
    CHECK_EQ(s1.live_bytes_by_tag[MK_TAG_EDGES]
                 - s0.live_bytes_by_tag[MK_TAG_EDGES], 100);
    CHECK(s1.peak_bytes >= s1.live_bytes);
    CHECK_EQ(mk_alloc_size(a), 100);
    CHECK_EQ(mk_alloc_size(NULL), 0);

    mk_free(a);
    mk_free(b);
    mk_free(NULL);
    mk_alloc_get_stats(&s1);
    CHECK_EQ(s1.live_blocks, s0.live_blocks);
    CHECK_EQ(s1.live_bytes, s0.live_bytes);
    CHECK_EQ(s1.total_frees - s0.total_frees, 2);
}

static void test_alignment(void)
{
    for (size_t size = 1; size < 64; size += 7)
    {
        void *p = mk_malloc(size, MK_TAG_OTHER);
        CHECK(p != NULL);
        CHECK_EQ((uintptr_t) p % sizeof(void *), 0);
        CHECK_EQ((uintptr_t) p % sizeof(double), 0);
        mk_free(p);
    }
}

static void test_realloc_and_calloc(void)
{
    mk_alloc_stats s0;
    mk_alloc_stats s1;
    mk_alloc_get_stats(&s0);

    int *v = mk_calloc(4, sizeof(int), MK_TAG_MATH);
    CHECK(v != NULL);
    CHECK(v[0] == 0 && v[3] == 0);
    for (int i = 0; i < 4; i++)
    {
        v[i] = i + 1;
    }
    int *w = mk_realloc(v, 1000 * sizeof(int), MK_TAG_MATH);
    CHECK(w != NULL);
    CHECK(w[0] == 1 && w[3] == 4);
    mk_alloc_get_stats(&s1);
    CHECK_EQ(s1.total_allocs - s0.total_allocs, 1); /* resize != new alloc */
    CHECK_EQ(s1.live_bytes - s0.live_bytes, 1000 * sizeof(int));
    mk_free(w);

    CHECK(mk_calloc(SIZE_MAX / 2, 4, MK_TAG_OTHER) == NULL); /* overflow */
    CHECK(mk_malloc(SIZE_MAX, MK_TAG_OTHER) == NULL);
    CHECK(mk_malloc(8, MK_TAG_COUNT) == NULL);              /* bad tag */
}

static void test_fault_injection(void)
{
    mk_alloc_fail_after(2);
    void *a = mk_malloc(8, MK_TAG_OTHER);
    void *b = mk_malloc(8, MK_TAG_OTHER);
    void *c = mk_malloc(8, MK_TAG_OTHER);
    CHECK(a != NULL && b != NULL && c == NULL);
    /* A failed realloc must leave the original block intact. */
    char *d = mk_realloc(a, 4096, MK_TAG_OTHER);
    CHECK(d == NULL);
    mk_alloc_fail_after(-1);
    void *e = mk_malloc(8, MK_TAG_OTHER);
    CHECK(e != NULL);
    mk_free(a);
    mk_free(b);
    mk_free(e);
}

typedef struct hook_log
{
    int allocs;
    int frees;
    size_t bytes;
} hook_log;

static void record(mk_alloc_event event, void *ptr, size_t size,
                   mk_alloc_tag tag, void *user)
{
    hook_log *log = user;
    (void) ptr;
    (void) tag;
    if (event == MK_ALLOC_EVENT_ALLOC)
    {
        log->allocs++;
        log->bytes += size;
    }
    else
    {
        log->frees++;
        log->bytes -= size;
    }
}

static void test_hook(void)
{
    hook_log log = {0, 0, 0};
    mk_alloc_set_hook(record, &log);
    void *p = mk_malloc(10, MK_TAG_OTHER);
    p = mk_realloc(p, 20, MK_TAG_OTHER);
    mk_free(p);
    mk_alloc_set_hook(NULL, NULL);
    CHECK_EQ(log.allocs, 2);
    CHECK_EQ(log.frees, 2);
    CHECK_EQ(log.bytes, 0);
    CHECK(strcmp(mk_alloc_tag_name(MK_TAG_ALIAS), "alias") == 0);
    CHECK(strcmp(mk_alloc_tag_name(MK_TAG_COUNT), "invalid") == 0);
}

void suite_alloc(void)
{
    RUN(test_accounting);
    RUN(test_alignment);
    RUN(test_realloc_and_calloc);
    RUN(test_fault_injection);
    RUN(test_hook);
}
