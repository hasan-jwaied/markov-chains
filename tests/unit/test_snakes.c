#include <string.h>
#include "mk_test.h"

static void test_classic_board(void)
{
    const mk_board *board = mk_snakes_classic_board();
    CHECK_OK(mk_snakes_validate(board));
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_cell(), &chain));
    CHECK_OK(mk_snakes_build(chain, board));
    CHECK_EQ(mk_chain_size(chain), 100);

    const mk_cell *c13 = mk_chain_data(chain, 12);
    CHECK_EQ(c13->number, 13);
    CHECK_EQ(c13->snake_to, 4);
    const mk_cell *c8 = mk_chain_data(chain, 7);
    CHECK_EQ(c8->ladder_to, 30);
    CHECK(mk_chain_is_terminal(chain, 99));
    CHECK(!mk_chain_is_terminal(chain, 98));

    /* A jump square moves deterministically; others roll a d6. */
    CHECK_EQ(mk_chain_out_degree(chain, 12), 1);
    CHECK_EQ(mk_chain_edges(chain, 12)[0].target, 3);
    CHECK_EQ(mk_chain_out_degree(chain, 0), 6);
    /* Near the end, overshooting throws are dropped. */
    CHECK_EQ(mk_chain_out_degree(chain, 97), 2); /* 98 -> 99, 100 */
    CHECK_EQ(mk_chain_out_degree(chain, 99), 0);

    /* Building twice into the same chain is refused. */
    CHECK_STATUS(mk_snakes_build(chain, board), MK_ERR_INVALID);
    mk_chain_destroy(&chain);
}

static void test_validation(void)
{
    const mk_jump self[] = {{5, 5}};
    const mk_jump from_last[] = {{10, 2}};
    const mk_jump off_board[] = {{3, 11}};
    const mk_jump twice[] = {{3, 7}, {3, 1}};
    const mk_jump chained[] = {{2, 6}, {6, 9}}; /* allowed: two steps */
    mk_board b = {10, 6, 1, self};
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    b.jumps = from_last;
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    b.jumps = off_board;
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    b.jumps = twice;
    b.n_jumps = 2;
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    b.jumps = chained;
    CHECK_OK(mk_snakes_validate(&b));
    b.size = 1;
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    b.size = 10;
    b.faces = 0;
    CHECK_STATUS(mk_snakes_validate(&b), MK_ERR_INVALID);
    CHECK_STATUS(mk_snakes_validate(NULL), MK_ERR_INVALID);

    mk_chain *words = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_word(), &words));
    CHECK_STATUS(mk_snakes_build(words, mk_snakes_classic_board()),
                 MK_ERR_INVALID); /* wrong plugin */
    mk_chain_destroy(&words);
}

static void test_cell_plugin_and_status_strings(void)
{
    const mk_vtable *vt = mk_plugin_cell();
    mk_cell a = {7, MK_CELL_EMPTY, 3, 0};
    mk_cell b = {9, 20, MK_CELL_EMPTY, 0};
    CHECK(vt->compare(&a, &b) < 0);
    CHECK(vt->compare(&b, &a) > 0);
    CHECK(vt->compare(&a, &a) == 0);
    CHECK(vt->hash(&a) != vt->hash(&b));
    char buf[8];
    CHECK_EQ(vt->format(&b, buf, sizeof(buf)), 3);
    CHECK(strcmp(buf, "[9]") == 0);
    mk_cell *copy = vt->copy(&b);
    CHECK(copy != NULL && copy->ladder_to == 20);
    vt->destroy(copy);

    /* Every status code has its own message. */
    for (int s = MK_OK; s <= MK_ERR_NOT_CONVERGED; s++)
    {
        CHECK(strcmp(mk_status_str((mk_status) s), "unknown status") != 0);
        for (int t = MK_OK; t < s; t++)
        {
            CHECK(strcmp(mk_status_str((mk_status) s),
                         mk_status_str((mk_status) t)) != 0);
        }
    }
    CHECK(strcmp(mk_status_str((mk_status) 99), "unknown status") == 0);
}

void suite_snakes(void)
{
    RUN(test_cell_plugin_and_status_strings);
    RUN(test_classic_board);
    RUN(test_validation);
}
