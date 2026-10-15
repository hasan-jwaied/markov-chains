#ifndef MK_TEST_H
#define MK_TEST_H

/*
 * A deliberately tiny test harness (no dependencies). Checks record the
 * failure and keep going; mk_test_run() additionally fails any test that
 * leaves engine allocations alive, so every test doubles as a leak test.
 */

#include <math.h>
#include <stdio.h>
#include "markov/markov.h"

extern int mk_test_failures;
extern int mk_test_checks;

typedef void (*mk_test_fn)(void);
void mk_test_run(const char *name, mk_test_fn fn);

#define MK_FAIL_AT(file, line, ...)                                   \
    do                                                                \
    {                                                                 \
        mk_test_failures++;                                           \
        fprintf(stderr, "  %s:%d: ", file, line);                     \
        fprintf(stderr, __VA_ARGS__);                                 \
        fprintf(stderr, "\n");                                        \
    } while (0)

#define CHECK(cond)                                                   \
    do                                                                \
    {                                                                 \
        mk_test_checks++;                                             \
        if (!(cond))                                                  \
        {                                                             \
            MK_FAIL_AT(__FILE__, __LINE__, "CHECK(%s)", #cond);       \
        }                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                \
    do                                                                \
    {                                                                 \
        unsigned long long a_ = (unsigned long long) (a);             \
        unsigned long long b_ = (unsigned long long) (b);             \
        mk_test_checks++;                                             \
        if (a_ != b_)                                                 \
        {                                                             \
            MK_FAIL_AT(__FILE__, __LINE__, "%s == %s (%llu vs %llu)", \
                       #a, #b, a_, b_);                               \
        }                                                             \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                         \
    do                                                                \
    {                                                                 \
        double a_ = (double) (a);                                     \
        double b_ = (double) (b);                                     \
        mk_test_checks++;                                             \
        if (!(fabs(a_ - b_) <= (eps)))                                \
        {                                                             \
            MK_FAIL_AT(__FILE__, __LINE__, "%s ~ %s (%.12g vs %.12g)", \
                       #a, #b, a_, b_);                               \
        }                                                             \
    } while (0)

#define CHECK_STATUS(expr, expected)                                  \
    do                                                                \
    {                                                                 \
        mk_status s_ = (expr);                                        \
        mk_test_checks++;                                             \
        if (s_ != (expected))                                         \
        {                                                             \
            MK_FAIL_AT(__FILE__, __LINE__, "%s -> '%s', expected '%s'", \
                       #expr, mk_status_str(s_),                      \
                       mk_status_str(expected));                      \
        }                                                             \
    } while (0)

#define CHECK_OK(expr) CHECK_STATUS(expr, MK_OK)

#define RUN(fn) mk_test_run(#fn, fn)

/* Suites (one per file). */
void suite_alloc(void);
void suite_rng(void);
void suite_alias(void);
void suite_chain(void);
void suite_text(void);
void suite_snakes(void);
void suite_math(void);
void suite_faults(void);

/* Shared helpers (test_main.c). */
mk_chain *make_text_chain(const char *text, uint32_t order);
/** State whose formatted label equals `label`, or MK_NO_STATE. */
mk_state find_label(const mk_chain *chain, const char *label);

#endif /* MK_TEST_H */
