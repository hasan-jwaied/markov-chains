#ifndef MARKOV_SRC_HASH_H
#define MARKOV_SRC_HASH_H

#include <stddef.h>
#include <stdint.h>

/* Internal hashing helpers shared by the chain and the plugins. */

/** 64-bit FNV-1a over a byte range, continuing from `h`. */
static inline uint64_t mk_fnv1a(uint64_t h, const void *data, size_t len)
{
    const unsigned char *p = data;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

#define MK_FNV_OFFSET 0xcbf29ce484222325ULL

/** SplitMix64 finalizer: spreads every input bit over the whole word. */
static inline uint64_t mk_mix64(uint64_t x)
{
    x ^= x >> 30u;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27u;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31u;
    return x;
}

#endif /* MARKOV_SRC_HASH_H */
