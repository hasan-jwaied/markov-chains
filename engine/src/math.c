#include "markov/math.h"

#include <math.h>
#include <string.h>
#include "markov/alloc.h"

bool mk_math_is_absorbing(const mk_chain *chain, mk_state state)
{
    uint32_t degree = mk_chain_out_degree(chain, state);
    if (mk_chain_is_terminal(chain, state) || degree == 0)
    {
        return true;
    }
    return degree == 1 && mk_chain_edges(chain, state)[0].target == state;
}

/* matrix export */

mk_status mk_math_dense(const mk_chain *chain, double *out, size_t n)
{
    if (chain == NULL || out == NULL || n != mk_chain_size(chain))
    {
        return MK_ERR_INVALID;
    }
    if (n > 0 && n > SIZE_MAX / sizeof(double) / n)
    {
        return MK_ERR_TOO_LARGE;
    }
    memset(out, 0, n * n * sizeof(double));
    for (mk_state i = 0; i < n; i++)
    {
        if (mk_math_is_absorbing(chain, i))
        {
            out[(size_t) i * n + i] = 1.0;
            continue;
        }
        const mk_edge *edges = mk_chain_edges(chain, i);
        double total = (double) mk_chain_out_total(chain, i);
        for (uint32_t k = 0; k < mk_chain_out_degree(chain, i); k++)
        {
            out[(size_t) i * n + edges[k].target] += edges[k].count / total;
        }
    }
    return MK_OK;
}

mk_status mk_math_csr(const mk_chain *chain, mk_csr *out)
{
    if (chain == NULL || out == NULL)
    {
        return MK_ERR_INVALID;
    }
    uint32_t n = mk_chain_size(chain);
    uint64_t nnz = 0;
    for (mk_state i = 0; i < n; i++)
    {
        nnz += mk_math_is_absorbing(chain, i) ? 1u : mk_chain_out_degree(chain, i);
    }
    if (nnz > UINT32_MAX)
    {
        return MK_ERR_LIMIT;
    }
    mk_status status = mk_csr_alloc(out, n, n, (uint32_t) nnz);
    if (status != MK_OK)
    {
        return status;
    }
    uint32_t k = 0;
    for (mk_state i = 0; i < n; i++)
    {
        out->row_ptr[i] = k;
        if (mk_math_is_absorbing(chain, i))
        {
            out->col[k] = i;
            out->val[k++] = 1.0;
            continue;
        }
        const mk_edge *edges = mk_chain_edges(chain, i);
        double total = (double) mk_chain_out_total(chain, i);
        for (uint32_t e = 0; e < mk_chain_out_degree(chain, i); e++)
        {
            out->col[k] = edges[e].target;
            out->val[k++] = edges[e].count / total;
        }
    }
    out->row_ptr[n] = k;
    return MK_OK;
}

/* stationary distribution */

double mk_math_power_step(const mk_csr *p, const double *x, double *y,
                          double damping)
{
    mk_csr_left_multiply(p, x, y);
    double teleport = p->cols ? (1.0 - damping) / p->cols : 0.0;
    double change = 0.0;
    for (uint32_t i = 0; i < p->cols; i++)
    {
        y[i] = damping * y[i] + teleport;
        change += fabs(y[i] - x[i]);
    }
    return change;
}

mk_stationary_opts mk_stationary_defaults(void)
{
    mk_stationary_opts opts;
    opts.damping = 1.0;
    opts.tolerance = 1e-12;
    opts.max_iterations = 10000;
    opts.initial = NULL;
    return opts;
}

mk_status mk_math_stationary(const mk_chain *chain,
                             const mk_stationary_opts *opts, double *pi,
                             mk_stationary_result *result)
{
    mk_stationary_opts defaults = mk_stationary_defaults();
    if (opts == NULL)
    {
        opts = &defaults;
    }
    if (chain == NULL || pi == NULL || !(opts->damping > 0.0)
        || opts->damping > 1.0)
    {
        return MK_ERR_INVALID;
    }
    uint32_t n = mk_chain_size(chain);
    if (n == 0)
    {
        return MK_ERR_EMPTY;
    }

    mk_csr p;
    mk_status status = mk_math_csr(chain, &p);
    if (status != MK_OK)
    {
        return status;
    }
    double *y = mk_malloc(sizeof(double) * n, MK_TAG_MATH);
    if (y == NULL)
    {
        mk_csr_free(&p);
        return MK_ERR_NOMEM;
    }
    for (uint32_t i = 0; i < n; i++)
    {
        pi[i] = opts->initial ? opts->initial[i] : 1.0 / n;
    }

    mk_stationary_result r = {0, INFINITY, false};
    while (r.iterations < opts->max_iterations)
    {
        r.residual = mk_math_power_step(&p, pi, y, opts->damping);
        memcpy(pi, y, sizeof(double) * n);
        r.iterations++;
        if (r.residual < opts->tolerance)
        {
            r.converged = true;
            break;
        }
    }
    mk_free(y);
    mk_csr_free(&p);
    if (result)
    {
        *result = r;
    }
    return r.converged ? MK_OK : MK_ERR_NOT_CONVERGED;
}

