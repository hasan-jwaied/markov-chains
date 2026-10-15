#include <stdlib.h>
#include <string.h>
#include "mk_test.h"

/* ---- helpers ------------------------------------------------------------ */

static mk_chain *line_of_states(uint32_t n)
{
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_word(), &chain));
    char name[16];
    for (uint32_t i = 0; i < n; i++)
    {
        mk_state id;
        snprintf(name, sizeof(name), "s%u", i);
        CHECK_OK(mk_chain_intern(chain, name, &id, NULL));
    }
    return chain;
}

/** Gambler's ruin on 0..n: step up with weight `up`, down with `down`. */
static mk_chain *gamblers_ruin(uint32_t n, uint32_t up, uint32_t down)
{
    mk_chain *chain = line_of_states(n + 1u);
    for (mk_state i = 1; i < n; i++)
    {
        CHECK_OK(mk_chain_add_transition(chain, i, i + 1u, up));
        CHECK_OK(mk_chain_add_transition(chain, i, i - 1u, down));
    }
    return chain;
}

static mk_chain *classic_snakes(void)
{
    mk_chain *chain = NULL;
    CHECK_OK(mk_chain_create(mk_plugin_cell(), &chain));
    CHECK_OK(mk_snakes_build(chain, mk_snakes_classic_board()));
    return chain;
}

/** Expected game length by plain value iteration on the board rules,
 *  written independently of the engine as a cross-check. */
static double snakes_value_iteration(const mk_board *b)
{
    double e[MK_SNAKES_MAX_SIZE + 1] = {0};
    for (int iter = 0; iter < 100000; iter++)
    {
        double change = 0.0;
        for (int32_t c = b->size - 1; c >= 1; c--)
        {
            double v = -1.0;
            for (uint32_t j = 0; j < b->n_jumps; j++)
            {
                if (b->jumps[j].from == c)
                {
                    v = 1.0 + e[b->jumps[j].to];
                }
            }
            if (v < 0.0)
            {
                double sum = 0.0;
                int faces = 0;
                for (int32_t f = 1; f <= b->faces && c + f <= b->size; f++)
                {
                    sum += e[c + f];
                    faces++;
                }
                v = 1.0 + sum / faces;
            }
            change = fmax(change, fabs(v - e[c]));
            e[c] = v;
        }
        if (change < 1e-13)
        {
            break;
        }
    }
    return e[1];
}

/* ---- linear algebra ----------------------------------------------------- */

static void test_linalg(void)
{
    double a[] = {2, 1, 1, 3};
    double b[] = {3, 5};
    CHECK_OK(mk_linalg_solve(a, b, 2, 1));
    CHECK_NEAR(b[0], 0.8, 1e-14);
    CHECK_NEAR(b[1], 1.4, 1e-14);

    double s[] = {1, 2, 2, 4};
    double rhs[] = {1, 1};
    CHECK_STATUS(mk_linalg_solve(s, rhs, 2, 1), MK_ERR_SINGULAR);

    double m[] = {4, 7, 2, 6};
    double inv[4];
    CHECK_OK(mk_linalg_inverse(m, inv, 2));
    CHECK_NEAR(inv[0], 0.6, 1e-14);
    CHECK_NEAR(inv[1], -0.7, 1e-14);
    CHECK_NEAR(inv[2], -0.2, 1e-14);
    CHECK_NEAR(inv[3], 0.4, 1e-14);

    /* Pivoting: a zero on the diagonal must not stop elimination. */
    double p[] = {0, 1, 1, 0};
    double q[] = {2, 3};
    CHECK_OK(mk_linalg_solve(p, q, 2, 1));
    CHECK_NEAR(q[0], 3.0, 1e-15);
    CHECK_NEAR(q[1], 2.0, 1e-15);
}

/* ---- transition matrix -------------------------------------------------- */

