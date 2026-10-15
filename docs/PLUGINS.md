# Writing a state plugin

A plugin teaches the chain about a new kind of state. It is a payload type
plus an `mk_vtable`; the chain itself never changes. The built-in plugins are
small enough to read in one sitting:

| plugin | payload | file |
|---|---|---|
| `word` | `char *` | [`engine/plugins/word.c`](../engine/plugins/word.c) |
| `ngram` | `mk_ngram`, k words in one block | [`engine/plugins/ngram.c`](../engine/plugins/ngram.c) |
| `cell` | `mk_cell`, a board square | [`engine/plugins/cell.c`](../engine/plugins/cell.c) |

## The contract

| function | must |
|---|---|
| `hash(data)` | return equal hashes for equal payloads (the chain re-mixes it) |
| `compare(a, b)` | return 0 exactly when the payloads are equal |
| `copy(data)` | return a deep copy allocated with `mk_malloc(…, MK_TAG_STATE)`, or NULL |
| `destroy(data)` | free a copy made by `copy` |
| `is_terminal(data)` | say whether a sequence stops here (it becomes absorbing in the math) |
| `format(data, buf, size)` | snprintf semantics: write a label, return the full length |

Two rules keep the engine fast and leak-free:

* `hash` and `compare` must agree, and must look at the same fields.
* Keep `copy` to a single allocation where you can (see `mk_ngram`): it is
  faster, and a failed copy can never leak half an object.

## Example: musical notes

The planned *Markov Music* lab needs states of (pitch, duration):

```c
typedef struct note { int16_t pitch; uint16_t ticks; uint8_t end; } note;

static uint64_t note_hash(const void *d)
{
    const note *n = d;
    return ((uint64_t) (uint16_t) n->pitch << 16) | n->ticks;
}
static int note_compare(const void *a, const void *b)
{
    const note *x = a, *y = b;
    return x->pitch != y->pitch ? x->pitch - y->pitch : (int) x->ticks - (int) y->ticks;
}
static void *note_copy(const void *d)
{
    note *n = mk_malloc(sizeof *n, MK_TAG_STATE);
    if (n) *n = *(const note *) d;
    return n;
}
static void note_destroy(void *d) { mk_free(d); }
static bool note_is_terminal(const void *d) { return ((const note *) d)->end; }
static int note_format(const void *d, char *buf, size_t size)
{
    const note *n = d;
    return snprintf(buf, size, "%d/%u", n->pitch, n->ticks);
}

static const mk_vtable NOTE_VTABLE = {
    "note", note_hash, note_compare, note_copy, note_destroy,
    note_is_terminal, note_format
};
```

Everything else already works for the new type: `mk_chain_intern`,
`mk_chain_add_transition`, alias sampling, `mk_chain_generate`, the stationary
distribution and the absorbing analysis. To expose it to the browser, add a
case to `plugin_vtable()` in `wasm/api.c` and an entry to `PLUGIN` in
`web/src/lib/engine/abi.ts`.

A *Dungeon* plugin is the same idea with room types as states; the boss room is
terminal, so `mk_math_absorbing` answers "how many rooms until the boss?"
exactly.
