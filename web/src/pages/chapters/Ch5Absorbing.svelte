<script lang="ts">
  import BarChart from "../../lib/charts/BarChart.svelte";
  import LineChart from "../../lib/charts/LineChart.svelte";
  import MatrixHeatmap from "../../lib/charts/MatrixHeatmap.svelte";
  import { palette } from "../../lib/charts/palette.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import Slider from "../../lib/components/Slider.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { href } from "../../lib/router.svelte";
  import { engine } from "../../lib/engine/client.svelte";
  import { engineModel } from "../../lib/engine/model.svelte";
  import type { AbsorbingResult, BoardAnalysis, BoardSpec } from "../../lib/engine/types";

  const SIZE = 10;
  let faces = $state(3);
  const jumps: Array<[number, number]> = [
    [2, 6],
    [8, 3],
  ];
  const board = $derived<BoardSpec>({ size: SIZE, faces, jumps });
  const mini = engineModel(() => engine.buildBoard(board));

  let P = $state<Float64Array | null>(null);
  let abs = $state<AbsorbingResult | null>(null);
  $effect(() => {
    const id = mini.id;
    if (id === null) return;
    let alive = true;
    Promise.all([engine.matrix(id), engine.absorbing(id)])
      .then(([m, a]) => {
        if (!alive) return;
        P = m;
        abs = a;
      })
      .catch(() => {});
    return () => {
      alive = false;
    };
  });

  const squares = Array.from({ length: SIZE }, (_, i) => String(i + 1));
  const transientLabels = $derived(abs ? Array.from(abs.transient, (s) => String(s + 1)) : []);

  // the classic board
  let classic = $state<BoardAnalysis | null>(null);
  let sim = $state<number[] | null>(null);
  $effect(() => {
    let alive = true;
    (async () => {
      const spec = await engine.classicBoard();
      const analysis = await engine.analyzeBoard(spec, 60, 200);
      const info = await engine.buildBoard(spec);
      const lengths = await engine.simulate(info.id, 0, 7, 20000, 200);
      void engine.free(info.id);
      const hist = new Array(201).fill(0);
      for (const l of lengths) hist[Math.min(200, l)] += 1 / lengths.length;
      if (!alive) return;
      classic = analysis;
      sim = hist;
    })().catch(() => {});
    return () => {
      alive = false;
    };
  });

  const W = 600;
  const sq = W / SIZE;
</script>