static void test_matrix_export(void)
{
    mk_chain *chain = classic_snakes();
    const size_t n = 100;
    double *dense = malloc(sizeof(double) * n * n);
    CHECK(dense != NULL);
    CHECK_OK(mk_math_dense(chain, dense, n));
    CHECK_STATUS(mk_math_dense(chain, dense, n - 1), MK_ERR_INVALID);
    for (size_t i = 0; i < n; i++)
    {
        double row = 0.0;
        for (size_t j = 0; j < n; j++)
        {
            row += dense[i * n + j];
        }
        CHECK_NEAR(row, 1.0, 1e-12);
    }
    CHECK_NEAR(dense[99 * n + 99], 1.0, 0.0);    /* absorbing identity row */
    CHECK_NEAR(dense[0 * n + 1], 1.0 / 6, 1e-15); /* 1 -> 2 */
    CHECK_NEAR(dense[12 * n + 3], 1.0, 0.0);     /* snake 13 -> 4 */

    mk_csr csr;
    CHECK_OK(mk_math_csr(chain, &csr));
    double max_diff = 0.0;
    for (uint32_t i = 0; i < csr.rows; i++)
    {
        for (uint32_t k = csr.row_ptr[i]; k < csr.row_ptr[i + 1]; k++)
        {
            max_diff = fmax(max_diff,
                            fabs(csr.val[k] - dense[i * n + csr.col[k]]));
        }
    }
    CHECK_NEAR(max_diff, 0.0, 0.0);
    CHECK(mk_math_is_absorbing(chain, 99));
    CHECK(!mk_math_is_absorbing(chain, 0));
    mk_csr_free(&csr);
    free(dense);
    mk_chain_destroy(&chain);
}

/* ---- stationary distribution ------------------------------------------- */

static void test_stationary_two_state(void)
{
    mk_chain *chain = line_of_states(2);
    CHECK_OK(mk_chain_add_transition(chain, 0, 0, 7));
    CHECK_OK(mk_chain_add_transition(chain, 0, 1, 3)); /* p = 0.3 */
    CHECK_OK(mk_chain_add_transition(chain, 1, 0, 2)); /* q = 0.2 */
    CHECK_OK(mk_chain_add_transition(chain, 1, 1, 8));
    double pi[2];
    mk_stationary_result r;
    CHECK_OK(mk_math_stationary(chain, NULL, pi, &r));
    CHECK(r.converged);
    CHECK_NEAR(pi[0], 0.4, 1e-10); /* q / (p + q) */
    CHECK_NEAR(pi[1], 0.6, 1e-10);
    mk_chain_destroy(&chain);
}

static void test_stationary_periodic(void)
{
    mk_chain *chain = line_of_states(2);
    CHECK_OK(mk_chain_add_transition(chain, 0, 1, 1));
    CHECK_OK(mk_chain_add_transition(chain, 1, 0, 1));
    const double start[] = {1.0, 0.0};
    mk_stationary_opts opts = mk_stationary_defaults();
    opts.initial = start;
    opts.max_iterations = 100;
    double pi[2];
    mk_stationary_result r;
    /* Period 2: plain power iteration oscillates forever... */
    CHECK_STATUS(mk_math_stationary(chain, &opts, pi, &r),
                 MK_ERR_NOT_CONVERGED);
    CHECK_NEAR(r.residual, 2.0, 1e-12);
    /* ...damping makes the chain aperiodic, as in PageRank. */
    opts.damping = 0.85;
    opts.max_iterations = 1000;
    CHECK_OK(mk_math_stationary(chain, &opts, pi, &r));
    CHECK_NEAR(pi[0], 0.5, 1e-10);
    opts.damping = 0.0;
    CHECK_STATUS(mk_math_stationary(chain, &opts, pi, &r), MK_ERR_INVALID);
    mk_chain_destroy(&chain);
}

/* ---- absorbing chains --------------------------------------------------- */

static void test_gamblers_ruin_fair(void)
{
    const uint32_t n = 10;
    mk_chain *chain = gamblers_ruin(n, 1, 1);
    mk_absorbing a;
    CHECK_OK(mk_math_absorbing(chain, &a));
    CHECK_EQ(a.n_transient, n - 1);
    CHECK_EQ(a.n_absorbing, 2);
    for (uint32_t k = 0; k < a.n_transient; k++)
    {
        double i = a.transient[k];
        CHECK_NEAR(a.expected_steps[k], i * (n - i), 1e-9);
        /* absorbing[1] is state n: P(reach n) = i / n */
        CHECK_NEAR(a.absorption[k * 2 + 1], i / n, 1e-12);
        CHECK_NEAR(a.absorption[k * 2 + 0] + a.absorption[k * 2 + 1], 1.0,
                   1e-12);
    }
    mk_absorbing_free(&a);
    mk_absorbing_free(&a); /* idempotent */
    mk_chain_destroy(&chain);
}