/* absorbing chains */

/**
 * Mark every state from which an absorbing state is reachable, by a
 * breadth-first search backwards from the absorbing states.
 * `absorbing` and `reaches` have one entry per state.
 */
static mk_status reach_absorbing(const mk_chain *chain, const uint8_t *absorbing,
                                 uint8_t *reaches)
{
    uint32_t n = mk_chain_size(chain);
    /* Reverse adjacency in CSR form: who points at each state. */
    uint32_t *in_ptr = mk_calloc((size_t) n + 1u, sizeof(uint32_t), MK_TAG_MATH);
    uint32_t *queue = mk_malloc(sizeof(uint32_t) * (n ? n : 1u), MK_TAG_MATH);
    uint64_t edges = 0;
    for (mk_state i = 0; i < n; i++)
    {
        edges += absorbing[i] ? 0u : mk_chain_out_degree(chain, i);
    }
    uint32_t *in_src = edges <= UINT32_MAX
        ? mk_malloc(sizeof(uint32_t) * (edges ? (size_t) edges : 1u), MK_TAG_MATH)
        : NULL;
    if (in_ptr == NULL || queue == NULL || in_src == NULL)
    {
        mk_free(in_ptr);
        mk_free(queue);
        mk_free(in_src);
        return edges > UINT32_MAX ? MK_ERR_LIMIT : MK_ERR_NOMEM;
    }
    for (mk_state i = 0; i < n; i++)
    {
        if (absorbing[i])
        {
            continue;
        }
        const mk_edge *e = mk_chain_edges(chain, i);
        for (uint32_t k = 0; k < mk_chain_out_degree(chain, i); k++)
        {
            in_ptr[e[k].target + 1u]++;
        }
    }
    for (uint32_t i = 0; i < n; i++)
    {
        in_ptr[i + 1u] += in_ptr[i];
    }
    /* Fill using queue[] as a temporary per-row cursor. */
    memcpy(queue, in_ptr, sizeof(uint32_t) * n);
    for (mk_state i = 0; i < n; i++)
    {
        if (absorbing[i])
        {
            continue;
        }
        const mk_edge *e = mk_chain_edges(chain, i);
        for (uint32_t k = 0; k < mk_chain_out_degree(chain, i); k++)
        {
            in_src[queue[e[k].target]++] = i;
        }
    }

    uint32_t head = 0;
    uint32_t tail = 0;
    memset(reaches, 0, n);
    for (mk_state i = 0; i < n; i++)
    {
        if (absorbing[i])
        {
            reaches[i] = 1;
            queue[tail++] = i;
        }
    }
    while (head < tail)
    {
        mk_state s = queue[head++];
        for (uint32_t k = in_ptr[s]; k < in_ptr[s + 1u]; k++)
        {
            if (!reaches[in_src[k]])
            {
                reaches[in_src[k]] = 1;
                queue[tail++] = in_src[k];
            }
        }
    }
    mk_free(in_ptr);
    mk_free(queue);
    mk_free(in_src);
    return MK_OK;
}

/** Classify states and verify the chain is absorbing. */
static mk_status classify(const mk_chain *chain, uint8_t **absorbing_out)
{
    uint32_t n = mk_chain_size(chain);
    uint8_t *absorbing = mk_malloc(n ? n : 1u, MK_TAG_MATH);
    uint8_t *reaches = mk_malloc(n ? n : 1u, MK_TAG_MATH);
    if (absorbing == NULL || reaches == NULL)
    {
        mk_free(absorbing);
        mk_free(reaches);
        return MK_ERR_NOMEM;
    }
    for (mk_state i = 0; i < n; i++)
    {
        absorbing[i] = mk_math_is_absorbing(chain, i) ? 1u : 0u;
    }
    mk_status status = reach_absorbing(chain, absorbing, reaches);
    for (mk_state i = 0; status == MK_OK && i < n; i++)
    {
        if (!reaches[i])
        {
            status = MK_ERR_NOT_ABSORBING;
        }
    }
    mk_free(reaches);
    if (status != MK_OK)
    {
        mk_free(absorbing);
        return status;
    }
    *absorbing_out = absorbing;
    return MK_OK;
}

