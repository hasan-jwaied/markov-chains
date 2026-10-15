<script lang="ts">
  import PageHero from "../lib/components/PageHero.svelte";
  import { onDestroy } from "svelte";
  import { palette } from "../lib/charts/palette.svelte";
  import CodeBlock from "../lib/components/CodeBlock.svelte";
  import Segmented from "../lib/components/Segmented.svelte";
  import StatTile from "../lib/components/StatTile.svelte";
  import { BUILD, SAMPLING, fmtSeconds } from "../lib/benchmarks";
  import { fmtBytes, fmtInt, listCorpora, loadCorpus, type Corpus } from "../lib/corpora";
  import { TAGS, type Tag } from "../lib/engine/abi";
  import { engine } from "../lib/engine/client.svelte";
  import { NATIVE_X86_64, type Layout } from "../lib/engine/layout";
  import type { FieldView, HeapEvent, HeapSnapshot, PluginKind } from "../lib/engine/types";

  const hex = (v: number, width = 0) => `0x${v.toString(16).padStart(width, "0")}`;
  const tagColor = (t: Tag) => palette.value.series[TAGS.indexOf(t) % 8];

  // ---- 1. the vtable -------------------------------------------------------------
  let plugin = $state<PluginKind>("word");
  let vtables = $state<Partial<Record<PluginKind, HeapSnapshot["vtable"]>>>({});
  const vt = $derived(vtables[plugin] ?? null);
  let layout = $state<Layout | null>(null);
  engine.getLayout().then((l) => (layout = l));

  /** Read each plugin's vtable once, before the heap trace starts, so these
   *  throwaway chains never show up in the allocator demo below. */
  async function readVtables() {
    const out: Partial<Record<PluginKind, HeapSnapshot["vtable"]>> = {};
    for (const p of ["word", "ngram", "cell"] as const) {
      const info = await engine.createChain(p);
      out[p] = (await engine.heapSnapshot(info.id, 0)).vtable;
      await engine.free(info.id);
    }
    vtables = out;
  }

  // ---- 2. the live heap -------------------------------------------------------------
  let demo = $state<number | null>(null);
  let words = $state<string[]>([]);
  let ids = $state<Map<string, number>>(new Map());
  let input = $state("the");
  let events = $state<HeapEvent[]>([]);
  let live = $state<Map<number, { size: number; tag: Tag; born: number }>>(new Map());
  let allocs = $state(0);
  let frees = $state(0);
  let snapshot = $state<HeapSnapshot | null>(null);
  let tick = 0;
  let freed = $state(false);

  async function startDemo() {
    await engine.setHeapTrace(false);
    await engine.setHeapTrace(true);
    allocs = 0;
    frees = 0;
    const info = await engine.createChain("word");
    demo = info.id;
    words = [];
    ids = new Map();
    live = new Map();
    events = [];
    freed = false;
    await refresh();
  }

  async function refresh() {
    const { events: ev } = await engine.heapEvents();
    const next = new Map(live);
    for (const e of ev) {
      tick++;
      if (e.kind === "alloc") {
        allocs++;
        next.set(e.address, { size: e.size, tag: e.tag, born: tick });
      } else {
        frees++;
        next.delete(e.address);
      }
    }
    live = next;
    events = [...ev.reverse(), ...events].slice(0, 40);
    snapshot = demo !== null && !freed ? await engine.heapSnapshot(demo, 64) : null;
  }

  async function addWord(word: string) {
    if (demo === null || freed) return;
    const w = word.trim();
    if (!w) return;
    let id = ids.get(w);
    if (id === undefined) {
      id = await engine.internWord(demo, w);
      ids = new Map(ids).set(w, id);
    }
    const prev = words.at(-1);
    if (prev !== undefined && !prev.endsWith(".")) await engine.addTransition(demo, ids.get(prev)!, id, 1);
    words = [...words, w];
    await refresh();
  }

  async function addSentence() {
    const pool = "the cat sat on a mat while the dog ran to the park and a bird sang over the old red barn.".split(" ");
    for (const w of pool) await addWord(w);
  }

  async function freeDemo() {
    if (demo === null) return;
    await engine.free(demo);
    freed = true;
    await refresh();
  }

  readVtables().then(startDemo).catch(() => {});
  onDestroy(() => {
    if (demo !== null && !freed) void engine.free(demo);
    void engine.setHeapTrace(false);
  });

  const blocks = $derived([...live.entries()].sort((a, b) => a[0] - b[0]));
  const totalLive = $derived(blocks.reduce((s, [, b]) => s + b.size, 0));
  const leaked = $derived(live.size);

  // ---- 3. a node, byte by byte ---------------------------------------------------------------
  let nodeIndex = $state(0);
  const node = $derived(snapshot?.nodes[nodeIndex] ?? snapshot?.nodes[0] ?? null);

  function fieldAt(fields: FieldView[], byte: number): FieldView | null {
    return fields.find((f) => byte >= f.offset && byte < f.offset + f.size) ?? null;
  }

  // ---- 4. in-browser benchmark -------------------------------------------------------------------
  let corpora = $state<Corpus[]>([]);
  listCorpora().then((c) => (corpora = c.filter((x) => x.id !== "tweets")));
  let runs = $state<Array<{ title: string; words: number; ms: number; states: number; bytes: number }>>([]);
  let running = $state(false);
  async function runBench() {
    running = true;
    runs = [];
    for (const c of corpora) {
      const text = await loadCorpus(c.id);
      const info = await engine.buildText(text, { order: 1, rule: "sentence" });
      runs = [...runs, { title: c.title, words: info.words ?? c.words, ms: info.buildMs, states: info.states, bytes: info.bytes }];
      await engine.free(info.id);
    }
    running = false;
  }

  const fieldColor = (name: string) => {
    const order = ["data", "edges", "n_edges", "cap_edges", "out_total", "hash", "alias", "start_count", "terminal", "is_starter"];
    const i = order.indexOf(name);
    return i < 0 ? "transparent" : palette.value.series[i % 8];
  };
