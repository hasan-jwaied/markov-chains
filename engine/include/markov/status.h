#ifndef MARKOV_STATUS_H
#define MARKOV_STATUS_H

/**
 * Status codes returned by every fallible libmarkov function.
 * MK_OK is zero so `if (status) { ... }` reads naturally.
 */
typedef enum mk_status
{
    MK_OK = 0,
    MK_ERR_NOMEM,          /* an allocation failed; nothing was leaked      */
    MK_ERR_INVALID,        /* a NULL pointer or out-of-range argument       */
    MK_ERR_EMPTY,          /* the chain has no states                       */
    MK_ERR_NO_START,       /* no state can begin a sequence                 */
    MK_ERR_DEAD_END,       /* the state has no outgoing transitions         */
    MK_ERR_LIMIT,          /* a 32-bit counter or id space would overflow   */
    MK_ERR_TOO_LARGE,      /* dense algorithm refused: too many states      */
    MK_ERR_NOT_ABSORBING,  /* some transient state can never be absorbed    */
    MK_ERR_SINGULAR,       /* a linear system has no unique solution        */
    MK_ERR_NOT_CONVERGED   /* an iterative method hit its iteration cap     */
} mk_status;

/** Human-readable description of a status code (never NULL). */
const char *mk_status_str(mk_status status);

#endif /* MARKOV_STATUS_H */
