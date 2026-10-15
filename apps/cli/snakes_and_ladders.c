/*
 * snakes_and_ladders: random walks on the classic Snakes & Ladders board.
 *
 *   snakes_and_ladders [--modern] <seed> <num_paths>
 *   snakes_and_ladders --analyze
 *
 * Walks reproduce the original assignment output exactly by default;
 * --modern uses PCG32 and alias sampling. --analyze prints the exact game
 * statistics from the absorbing-chain fundamental matrix N = (I - Q)^-1.
 */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli_common.h"
#include "markov/markov.h"

#define MAX_GENERATION_LENGTH 60
#define ANALYSIS_HORIZON 1000

static void print_walk(const mk_chain *chain, const mk_state *states, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        const mk_cell *cell = mk_chain_data(chain, states[i]);
        printf("[%d]", (int) cell->number);
        if (cell->is_last)
        {
            continue;
        }
        if (cell->ladder_to != MK_CELL_EMPTY)
        {
            printf(" -ladder to-> ");
        }
        else if (cell->snake_to != MK_CELL_EMPTY)
        {
            printf(" -snake to-> ");
        }
        else
        {
            printf(" -> ");
        }
    }
    printf("\n");
}

static mk_status analyze(mk_chain *chain)
{
    mk_absorbing a;
    mk_status status = mk_math_absorbing(chain, &a);
    if (status != MK_OK)
    {
        return status;
    }
    double *pmf = malloc(sizeof(double) * (ANALYSIS_HORIZON + 1));
    double tail = 0.0;
    if (pmf == NULL)
    {
        mk_absorbing_free(&a);
        return MK_ERR_NOMEM;
    }
    status = mk_math_absorption_pmf(chain, 0, ANALYSIS_HORIZON, pmf, &tail);
    if (status == MK_OK)
    {
        uint32_t start = a.position[0];
        double mean = a.expected_steps[start];
        double cdf = 0.0;
        double within_limit = 0.0;
        uint32_t median = 0;
        uint32_t mode = 0;
        for (uint32_t k = 0; k <= ANALYSIS_HORIZON; k++)
        {
            cdf += pmf[k];
            if (k <= MAX_GENERATION_LENGTH)
            {
                within_limit = cdf;
            }
            if (median == 0 && cdf >= 0.5)
            {
                median = k;
            }
            if (pmf[k] > pmf[mode])
            {
                mode = k;
            }
        }
        printf("Classic board: %u transient squares, %u absorbing\n",
               a.n_transient, a.n_absorbing);
        printf("Expected moves from square 1   E[T]  = %.4f\n", mean);
        printf("Standard deviation             sd(T) = %.4f\n",
               sqrt(a.variance_steps[start]));
        printf("Median / most likely length          = %u / %u\n", median,
               mode);
        printf("P(game ends within %d moves)        = %.4f\n",
               MAX_GENERATION_LENGTH, within_limit);
        printf("P(game longer than %d moves)      = %.2e\n",
               ANALYSIS_HORIZON, tail);
        printf("Expected visits to square 1 (N[1][1]) = %.4f\n",
               a.fundamental[(size_t) start * a.n_transient + start]);
    }
    free(pmf);
    mk_absorbing_free(&a);
    return status;
}

int main(int argc, char *argv[])
{
    bool modern = false;
    bool analysis = false;
    int arg = 1;
    for (; arg < argc && strncmp(argv[arg], "--", 2) == 0; arg++)
    {
        if (strcmp(argv[arg], "--modern") == 0)
        {
            modern = true;
        }
        else if (strcmp(argv[arg], "--analyze") == 0)
        {
            analysis = true;
        }
        else
        {
            fprintf(stderr, "Usage: snakes_and_ladders [--modern] <seed> "
                            "<num_paths> | --analyze\n");
            return EXIT_FAILURE;
        }
    }

    unsigned long seed = 0;
    unsigned long num_paths = 0;
    if (!analysis)
    {
        if (argc - arg != 2)
        {
            fprintf(stderr, "Usage: invalid number of arguments\n");
            return EXIT_FAILURE;
        }
        if (!cli_parse_ulong(argv[arg], UINT_MAX, "seed", &seed)
            || !cli_parse_ulong(argv[arg + 1], INT_MAX, "num_paths",
                                &num_paths))
        {
            return EXIT_FAILURE;
        }
    }

    mk_chain *chain = NULL;
    mk_status status = mk_chain_create(mk_plugin_cell(), &chain);
    if (status == MK_OK)
    {
        status = mk_snakes_build(chain, mk_snakes_classic_board());
    }
    if (status == MK_OK && analysis)
    {
        status = analyze(chain);
    }

    mk_rng rng;
    mk_rng_seed(&rng, modern ? MK_RNG_PCG32 : MK_RNG_GLIBC, seed);
    mk_generate_opts gen = mk_generate_defaults();
    gen.start = 0; /* square 1 */
    gen.max_length = MAX_GENERATION_LENGTH;
    gen.sampler = modern ? MK_SAMPLE_ALIAS : MK_SAMPLE_LINEAR;

    mk_state walk[MAX_GENERATION_LENGTH];
    for (unsigned long i = 0; status == MK_OK && i < num_paths; i++)
    {
        size_t n;
        status = mk_chain_generate(chain, &rng, &gen, walk,
                                   MAX_GENERATION_LENGTH, &n);
        if (status == MK_OK)
        {
            printf("Random Walk %lu: ", i + 1u);
            print_walk(chain, walk, n);
        }
    }

    mk_chain_destroy(&chain);
    if (status != MK_OK)
    {
        fprintf(stderr, "Error: %s\n", mk_status_str(status));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
