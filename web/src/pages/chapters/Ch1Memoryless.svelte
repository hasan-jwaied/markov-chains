<script lang="ts">
  import ChainGraph from "../../lib/charts/ChainGraph.svelte";
  import MatrixHeatmap from "../../lib/charts/MatrixHeatmap.svelte";
  import { palette } from "../../lib/charts/palette.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import Slider from "../../lib/components/Slider.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { engine } from "../../lib/engine/client.svelte";
  import { engineModel } from "../../lib/engine/model.svelte";

  const labels = ["Sunny", "Cloudy", "Rainy"];
  let rows = $state([
    [0.7, 0.2, 0.1],
    [0.3, 0.4, 0.3],
    [0.2, 0.4, 0.4],
  ]);
  const P = $derived(rows.flat());

  // The chain lives in the C engine; it is rebuilt whenever a row changes.
  const model = engineModel(() =>
    engine.buildCustom({ labels, weights: P.map((p) => Math.round(p * 1000)) }),
  );

  let current = $state(0);
  let previous = $state<number | null>(null);
  let stepKey = $state(0);
  let history = $state<number[]>([0]);
  let counts = $state([1, 0, 0]);
  let playing = $state(false);
  let seed = 1;
  let editing = $state(0);

  async function step() {
    if (model.id === null) return;
    try {
      const next = await engine.step(model.id, current, seed++);
      if (next === null) return;
      previous = current;
      current = next;
      stepKey++;
      history = [...history.slice(-35), next];
      counts[next]++;
    } catch {
      /* the model was swapped mid-flight; the next tick uses the new one */
    }
  }

  $effect(() => {
    if (!playing) return;
    const timer = setInterval(step, 650);
    return () => clearInterval(timer);
  });

  function reset() {
    current = 0;
    previous = null;
    history = [0];
    counts = [1, 0, 0];
  }

  /** Set P[row][col] and rescale the rest of the row so it still sums to 1. */
  function setProbability(row: number, col: number, value: number) {
    const r = rows[row];
    const rest = 1 - r[col];
    const next = r.map((p, j) => {
      if (j === col) return value;
      return rest > 1e-9 ? (p / rest) * (1 - value) : (1 - value) / (r.length - 1);
    });
    rows[row] = next.map((p) => Math.round(p * 1000) / 1000);
  }

  const total = $derived(counts.reduce((a, b) => a + b, 0));
</script>

