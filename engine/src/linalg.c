#include "markov/linalg.h"

#include <math.h>
#include <string.h>
#include "markov/alloc.h"

mk_status mk_linalg_solve(double *a, double *b, size_t n, size_t m)
{
    if (a == NULL || b == NULL || n == 0)
    {
        return MK_ERR_INVALID;
    }
    /* Pivots smaller than this (relative to the largest entry) are treated
     * as zero: the system is singular to working precision. */
    double scale = 0.0;
    for (size_t i = 0; i < n * n; i++)
    {
        scale = fmax(scale, fabs(a[i]));
    }
    double eps = scale * 1e-12;
    if (scale == 0.0)
    {
        return MK_ERR_SINGULAR;
    }

    for (size_t col = 0; col < n; col++)
    {
        /* Partial pivoting: bring the largest remaining entry up. */
        size_t pivot = col;
        for (size_t r = col + 1; r < n; r++)
        {
            if (fabs(a[r * n + col]) > fabs(a[pivot * n + col]))
            {
                pivot = r;
            }
        }
        if (fabs(a[pivot * n + col]) <= eps)
        {
            return MK_ERR_SINGULAR;
        }
        if (pivot != col)
        {
            for (size_t k = 0; k < n; k++)
            {
                double t = a[col * n + k];
                a[col * n + k] = a[pivot * n + k];
                a[pivot * n + k] = t;
            }
            for (size_t k = 0; k < m; k++)
            {
                double t = b[col * m + k];
                b[col * m + k] = b[pivot * m + k];
                b[pivot * m + k] = t;
            }
        }
        /* Eliminate below the pivot. */
        double inv = 1.0 / a[col * n + col];
        for (size_t r = col + 1; r < n; r++)
        {
            double f = a[r * n + col] * inv;
            if (f == 0.0)
            {
                continue;
            }
            a[r * n + col] = 0.0;
            for (size_t k = col + 1; k < n; k++)
            {
                a[r * n + k] -= f * a[col * n + k];
            }
            for (size_t k = 0; k < m; k++)
            {
                b[r * m + k] -= f * b[col * m + k];
            }
        }
    }
    /* Back substitution, one right-hand side column at a time. */
    for (size_t i = n; i-- > 0;)
    {
        double inv = 1.0 / a[i * n + i];
        for (size_t k = 0; k < m; k++)
        {
            double s = b[i * m + k];
            for (size_t j = i + 1; j < n; j++)
            {
                s -= a[i * n + j] * b[j * m + k];
            }
            b[i * m + k] = s * inv;
        }
    }
    return MK_OK;
}

mk_status mk_linalg_inverse(double *a, double *out, size_t n)
{
    if (a == NULL || out == NULL || n == 0)
    {
        return MK_ERR_INVALID;
    }
    memset(out, 0, n * n * sizeof(double));
    for (size_t i = 0; i < n; i++)
    {
        out[i * n + i] = 1.0;
    }
    return mk_linalg_solve(a, out, n, n);
}

mk_status mk_csr_alloc(mk_csr *m, uint32_t rows, uint32_t cols, uint32_t nnz)
{
    if (m == NULL)
    {
        return MK_ERR_INVALID;
    }
    m->rows = rows;
    m->cols = cols;
    m->nnz = nnz;
    m->row_ptr = mk_calloc((size_t) rows + 1u, sizeof(uint32_t), MK_TAG_MATH);
    m->col = mk_malloc(sizeof(uint32_t) * (nnz ? nnz : 1u), MK_TAG_MATH);
    m->val = mk_malloc(sizeof(double) * (nnz ? nnz : 1u), MK_TAG_MATH);
    if (m->row_ptr == NULL || m->col == NULL || m->val == NULL)
    {
        mk_csr_free(m);
        return MK_ERR_NOMEM;
    }
    return MK_OK;
}

void mk_csr_free(mk_csr *m)
{
    if (m == NULL)
    {
        return;
    }
    mk_free(m->row_ptr);
    mk_free(m->col);
    mk_free(m->val);
    memset(m, 0, sizeof(*m));
}

void mk_csr_left_multiply(const mk_csr *m, const double *x, double *y)
{
    memset(y, 0, sizeof(double) * m->cols);
    for (uint32_t i = 0; i < m->rows; i++)
    {
        double xi = x[i];
        if (xi == 0.0)
        {
            continue;
        }
        for (uint32_t k = m->row_ptr[i]; k < m->row_ptr[i + 1u]; k++)
        {
            y[m->col[k]] += xi * m->val[k];
        }
    }
}
