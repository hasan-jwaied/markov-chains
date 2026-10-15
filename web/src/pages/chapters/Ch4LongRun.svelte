<script lang="ts">
  import BarChart from "../../lib/charts/BarChart.svelte";
  import ChainGraph from "../../lib/charts/ChainGraph.svelte";
  import LineChart from "../../lib/charts/LineChart.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import Segmented from "../../lib/components/Segmented.svelte";
  import Slider from "../../lib/components/Slider.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { engine } from "../../lib/engine/client.svelte";
  import { engineModel } from "../../lib/engine/model.svelte";
  import type { StationaryResult } from "../../lib/engine/types";

  const labels = ["Sunny", "Cloudy", "Rainy"];
  const P = [0.7, 0.2, 0.1, 0.3, 0.4, 0.3, 0.2, 0.4, 0.4];
  const STEPS = 30;

  const weather = engineModel(() => engine.buildCustom({ labels, weights: P.map((p) => p * 1000) }));

  let start = $state<"0" | "1" | "2" | "u">("2");
  let k = $state(0);
  let trajectory = $state<Float64Array[]>([]);
  let stationary = $state<StationaryResult | null>(null);

  $effect(() => {
    const id = weather.id;
    const s = start;
    if (id === null) return;
    const x0 = s === "u" ? [1 / 3, 1 / 3, 1 / 3] : labels.map((_, i) => (String(i) === s ? 1 : 0));
    let alive = true;
    Promise.all([engine.powerTrajectory(id, x0, STEPS), engine.stationary(id)])
      .then(([t, st]) => {
        if (!alive) return;
        trajectory = t;
        stationary = st;
      })
      .catch(() => {});
    return () => {
      alive = false;
    };
  });

  let playing = $state(false);
  $effect(() => {
    if (!playing) return;
    const timer = setInterval(() => {
      if (k >= STEPS) {
        playing = false;
        return;
      }
      k++;
    }, 300);
    return () => clearInterval(timer);
  });

  const x = $derived(trajectory[k] ?? new Float64Array(3));
  const series = $derived(
    labels.map((name, i) => ({ name, colorIndex: i, values: trajectory.map((t) => t[i]) })),
  );

  // ---- periodicity -------------------------------------------------------------
  const flip = engineModel(() => engine.buildCustom({ labels: ["A", "B"], weights: [0, 1, 1, 0] }));
  let damping = $state(0.85);
  let flipPlain = $state<Float64Array[]>([]);
  let flipDamped = $state<Float64Array[]>([]);
  $effect(() => {
    const id = flip.id;
    const d = damping;
    if (id === null) return;
    let alive = true;
    Promise.all([engine.powerTrajectory(id, [1, 0], 24, 1), engine.powerTrajectory(id, [1, 0], 24, d)])
      .then(([a, b]) => {
        if (!alive) return;
        flipPlain = a;
        flipDamped = b;
      })
      .catch(() => {});
    return () => {
      alive = false;
    };
  });
</script>

