#include "markov/text.h"

#include <string.h>
#include "markov/alloc.h"
#include "markov/plugins.h"

typedef struct span
{
    const char *ptr;
    size_t len;
} span;

mk_text_options mk_text_defaults(void)
{
    mk_text_options opts;
    opts.order = 1;
    opts.max_words = -1;
    opts.terminal_rule = MK_TERMINAL_PERIOD;
    return opts;
}

static bool is_delimiter(char c)
{
    /* The original " \t\r\n", plus NUL (which used to end a line early). */
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0';
}

static bool is_closer(const char *word, size_t *len)
{
    char c = word[*len - 1u];
    if (c == '"' || c == '\'' || c == ')' || c == ']')
    {
        (*len)--;
        return true;
    }
    /* UTF-8 right single (E2 80 99) and double (E2 80 9D) quotes */
    if (*len >= 3u && (unsigned char) word[*len - 3u] == 0xE2u
        && (unsigned char) word[*len - 2u] == 0x80u
        && ((unsigned char) c == 0x99u || (unsigned char) c == 0x9Du))
    {
        *len -= 3u;
        return true;
    }
    return false;
}

bool mk_text_is_terminal_word(const char *word, size_t len,
                              mk_terminal_rule rule)
{
    if (word == NULL || len == 0)
    {
        return false;
    }
    if (rule == MK_TERMINAL_PERIOD)
    {
        return word[len - 1u] == '.';
    }
    while (len > 0 && is_closer(word, &len))
    {
    }
    if (len == 0)
    {
        return false;
    }
    char c = word[len - 1u];
    return c == '.' || c == '!' || c == '?';
}

/* ---- building ----------------------------------------------------------- */

typedef struct builder
{
    mk_ngram *probe; /* scratch state used for lookups          */
    size_t probe_cap;
} builder;

/** Assemble the words in `window` (oldest first) into the probe state. */
static mk_status make_probe(builder *b, const span *window, uint32_t count,
                            uint32_t head, uint32_t order, bool terminal)
{
    size_t bytes = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        bytes += window[(head + i) % order].len + 1u;
    }
    if (bytes > UINT32_MAX)
    {
        return MK_ERR_LIMIT;
    }
    if (sizeof(mk_ngram) + bytes > b->probe_cap)
    {
        size_t cap = 2u * (sizeof(mk_ngram) + bytes);
        mk_ngram *grown = mk_realloc(b->probe, cap, MK_TAG_TEXT);
        if (grown == NULL)
        {
            return MK_ERR_NOMEM;
        }
        b->probe = grown;
        b->probe_cap = cap;
    }
    char *out = b->probe->text;
    for (uint32_t i = 0; i < count; i++)
    {
        const span *w = &window[(head + i) % order];
        memcpy(out, w->ptr, w->len);
        out[w->len] = '\0';
        out += w->len + 1u;
    }
    b->probe->order = count;
    b->probe->terminal = terminal ? 1u : 0u;
    b->probe->size = (uint32_t) bytes;
    return MK_OK;
}

mk_status mk_text_build(mk_chain *chain, const char *text, size_t len,
                        const mk_text_options *opts, mk_text_stats *stats)
{
    mk_text_options defaults = mk_text_defaults();
    if (opts == NULL)
    {
        opts = &defaults;
    }
    if (chain == NULL || (text == NULL && len > 0)
        || mk_chain_vtable(chain) != mk_plugin_ngram() || opts->order == 0
        || opts->order > MK_NGRAM_MAX_ORDER)
    {
        return MK_ERR_INVALID;
    }

    const uint32_t order = opts->order;
    span window[MK_NGRAM_MAX_ORDER];
    uint32_t head = 0;  /* index of the oldest word in the ring */
    uint32_t count = 0; /* words currently in the ring          */
    mk_state prev = MK_NO_STATE;
    builder b = {NULL, 0};
    mk_text_stats local = {0, 0};
    mk_status status = MK_OK;

    size_t i = 0;
    while (status == MK_OK)
    {
        while (i < len && is_delimiter(text[i]))
        {
            i++;
        }
        if (i >= len)
        {
            break;
        }
        if (opts->max_words >= 0 && local.words >= (uint64_t) opts->max_words)
        {
            break;
        }
        size_t start = i;
        while (i < len && !is_delimiter(text[i]))
        {
            i++;
        }
        span word = {text + start, i - start};
        local.words++;

        /* Slide the window: keep the most recent `order` words. */
        if (count < order)
        {
            window[(head + count++) % order] = word;
        }
        else
        {
            window[head] = word;
            head = (head + 1u) % order;
        }

        bool terminal = mk_text_is_terminal_word(word.ptr, word.len,
                                                 opts->terminal_rule);
        if (count < order && !terminal)
        {
            continue; /* not enough context yet */
        }

        status = make_probe(&b, window, count, head, order, terminal);
        mk_state current = MK_NO_STATE;
        if (status == MK_OK)
        {
            status = mk_chain_intern(chain, b.probe, &current, NULL);
        }
        if (status == MK_OK)
        {
            status = prev == MK_NO_STATE
                         ? mk_chain_mark_start(chain, current)
                         : mk_chain_add_transition(chain, prev, current, 1);
        }
        if (terminal)
        {
            /* Sentences are independent: forget the context. */
            prev = MK_NO_STATE;
            count = 0;
            head = 0;
            local.sentences++;
        }
        else
        {
            prev = current;
        }
    }

    mk_free(b.probe);
    if (stats)
    {
        stats->words += local.words;
        stats->sentences += local.sentences;
    }
    return status;
}

/* ---- rendering ---------------------------------------------------------- */

static void append(char *buf, size_t size, size_t *len, const char *s,
                   size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (buf && *len + 1u < size)
        {
            buf[*len] = s[i];
        }
        (*len)++;
    }
}

static void emit_word(char *buf, size_t size, size_t *len, const char *word,
                      mk_render_style style, bool first)
{
    size_t n = strlen(word);
    if (style == MK_RENDER_CLEAN)
    {
        if (!first)
        {
            append(buf, size, len, " ", 1);
        }
        append(buf, size, len, word, n);
        return;
    }
    /* The original print_func: "%s" after a word ending in '.', else "%s ". */
    append(buf, size, len, word, n);
    if (n == 0 || word[n - 1u] != '.')
    {
        append(buf, size, len, " ", 1);
    }
}

size_t mk_text_render(const mk_chain *chain, const mk_state *states, size_t n,
                      mk_render_style style, char *buf, size_t size)
{
    size_t len = 0;
    const mk_vtable *vt = mk_chain_vtable(chain);
    bool is_word = vt == mk_plugin_word();
    if (states != NULL && (is_word || vt == mk_plugin_ngram()))
    {
        bool first = true;
        for (size_t i = 0; i < n; i++)
        {
            const void *data = mk_chain_data(chain, states[i]);
            if (data == NULL)
            {
                continue;
            }
            if (is_word)
            {
                emit_word(buf, size, &len, data, style, first);
                first = false;
                continue;
            }
            const mk_ngram *gram = data;
            uint32_t from = i == 0 ? 0 : gram->order - 1u;
            for (uint32_t w = from; w < gram->order; w++)
            {
                emit_word(buf, size, &len, mk_ngram_word(gram, w), style,
                          first);
                first = false;
            }
        }
    }
    if (buf && size > 0)
    {
        buf[len < size ? len : size - 1u] = '\0';
    }
    return len;
}
