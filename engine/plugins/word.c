/*
 * The "word" plugin: a state is a NUL-terminated string.
 *
 * This is the original Part B tweet state, kept deliberately small as the
 * reference example of a plugin: six functions and a vtable.
 */
#include <stdio.h>
#include <string.h>
#include "../src/hash.h"
#include "markov/alloc.h"
#include "markov/plugins.h"

static uint64_t word_hash(const void *data)
{
    const char *word = data;
    return mk_fnv1a(MK_FNV_OFFSET, word, strlen(word));
}

static int word_compare(const void *a, const void *b)
{
    return strcmp(a, b);
}

static void *word_copy(const void *data)
{
    size_t len = strlen(data) + 1u;
    char *copy = mk_malloc(len, MK_TAG_STATE);
    if (copy)
    {
        memcpy(copy, data, len);
    }
    return copy;
}

static void word_destroy(void *data)
{
    mk_free(data);
}

static bool word_is_terminal(const void *data)
{
    const char *word = data;
    size_t len = strlen(word);
    return len > 0 && word[len - 1u] == '.';
}

static int word_format(const void *data, char *buf, size_t size)
{
    return snprintf(buf, size, "%s", (const char *) data);
}

static const mk_vtable WORD_VTABLE = {
    "word", word_hash, word_compare, word_copy, word_destroy,
    word_is_terminal, word_format
};

const mk_vtable *mk_plugin_word(void)
{
    return &WORD_VTABLE;
}
