#include "markov/alias.h"

#include <stddef.h>
#include "markov/alloc.h"

mk_status mk_alias_build(mk_alias *table, const uint64_t *weights, uint32_t n)
{
    if (table == NULL || weights == NULL || n == 0)
    {
        return MK_ERR_INVALID;
    }
    table->n = 0;
    table->total = 0;
    table->threshold = NULL;
    table->alias = NULL;

    uint64_t total = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        if (weights[i] > UINT64_MAX / n || weights[i] > UINT64_MAX - total)
        {
            return MK_ERR_LIMIT;
        }
        total += weights[i];
    }
    if (total == 0)
    {
        return MK_ERR_INVALID;
    }

    uint64_t *threshold = mk_malloc(sizeof(uint64_t) * n, MK_TAG_ALIAS);
    uint32_t *alias = mk_malloc(sizeof(uint32_t) * n, MK_TAG_ALIAS);
    /* One work array holds both stacks: "small" grows up from the front,
     * "large" grows down from the back. */
    uint32_t *work = mk_malloc(sizeof(uint32_t) * n, MK_TAG_ALIAS);
    if (threshold == NULL || alias == NULL || work == NULL)
    {
        mk_free(threshold);
        mk_free(alias);
        mk_free(work);
        return MK_ERR_NOMEM;
    }

    /* Scale so that a "full" column holds exactly `total` units. */
    size_t n_small = 0;
    size_t n_large = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        threshold[i] = weights[i] * n;
        alias[i] = i;
        if (threshold[i] < total)
        {
            work[n_small++] = i;
        }
        else
        {
            work[n - 1 - n_large++] = i;
        }
    }

    /* Each step fills one small column completely by borrowing the deficit
     * from a large column, which may then become small itself. */
    while (n_small > 0 && n_large > 0)
    {
        uint32_t s = work[--n_small];
        uint32_t l = work[n - n_large];
        alias[s] = l;
        threshold[l] -= total - threshold[s];
        if (threshold[l] < total)
        {
            n_large--;
            work[n_small++] = l;
        }
    }
    /* The scaled weights sum to n * total and every finished column holds
     * exactly `total`, so whatever is left is exactly full. With integers
     * this holds exactly; there is no rounding residue to clean up. */
    while (n_large > 0)
    {
        threshold[work[n - n_large--]] = total;
    }
    while (n_small > 0)
    {
        threshold[work[--n_small]] = total; /* unreachable with exact math */
    }

    mk_free(work);
    table->n = n;
    table->total = total;
    table->threshold = threshold;
    table->alias = alias;
    return MK_OK;
}

uint32_t mk_alias_sample(const mk_alias *table, mk_rng *rng)
{
    uint32_t column = mk_rng_below(rng, table->n);
    uint64_t r = mk_rng_below64(rng, table->total);
    return r < table->threshold[column] ? column : table->alias[column];
}

void mk_alias_free(mk_alias *table)
{
    if (table == NULL)
    {
        return;
    }
    mk_free(table->threshold);
    mk_free(table->alias);
    table->threshold = NULL;
    table->alias = NULL;
    table->n = 0;
    table->total = 0;
}
