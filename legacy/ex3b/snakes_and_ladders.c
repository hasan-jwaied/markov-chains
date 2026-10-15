#include <string.h> // For strlen(), strcmp(), strcpy()
#include <errno.h>  // For errno (argument parsing)
#include "markov_chain.h"
#include <limits.h> // For INT_MAX, UINT_MAX

#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))

#define EMPTY -1
#define BOARD_SIZE 100
#define MAX_GENERATION_LENGTH 60

#define DICE_MAX 6
#define NUM_OF_TRANSITIONS 20

#define NUM_ARGS_ERROR "Usage: invalid number of arguments"
/**
 * represents the transitions by ladders and snakes in the game
 * each tuple (x,y) represents a ladder from x to if x<y or a snake otherwise
 */
const int transitions[][2] = {
    {13, 4},
    {85, 17},
    {95, 67},
    {97, 58},
    {66, 89},
    {87, 31},
    {57, 83},
    {91, 25},
    {28, 50},
    {35, 11},
    {8, 30},
    {41, 62},
    {81, 43},
    {69, 32},
    {20, 39},
    {33, 70},
    {79, 99},
    {23, 76},
    {15, 47},
    {61, 14}
};

/**
 * struct represents a Cell in the game board
 */
typedef struct Cell
{
    int number; // Cell number 1-100
    int ladder_to; // cell which ladder leads to, if there is one
    int snake_to; // cell which snake leads to, if there is one
    //both ladder_to and snake_to should be -1 if the Cell doesn't have them
} Cell;

// Forward declarations
static void free_cell(void *data);
static int comp_cells(void *data1, void *data2);
static void print_cell(void *data);
static bool is_last_cell(void *data);
static void *copy_cell(void *data);
static MarkovNode *find_cell_node(MarkovChain *markov_chain, Cell *cell);
static long parse_long(const char *str, const char *arg_name, unsigned long max);

/**
 * allocates memory for cells on the board and initalizes them
 * @param cells Array of pointer to Cell, represents game board
 * @return EXIT_SUCCESS if successful, else EXIT_FAILURE
 */
int create_board(Cell *cells[BOARD_SIZE])
{
    for (int i = 0; i < BOARD_SIZE; i++)
    {
        cells[i] = malloc(sizeof(Cell));
        if (cells[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                free(cells[j]);
            }
            printf(ALLOCATION_ERROR_MESSAGE);
            return EXIT_FAILURE;
        }
        *(cells[i]) = (Cell){i + 1, EMPTY, EMPTY};
    }

    for (int i = 0; i < NUM_OF_TRANSITIONS; i++)
    {
        int from = transitions[i][0];
        int to = transitions[i][1];
        if (from < to)
        {
            cells[from - 1]->ladder_to = to;
        } else
        {
            cells[from - 1]->snake_to = to;
        }
    }
    return EXIT_SUCCESS;
}

