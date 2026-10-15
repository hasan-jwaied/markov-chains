#ifndef MARKOV_TEXT_H
#define MARKOV_TEXT_H

#include <stddef.h>
#include <stdint.h>
#include "markov/chain.h"
#include "markov/status.h"

/**
 * Text corpora -> order-k word chains (chains created with mk_plugin_ngram).
 *
 * Words are separated by ' ', '\t', '\r', '\n' (the original delimiters) and
 * NUL. A word that ends a sentence makes its state terminal and resets the
 * context, so transitions never cross sentence boundaries. For order k the
 * state is the last k words of the current sentence; a sentence shorter than
 * k words becomes a single terminal state.
 */

typedef enum mk_terminal_rule
{
    /** Ends with '.' (the original assignment's rule). */
    MK_TERMINAL_PERIOD = 0,
    /** Ends with '.', '!' or '?', optionally followed by closing quotes or
     *  brackets (ASCII, or UTF-8 ’ and ”). Better for prose. */
    MK_TERMINAL_SENTENCE
} mk_terminal_rule;

typedef struct mk_text_options
{
    uint32_t order;                 /* 1..MK_NGRAM_MAX_ORDER                */
    int64_t max_words;              /* stop after this many words; < 0: all */
    mk_terminal_rule terminal_rule;
} mk_text_options;

typedef struct mk_text_stats
{
    uint64_t words;      /* tokens consumed                       */
    uint64_t sentences;  /* sequences that ended on a terminal    */
} mk_text_stats;

/** Order 1, every word, MK_TERMINAL_PERIOD: the original behaviour. */
mk_text_options mk_text_defaults(void);

/** True if `word` (of length `len`) ends a sentence under `rule`. */
bool mk_text_is_terminal_word(const char *word, size_t len,
                              mk_terminal_rule rule);

/**
 * Tokenize `text` and add its transitions to `chain` (an ngram chain).
 * May be called repeatedly to train on several documents. On MK_ERR_NOMEM
 * the chain stays valid and contains a prefix of the input.
 */
mk_status mk_text_build(mk_chain *chain, const char *text, size_t len,
                        const mk_text_options *opts, mk_text_stats *stats);

typedef enum mk_render_style
{
    /** Each word followed by a space unless it is terminal: exactly what the
     *  original print_func produced (including a trailing space). */
    MK_RENDER_COMPAT = 0,
    /** Words separated by single spaces, no trailing space. */
    MK_RENDER_CLEAN
} mk_render_style;

/**
 * Render a generated state sequence as text: every word of the first state,
 * then the last word of each following state. snprintf semantics: writes at
 * most `size` bytes (NUL-terminated) and returns the full length needed.
 */
size_t mk_text_render(const mk_chain *chain, const mk_state *states, size_t n,
                      mk_render_style style, char *buf, size_t size);

#endif /* MARKOV_TEXT_H */
