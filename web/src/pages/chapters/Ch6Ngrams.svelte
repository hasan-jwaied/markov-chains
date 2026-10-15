<script lang="ts">
  import BarChart from "../../lib/charts/BarChart.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import Segmented from "../../lib/components/Segmented.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { fmtInt, loadCorpus } from "../../lib/corpora";
  import { engine } from "../../lib/engine/client.svelte";
  import type { Generated, ModelInfo, TextStats } from "../../lib/engine/types";
  import { href } from "../../lib/router.svelte";

  interface OrderResult {
    k: number;
    info: ModelInfo;
    stats: TextStats;
    samples: Generated[];
    verbatimShare: number;
  }

  const corpora = [
    { value: "alice", label: "Alice" },
    { value: "sherlock-holmes", label: "Sherlock Holmes" },
    { value: "pride-and-prejudice", label: "Pride & Prejudice" },
  ];
  let corpus = $state("alice");
  let order = $state(2);
  let results = $state<OrderResult[]>([]);
  let loading = $state(true);
  let seed = $state(11);

  $effect(() => {
    const id = corpus;
    const s = seed;
    let alive = true;
    const created: number[] = [];
    loading = true;
    (async () => {
      const text = await loadCorpus(id);
      const out: OrderResult[] = [];
      for (const k of [1, 2, 3, 4]) {
        const info = await engine.buildText(text, { order: k, rule: "sentence" });
        created.push(info.id);
        const [stats, samples, probe] = await Promise.all([
          engine.textStats(info.id),
          engine.generate(info.id, { seed: s, count: 3, maxLength: 40 }),
          engine.generate(info.id, { seed: s + 1000, count: 200, maxLength: 40 }),
        ]);
        out.push({ k, info, stats, samples, verbatimShare: probe.filter((g) => g.verbatim).length / probe.length });
        if (!alive) return;
      }
      if (alive) {
        results = out;
        loading = false;
      }
    })().catch(() => (loading = false));
    return () => {
      alive = false;
      for (const c of created) void engine.free(c);
    };
  });

  const current = $derived(results.find((r) => r.k === order));
</script>