<div class="prose">
  <p class="eyebrow">Chapter 5</p>
  <h2>Snakes, ladders and absorbing states</h2>
  <p>
    Some states are traps. The last square of a Snakes &amp; Ladders board, or the period that ends a tweet, can be
    entered but never left: the chain is <strong>absorbed</strong>. A chain where every state can eventually reach
    such a trap is an <strong>absorbing chain</strong>, and it answers a new kind of question: not "where does it
    spend its time?" but <em>how long until it ends?</em>
  </p>
  <p>
    Here is a tiny board: ten squares, a die with <Tex tex="f" /> faces, one ladder and one snake. Following the
    original assignment, throws that would overshoot the last square are discarded, and a snake or ladder is its
    own move.
  </p>

  <Figure wide caption="A 10-square board as a Markov chain. Square 10 is absorbing. The engine reorders states into canonical form (transient first), solves for N = (I − Q)⁻¹ and reads every answer off it.">
    {#snippet controls()}
      <Slider label="Die faces f" bind:value={faces} min={2} max={6} width={160} />
    {/snippet}
    <svg viewBox="0 -60 {W} {sq + 124}" class="mini" role="img" aria-label="The ten-square board">
      {#each squares as s, i (s)}
        <rect x={i * sq + 2} y="0" width={sq - 4} height={sq - 4} rx="6" fill={i === SIZE - 1 ? palette.value.accent : "var(--surface-2)"} stroke="var(--border)" />
        <text x={i * sq + sq / 2} y={sq / 2 + 3} text-anchor="middle" class="sq" fill={i === SIZE - 1 ? "#fff" : "var(--ink-strong)"}>{s}</text>
      {/each}
      {#each jumps as [from, to] (from)}
        {@const x1 = (from - 1) * sq + sq / 2}
        {@const x2 = (to - 1) * sq + sq / 2}
        {@const up = to > from}
        <path
          d={up ? `M${x1},-2 C${x1},-48 ${x2},-48 ${x2},-6` : `M${x1},${sq - 2} C${x1},${sq + 44} ${x2},${sq + 44} ${x2},${sq + 2}`}
          fill="none"
          stroke={up ? palette.value.ladder : palette.value.snake}
          stroke-width="3"
          stroke-dasharray={up ? "none" : "7 4"}
          marker-end="url(#mini-arrow-{up ? 'up' : 'down'})"
        />
        <text x={(x1 + x2) / 2} y={up ? -44 : sq + 52} text-anchor="middle" class="jl">{up ? "ladder" : "snake"} {from}→{to}</text>
      {/each}
      <defs>
        <marker id="mini-arrow-up" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="M0,1 L8,5 L0,9 z" fill={palette.value.ladder} /></marker>
        <marker id="mini-arrow-down" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="M0,1 L8,5 L0,9 z" fill={palette.value.snake} /></marker>
      </defs>
    </svg>
    <div class="mats">
      <div>
        <h4>Transition matrix P</h4>
        {#if P}
          <MatrixHeatmap rows={squares} values={P} max={1} cell={30} showValues={false} format={(v) => v.toFixed(3)} describe={(r, c, v) => `P(${r} → ${c}) = ${v}`} />
        {/if}
      </div>
      <div>
        <h4>Fundamental matrix N = (I − Q)⁻¹</h4>
        {#if abs?.ok}
          <MatrixHeatmap rows={transientLabels} values={abs.N} cell={30} showValues={false} format={(v) => v.toFixed(3)} describe={(r, c, v) => `from ${r}, expected visits to ${c}: ${v}`} />
        {/if}
      </div>
    </div>
    {#if abs?.ok}
      <h4>Expected moves to finish, t = N·1</h4>
      <BarChart labels={transientLabels} values={abs.expected} colorIndex={transientLabels.map(() => 0)} max={Math.max(...abs.expected) * 1.15} format={(v) => v.toFixed(2)} height={170} />
    {/if}
  </Figure>

  <p>
    Order the states with the transient ones first and <Tex tex="P" /> splits into blocks: <Tex tex="Q" /> moves
    between transient states, <Tex tex="R" /> moves into the absorbing ones, and the absorbing states only map to
    themselves.
  </p>
  <Tex display tex={String.raw`P = \begin{pmatrix} Q & R \\ 0 & I \end{pmatrix}, \qquad N = (I - Q)^{-1} = I + Q + Q^2 + Q^3 + \cdots`} />
  <p>
    Read the series aloud: <Tex tex={String.raw`(Q^k)_{ij}`} /> is the probability of being in transient state <Tex tex="j" />
    after exactly <Tex tex="k" /> moves from <Tex tex="i" />, so summing over all <Tex tex="k" /> counts the
    <strong>expected number of visits</strong>. That single matrix answers everything:
  </p>
  <Tex display tex={String.raw`\begin{aligned} t &= N\mathbf{1} && \text{expected moves to absorption} \\ \operatorname{Var}(T) &= (2N - I)\,t - t \circ t && \text{its variance} \\ B &= NR && \text{where the chain is absorbed} \end{aligned}`} />
  <p>
    The engine builds <Tex tex="I - Q" /> densely and inverts it by Gaussian elimination with partial pivoting. For
    chains too large for a dense solve it switches to a sparse Gauss–Seidel iteration on
    <Tex tex={String.raw`t = \mathbf{1} + Qt`} />, and the tests check that both solvers agree.
  </p>

  <h3>The board from the assignment</h3>
  {#if classic?.ok}
    <p>
      The original assignment shipped a 100-square board with 20 snakes and ladders, and simulated games capped at
      60 moves. The engine solves that board exactly: a game takes
      <strong>{classic.expected.toFixed(2)}</strong> moves on average (standard deviation
      {classic.sd.toFixed(1)}), and the single most likely length is {classic.mode}. Only
      <strong>{(classic.pWithin * 100).toFixed(1)}%</strong> of games finish within 60 moves, so the original program
      quietly cut off {(100 - classic.pWithin * 100).toFixed(0)}% of its random walks.
    </p>
    <Figure wide caption="Distribution of the game length T on the classic board. Line: the exact P(T = k) from the absorbing-chain analysis. Bars: 20,000 games simulated by the engine. The dashed lines mark the mean and the original 60-move cap.">
      <LineChart
        series={[{ name: "exact P(T = k)", colorIndex: 0, values: classic.pmf.slice(0, 161), area: true }]}
        bars={sim ? { name: "simulated (20,000 games)", values: sim.slice(0, 161) } : null}
        markers={[{ x: Math.round(classic.expected), label: `mean ${classic.expected.toFixed(1)}` }, { x: 60, label: "60-move cap" }]}
        xLabel="moves"
        yFormat={(v) => `${(v * 100).toFixed(1)}%`}
        valueFormat={(v) => `${(v * 100).toFixed(2)}%`}
        directLabels={false}
        height={250}
      />
    </Figure>
  {:else}
    <p class="muted">Solving the classic board…</p>
  {/if}
  <CodeBlock
    title="engine/include/markov/math.h"
    code={`typedef struct mk_absorbing
{
    uint32_t n_transient, n_absorbing;
    double *fundamental;      /* N, n_transient x n_transient */
    double *expected_steps;   /* t = N 1                      */
    double *variance_steps;   /* (2N - I) t - t*t             */
    double *absorption;       /* B = N R                      */
    /* ... */
} mk_absorbing;

mk_status mk_math_absorbing(const mk_chain *chain, mk_absorbing *out);`}
  />
  <p>Design your own board, and try to make the longest possible game, in the <a href={href("snakes")}>Snakes &amp; Ladders Studio</a>.</p>
</div>

<style>
  .mini {
    width: 100%;
    height: auto;
    display: block;
    margin-bottom: 18px;
    overflow: visible;
  }
  .sq {
    font: 600 14px var(--font-sans);
  }
  .jl {
    font: 600 11.5px var(--font-sans);
    fill: var(--ink-2);
  }
  .mats {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 24px;
    margin-bottom: 16px;
  }
  h4 {
    font-size: 13px;
    font-weight: 600;
    color: var(--ink-2);
    margin: 0 0 8px;
  }
  @media (max-width: 760px) {
    .mats {
      grid-template-columns: 1fr;
    }
  }
</style>
