/*
 * Standalone driver for the fuzz target, for compilers without libFuzzer.
 *
 *   fuzz_driver FILE...                     replay inputs (e.g. a corpus)
 *   fuzz_driver --iterations N [--seed S]   random inputs biased towards
 *                                           text-like bytes
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "markov/rng.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int replay(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    static uint8_t buf[1u << 20u];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    LLVMFuzzerTestOneInput(buf, n);
    return 0;
}

static uint8_t random_byte(mk_rng *rng)
{
    static const char alphabet[] = "aabbcdeeefghiilmnoorsttuy  ..!?\"'\n";
    uint32_t kind = mk_rng_below(rng, 10);
    if (kind < 8)
    {
        return (uint8_t) alphabet[mk_rng_below(rng, sizeof(alphabet) - 1u)];
    }
    return (uint8_t) mk_rng_below(rng, 256);
}

int main(int argc, char *argv[])
{
    long iterations = -1;
    unsigned long seed = 1;
    int files = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
        {
            iterations = strtol(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
        {
            seed = strtoul(argv[++i], NULL, 10);
        }
        else if (replay(argv[i]) != 0)
        {
            return EXIT_FAILURE;
        }
        else
        {
            files++;
        }
    }
    if (iterations < 0)
    {
        iterations = files ? 0 : 1000;
    }

    mk_rng rng;
    mk_rng_seed(&rng, MK_RNG_PCG32, seed);
    uint8_t buf[2048];
    for (long it = 0; it < iterations; it++)
    {
        size_t n = mk_rng_below(&rng, sizeof(buf));
        for (size_t i = 0; i < n; i++)
        {
            buf[i] = random_byte(&rng);
        }
        LLVMFuzzerTestOneInput(buf, n);
    }
    printf("fuzz: %d file(s), %ld random input(s), no failures\n", files,
           iterations);
    return EXIT_SUCCESS;
}
