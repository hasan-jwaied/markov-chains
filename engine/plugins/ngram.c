/*
 * The "ngram" plugin: a state is a tuple of k words stored in one block.
 * Order 1 reproduces the original tweet chain; higher orders remember more
 * context, trading creativity for fidelity to the source.
 */
#include <stdio.h>
#include <string.h>
#include "../src/hash.h"
#include "markov/alloc.h"
#include "markov/plugins.h"

static uint64_t ngram_hash(const void *data)
{
    const mk_ngram *gram = data;
    uint64_t h = mk_fnv1a(MK_FNV_OFFSET, &gram->order, sizeof(gram->order));
    return mk_fnv1a(h, gram->text, gram->size);
}

static int ngram_compare(const void *a, const void *b)
{
    const mk_ngram *x = a;
    const mk_ngram *y = b;
    if (x->order != y->order)
    {
        return x->order < y->order ? -1 : 1;
    }
    if (x->size != y->size)
    {
        return x->size < y->size ? -1 : 1;
    }
    return memcmp(x->text, y->text, x->size);
}

static void *ngram_copy(const void *data)
{
    const mk_ngram *gram = data;
    size_t bytes = sizeof(mk_ngram) + gram->size;
    mk_ngram *copy = mk_malloc(bytes, MK_TAG_STATE);
    if (copy)
    {
        memcpy(copy, gram, bytes);
    }
    return copy;
}

static void ngram_destroy(void *data)
{
    mk_free(data);
}

static bool ngram_is_terminal(const void *data)
{
    return ((const mk_ngram *) data)->terminal != 0;
}

/* Append one character, snprintf-style: count it even when it doesn't fit. */
static void put_char(char *buf, size_t size, size_t *len, char c)
{
    if (buf && *len + 1u < size)
    {
        buf[*len] = c;
    }
    (*len)++;
}

static int ngram_format(const void *data, char *buf, size_t size)
{
    const mk_ngram *gram = data;
    size_t len = 0;
    for (uint32_t i = 0; i < gram->order; i++)
    {
        if (i > 0)
        {
            put_char(buf, size, &len, ' ');
        }
        for (const char *w = mk_ngram_word(gram, i); *w; w++)
        {
            put_char(buf, size, &len, *w);
        }
    }
    if (buf && size > 0)
    {
        buf[len < size ? len : size - 1u] = '\0';
    }
    return len > INT32_MAX ? INT32_MAX : (int) len;
}

static const mk_vtable NGRAM_VTABLE = {
    "ngram", ngram_hash, ngram_compare, ngram_copy, ngram_destroy,
    ngram_is_terminal, ngram_format
};

const mk_vtable *mk_plugin_ngram(void)
{
    return &NGRAM_VTABLE;
}

const char *mk_ngram_word(const mk_ngram *gram, uint32_t i)
{
    const char *p = gram->text;
    for (uint32_t k = 0; k < i && k + 1u < gram->order; k++)
    {
        p += strlen(p) + 1u;
    }
    return p;
}

const char *mk_ngram_last_word(const mk_ngram *gram)
{
    return mk_ngram_word(gram, gram->order ? gram->order - 1u : 0u);
}