int add_cells_to_database(MarkovChain *markov_chain, Cell *cells[BOARD_SIZE])
{
    for (size_t i = 0; i < BOARD_SIZE; i++)
    {
        Node *tmp = add_to_database(markov_chain, cells[i]);
        if (tmp == NULL)
        {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int set_nodes_frequencies(MarkovChain *markov_chain, Cell *cells[BOARD_SIZE])
{
    MarkovNode *from_node = NULL, *to_node = NULL;
    size_t index_to;

    for (size_t i = 0; i < BOARD_SIZE; i++)
    {
        from_node = find_cell_node(markov_chain, cells[i]);
        if (from_node == NULL)
        {
            return EXIT_FAILURE;
        }
        if (cells[i]->snake_to != EMPTY || cells[i]->ladder_to != EMPTY)
        {
            index_to = (size_t) (MAX(cells[i]->snake_to, cells[i]->ladder_to) - 1);
            to_node = find_cell_node(markov_chain, cells[index_to]);
            if (to_node == NULL)
            {
                return EXIT_FAILURE;
            }
            int res = add_node_to_frequency_list(from_node, to_node);
            if (res == EXIT_FAILURE)
            {
                return EXIT_FAILURE;
            }
        }
        else
        {
            for (int j = 1; j <= DICE_MAX; j++)
            {
                index_to = (size_t) (((Cell *) (from_node->data))->number + j - 1);
                if (index_to >= BOARD_SIZE)
                {
                    break;
                }
                to_node = find_cell_node(markov_chain, cells[index_to]);
                if (to_node == NULL)
                {
                    return EXIT_FAILURE;
                }
                int res = add_node_to_frequency_list(from_node, to_node);
                if (res == EXIT_FAILURE)
                {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

/**
 * fills database
 * @param markov_chain
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int fill_database_snakes(MarkovChain *markov_chain)
{
    Cell *cells[BOARD_SIZE];
    if (create_board(cells) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }
    if (add_cells_to_database(markov_chain, cells) == EXIT_FAILURE)
    {
        for (size_t i = 0; i < BOARD_SIZE; i++)
        {
            free(cells[i]);
        }
        return EXIT_FAILURE;
    }

    if(set_nodes_frequencies(markov_chain, cells) == EXIT_FAILURE)
    {
        for (size_t i = 0; i < BOARD_SIZE; i++)
        {
            free(cells[i]);
        }
        return EXIT_FAILURE;
    }
    
    for (size_t i = 0; i < BOARD_SIZE; i++) // free temp arr
    {
        free(cells[i]);
    }
    return EXIT_SUCCESS;
}

static void print_cell(void *data)
{
    Cell *c = (Cell *)data;
    printf("[%d]", c->number);

    if (c->number == BOARD_SIZE)
    {
        return;
    }
    if (c->ladder_to != EMPTY)
    {
        printf(" -ladder to-> ");
    }
    else if (c->snake_to != EMPTY)
    {
        printf(" -snake to-> ");
    } else {
        printf (" -> ");
    }
}

static int comp_cells (void *data1, void *data2)
{
    Cell *c1 = (Cell *)data1;
    Cell *c2 = (Cell *)data2;
    return c1->number - c2->number;
}

static void free_cell(void *data)
{
    free(data);
}

static void *copy_cell(void *data)
{
    Cell *src = (Cell *)data;
    Cell *dest = malloc(sizeof(Cell));
    if (dest != NULL)
    {
        *dest = *src;
    }
    return dest;
}

static bool is_last_cell(void *data)
{
    Cell *c = (Cell *)data;
    return c->number == BOARD_SIZE;
}

/**
 * Look up the MarkovNode holding the given cell.
 * @return the node, or NULL if the cell is not in the database
 */
static MarkovNode *find_cell_node(MarkovChain *markov_chain, Cell *cell)
{
    Node *node = get_node_from_database(markov_chain, cell);
    return node ? node->data : NULL;
}

/**
 * Parse a non-negative base-10 integer argument.
 * @return the value, or -1 (after printing an error) if it is invalid
 */
static long parse_long(const char *str, const char *arg_name, unsigned long max)
{
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0' || val < 0
        || (unsigned long) val > max)
    {
        fprintf(stderr, "Error: Invalid value for %s\n", arg_name);
        return -1;
    }
    return val;
}

/**
 * @param argc num of arguments
 * @param argv 1) Seed
 * 2) Number of sentences to generate
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf(NUM_ARGS_ERROR);
        return EXIT_FAILURE;
    }
    long seed_val = parse_long(argv[1], "seed", UINT_MAX);
    long paths_val = parse_long(argv[2], "num_paths", (unsigned long) INT_MAX);
    if (seed_val == -1 || paths_val == -1)
    {
        return EXIT_FAILURE;
    }
    unsigned int seed = (unsigned int) seed_val;
    int num_paths = (int) paths_val;
    srand(seed);

    MarkovChain *chain = malloc(sizeof(MarkovChain));
    if (!chain)
    {
        printf(ALLOCATION_ERROR_MESSAGE);
        return EXIT_FAILURE;
    }

    chain->database = malloc(sizeof(LinkedList));
    if (!chain->database)
    {
        printf(ALLOCATION_ERROR_MESSAGE);
        free(chain);
        return EXIT_FAILURE;
    }
    chain->database->first = NULL;
    chain->database->last = NULL;
    chain->database->size = 0;
    chain->print_func = print_cell;
    chain->comp_func = comp_cells;
    chain->free_data = free_cell;
    chain->copy_func = copy_cell;
    chain->is_last = is_last_cell;

    if (fill_database_snakes(chain) != EXIT_SUCCESS)
    {
        free_database(&chain);
        return EXIT_FAILURE;
    }

    for (int i=0; i < num_paths; i++)
    {
        printf("Random Walk %d: ", i+1);
        Cell start_val = {1, EMPTY, EMPTY};
        Node *start_node = get_node_from_database(chain, &start_val);

        if (start_node)
        {
            generate_random_sequence(chain, start_node->data, MAX_GENERATION_LENGTH);
        }
    }
    free_database(&chain);
    return EXIT_SUCCESS;
}
