<script lang="ts">
  import PageHero from "../lib/components/PageHero.svelte";
  import LineChart from "../lib/charts/LineChart.svelte";
  import MatrixHeatmap from "../lib/charts/MatrixHeatmap.svelte";
  import { inkOn, palette } from "../lib/charts/palette.svelte";
  import Segmented from "../lib/components/Segmented.svelte";
  import Slider from "../lib/components/Slider.svelte";
  import { engine } from "../lib/engine/client.svelte";

  const labels = ["A", "B", "C"];
  const n = labels.length;

  let truth = $state<number[]>([]);
  let observed = $state<number[]>([]);
  let length = $state(60);
  let guess = $state<number[][]>(labels.map(() => labels.map(() => 1 / n)));
  let revealed = $state(false);
  let mle = $state<number[] | null>(null);
  let curve = $state<{ lengths: number[]; errors: number[] } | null>(null);
  let round = $state(0);
  let seed = Math.floor(Math.random() * 1e6);

  /**
   * A random row for state `i`. Every off-diagonal weight is positive, so the
   * hidden chain is irreducible: no state can trap the walk, and every row
   * keeps being observed as the sequence grows.
   */
  function randomRow(i: number): number[] {
    const w = labels.map((_, j) => (j === i ? Math.floor(Math.random() * 10) : 1 + Math.floor(Math.random() * 9)));
    const t = w.reduce((a, b) => a + b, 0);
    return w.map((x) => x / t);
  }

  async function sequence(P: number[], len: number, s: number): Promise<number[]> {
    const info = await engine.buildCustom({ labels, weights: P.map((p) => Math.round(p * 1000)) });
    try {
      const [g] = await engine.generate(info.id, { seed: s, maxLength: len, policy: "uniform", sampler: "alias" });
      return g.states;
    } finally {
      void engine.free(info.id);
    }
  }

  /** Maximum-likelihood estimate of P from a state sequence, by the engine. */
  async function estimate(seq: number[]): Promise<number[]> {
    const info = await engine.buildText(seq.map((s) => labels[s]).join(" "), { order: 1, rule: "period" });
    try {
      const out = new Array(n * n).fill(0);
      for (let s = 0; s < info.states; s++) {
        const st = await engine.stateInfo(info.id, s, n);
        const i = labels.indexOf(st.label);
        for (const e of st.edges) out[i * n + labels.indexOf(e.label)] = e.p;
      }
      return out;
    } finally {
      void engine.free(info.id);
    }
  }

  /** Mean total-variation distance between the rows of two matrices (0..1). */
  function distance(a: ArrayLike<number>, b: ArrayLike<number>): number {
    let sum = 0;
    for (let i = 0; i < n; i++) {
      let tv = 0;
      for (let j = 0; j < n; j++) tv += Math.abs(a[i * n + j] - b[i * n + j]);
      sum += tv / 2;
    }
    return sum / n;
  }

  async function newRound() {
    truth = labels.flatMap((_, i) => randomRow(i));
    guess = labels.map(() => labels.map(() => 1 / n));
    revealed = false;
    mle = null;
    curve = null;
    round++;
    observed = await sequence(truth, length, seed++);
  }

  async function observeMore(len: number) {
    length = len;
    if (truth.length) observed = await sequence(truth, len, seed++);
  }

  async function reveal() {
    revealed = true;
    mle = await estimate(observed);
    const lengths = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000];
    const errors: number[] = [];
    for (const L of lengths) {
      let e = 0;
      for (let rep = 0; rep < 4; rep++) e += distance(await estimate(await sequence(truth, L, seed++)), truth);
      errors.push(e / 4);
    }
    curve = { lengths, errors };
  }

  function setProbability(row: number, col: number, value: number) {
    const r = guess[row];
    const rest = 1 - r[col];
    guess[row] = r.map((p, j) => (j === col ? value : rest > 1e-9 ? (p / rest) * (1 - value) : (1 - value) / (n - 1)));
  }

  newRound();

  const guessFlat = $derived(guess.flat());
  const score = $derived(truth.length ? Math.round(100 * (1 - distance(guessFlat, truth))) : 0);
  const mleScore = $derived(mle ? Math.round(100 * (1 - distance(mle, truth))) : null);
  const counts = $derived.by(() => {
    const c = new Array(n * n).fill(0);
    for (let k = 1; k < observed.length; k++) c[observed[k - 1] * n + observed[k]]++;
    return c;
  });
</script>