<div class="prose">
  <p class="eyebrow">Chapter 1</p>
  <h2>A machine with no memory</h2>
  <p>
    Imagine a very forgetful weather system. Each morning it looks at today's weather, and only today's, and rolls
    a die to decide tomorrow's. A sunny day is usually followed by another sunny day; rain tends to linger. Those
    tendencies are the whole machine: three <strong>states</strong> and, for every pair of states, the
    <strong>probability of a transition</strong> from one to the other.
  </p>

  <Figure wide caption="A three-state weather chain running in the C engine. Each step the engine samples tomorrow from today's row of the matrix. The blue token shows the transition just taken; edge thickness is the probability.">
    {#snippet controls()}
      <button class="btn primary" onclick={() => (playing = !playing)}>{playing ? "Pause" : "Play"}</button>
      <button class="btn" onclick={step} disabled={playing}>Step</button>
      <button class="btn ghost" onclick={reset}>Reset</button>
      <span class="muted small">Day {history.length} · today is <strong>{labels[current]}</strong></span>
    {/snippet}
    <div class="grid">
      <div class="graph">
        <ChainGraph
          {labels}
          {P}
          active={current}
          {previous}
          {stepKey}
          positions={[[260, 70], [110, 270], [410, 270]]}
          onselect={(i) => (editing = i)}
        />
      </div>
      <div class="side">
        <div class="history" aria-label="Recent days">
          {#each history as s, i (i)}
            <span class="chip" style:background={palette.value.series[s]} title={labels[s]}></span>
          {/each}
        </div>
        <p class="small muted">Days spent in each state so far</p>
        {#each labels as label, i (label)}
          <div class="freq">
            <span class="name"><span class="key" style:background={palette.value.series[i]}></span>{label}</span>
            <span class="bar"><span style:width="{(counts[i] / total) * 100}%" style:background={palette.value.series[i]}></span></span>
            <span class="num">{((counts[i] / total) * 100).toFixed(0)}%</span>
          </div>
        {/each}
        <div class="editor">
          <p class="small muted">
            Edit the row for <strong>{labels[editing]}</strong> (click a state to choose). The other entries rescale
            so the row still sums to 1.
          </p>
          {#each labels as label, j (label)}
            <Slider
              label="P({labels[editing]} → {label})"
              value={rows[editing][j]}
              min={0}
              max={1}
              step={0.01}
              width={220}
              format={(v) => v.toFixed(2)}
              oninput={(v) => setProbability(editing, j, v)}
            />
          {/each}
        </div>
      </div>
    </div>
  </Figure>

  <p>
    Write the probabilities in a table with one row per <em>today</em> and one column per <em>tomorrow</em> and you get
    the <strong>transition matrix</strong> <Tex tex="P" />. Every entry is a probability, and every row sums to 1,
    because tomorrow has to be <em>something</em>. A matrix with those two properties is called
    <strong>row-stochastic</strong>.
  </p>

  <Figure caption="The same chain as a matrix. Row = today, column = tomorrow. The highlighted row is the one the engine is sampling from right now.">
    <MatrixHeatmap rows={labels} values={P} max={1} highlightRow={current} cell={64} rowTitle="today ↓" colTitle="tomorrow →" />
  </Figure>

  <p>
    The defining rule, the <strong>Markov property</strong>, says the past matters only through the present:
  </p>
  <Tex display tex={String.raw`\Pr(X_{n+1} = j \mid X_n = i,\; X_{n-1}, \dots, X_0) \;=\; \Pr(X_{n+1} = j \mid X_n = i) \;=\; P_{ij}`} />
  <p>
    It sounds like a limitation, and it is a strong one, but it is also what makes chains tractable: to simulate
    one you need only the current state and its row. In the engine that is a dense integer id,
    <code>mk_state</code>, and an array of weighted edges. A row is stored as raw counts rather than probabilities;
    the next chapter explains why.
  </p>

  <CodeBlock
    title="engine/include/markov/chain.h"
    code={`typedef uint32_t mk_state;          /* a state is just a dense index */

typedef struct mk_edge
{
    mk_state target;                /* where the transition goes       */
    uint32_t count;                 /* how often it was observed       */
} mk_edge;

/* Draw a successor of \`from\`; MK_ERR_DEAD_END if it has none. */
mk_status mk_chain_next(mk_chain *chain, mk_rng *rng, mk_state from,
                        mk_sampler sampler, mk_state *out);`}
  />
</div>

<style>
  .grid {
    display: grid;
    grid-template-columns: minmax(0, 1.3fr) minmax(0, 1fr);
    gap: 24px;
    align-items: start;
  }
  .graph {
    min-width: 0;
  }
  .side {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .history {
    display: flex;
    flex-wrap: wrap;
    gap: 3px;
    min-height: 34px;
    align-content: flex-start;
  }
  .chip {
    width: 12px;
    height: 12px;
    border-radius: 3px;
  }
  .small {
    font-size: 13px;
    margin: 4px 0;
  }
  .freq {
    display: grid;
    grid-template-columns: 72px 1fr 38px;
    gap: 8px;
    align-items: center;
    font-size: 13px;
  }
  .name {
    color: var(--ink-2);
  }
  .key {
    display: inline-block;
    width: 9px;
    height: 9px;
    border-radius: 2px;
    margin-right: 6px;
  }
  .bar {
    height: 8px;
    background: var(--surface-2);
    border-radius: 4px;
    overflow: hidden;
  }
  .bar span {
    display: block;
    height: 100%;
    border-radius: 4px;
    transition: width 300ms;
  }
  .num {
    text-align: right;
    color: var(--ink-strong);
    font-weight: 600;
  }
  .editor {
    margin-top: 10px;
    padding-top: 10px;
    border-top: 1px solid var(--hairline);
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  @media (max-width: 760px) {
    .grid {
      grid-template-columns: 1fr;
    }
  }
</style>