void mk_absorbing_free(mk_absorbing *a)
{
    if (a == NULL)
    {
        return;
    }
    mk_free(a->transient);
    mk_free(a->absorbing);
    mk_free(a->position);
    mk_free(a->fundamental);
    mk_free(a->expected_steps);
    mk_free(a->variance_steps);
    mk_free(a->absorption);
    memset(a, 0, sizeof(*a));
}

mk_status mk_math_absorbing(const mk_chain *chain, mk_absorbing *out)
{
    if (chain == NULL || out == NULL)
    {
        return MK_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    uint32_t n = mk_chain_size(chain);
    if (n == 0)
    {
        return MK_ERR_EMPTY;
    }
    uint8_t *is_abs;
    mk_status status = classify(chain, &is_abs);
    if (status != MK_OK)
    {
        return status;
    }
    uint32_t t = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        t += is_abs[i] ? 0u : 1u;
    }
    uint32_t a = n - t;
    if (t > MK_MATH_MAX_DENSE)
    {
        mk_free(is_abs);
        return MK_ERR_TOO_LARGE;
    }

    size_t tt = (size_t) t * t;
    out->n_transient = t;
    out->n_absorbing = a;
    out->transient = mk_malloc(sizeof(mk_state) * (t ? t : 1u), MK_TAG_MATH);
    out->absorbing = mk_malloc(sizeof(mk_state) * (a ? a : 1u), MK_TAG_MATH);
    out->position = mk_malloc(sizeof(uint32_t) * n, MK_TAG_MATH);
    out->fundamental = mk_malloc(sizeof(double) * (tt ? tt : 1u), MK_TAG_MATH);
    out->expected_steps = mk_calloc(t ? t : 1u, sizeof(double), MK_TAG_MATH);
    out->variance_steps = mk_calloc(t ? t : 1u, sizeof(double), MK_TAG_MATH);
    out->absorption = mk_calloc((size_t) (t ? t : 1u) * (a ? a : 1u),
                                sizeof(double), MK_TAG_MATH);
    double *i_minus_q = mk_calloc(tt ? tt : 1u, sizeof(double), MK_TAG_MATH);
    if (!out->transient || !out->absorbing || !out->position
        || !out->fundamental || !out->expected_steps || !out->variance_steps
        || !out->absorption || !i_minus_q)
    {
        mk_free(is_abs);
        mk_free(i_minus_q);
        mk_absorbing_free(out);
        return MK_ERR_NOMEM;
    }

    /* Canonical ordering: transient states first, then absorbing ones. */
    uint32_t ti = 0;
    uint32_t ai = 0;
    for (mk_state i = 0; i < n; i++)
    {
        if (is_abs[i])
        {
            out->position[i] = ai;
            out->absorbing[ai++] = i;
        }
        else
        {
            out->position[i] = ti;
            out->transient[ti++] = i;
        }
    }

    if (t > 0)
    {
        /* I - Q */
        for (uint32_t r = 0; r < t; r++)
        {
            mk_state s = out->transient[r];
            const mk_edge *e = mk_chain_edges(chain, s);
            double total = (double) mk_chain_out_total(chain, s);
            i_minus_q[(size_t) r * t + r] += 1.0;
            for (uint32_t k = 0; k < mk_chain_out_degree(chain, s); k++)
            {
                if (!is_abs[e[k].target])
                {
                    i_minus_q[(size_t) r * t + out->position[e[k].target]]
                        -= e[k].count / total;
                }
            }
        }
        status = mk_linalg_inverse(i_minus_q, out->fundamental, t);
    }
    mk_free(i_minus_q);
    if (status != MK_OK)
    {
        mk_free(is_abs);
        mk_absorbing_free(out);
        return status;
    }

    const double *nm = out->fundamental;
    for (uint32_t r = 0; r < t; r++)
    {
        double sum = 0.0;
        for (uint32_t c = 0; c < t; c++)
        {
            sum += nm[(size_t) r * t + c];
        }
        out->expected_steps[r] = sum;
    }
    /* Var = (2N - I) t - t^2, elementwise square */
    for (uint32_t r = 0; r < t; r++)
    {
        double v = -out->expected_steps[r];
        for (uint32_t c = 0; c < t; c++)
        {
            v += 2.0 * nm[(size_t) r * t + c] * out->expected_steps[c];
        }
        out->variance_steps[r] = v - out->expected_steps[r]
                                         * out->expected_steps[r];
    }
    /* B = N R, using R's sparsity: only transient -> absorbing edges. */
    for (uint32_t k = 0; k < t; k++)
    {
        mk_state s = out->transient[k];
        const mk_edge *e = mk_chain_edges(chain, s);
        double total = (double) mk_chain_out_total(chain, s);
        for (uint32_t j = 0; j < mk_chain_out_degree(chain, s); j++)
        {
            if (!is_abs[e[j].target])
            {
                continue;
            }
            double rkj = e[j].count / total;
            uint32_t col = out->position[e[j].target];
            for (uint32_t r = 0; r < t; r++)
            {
                out->absorption[(size_t) r * a + col]
                    += nm[(size_t) r * t + k] * rkj;
            }
        }
    }
    mk_free(is_abs);
    return MK_OK;
}