<div class="page">
  <PageHero compact eyebrow="Game" title="Guess the Chain">
    A hidden three-state chain has emitted the sequence below. Estimate its transition matrix, then see how you compare with the truth and with the statistician's answer: just counting.
  </PageHero>

  <div class="card block">
    <div class="controls">
      <Segmented
        label="Observed steps"
        options={[30, 60, 150, 400].map((v) => ({ value: v, label: String(v) }))}
        value={length}
        onchange={(v) => observeMore(v)}
      />
      <button class="btn" onclick={newRound}>New hidden chain</button>
      <span class="small muted">Round {round}</span>
    </div>
    <div class="seq" aria-label="Observed sequence">
      {#each observed as s, i (i)}
        <span class="chip" style:background={palette.value.series[s]} style:color={inkOn(palette.value.series[s])} title="step {i}: {labels[s]}">{labels[s]}</span>
      {/each}
    </div>
    <div class="legend">
      {#each labels as l, i (l)}<span><span class="key" style:background={palette.value.series[i]}></span>state {l}</span>{/each}
    </div>
  </div>

  <div class="grid">
    <div class="card block">
      <h2>Your estimate</h2>
      <p class="small muted">Each row is "from"; the three sliders give where it goes next and always sum to 1.</p>
      {#each labels as from, i (from)}
        <div class="row">
          <span class="from" style:border-color={palette.value.series[i]}>{from} →</span>
          {#each labels as to, j (to)}
            <Slider label={to} value={guess[i][j]} min={0} max={1} step={0.01} width={110} format={(v) => v.toFixed(2)} oninput={(v) => setProbability(i, j, v)} />
          {/each}
        </div>
      {/each}
      {#if !revealed}
        <button class="btn primary" onclick={reveal} disabled={!observed.length}>Reveal the chain</button>
      {/if}
    </div>

    <div class="card block">
      <h2>Hint: transition counts</h2>
      <p class="small muted">How often each "from → to" pair appears in the sequence. Counting is all the original assignment's frequency lists did.</p>
      <MatrixHeatmap rows={labels} values={counts} format={(v) => String(v)} describe={(r, c, v) => `${r} → ${c} seen ${v} times`} cell={58} />
    </div>
  </div>

  {#if revealed}
    <div class="card block">
      <h2>The answer</h2>
      <div class="results">
        <div>
          <h3>Truth</h3>
          <MatrixHeatmap rows={labels} values={truth} max={1} cell={52} />
        </div>
        <div>
          <h3>You · <span class="num">{score}</span>/100</h3>
          <MatrixHeatmap rows={labels} values={guessFlat} max={1} cell={52} />
        </div>
        <div>
          <h3>Counting (MLE) · <span class="num">{mleScore ?? "…"}</span>/100</h3>
          {#if mle}<MatrixHeatmap rows={labels} values={mle} max={1} cell={52} />{/if}
        </div>
      </div>
      <p class="small">
        Score = 100 × (1 − mean total-variation distance between rows). The maximum-likelihood estimate
        P̂(i→j) = count(i→j) / count(i→·) was computed by building a chain from the sequence in the C engine. Rows
        that were rarely visited are where every estimate struggles.
      </p>
      {#if curve}
        <h3>More data, better estimates</h3>
        <LineChart
          series={[{ name: "MLE error", colorIndex: 0, values: curve.errors }]}
          xLabel="observed steps"
          xFormat={(i) => (curve ? curve.lengths[i]?.toLocaleString("en-US") ?? "" : "")}
          yFormat={(v) => v.toFixed(2)}
          valueFormat={(v) => v.toFixed(3)}
          directLabels={false}
          height={200}
        />
        <p class="small muted">
          Sequence lengths {curve.lengths.join(", ")}, four runs each. The error falls roughly like 1/√length:
          the law of large numbers, applied to every row at once.
        </p>
      {/if}
    </div>
  {/if}
</div>

<style>
  .page {
    max-width: 1100px;
    margin: 0 auto;
    padding: 0 var(--gutter);
  }
  .block {
    padding: 16px 18px;
    margin-bottom: 16px;
  }
  .block h2 {
    font-size: 16px;
    margin: 0 0 4px;
  }
  .block h3 {
    font-size: 14px;
    margin: 12px 0 8px;
    color: var(--ink-2);
  }
  .seq {
    display: flex;
    flex-wrap: wrap;
    gap: 3px;
    margin: 14px 0 10px;
  }
  .chip {
    width: 22px;
    height: 22px;
    border-radius: 5px;
    font: 700 11px/22px var(--font-sans);
    text-align: center;
  }
  .grid {
    display: grid;
    grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr);
    gap: 16px;
  }
  .row {
    display: flex;
    flex-wrap: wrap;
    gap: 6px 16px;
    align-items: flex-end;
    padding: 10px 0;
    border-bottom: 1px solid var(--hairline);
  }
  .from {
    font: 700 15px var(--font-sans);
    border-left: 4px solid;
    padding-left: 8px;
    min-width: 42px;
  }
  .row + .btn,
  .block > .btn {
    margin-top: 14px;
  }
  .results {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 18px;
  }
  .small {
    font-size: 13.5px;
    margin: 4px 0 8px;
  }
  @media (max-width: 860px) {
    .grid,
    .results {
      grid-template-columns: 1fr;
    }
  }
</style>
