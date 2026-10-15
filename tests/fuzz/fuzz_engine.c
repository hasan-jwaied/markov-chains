/*
 * Fuzz target for libmarkov (libFuzzer entry point).
 *
 * Byte 0 selects the scenario and its options:
 *   - text:   build an order-k chain from the rest of the input, generate
 *             with every start policy / sampler, render, and run the math.
 *   - board:  decode a Snakes & Ladders board, build it and analyse it.
 *             Arbitrary boards can trap a walk in a snake/ladder loop, so
 *             MK_ERR_NOT_ABSORBING is a legitimate outcome.
 * Any crash, sanitizer report or leaked engine allocation is a failure.
 */
#include <stdint.h>
#include <stdlib.h>
#include "markov/markov.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define REQUIRE(cond)    \
    do                   \
    {                    \
        if (!(cond))     \
        {                \
            abort();     \
        }                \
    } while (0)

static bool acceptable(mk_status st)
{
    return st == MK_OK || st == MK_ERR_EMPTY || st == MK_ERR_NO_START
           || st == MK_ERR_NOT_ABSORBING || st == MK_ERR_NOT_CONVERGED;
}

static void fuzz_text(const uint8_t *data, size_t size, uint8_t cfg)
{
    mk_chain *chain = NULL;
    REQUIRE(mk_chain_create(mk_plugin_ngram(), &chain) == MK_OK);
    mk_text_options opts = mk_text_defaults();
    opts.order = 1u + (cfg >> 1u) % 4u;
    opts.terminal_rule = (cfg >> 3u) & 1u ? MK_TERMINAL_SENTENCE
                                          : MK_TERMINAL_PERIOD;
    opts.max_words = (cfg >> 4u) & 1u ? (int64_t) (size / 3u) : -1;
    REQUIRE(mk_text_build(chain, (const char *) data, size, &opts, NULL)
            == MK_OK);

    mk_rng rng;
    mk_rng_seed(&rng, (cfg >> 5u) & 1u ? MK_RNG_GLIBC : MK_RNG_PCG32, size);
    mk_state out[24];
    char text[256];
    for (int policy = MK_START_COMPAT; policy <= MK_START_EMPIRICAL; policy++)
    {
        for (int sampler = MK_SAMPLE_LINEAR; sampler <= MK_SAMPLE_ALIAS;
             sampler++)
        {
            mk_generate_opts gen = mk_generate_defaults();
            gen.start_policy = (mk_start_policy) policy;
            gen.sampler = (mk_sampler) sampler;
            gen.max_length = 24;
            gen.temperature = (cfg >> 6u) & 1u ? 0.5 : 1.0;
            size_t n = 0;
            mk_status st = mk_chain_generate(chain, &rng, &gen, out, 24, &n);
            REQUIRE(acceptable(st));
            REQUIRE(n <= 24);
            for (size_t i = 0; i < n; i++)
            {
                REQUIRE(out[i] < mk_chain_size(chain));
            }
            size_t len = mk_text_render(chain, out, n,
                                        (mk_render_style) sampler, text,
                                        sizeof(text));
            REQUIRE(len >= n || n == 0);
        }
    }

    uint32_t states = mk_chain_size(chain);
    if (states > 0 && states <= 256)
    {
        double pi[256];
        mk_stationary_opts so = mk_stationary_defaults();
        so.damping = 0.85;
        so.max_iterations = 500;
        REQUIRE(acceptable(mk_math_stationary(chain, &so, pi, NULL)));
        double sum = 0.0;
        for (uint32_t i = 0; i < states; i++)
        {
            REQUIRE(pi[i] >= 0.0);
            sum += pi[i];
        }
        REQUIRE(sum > 0.999 && sum < 1.001);

        mk_absorbing a;
        mk_status st = mk_math_absorbing(chain, &a);
        REQUIRE(acceptable(st));
        if (st == MK_OK)
        {
            for (uint32_t i = 0; i < a.n_transient; i++)
            {
                REQUIRE(a.expected_steps[i] >= 1.0 - 1e-9);
            }
            mk_absorbing_free(&a);
        }
    }
    mk_chain_destroy(&chain);
}

static void fuzz_board(const uint8_t *data, size_t size, uint8_t cfg)
{
    if (size < 2)
    {
        return;
    }
    mk_jump jumps[64];
    mk_board board;
    board.size = 2 + data[0] % 120;
    board.faces = 1 + data[1] % 8;
    board.n_jumps = 0;
    board.jumps = jumps;
    for (size_t i = 2; i + 1 < size && board.n_jumps < 64; i += 2)
    {
        jumps[board.n_jumps].from = 1 + data[i] % board.size;
        jumps[board.n_jumps].to = 1 + data[i + 1] % board.size;
        board.n_jumps++;
    }
    mk_status valid = mk_snakes_validate(&board);
    mk_chain *chain = NULL;
    REQUIRE(mk_chain_create(mk_plugin_cell(), &chain) == MK_OK);
    mk_status st = mk_snakes_build(chain, &board);
    REQUIRE(st == valid);
    if (st == MK_OK)
    {
        mk_absorbing a;
        st = mk_math_absorbing(chain, &a);
        REQUIRE(acceptable(st));
        double steps[121];
        mk_status iterative = mk_math_expected_steps(chain, steps, 1e-10,
                                                     200000);
        REQUIRE(acceptable(iterative) || iterative == MK_ERR_NOT_CONVERGED);
        if (st == MK_OK)
        {
            REQUIRE(a.n_transient + a.n_absorbing == (uint32_t) board.size);
            if (iterative == MK_OK)
            {
                double dense = a.expected_steps[a.position[0]];
                double diff = dense - steps[0];
                REQUIRE(diff < 1e-5 * (1.0 + dense)
                        && -diff < 1e-5 * (1.0 + dense));
            }
            mk_absorbing_free(&a);
        }
        else
        {
            REQUIRE(iterative == MK_ERR_NOT_ABSORBING);
        }
        mk_rng rng;
        mk_rng_seed(&rng, MK_RNG_PCG32, cfg);
        mk_generate_opts gen = mk_generate_defaults();
        gen.start = 0;
        gen.max_length = 64;
        mk_state walk[64];
        size_t n;
        REQUIRE(mk_chain_generate(chain, &rng, &gen, walk, 64, &n) == MK_OK);
    }
    mk_chain_destroy(&chain);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
    {
        return 0;
    }
    mk_alloc_stats before;
    mk_alloc_stats after;
    mk_alloc_get_stats(&before);

    uint8_t cfg = data[0];
    if (cfg & 1u)
    {
        fuzz_board(data + 1, size - 1, cfg);
    }
    else
    {
        fuzz_text(data + 1, size - 1, cfg);
    }

    mk_alloc_get_stats(&after);
    REQUIRE(after.live_blocks == before.live_blocks);
    return 0;
}
