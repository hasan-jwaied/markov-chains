/*
 * The "cell" plugin: a state is one Snakes & Ladders square.
 * Squares are identified by number alone; the jump fields are payload.
 */
#include <stdio.h>
#include "markov/alloc.h"
#include "markov/plugins.h"
#include "../src/hash.h"

static uint64_t cell_hash(const void *data)
{
    return mk_mix64((uint64_t) (uint32_t) ((const mk_cell *) data)->number);
}

static int cell_compare(const void *a, const void *b)
{
    int32_t x = ((const mk_cell *) a)->number;
    int32_t y = ((const mk_cell *) b)->number;
    return (x > y) - (x < y); /* no subtraction: it can overflow */
}

static void *cell_copy(const void *data)
{
    mk_cell *copy = mk_malloc(sizeof(mk_cell), MK_TAG_STATE);
    if (copy)
    {
        *copy = *(const mk_cell *) data;
    }
    return copy;
}

static void cell_destroy(void *data)
{
    mk_free(data);
}

static bool cell_is_terminal(const void *data)
{
    return ((const mk_cell *) data)->is_last != 0;
}

static int cell_format(const void *data, char *buf, size_t size)
{
    return snprintf(buf, size, "[%d]", (int) ((const mk_cell *) data)->number);
}

static const mk_vtable CELL_VTABLE = {
    "cell", cell_hash, cell_compare, cell_copy, cell_destroy,
    cell_is_terminal, cell_format
};

const mk_vtable *mk_plugin_cell(void)
{
    return &CELL_VTABLE;
}
