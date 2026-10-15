#include "markov/snakes.h"

#include <stddef.h>
#include "markov/alloc.h"
#include "markov/plugins.h"

/* The board from the original assignment, in its original order. */
static const mk_jump CLASSIC_JUMPS[] = {
    {13, 4},  {85, 17}, {95, 67}, {97, 58}, {66, 89},
    {87, 31}, {57, 83}, {91, 25}, {28, 50}, {35, 11},
    {8, 30},  {41, 62}, {81, 43}, {69, 32}, {20, 39},
    {33, 70}, {79, 99}, {23, 76}, {15, 47}, {61, 14}
};

static const mk_board CLASSIC_BOARD = {
    100, 6, sizeof(CLASSIC_JUMPS) / sizeof(CLASSIC_JUMPS[0]), CLASSIC_JUMPS
};

const mk_board *mk_snakes_classic_board(void)
{
    return &CLASSIC_BOARD;
}

mk_status mk_snakes_validate(const mk_board *board)
{
    if (board == NULL || board->size < 2 || board->size > MK_SNAKES_MAX_SIZE
        || board->faces < 1 || board->faces > board->size
        || (board->n_jumps > 0 && board->jumps == NULL)
        || board->n_jumps >= (uint32_t) board->size)
    {
        return MK_ERR_INVALID;
    }
    for (uint32_t i = 0; i < board->n_jumps; i++)
    {
        const mk_jump *j = &board->jumps[i];
        if (j->from < 1 || j->from >= board->size || j->to < 1
            || j->to > board->size || j->to == j->from)
        {
            return MK_ERR_INVALID;
        }
        for (uint32_t k = 0; k < i; k++)
        {
            if (board->jumps[k].from == j->from)
            {
                return MK_ERR_INVALID;
            }
        }
    }
    return MK_OK;
}

mk_status mk_snakes_build(mk_chain *chain, const mk_board *board)
{
    mk_status status = mk_snakes_validate(board);
    if (status != MK_OK)
    {
        return status;
    }
    if (chain == NULL || mk_chain_vtable(chain) != mk_plugin_cell()
        || mk_chain_size(chain) != 0)
    {
        return MK_ERR_INVALID;
    }

    /* Square n gets id n - 1 because squares are interned in order. */
    for (int32_t n = 1; n <= board->size; n++)
    {
        mk_cell cell = {n, MK_CELL_EMPTY, MK_CELL_EMPTY, n == board->size};
        for (uint32_t i = 0; i < board->n_jumps; i++)
        {
            if (board->jumps[i].from == n)
            {
                if (board->jumps[i].to > n)
                {
                    cell.ladder_to = board->jumps[i].to;
                }
                else
                {
                    cell.snake_to = board->jumps[i].to;
                }
            }
        }
        mk_state id;
        status = mk_chain_intern(chain, &cell, &id, NULL);
        if (status != MK_OK)
        {
            return status;
        }
    }

    /* Transitions in the original order: per square, the jump or the die
     * faces 1..faces that stay on the board. */
    for (int32_t n = 1; n < board->size; n++)
    {
        const mk_cell *cell = mk_chain_data(chain, (mk_state) (n - 1));
        int32_t jump = cell->ladder_to != MK_CELL_EMPTY ? cell->ladder_to
                                                        : cell->snake_to;
        if (jump != MK_CELL_EMPTY)
        {
            status = mk_chain_add_transition(chain, (mk_state) (n - 1),
                                             (mk_state) (jump - 1), 1);
            if (status != MK_OK)
            {
                return status;
            }
            continue;
        }
        for (int32_t face = 1; face <= board->faces; face++)
        {
            int32_t to = n + face;
            if (to > board->size)
            {
                break;
            }
            status = mk_chain_add_transition(chain, (mk_state) (n - 1),
                                             (mk_state) (to - 1), 1);
            if (status != MK_OK)
            {
                return status;
            }
        }
    }
    return MK_OK;
}
