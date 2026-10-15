#ifndef MARKOV_LINALG_H
#define MARKOV_LINALG_H

#include <stddef.h>
#include <stdint.h>
#include "markov/status.h"

/**
 * Small dense and sparse linear algebra used by the Markov math module.
 * Matrices are row-major arrays of doubles.
 */

/**
 * Solve A X = B in place by Gaussian elimination with partial pivoting.
 * A is n x n and is destroyed; B is n x m and is overwritten with X.
 * Returns MK_ERR_SINGULAR if a pivot is (numerically) zero.
 */
mk_status mk_linalg_solve(double *a, double *b, size_t n, size_t m);

/** out = A^-1 (A is n x n and is destroyed). */
mk_status mk_linalg_inverse(double *a, double *out, size_t n);

/** Compressed sparse row matrix. */
typedef struct mk_csr
{
    uint32_t rows;
    uint32_t cols;
    uint32_t nnz;
    uint32_t *row_ptr; /* rows + 1 offsets into col / val */
    uint32_t *col;     /* nnz column indices              */
    double *val;       /* nnz values                      */
} mk_csr;

/** Allocate an empty CSR with room for `nnz` entries (row_ptr zeroed). */
mk_status mk_csr_alloc(mk_csr *m, uint32_t rows, uint32_t cols, uint32_t nnz);
void mk_csr_free(mk_csr *m);

/** y = x M, for a row vector x of length rows and y of length cols. */
void mk_csr_left_multiply(const mk_csr *m, const double *x, double *y);

#endif /* MARKOV_LINALG_H */