</script>

<div class="page">
  <PageHero eyebrow="Under the hood" title="Inside the engine.">
    Real structs read out of WebAssembly memory, the function pointers that make <code>void *</code> polymorphism
    work, and the allocator watching every byte.
  </PageHero>

  <!-- 1. vtable -------------------------------------------------------------------------- -->
  <section class="prose">
    <h2>Polymorphism in C: a vtable by hand</h2>
    <p>
      C has no classes, but it has function pointers. The original assignment's <code>MarkovChain</code> held five
      of them (print, compare, free, copy, is_last), which let the same chain code handle tweets and board squares.
      libmarkov keeps that idea and gathers the functions into one constant table per payload type, a
      <strong>vtable</strong>, shared by every chain of that type. It is the same mechanism C++ generates for
      <code>virtual</code> methods and Rust for trait objects.
    </p>
  </section>
  <div class="wide-block">
    <div class="card pad">
      <div class="controls">
        <Segmented options={[{ value: "word", label: "word" }, { value: "ngram", label: "ngram" }, { value: "cell", label: "cell" }]} bind:value={plugin} label="Plugin" />
      </div>
      <div class="two">
        <div>
          {#if vt}
            <table class="struct">
              <caption>
                <span class="mono">const mk_vtable</span> “{vt.name}” at <span class="mono">{hex(vt.address)}</span> · {vt.size} bytes
              </caption>
              <thead><tr><th>offset</th><th>field</th><th>value in memory</th></tr></thead>
              <tbody>
                {#each vt.fields as f (f.name)}
                  <tr>
                    <td class="mono num">+{f.offset}</td>
                    <td class="mono">{f.name}</td>
                    <td class="mono">
                      {#if f.kind === "fn"}function table index <strong>{f.value}</strong>{:else}{hex(f.value)} → "{vt.name}"{/if}
                    </td>
                  </tr>
                {/each}
              </tbody>
            </table>
            <p class="small muted">
              In WebAssembly a function pointer is not an address: it is an index into the module's indirect function
              table, and a call through it is a <span class="mono">call_indirect</span> that checks the signature at
              run time. Natively the same fields hold 8-byte code addresses.
            </p>
          {/if}
        </div>
        <CodeBlock
          title="the same idea in three languages"
          code={`/* C (libmarkov): an explicit table of function pointers */
typedef struct mk_vtable {
    uint64_t (*hash)(const void *data);
    int      (*compare)(const void *a, const void *b);
    void    *(*copy)(const void *data);
    /* ... */
} mk_vtable;
chain->vt->compare(node->data, probe);   /* dynamic dispatch */

// C++: the compiler builds the vtable for you
struct State { virtual size_t hash() const = 0; /* ... */ };

// Rust: a trait object carries a vtable pointer
trait State { fn hash(&self) -> u64; /* ... */ }
let s: Box<dyn State> = /* ... */;`}
        />
      </div>
    </div>
  </div>

  <!-- 2. live heap ------------------------------------------------------------------------------- -->
  <section class="prose">
    <h2>Watching the allocator</h2>
    <p>
      Every byte the engine allocates goes through <code>mk_malloc</code>, which prepends a small header (size and
      tag) and reports the event. Build a chain word by word below and watch the blocks appear, in address order and
      coloured by what they hold. Notice the node array and the hash index being <em>replaced</em> by bigger blocks
      as they double, the per-word state blocks, and the eight-byte edge arrays. Then free the chain: every block
      goes, and the leak counter reads zero. The C test suite checks the same thing after every test and after every
      simulated allocation failure.
    </p>
  </section>
  <div class="wide-block">
    <div class="card pad">
      <div class="controls">
        <form onsubmit={(e) => { e.preventDefault(); addWord(input); input = ""; }}>
          <input type="text" bind:value={input} placeholder="type a word" aria-label="Word to add" disabled={freed} />
          <button class="btn primary" type="submit" disabled={freed}>Add word</button>
        </form>
        <button class="btn" onclick={addSentence} disabled={freed}>Add a sentence</button>
        <button class="btn" onclick={freeDemo} disabled={freed || demo === null}>mk_chain_destroy()</button>
        <button class="btn ghost" onclick={startDemo}>Reset</button>
      </div>
      <p class="sentence mono">{words.join(" ") || "—"}</p>
      <div class="tiles">
        <StatTile label="Live engine blocks" value={fmtInt(live.size)} note={freed ? (leaked === 0 ? "all freed: 0 leaks" : `${leaked} leaked`) : undefined} />
        <StatTile label="Live bytes" value={fmtBytes(totalLive)} />
        <StatTile label="Allocation events" value={fmtInt(allocs)} note="resizes count as free + alloc" />
        <StatTile label="Free events" value={fmtInt(frees)} />
      </div>
      <div class="legend">
        {#each TAGS.filter((t) => t !== "other" && t !== "math" && t !== "text" && t !== "alias") as t (t)}
          <span><span class="key" style:background={tagColor(t)}></span>{t}</span>
        {/each}
      </div>
      <div class="heap" aria-label="Live blocks in address order">
        {#each blocks as [addr, b] (addr)}
          <span
            class="block"
            style:background={tagColor(b.tag)}
            style:width="{Math.max(10, Math.min(260, Math.sqrt(b.size) * 7))}px"
            title="{b.tag} · {b.size} bytes at {hex(addr)}"
          ><span class="sz">{b.size}</span></span>
        {:else}
          <span class="muted small">{freed ? "The heap is empty again." : "No engine blocks yet."}</span>
        {/each}
      </div>
      <details>
        <summary>Allocation log ({events.length} most recent events)</summary>
        <ol class="log mono">
          {#each events as e, i (i)}
            <li class:free={e.kind === "free"}><span class="kind">{e.kind}</span> {String(e.size).padStart(6)} B <span class="key" style:background={tagColor(e.tag)}></span>{e.tag.padEnd(6)} @ {hex(e.address)}</li>
          {/each}
        </ol>
      </details>
    </div>
  </div>

  <!-- 3. a node byte by byte --------------------------------------------------------------------------- -->
  <section class="prose">
    <h2>One state, byte by byte</h2>
    <p>
      Each state is an <code>mk_node</code> in one dense array, so state <em>i</em> lives at
      <code>nodes + i * sizeof(mk_node)</code>. Below are the actual bytes from the chain you just built, decoded with
      offsets the compiler reported through <code>mkw_layout</code>, not hard-coded ones. Grey bytes are padding: the struct
      holds 8-byte fields, so its size is rounded up to a multiple of 8, and natively the 8-byte pointers shift every
      field after them.
    </p>
  </section>
  <div class="wide-block">
    <div class="card pad">
      {#if node && snapshot && layout}
        <div class="controls">
          <label class="small" for="node-pick">State</label>
          <select id="node-pick" bind:value={nodeIndex}>
            {#each snapshot.nodes as n (n.id)}<option value={n.id}>#{n.id} “{n.label}”</option>{/each}
          </select>
          <span class="small muted">node array at {hex(snapshot.nodeArray.address)}: {snapshot.nodeArray.length} of {snapshot.nodeArray.capacity} slots used, {snapshot.nodeArray.stride} bytes each</span>
        </div>
        <div class="bytes" role="table" aria-label="Raw bytes of the node">
          {#each node.raw as byte, i (i)}
            {@const f = fieldAt(node.fields, i)}
            <span class="byte mono" class:pad={!f} style:border-color={f ? fieldColor(f.name) : "transparent"} title={f ? `${f.name} (+${f.offset})` : "padding"}>{byte.toString(16).padStart(2, "0")}</span>
          {/each}
        </div>
        <table class="struct">
          <thead><tr><th>offset</th><th>size</th><th>field</th><th>value</th></tr></thead>
          <tbody>
            {#each node.fields as f (f.name)}
              <tr>
                <td class="mono num nowrap"><span class="key" style:background={fieldColor(f.name)}></span>+{f.offset}</td>
                <td class="num">{f.size}</td>
                <td class="mono">{f.name}</td>
                <td class="mono">
                  {#if f.name === "data"}{hex(f.value)} → {node.data.size}-byte <em>{node.data.tag}</em> block: <strong>“{String.fromCharCode(...node.data.raw.filter((c) => c !== 0))}”</strong>
                  {:else if f.name === "edges"}{f.value ? hex(f.value) : "NULL"}{#if node.edges.items.length} → {node.edges.items.map((e) => `{target ${e.target} “${snapshot?.nodes[e.target]?.label ?? "?"}”, count ${e.count}}`).join(", ")}{/if}
                  {:else if f.name === "hash"}0x{f.hex} (FNV-1a of the word)
                  {:else if f.kind === "ptr"}{f.value ? hex(f.value) : "NULL"}
                  {:else}{f.value}{/if}
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
        <div class="layouts">
          <div>
            <h3>wasm32 (this page): {layout.node.size} bytes</h3>
            <div class="ruler">
              {#each Object.entries(layout.node.fields) as [name, off] (name)}
                <span style:left="{(off / 56) * 100}%" style:background={fieldColor(name)} title="{name} @ {off}"></span>
              {/each}
              <span class="end" style:left="{(layout.node.size / 56) * 100}%"></span>
            </div>
          </div>
          <div>
            <h3>x86-64 (native build): {NATIVE_X86_64.node.size} bytes</h3>
            <div class="ruler">
              {#each Object.entries(NATIVE_X86_64.node.fields) as [name, off] (name)}
                <span style:left="{(off / 56) * 100}%" style:background={fieldColor(name)} title="{name} @ {off}"></span>
              {/each}
              <span class="end" style:left="100%"></span>
            </div>
          </div>
        </div>
        <p class="small muted">Pointers shrink from 8 to 4 bytes in wasm32, so the same struct is 8 bytes smaller; the uint64 fields keep their 8-byte alignment on both.</p>
      {:else}
        <p class="muted">Add a word above to see its node.</p>
      {/if}
    </div>
  </div>

  <!-- 4. benchmarks --------------------------------------------------------------------------- -->
  <section class="prose">
    <h2>Before and after</h2>
    <p>
      The original engine found each word with a linear scan of a linked list and grew every frequency array one
      element at a time, so learning a corpus cost <em>O</em>(words × vocabulary). libmarkov uses open-addressing hash
      tables for states and edges and geometric growth, so it is linear in the corpus. Native timings, same machine,
      same compiler flags:
    </p>
  </section>
  <div class="wide-block">
    <div class="card pad">
      <table class="bench">
        <thead><tr><th>corpus</th><th>words</th><th>states</th><th>original</th><th>libmarkov</th><th>speedup</th></tr></thead>
        <tbody>
          {#each BUILD as r (r.corpus)}
            <tr>
              <td>{r.corpus}</td>
              <td class="num">{fmtInt(r.words)}</td>
              <td class="num">{fmtInt(r.states)}</td>
              <td class="num">{r.originalS === null ? "(minutes; skipped)" : fmtSeconds(r.originalS)}</td>
              <td class="num">{fmtSeconds(r.libmarkovS)}</td>
              <td class="num strong">{r.originalS === null ? "—" : `${Math.round(r.originalS / r.libmarkovS)}×`}</td>
            </tr>
          {/each}
        </tbody>
      </table>
      <h3>Sampling one successor</h3>
      <table class="bench">
        <thead><tr><th>successors</th><th>original (linear scan)</th><th>alias table</th><th>speedup</th></tr></thead>
        <tbody>
          {#each SAMPLING as r (r.degree)}
            <tr>
              <td class="num">{fmtInt(r.degree)}</td>
              <td class="num">{r.originalNs.toFixed(0)} ns</td>
              <td class="num">{r.aliasNs.toFixed(0)} ns</td>
              <td class="num strong">{(r.originalNs / r.aliasNs).toFixed(1)}×</td>
            </tr>
          {/each}
        </tbody>
      </table>
      <p class="small muted">Native Release build, single thread; medians of repeated runs (bench/bench_main.c).</p>

      <h3>Now in your browser</h3>
      <div class="controls">
        <button class="btn primary" onclick={runBench} disabled={running || !corpora.length}>{running ? "Building…" : "Build every corpus in WebAssembly"}</button>
      </div>
      {#if runs.length}
        <table class="bench">
          <thead><tr><th>corpus</th><th>words</th><th>states</th><th>build time</th><th>memory</th><th>throughput</th></tr></thead>
          <tbody>
            {#each runs as r (r.title)}
              <tr>
                <td>{r.title}</td>
                <td class="num">{fmtInt(r.words)}</td>
                <td class="num">{fmtInt(r.states)}</td>
                <td class="num">{r.ms.toFixed(1)} ms</td>
                <td class="num">{fmtBytes(r.bytes)}</td>
                <td class="num">{(r.words / r.ms / 1000).toFixed(1)}M words/s</td>
              </tr>
            {/each}
          </tbody>
        </table>
      {/if}
    </div>
  </div>
</div>

<style>
  .wide-block {
    width: min(var(--wide), calc(100vw - 2 * var(--gutter)));
    margin: 1.4em auto 2.4em;
  }
  .pad {
    padding: 18px;
  }
  .two {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 20px;
    margin-top: 14px;
    align-items: start;
  }
  .two :global(.code) {
    margin: 0;
  }
  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 13.5px;
  }
  caption {
    text-align: left;
    font-size: 13px;
    color: var(--ink-2);
    padding-bottom: 8px;
  }
  th {
    text-align: left;
    font-weight: 600;
    color: var(--ink-2);
    border-bottom: 1px solid var(--axis);
    padding: 6px 8px;
  }
  td {
    padding: 6px 8px;
    border-bottom: 1px solid var(--hairline);
    vertical-align: top;
  }
  .bench th:not(:first-child),
  .bench td:not(:first-child) {
    text-align: right;
  }
  .nowrap {
    white-space: nowrap;
  }
  .strong {
    font-weight: 700;
    color: var(--ink-strong);
  }
  .small {
    font-size: 13px;
  }
  form {
    display: flex;
    gap: 8px;
  }
  .sentence {
    font-size: 13px;
    color: var(--ink-2);
    margin: 12px 0;
    min-height: 1.4em;
    overflow-wrap: anywhere;
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 10px;
    margin-bottom: 12px;
  }
  .heap {
    display: flex;
    flex-wrap: wrap;
    gap: 2px;
    padding: 10px;
    margin: 10px 0;
    background: var(--surface-2);
    border-radius: 8px;
    min-height: 60px;
    align-content: flex-start;
  }
  .block {
    height: 26px;
    border-radius: 3px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
    animation: born 400ms ease-out;
  }
  .sz {
    font: 600 10px var(--font-sans);
    color: #fff;
    mix-blend-mode: normal;
    text-shadow: 0 0 2px rgba(0, 0, 0, 0.6);
  }
  @keyframes born {
    from {
      transform: scale(0.4);
      opacity: 0;
    }
  }
  .key {
    display: inline-block;
    width: 9px;
    height: 9px;
    border-radius: 2px;
    margin-right: 6px;
    vertical-align: -1px;
  }
  details {
    font-size: 13px;
  }
  summary {
    cursor: pointer;
    color: var(--ink-2);
  }
  .log {
    max-height: 220px;
    overflow-y: auto;
    font-size: 12px;
    padding-left: 0;
    list-style: none;
    white-space: pre;
  }
  .log .kind {
    display: inline-block;
    width: 44px;
    font-weight: 700;
    color: var(--good-ink);
  }
  .log .free .kind {
    color: var(--critical);
  }
  .bytes {
    display: grid;
    grid-template-columns: repeat(16, minmax(0, 1fr));
    gap: 3px;
    margin: 14px 0;
    max-width: 640px;
  }
  .byte {
    font-size: 12px;
    text-align: center;
    padding: 4px 0;
    border-bottom: 3px solid;
    background: var(--surface-2);
    border-radius: 3px;
  }
  .byte.pad {
    color: var(--muted);
    background: repeating-linear-gradient(45deg, var(--surface-2), var(--surface-2) 3px, var(--surface-3) 3px, var(--surface-3) 6px);
  }
  .layouts {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
    margin-top: 16px;
  }
  .layouts h3 {
    font-size: 13px;
    margin: 0 0 8px;
    color: var(--ink-2);
  }
  .ruler {
    position: relative;
    height: 22px;
    background: var(--surface-2);
    border-radius: 4px;
  }
  .ruler span {
    position: absolute;
    top: 3px;
    width: 4px;
    height: 16px;
    border-radius: 2px;
  }
  .ruler .end {
    width: 2px;
    background: var(--ink-strong);
    top: 0;
    height: 22px;
  }
  h3 {
    font-size: 15px;
    margin: 20px 0 8px;
  }
  @media (max-width: 860px) {
    .two,
    .layouts {
      grid-template-columns: 1fr;
    }
    .tiles {
      grid-template-columns: 1fr 1fr;
    }
    .bytes {
      grid-template-columns: repeat(8, minmax(0, 1fr));
    }
  }
</style>
