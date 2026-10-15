#ifndef MARKOV_PLUGINS_H
#define MARKOV_PLUGINS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "markov/chain.h"

/**
 * Built-in state plugins. Each one is just an mk_vtable plus a payload type;
 * see docs/PLUGINS.md for how to write your own.
 */

/* ---- word: a NUL-terminated string (the original Part B tweet state) ---- */

/** Payload: `char *`. Terminal when the word ends with '.'. */
const mk_vtable *mk_plugin_word(void);

/* ---- ngram: a tuple of k words (the state of an order-k text chain) ----- */

#define MK_NGRAM_MAX_ORDER 8

/**
 * Payload: a single allocation holding `order` NUL-terminated words back to
 * back. One block per state keeps memory compact and cache-friendly.
 */
typedef struct mk_ngram
{
    uint32_t order;    /* number of words                              */
    uint32_t terminal; /* 1 if the last word ends a sentence           */
    uint32_t size;     /* bytes used in text[], NULs included          */
    char text[];       /* "w1\0w2\0...wk\0"                            */
} mk_ngram;

const mk_vtable *mk_plugin_ngram(void);

/** Pointer to word `i` (0-based) of an n-gram. */
const char *mk_ngram_word(const mk_ngram *gram, uint32_t i);
/** The last word: the one a generated sequence emits for this state. */
const char *mk_ngram_last_word(const mk_ngram *gram);

/* ---- cell: a Snakes & Ladders board square ------------------------------ */

#define MK_CELL_EMPTY (-1)

typedef struct mk_cell
{
    int32_t number;    /* 1-based square number                        */
    int32_t ladder_to; /* destination of a ladder, or MK_CELL_EMPTY    */
    int32_t snake_to;  /* destination of a snake, or MK_CELL_EMPTY     */
    int32_t is_last;   /* 1 on the board's final square                */
} mk_cell;

/**
 * Payload: mk_cell, identified by `number`. Terminal when `is_last` is set,
 * which lets boards of any size share one plugin.
 */
const mk_vtable *mk_plugin_cell(void);

#endif /* MARKOV_PLUGINS_H */
