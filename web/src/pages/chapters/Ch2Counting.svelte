<script lang="ts">
  import MatrixHeatmap from "../../lib/charts/MatrixHeatmap.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import StatTile from "../../lib/components/StatTile.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { fmtBytes, fmtInt, loadCorpus } from "../../lib/corpora";
  import { engine } from "../../lib/engine/client.svelte";
  import { engineModel } from "../../lib/engine/model.svelte";
  import type { ModelInfo, StateInfo } from "../../lib/engine/types";

  let text = $state("the cat sat on the mat. the dog sat on the cat. the cat ran.");
  const model = engineModel(() => engine.buildText(text, { order: 1, rule: "period" }));

  let rows = $state<StateInfo[]>([]);
  let matrix = $state<Float64Array | null>(null);
  let labels = $state<string[]>([]);
  let selected = $state(0);

  $effect(() => {
    const id = model.id;
    const n = model.info?.states ?? 0;
    if (id === null) return;
    let alive = true;
    (async () => {
      const ids = Array.from({ length: n }, (_, i) => i);
      const [infos, m] = await Promise.all([
        Promise.all(ids.map((s) => engine.stateInfo(id, s, 12))),
        n <= 24 ? engine.matrix(id) : Promise.resolve(null),
      ]);
      if (!alive) return;
      rows = infos;
      labels = infos.map((r) => r.label);
      matrix = m;
      if (selected >= n) selected = 0;
    })().catch(() => {});
    return () => {
      alive = false;
    };
  });

  // The full tweet corpus from the original assignment, for the sparsity numbers.
  let tweets = $state<ModelInfo | null>(null);
  $effect(() => {
    let id: number | null = null;
    loadCorpus("tweets")
      .then((t) => engine.buildText(t, { order: 1, rule: "period" }))
      .then((info) => {
        id = info.id;
        tweets = info;
      })
      .catch(() => {});
    return () => {
      if (id !== null) void engine.free(id);
    };
  });

  const sel = $derived(rows[selected]);
</script>

