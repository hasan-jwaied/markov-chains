#ifndef MARKOV_MATH_H
#define MARKOV_MATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "markov/chain.h"
#include "markov/linalg.h"
#include "markov/status.h"

/**
 * The mathematics of a chain: its transition matrix P, long-run behaviour
 * (stationary distribution) and absorption analysis (fundamental matrix).
 *
 * Convention: a state is ABSORBING when generation would stop there, i.e. it
 * is terminal, has no outgoing transitions, or only transitions to itself.
 * In every exported matrix an absorbing state's row is the identity row
 * (P[i][i] = 1), so every row sums to 1 and P is row-stochastic.
 * Otherwise P[i][j] = count(i -> j) / sum_k count(i -> k): the maximum
 * likelihood estimate from the observed transitions.
 */

/** Largest number of transient states the dense absorbing analysis accepts
 *  (its N matrix is t x t doubles: 2048^2 * 8 bytes = 32 MiB). */
#define MK_MATH_MAX_DENSE 2048

bool mk_math_is_absorbing(const mk_chain *chain, mk_state state);

/** Write P as a dense n x n row-major matrix; n must equal the chain size. */
mk_status mk_math_dense(const mk_chain *chain, double *out, size_t n);

/** Export P in compressed sparse row form (free with mk_csr_free). */
mk_status mk_math_csr(const mk_chain *chain, mk_csr *out);

/**
 * One power-iteration step: y = damping * (x P) + (1 - damping) / n.
 * Returns the L1 distance between x and y.
 */
double mk_math_power_step(const mk_csr *p, const double *x, double *y,
                          double damping);

typedef struct mk_stationary_opts
{
    double damping;         /* 1.0 = plain chain; 0.85 = PageRank-style */
    double tolerance;       /* stop when the L1 change drops below this */
    uint32_t max_iterations;
    const double *initial;  /* starting distribution; NULL = uniform    */
} mk_stationary_opts;

typedef struct mk_stationary_result
{
    uint32_t iterations;
    double residual;        /* L1 change of the last step */
    bool converged;
} mk_stationary_result;

mk_stationary_opts mk_stationary_defaults(void);

/**
 * Stationary distribution pi = pi P by power iteration (pi has chain-size
 * entries). On a periodic chain the iteration may not converge; with
 * damping < 1 it always does. Returns MK_ERR_NOT_CONVERGED (with pi holding
 * the last iterate) if the tolerance was not reached.
 */
mk_status mk_math_stationary(const mk_chain *chain,
                             const mk_stationary_opts *opts, double *pi,
                             mk_stationary_result *result);

/**
 * Absorbing-chain analysis. Reordering states as (transient, absorbing)
 * puts P in canonical form
 *
 *         | Q  R |
 *     P = | 0  I |
 *
 * The fundamental matrix N = (I - Q)^-1 counts expected visits: N[i][j] is
 * the expected number of times a walk from transient i visits transient j.
 * Then t = N 1 is the expected number of steps to absorption, its variance
 * is (2N - I) t - t*t, and B = N R gives the absorption probabilities.
 */
typedef struct mk_absorbing
{
    uint32_t n_transient;
    uint32_t n_absorbing;
    mk_state *transient;      /* chain ids of the transient states        */
    mk_state *absorbing;      /* chain ids of the absorbing states        */
    uint32_t *position;       /* chain id -> index in its own list        */
    double *fundamental;      /* N, n_transient x n_transient             */
    double *expected_steps;   /* t, n_transient                           */
    double *variance_steps;   /* Var, n_transient                         */
    double *absorption;       /* B, n_transient x n_absorbing             */
} mk_absorbing;

/**
 * Dense analysis via Gaussian elimination. Fails with MK_ERR_NOT_ABSORBING
 * if some transient state cannot reach an absorbing one, and with
 * MK_ERR_TOO_LARGE above MK_MATH_MAX_DENSE transient states.
 */
mk_status mk_math_absorbing(const mk_chain *chain, mk_absorbing *out);
void mk_absorbing_free(mk_absorbing *a);

/**
 * Expected steps to absorption from every state (0 for absorbing states)
 * using sparse Gauss-Seidel iteration on t = 1 + Q t. Scales to chains far
 * beyond MK_MATH_MAX_DENSE.
 */
mk_status mk_math_expected_steps(const mk_chain *chain, double *steps,
                                 double tolerance, uint32_t max_iterations);

/**
 * Distribution of the absorption time T from `start`: pmf[k] = P(T = k) for
 * k = 0..max_steps (pmf has max_steps + 1 entries). `tail` (optional)
 * receives P(T > max_steps).
 */
mk_status mk_math_absorption_pmf(const mk_chain *chain, mk_state start,
                                 uint32_t max_steps, double *pmf,
                                 double *tail);

#endif /* MARKOV_MATH_H */
