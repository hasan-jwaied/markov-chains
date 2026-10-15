#include "markov/status.h"

const char *mk_status_str(mk_status status)
{
    switch (status)
    {
        case MK_OK:                return "ok";
        case MK_ERR_NOMEM:         return "out of memory";
        case MK_ERR_INVALID:       return "invalid argument";
        case MK_ERR_EMPTY:         return "the chain has no states";
        case MK_ERR_NO_START:      return "no state can start a sequence";
        case MK_ERR_DEAD_END:      return "state has no outgoing transitions";
        case MK_ERR_LIMIT:         return "capacity limit exceeded";
        case MK_ERR_TOO_LARGE:     return "too many states for a dense algorithm";
        case MK_ERR_NOT_ABSORBING: return "some state can never be absorbed";
        case MK_ERR_SINGULAR:      return "singular linear system";
        case MK_ERR_NOT_CONVERGED: return "iteration did not converge";
    }
    return "unknown status";
}