<div class="prose">
  <p class="eyebrow">Chapter 2</p>
  <h2>Counting becomes probability</h2>
  <p>
    Where does <Tex tex="P" /> come from? For text, from counting. Read a corpus word by word; every time word
    <Tex tex="i" /> is followed by word <Tex tex="j" />, add one to a counter <Tex tex={String.raw`c_{ij}`} />. The best estimate
    of the transition probability, in the maximum-likelihood sense, is just the share of each successor:
  </p>
  <Tex display tex={String.raw`\hat P_{ij} \;=\; \frac{c_{ij}}{\sum_k c_{ik}}`} />
  <p>
    That is exactly what the original assignment built: each word kept a <em>frequency list</em> of the words seen
    after it. Edit the text below and the engine re-learns the chain as you type. Sentences end at a word ending in a
    period, and transitions never cross a sentence boundary. That makes every word ending in a period a
    <strong>terminal</strong> state, which matters in chapter 5.
  </p>

  <Figure wide caption="Left: each state's frequency list, the counts stored in its mk_edge array (most frequent first). Right: the estimated matrix P. Click a word to select its row.">
    {#snippet controls()}
      <textarea bind:value={text} rows="2" aria-label="Training text" spellcheck="false"></textarea>
    {/snippet}
    {#if model.error}
      <p class="error">{model.error}</p>
    {/if}
    <div class="grid">
      <div class="lists">
        {#each rows as r, i (r.id)}
          <button class="row" class:on={i === selected} onclick={() => (selected = i)}>
            <span class="word mono">{r.label}</span>
            {#if r.edges.length}
              <span class="succ">
                {#each r.edges as e (e.target)}
                  <span class="pill mono">{e.label} <b>×{e.count}</b></span>
                {/each}
              </span>
            {:else}
              <span class="muted small">{r.terminal ? "terminal: the sentence ends here" : "no successors"}</span>
            {/if}
          </button>
        {/each}
      </div>
      <div class="matrix">
        {#if matrix && labels.length}
          <MatrixHeatmap rows={labels} values={matrix} max={1} highlightRow={selected} cell={labels.length > 12 ? 26 : 38} />
        {:else if labels.length}
          <p class="muted">Too many distinct words to draw the matrix; the frequency lists still tell the whole story.</p>
        {/if}
        {#if sel && sel.edges.length}
          <p class="small">
            <span class="mono">{sel.label}</span> was followed {sel.outTotal} time{sel.outTotal === 1 ? "" : "s"}:
            {#each sel.edges as e, k (e.target)}{k ? ", " : ""}<span class="mono">{e.label}</span> {e.count}/{sel.outTotal} = {e.p.toFixed(2)}{/each}.
            An identity-row entry on a terminal word just marks "stop here" (it is absorbing).
          </p>
        {/if}
      </div>
    </div>
  </Figure>

  <h3>Why the engine never stores the matrix</h3>
  <p>
    On a toy sentence the matrix looks reasonable. On real text it is almost entirely zeros: most words are only
    ever followed by a handful of others. Here is the original assignment's tweet corpus, learned by the engine in
    your browser:
  </p>
  <div class="tiles wide-ish">
    <StatTile label="States (distinct words)" value={tweets ? fmtInt(tweets.states) : "…"} />
    <StatTile label="Non-zero entries" value={tweets ? fmtInt(tweets.edges) : "…"} note={tweets ? `${((tweets.edges / tweets.states ** 2) * 100).toFixed(3)}% of the matrix` : undefined} />
    <StatTile label="Dense matrix (doubles)" value={tweets ? fmtBytes(tweets.states ** 2 * 8) : "…"} />
    <StatTile label="What the engine used" value={tweets ? fmtBytes(tweets.bytes) : "…"} note={tweets ? `built in ${tweets.buildMs.toFixed(0)} ms` : undefined} />
  </div>
  <p>
    So each state keeps only its non-zero entries: a growable array of <code>mk_edge</code>, eight bytes per
    transition. The original code did the same with <code>MarkovNodeFrequency</code>, but it grew that array with
    <code>realloc(len + 1)</code> on every new successor and found words by walking a linked list. That made
    learning a corpus <Tex tex="O(N \cdot V)" /> for <Tex tex="N" /> words and <Tex tex="V" /> distinct words. The
    engine indexes states and edges in hash tables and doubles its arrays, so each word costs
    <Tex tex="O(1)" /> on average.
  </p>
  <CodeBlock
    title="before: legacy/ex3b/markov_chain.c  →  after: engine/src/chain.c"
    code={`/* before: a linear scan of a linked list for every word read */
Node *curr = markov_chain->database->first;
while (curr) {
    if (markov_chain->comp_func(((MarkovNode *) curr->data)->data, data_ptr) == 0)
        return curr;
    curr = curr->next;
}

/* after: open addressing over dense ids, hash cached per node */
uint32_t i = (uint32_t) (mk_mix64(hash) & mask);
while (chain->slots[i] != 0) {
    mk_state id = chain->slots[i] - 1u;
    if (chain->nodes[id].hash == hash && chain->vt->compare(chain->nodes[id].data, data) == 0)
        return id;
    i = (i + 1u) & mask;
}`}
    highlight={[10, 11, 12, 13, 14, 15, 16]}
  />
</div>

<style>
  textarea {
    min-height: 54px;
  }
  .grid {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 20px;
    align-items: start;
  }
  .lists {
    display: flex;
    flex-direction: column;
    gap: 2px;
    max-height: 420px;
    overflow-y: auto;
  }
  .row {
    display: flex;
    gap: 10px;
    align-items: baseline;
    text-align: left;
    border: 1px solid transparent;
    background: transparent;
    padding: 6px 8px;
    border-radius: 6px;
    cursor: pointer;
    color: var(--ink);
    font: 13px var(--font-sans);
  }
  .row:hover {
    background: var(--surface-2);
  }
  .row.on {
    border-color: var(--accent);
    background: var(--accent-wash);
  }
  .word {
    min-width: 64px;
    font-weight: 600;
    color: var(--ink-strong);
  }
  .succ {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
  }
  .pill {
    font-size: 12px;
    padding: 1px 6px;
    border-radius: 999px;
    background: var(--surface-2);
    border: 1px solid var(--border);
  }
  .pill b {
    color: var(--accent-ink);
  }
  .small {
    font-size: 13px;
  }
  .error {
    color: var(--critical);
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 10px;
    margin: 1.4em 0;
    font-family: var(--font-sans);
  }
  @media (max-width: 760px) {
    .grid {
      grid-template-columns: 1fr;
    }
  }
</style>