<div class="prose">
  <p class="eyebrow">Chapter 6</p>
  <h2>Remembering more</h2>
  <p>
    A first-order chain over words has the memory of a goldfish: it picks each word knowing only the word before.
    The cure keeps the Markov property and changes what a <em>state</em> is. Let the state be the last
    <Tex tex="k" /> words, and the chain becomes an order-<Tex tex="k" /> model of language:
  </p>
  <Tex display tex={String.raw`\Pr(w_{n+1} \mid w_n, w_{n-1}, \dots) \;\approx\; \Pr(w_{n+1} \mid \underbrace{w_{n-k+1}, \dots, w_n}_{\text{the state}})`} />
  <p>
    This is where the assignment's <code>void *</code> design pays off. The chain never looks inside its states; it
    only calls the functions in its <em>vtable</em>. An n-gram state is a new payload type with its own hash,
    compare and copy, and not a single line of the chain changes.
  </p>

  <CodeBlock
    title="engine/plugins/ngram.c (abridged)"
    code={`typedef struct mk_ngram
{
    uint32_t order;      /* number of words                     */
    uint32_t terminal;   /* does the last word end a sentence?  */
    uint32_t size;       /* bytes used in text[]                */
    char text[];         /* "w1\\0w2\\0...wk\\0" in one allocation */
} mk_ngram;

static const mk_vtable NGRAM_VTABLE = {
    "ngram", ngram_hash, ngram_compare, ngram_copy, ngram_destroy,
    ngram_is_terminal, ngram_format
};`}
  />

  <Figure wide caption="The same novel learned at orders 1 to 4, all in the engine in your browser. 'Copied verbatim' is the share of 200 generated sentences that appear word for word in the source.">
    {#snippet controls()}
      <Segmented options={corpora} bind:value={corpus} />
      <Segmented label="Order k" options={[1, 2, 3, 4].map((k) => ({ value: k, label: `k = ${k}` }))} bind:value={order} />
      <button class="btn" onclick={() => seed++}>New sentences</button>
    {/snippet}
    {#if loading && !results.length}
      <p class="muted">Learning four chains…</p>
    {:else if current}
      <ol class="samples" class:stale={loading}>
        {#each current.samples as g, i (i)}
          <li>{g.text}{#if g.verbatim}<span class="tag verb">verbatim</span>{/if}</li>
        {/each}
      </ol>
      <div class="cols">
        <div>
          <h4>Next-word entropy (bits)</h4>
          <BarChart labels={results.map((r) => `k=${r.k}`)} values={results.map((r) => r.stats.entropyBits)} max={Math.max(...results.map((r) => r.stats.entropyBits)) * 1.2} format={(v) => v.toFixed(2)} colorIndex={[0, 0, 0, 0]} height={170} />
        </div>
        <div>
          <h4>Copied verbatim from the book</h4>
          <BarChart labels={results.map((r) => `k=${r.k}`)} values={results.map((r) => r.verbatimShare)} max={1} colorIndex={[1, 1, 1, 1]} height={170} />
        </div>
      </div>
      <table>
        <thead><tr><th>order</th><th>states</th><th>transitions</th><th>avg. successors</th><th>deterministic states</th><th>built in</th></tr></thead>
        <tbody>
          {#each results as r (r.k)}
            <tr class:on={r.k === order}>
              <td>k = {r.k}</td>
              <td class="num">{fmtInt(r.info.states)}</td>
              <td class="num">{fmtInt(r.info.edges)}</td>
              <td class="num">{r.stats.branching.toFixed(2)}</td>
              <td class="num">{(r.stats.deterministicShare * 100).toFixed(0)}%</td>
              <td class="num">{r.info.buildMs.toFixed(0)} ms</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </Figure>

  <p>
    The numbers tell the trade-off. With more memory every state is rarer, so the chain has fewer choices. Entropy
    (the average uncertainty about the next word, weighted by how often each state occurs) falls fast. At
    <Tex tex="k = 1" /> the text is word salad. By <Tex tex="k = 3" /> it is fluent, and by <Tex tex="k = 4" />
    most states have exactly one successor and the "generator" mostly recites the book. A Markov chain can only
    recombine what it has seen; the art is choosing <Tex tex="k" /> so it still has room to.
  </p>
  <p>Push it further in the <a href={href("text")}>Text Lab</a>: any corpus, temperature, and a steering mode where you choose each word yourself.</p>
</div>

<style>
  .samples {
    margin: 0 0 18px;
    padding-left: 22px;
    font-family: var(--font-serif);
    font-size: 17px;
    line-height: 1.55;
    transition: opacity 200ms;
  }
  .samples.stale {
    opacity: 0.5;
  }
  .samples li {
    margin-bottom: 8px;
  }
  .verb {
    margin-left: 8px;
    vertical-align: 2px;
  }
  .cols {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 24px;
  }
  h4 {
    font-size: 13px;
    font-weight: 600;
    color: var(--ink-2);
    margin: 0 0 4px;
  }
  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 13.5px;
    margin-top: 14px;
  }
  th {
    text-align: right;
    font-weight: 600;
    color: var(--ink-2);
    border-bottom: 1px solid var(--axis);
    padding: 6px 8px;
  }
  td {
    text-align: right;
    padding: 6px 8px;
    border-bottom: 1px solid var(--hairline);
  }
  th:first-child,
  td:first-child {
    text-align: left;
  }
  tr.on td {
    background: var(--accent-wash);
    font-weight: 600;
  }
  @media (max-width: 760px) {
    .cols {
      grid-template-columns: 1fr;
    }
    table {
      font-size: 12px;
    }
  }
</style>