static void test_gamblers_ruin_biased(void)
{
    const uint32_t n = 8;
    mk_chain *chain = gamblers_ruin(n, 2, 1); /* p = 2/3, r = q/p = 1/2 */
    mk_absorbing a;
    CHECK_OK(mk_math_absorbing(chain, &a));
    for (uint32_t k = 0; k < a.n_transient; k++)
    {
        double i = a.transient[k];
        double win = (1.0 - pow(0.5, i)) / (1.0 - pow(0.5, n));
        CHECK_NEAR(a.absorption[k * 2 + 1], win, 1e-12);
    }
    mk_absorbing_free(&a);
    mk_chain_destroy(&chain);
}

static void test_snakes_expected_length(void)
{
    mk_chain *chain = classic_snakes();
    double reference = snakes_value_iteration(mk_snakes_classic_board());
    CHECK_NEAR(reference, 47.5712, 5e-5);

    mk_absorbing a;
    CHECK_OK(mk_math_absorbing(chain, &a));
    uint32_t start = a.position[0];
    CHECK_NEAR(a.expected_steps[start], reference, 1e-9);
    CHECK_NEAR(a.absorption[start], 1.0, 1e-12);

    /* The sparse iterative solver agrees with the dense one. */
    double steps[100];
    CHECK_OK(mk_math_expected_steps(chain, steps, 1e-14, 100000));
    CHECK_NEAR(steps[0], reference, 1e-9);
    CHECK_NEAR(steps[99], 0.0, 0.0);

    /* The absorption-time distribution reproduces mean and variance. */
    enum { HORIZON = 3000 };
    double *pmf = malloc(sizeof(double) * (HORIZON + 1));
    double tail;
    CHECK_OK(mk_math_absorption_pmf(chain, 0, HORIZON, pmf, &tail));
    double total = tail;
    double mean = 0.0;
    double second = 0.0;
    for (int k = 0; k <= HORIZON; k++)
    {
        total += pmf[k];
        mean += k * pmf[k];
        second += (double) k * k * pmf[k];
    }
    CHECK_NEAR(total, 1.0, 1e-12);
    CHECK(tail < 1e-30);
    CHECK_NEAR(mean, reference, 1e-8);
    CHECK_NEAR(second - mean * mean, a.variance_steps[start], 1e-6);
    CHECK_NEAR(pmf[0], 0.0, 0.0);

    CHECK_OK(mk_math_absorption_pmf(chain, 99, 5, pmf, &tail));
    CHECK_NEAR(pmf[0], 1.0, 0.0);
    free(pmf);
    mk_absorbing_free(&a);
    mk_chain_destroy(&chain);
}

static void test_not_absorbing_and_too_large(void)
{
    /* a <-> b can never leave; c is absorbing but unreachable from them. */
    mk_chain *chain = line_of_states(3);
    CHECK_OK(mk_chain_add_transition(chain, 0, 1, 1));
    CHECK_OK(mk_chain_add_transition(chain, 1, 0, 1));
    mk_absorbing a;
    double steps[3];
    CHECK_STATUS(mk_math_absorbing(chain, &a), MK_ERR_NOT_ABSORBING);
    CHECK_STATUS(mk_math_expected_steps(chain, steps, 1e-9, 100),
                 MK_ERR_NOT_ABSORBING);
    mk_chain_destroy(&chain);

    /* 3000 states in a line: too big for the dense solver, easy for the
     * sparse one (E[steps from i] = 2999 - i). */
    const uint32_t n = 3000;
    chain = line_of_states(n);
    for (mk_state i = 0; i + 1 < n; i++)
    {
        CHECK_OK(mk_chain_add_transition(chain, i, i + 1, 1));
    }
    CHECK_STATUS(mk_math_absorbing(chain, &a), MK_ERR_TOO_LARGE);
    double *big = malloc(sizeof(double) * n);
    CHECK_OK(mk_math_expected_steps(chain, big, 1e-12, 10000));
    CHECK_NEAR(big[0], n - 1, 1e-6);
    CHECK_NEAR(big[1000], n - 1001, 1e-6);
    free(big);
    mk_chain_destroy(&chain);
}

void suite_math(void)
{
    RUN(test_linalg);
    RUN(test_matrix_export);
    RUN(test_stationary_two_state);
    RUN(test_stationary_periodic);
    RUN(test_gamblers_ruin_fair);
    RUN(test_gamblers_ruin_biased);
    RUN(test_snakes_expected_length);
    RUN(test_not_absorbing_and_too_large);
}