mk_status mk_math_expected_steps(const mk_chain *chain, double *steps,
                                 double tolerance, uint32_t max_iterations)
{
    if (chain == NULL || steps == NULL || !(tolerance > 0.0))
    {
        return MK_ERR_INVALID;
    }
    uint32_t n = mk_chain_size(chain);
    if (n == 0)
    {
        return MK_ERR_EMPTY;
    }
    uint8_t *is_abs;
    mk_status status = classify(chain, &is_abs);
    if (status != MK_OK)
    {
        return status;
    }
    memset(steps, 0, sizeof(double) * n);

    /* Gauss-Seidel on t_i = 1 + sum_j P_ij t_j, solving each row for t_i so
     * self-loops (P_ii > 0) do not slow convergence. */
    status = MK_ERR_NOT_CONVERGED;
    for (uint32_t iter = 0; iter < max_iterations; iter++)
    {
        double change = 0.0;
        for (mk_state i = 0; i < n; i++)
        {
            if (is_abs[i])
            {
                continue;
            }
            const mk_edge *e = mk_chain_edges(chain, i);
            double total = (double) mk_chain_out_total(chain, i);
            double self = 0.0;
            double acc = 1.0;
            for (uint32_t k = 0; k < mk_chain_out_degree(chain, i); k++)
            {
                double p = e[k].count / total;
                if (e[k].target == i)
                {
                    self += p;
                }
                else
                {
                    acc += p * steps[e[k].target];
                }
            }
            double value = acc / (1.0 - self);
            change = fmax(change, fabs(value - steps[i]) / fmax(1.0, value));
            steps[i] = value;
        }
        if (change < tolerance)
        {
            status = MK_OK;
            break;
        }
    }
    mk_free(is_abs);
    return status;
}

mk_status mk_math_absorption_pmf(const mk_chain *chain, mk_state start,
                                 uint32_t max_steps, double *pmf, double *tail)
{
    uint32_t n = mk_chain_size(chain);
    if (chain == NULL || pmf == NULL || start >= n)
    {
        return MK_ERR_INVALID;
    }
    memset(pmf, 0, sizeof(double) * ((size_t) max_steps + 1u));
    if (mk_math_is_absorbing(chain, start))
    {
        pmf[0] = 1.0;
        if (tail)
        {
            *tail = 0.0;
        }
        return MK_OK;
    }
    double *x = mk_calloc(n, sizeof(double), MK_TAG_MATH);
    double *y = mk_calloc(n, sizeof(double), MK_TAG_MATH);
    if (x == NULL || y == NULL)
    {
        mk_free(x);
        mk_free(y);
        return MK_ERR_NOMEM;
    }
    /* x holds the probability of being in each transient state; mass that
     * steps into an absorbing state is recorded in pmf and removed. */
    x[start] = 1.0;
    for (uint32_t k = 1; k <= max_steps; k++)
    {
        memset(y, 0, sizeof(double) * n);
        for (mk_state i = 0; i < n; i++)
        {
            if (x[i] == 0.0)
            {
                continue;
            }
            const mk_edge *e = mk_chain_edges(chain, i);
            double scale = x[i] / (double) mk_chain_out_total(chain, i);
            for (uint32_t j = 0; j < mk_chain_out_degree(chain, i); j++)
            {
                y[e[j].target] += e[j].count * scale;
            }
        }
        for (mk_state i = 0; i < n; i++)
        {
            if (y[i] != 0.0 && mk_math_is_absorbing(chain, i))
            {
                pmf[k] += y[i];
                y[i] = 0.0;
            }
        }
        double *swap = x;
        x = y;
        y = swap;
    }
    if (tail)
    {
        double rest = 0.0;
        for (mk_state i = 0; i < n; i++)
        {
            rest += x[i];
        }
        *tail = rest;
    }
    mk_free(x);
    mk_free(y);
    return MK_OK;
}