<div class="prose">
  <p class="eyebrow">Chapter 4</p>
  <h2>The long run</h2>
  <p>
    Forget individual walks and track <em>uncertainty</em> instead. If <Tex tex="x_n" /> is the probability of each
    weather state on day <Tex tex="n" /> (a row vector that sums to 1), then the next day's distribution is one
    matrix product away:
  </p>
  <Tex display tex={String.raw`x_{n+1} \;=\; x_n P \qquad\Longrightarrow\qquad x_n \;=\; x_0 P^{\,n}`} />
  <p>
    Start from certainty, say "it is raining today", and step forward. The probability mass spreads through the
    chain like ink in water and settles into a fixed shape, whatever you started from.
  </p>

  <Figure wide caption="Power iteration in the engine: x(k+1) = x(k) P. Node fill shows the current distribution; the black ticks mark the stationary distribution π that the engine solved for directly.">
    {#snippet controls()}
      <Segmented
        label="Start from"
        options={[{ value: "0", label: "Sunny" }, { value: "1", label: "Cloudy" }, { value: "2", label: "Rainy" }, { value: "u", label: "Uniform" }]}
        bind:value={start}
        onchange={() => (k = 0)}
      />
      <Slider label="Day k" bind:value={k} min={0} max={STEPS} width={200} />
      <button class="btn primary" onclick={() => { if (k >= STEPS) k = 0; playing = !playing; }}>{playing ? "Pause" : "Play"}</button>
    {/snippet}
    <div class="grid">
      <ChainGraph {labels} {P} mass={x} positions={[[260, 70], [110, 270], [410, 270]]} showProbabilities={false} />
      <div>
        <BarChart {labels} values={x} reference={stationary?.pi ?? null} referenceLabel="stationary π" />
      </div>
    </div>
    <LineChart {series} xLabel="day k" yMax={1} height={210} />
  </Figure>

  <p>
    The fixed point is the <strong>stationary distribution</strong> <Tex tex="\pi" />: a distribution that the chain
    maps to itself.
  </p>
  <Tex display tex={String.raw`\pi = \pi P, \qquad \sum_i \pi_i = 1`} />
  {#if stationary}
    <p>
      For this weather the engine finds
      <Tex tex={String.raw`\pi \approx (${Array.from(stationary.pi).map((v) => v.toFixed(3)).join(",\\ ")})`} />
      after {stationary.iterations} iterations (to a tolerance of <Tex tex={String.raw`10^{-12}`} />). In the long run it is sunny
      {(stationary.pi[0] * 100).toFixed(0)}% of the time, whatever today looks like. This is also the fraction of
      days a single long walk spends in each state, which is why the counters in chapter 1 drift toward these
      numbers.
    </p>
  {/if}

  <h3>When the iteration never settles</h3>
  <p>
    Not every chain converges. A chain that deterministically flips between two states, A → B → A → …, has a
    perfectly good stationary distribution, (½, ½), yet the power iteration started from A just bounces forever.
    The chain is <strong>periodic</strong>. The fix PageRank made famous is <strong>damping</strong>: with
    probability <Tex tex="1-\alpha" />, jump to a uniformly random state instead.
  </p>
  <Tex display tex={String.raw`x_{n+1} \;=\; \alpha\, x_n P \;+\; \frac{1-\alpha}{|S|}\,\mathbf{1}`} />
  <Figure caption="P(A) under plain power iteration (it oscillates forever) and under damping (it converges to ½). Both curves computed by mk_math_power_step.">
    {#snippet controls()}
      <Slider label="Damping α" bind:value={damping} min={0.5} max={0.99} step={0.01} format={(v) => v.toFixed(2)} />
    {/snippet}
    <LineChart
      series={[
        { name: "plain", colorIndex: 1, values: flipPlain.map((t) => t[0]) },
        { name: `damped α=${damping.toFixed(2)}`, colorIndex: 0, values: flipDamped.map((t) => t[0]) },
      ]}
      xLabel="step"
      yMax={1}
      height={200}
    />
  </Figure>
  <p>
    The engine's power iteration works on the sparse (CSR) form of <Tex tex="P" />, so one step costs time
    proportional to the number of transitions, not to the square of the number of states. That is what makes it
    usable on a vocabulary of tens of thousands of words.
  </p>
  <CodeBlock
    title="engine/src/math.c"
    code={`double mk_math_power_step(const mk_csr *p, const double *x, double *y, double damping)
{
    mk_csr_left_multiply(p, x, y);                  /* y = x P, O(nnz) */
    double teleport = (1.0 - damping) / p->cols;
    double change = 0.0;
    for (uint32_t i = 0; i < p->cols; i++)
    {
        y[i] = damping * y[i] + teleport;
        change += fabs(y[i] - x[i]);                /* L1 residual */
    }
    return change;
}`}
  />
</div>

<style>
  .grid {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 16px;
    align-items: center;
    margin-bottom: 10px;
  }
  @media (max-width: 760px) {
    .grid {
      grid-template-columns: 1fr;
    }
  }
</style>
