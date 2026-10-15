#ifndef MARKOV_SNAKES_H
#define MARKOV_SNAKES_H

#include <stdint.h>
#include "markov/chain.h"
#include "markov/status.h"

/**
 * Snakes & Ladders boards as Markov chains (chains created with
 * mk_plugin_cell).
 *
 * Rules, matching the original assignment: from square c a die with `faces`
 * sides moves to c+1..c+faces, dropping throws that would overshoot the last
 * square (the remaining faces stay equally likely). A square with a snake or
 * a ladder instead moves to its destination with probability 1, as its own
 * step. The last square is terminal (absorbing).
 */

typedef struct mk_jump
{
    int32_t from; /* 1-based square holding the snake head / ladder foot */
    int32_t to;   /* destination square                                 */
} mk_jump;

typedef struct mk_board
{
    int32_t size;        /* number of squares, >= 2           */
    int32_t faces;       /* die faces, >= 1                   */
    uint32_t n_jumps;
    const mk_jump *jumps;
} mk_board;

#define MK_SNAKES_MAX_SIZE 10000

/** The 100-square, 20-jump board from the original assignment (d6). */
const mk_board *mk_snakes_classic_board(void);

/**
 * Check a board: size and faces in range, every jump between existing
 * squares, none from the last square or onto itself, at most one per square.
 * A jump may land on another jump's start; that simply takes two steps.
 */
mk_status mk_snakes_validate(const mk_board *board);

/**
 * Add every square (in order, so square n has id n-1) and its transitions to
 * an empty cell chain.
 */
mk_status mk_snakes_build(mk_chain *chain, const mk_board *board);

#endif /* MARKOV_SNAKES_H */
