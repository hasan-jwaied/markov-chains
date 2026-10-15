<script lang="ts">
  import PageHero from "../lib/components/PageHero.svelte";
  import LineChart from "../lib/charts/LineChart.svelte";
  import SnakesBoard from "../lib/charts/SnakesBoard.svelte";
  import { palette } from "../lib/charts/palette.svelte";
  import Segmented from "../lib/components/Segmented.svelte";
  import Slider from "../lib/components/Slider.svelte";
  import StatTile from "../lib/components/StatTile.svelte";
  import { engine } from "../lib/engine/client.svelte";
  import type { BoardAnalysis } from "../lib/engine/types";

  type Jump = [number, number];

  const MILTON_BRADLEY: Jump[] = [
    [1, 38], [4, 14], [9, 31], [21, 42], [28, 84], [36, 44], [51, 67], [71, 91], [80, 100],
    [16, 6], [47, 26], [49, 11], [56, 53], [62, 19], [64, 60], [87, 24], [93, 73], [95, 75], [98, 78],
  ];

  let jumps = $state<Jump[]>([]);
  let faces = $state(6);
  let classic: Jump[] = [];
  let preset = $state<"classic" | "mb" | "empty" | "random">("classic");

  engine.classicBoard().then((b) => {
    classic = b.jumps;
    if (preset === "classic") jumps = b.jumps.map(([f, t]) => [f, t] as Jump);
  });

  function applyPreset(p: typeof preset) {
    selected = null;
    pending = null;
    if (p === "classic") jumps = classic.map(([f, t]) => [f, t] as Jump);
    else if (p === "mb") jumps = MILTON_BRADLEY.map(([f, t]) => [f, t] as Jump);
    else if (p === "empty") jumps = [];
    else jumps = randomBoard(12);
  }

  function randomBoard(n: number): Jump[] {
    const used = new Set<number>([100]);
    const out: Jump[] = [];
    while (out.length < n) {
      const from = 2 + Math.floor(Math.random() * 97);
      const to = 1 + Math.floor(Math.random() * 99);
      if (used.has(from) || to === from || Math.abs(to - from) < 8) continue;
      used.add(from);
      out.push([from, to]);
    }
    return out;
  }

  // analysis (exact, from the fundamental matrix)
  let analysis = $state<BoardAnalysis | null>(null);
  $effect(() => {
    const spec = { size: 100, faces, jumps: jumps.map(([f, t]) => [f, t] as Jump) };
    let alive = true;
    engine.analyzeBoard(spec, 60, 300).then((a) => alive && (analysis = a)).catch(() => {});
    return () => {
      alive = false;
    };
  });

  let shadeBy = $state<"moves" | "visits" | "none">("moves");
  const shade = $derived(
    !analysis?.ok || shadeBy === "none" ? null : shadeBy === "moves" ? analysis.expectedBySquare : analysis.visits,
  );
  const shadeMax = $derived(shade ? Math.max(1e-9, ...Array.from(shade)) : 1);

  // editing
  let pending = $state<number | null>(null);
  let selected = $state<number | null>(null);
  let message = $state("Click a square to start a snake or ladder, then click where it should lead.");

  function onSquare(n: number) {
    const existing = jumps.findIndex(([f]) => f === n);
    if (pending === null) {
      if (existing >= 0) {
        selected = n;
        message = `Selected the ${jumps[existing][1] > n ? "ladder" : "snake"} ${n} → ${jumps[existing][1]}.`;
        return;
      }
      if (n === 100) {
        message = "The last square can't hold a snake or ladder.";
        return;
      }
      selected = null;
      pending = n;
      message = `Start at ${n}. Now click the destination (higher for a ladder, lower for a snake).`;
      return;
    }
    if (n === pending) {
      pending = null;
      message = "Cancelled.";
      return;
    }
    if (challenge && jumps.length >= CHALLENGE_LIMIT) {
      message = `The challenge allows at most ${CHALLENGE_LIMIT} snakes and ladders.`;
      pending = null;
      return;
    }
    jumps = [...jumps, [pending, n]];
    preset = "empty";
    message = `Added a ${n > pending ? "ladder" : "snake"} ${pending} → ${n}.`;
    pending = null;
  }

  function removeSelected() {
    jumps = jumps.filter(([f]) => f !== selected);
    message = `Removed the jump from ${selected}.`;
    selected = null;
  }

  // playing a game
  let token = $state<number | null>(null);
  let moves = $state(0);
  let playing = $state(false);
  let gameSeed = 1;

  async function playGame() {
    if (!analysis?.ok || playing) return;
    playing = true;
    const info = await engine.buildBoard({ size: 100, faces, jumps });
    try {
      const [g] = await engine.generate(info.id, { seed: gameSeed++, start: 0, maxLength: 400, sampler: "alias", policy: "uniform" });
      for (let i = 0; i < g.states.length && playing; i++) {
        token = g.states[i] + 1;
        moves = i;
        await new Promise((r) => setTimeout(r, i === 0 ? 300 : 230));
      }
    } finally {
      void engine.free(info.id);
      playing = false;
    }
  }

  // Monte Carlo
  let sim = $state<number[] | null>(null);
  let simKey = $state("");
  const boardKey = $derived(`${faces}|${jumps.map((j) => j.join(">")).join(",")}`);
  async function simulate() {
    if (!analysis?.ok) return;
    const info = await engine.buildBoard({ size: 100, faces, jumps });
    try {
      const lengths = await engine.simulate(info.id, 0, 99, 20000, 300);
      const hist = new Array(301).fill(0);
      for (const l of lengths) hist[Math.min(300, l)] += 1 / lengths.length;
      sim = hist;
      simKey = boardKey;
    } finally {
      void engine.free(info.id);
    }
  }

  // the challenge
  const CHALLENGE_LIMIT = 5;
  let challenge = $state(false);
  let best = $state(readBest());
  function readBest(): number {
    try {
      const best = Number(localStorage.getItem("snakes-best"));
      return Number.isFinite(best) && best > 0 ? best : 0;
    } catch {
      return 0;
    }
  }
  $effect(() => {
    if (!challenge || !analysis?.ok || jumps.length > CHALLENGE_LIMIT || faces !== 6) return;
    if (analysis.expected > best) {
      best = analysis.expected;
      try {
        localStorage.setItem("snakes-best", String(best));
      } catch {
        /* not remembered in private mode */
      }
    }
  });
  function startChallenge() {
    challenge = true;
    faces = 6;
    jumps = [];
    preset = "empty";
    message = `Build the longest expected game with at most ${CHALLENGE_LIMIT} snakes and ladders (d6).`;
  }

  const horizon = $derived(analysis?.ok ? Math.min(300, Math.max(80, Math.ceil(analysis.expected * 3.2))) : 120);
</script>

<div class="page">
  <PageHero compact eyebrow="Lab" title="Snakes &amp; Ladders Studio">
    Every board is an absorbing Markov chain. Edit this one and the engine re-solves it exactly, through the fundamental matrix N = (I − Q)⁻¹, faster than you can blink.
  </PageHero>

  <div class="layout">
    <div class="boardcol">
      <div class="card boardcard">
        <div class="controls toolbar">
          <Segmented
            options={[{ value: "classic", label: "Assignment" }, { value: "mb", label: "1943 classic" }, { value: "empty", label: "Empty" }, { value: "random", label: "Random" }]}
            bind:value={preset}
            onchange={applyPreset}
          />
          <Slider label="Die faces" bind:value={faces} min={1} max={12} width={130} />
        </div>
        <SnakesBoard {jumps} {shade} {shadeMax} {token} {pending} {selected} onsquare={onSquare} />
        <div class="controls below">
          <span class="msg">{message}</span>
          {#if selected !== null}<button class="btn small" onclick={removeSelected}>Remove</button>{/if}
        </div>
        <div class="controls below">
          <Segmented label="Shade squares by" options={[{ value: "moves", label: "Moves left" }, { value: "visits", label: "Visits" }, { value: "none", label: "Nothing" }]} bind:value={shadeBy} />
          {#if shade}
            <div class="ramp" aria-hidden="true">
              <span>0</span>
              <span class="bar" style:background="linear-gradient(90deg, {palette.value.seq(0.04)}, {palette.value.seq(0.33)}, {palette.value.seq(0.62)})"></span>
              <span>{shadeMax.toFixed(1)}</span>
            </div>
          {/if}
        </div>
      </div>
    </div>

    <div class="side">
      {#if analysis && !analysis.ok}
        <div class="card warn">
          <strong>No finish line in sight.</strong>
          <p>{analysis.error} The fundamental matrix doesn't exist: <span class="mono">mk_math_absorbing</span> returned <span class="mono">MK_ERR_NOT_ABSORBING</span>.</p>
        </div>
      {:else if analysis}
        <div class="tiles">
          <StatTile label="Expected moves" value={analysis.expected.toFixed(2)} note="E[T] from square 1" />
          <StatTile label="Standard deviation" value={analysis.sd.toFixed(1)} />
          <StatTile label="Median / most likely" value="{analysis.median} / {analysis.mode}" />
          <StatTile label="Done within 60 moves" value="{(analysis.pWithin * 100).toFixed(1)}%" note="the original program's cap" />
        </div>
        <p class="small muted">Solved exactly in {analysis.ms.toFixed(1)} ms ({jumps.length} jumps, d{faces}).</p>
      {/if}

      <div class="card section">
        <div class="controls">
          <button class="btn primary" onclick={playGame} disabled={!analysis?.ok || playing}>{playing ? `Move ${moves}…` : "Play a game"}</button>
          {#if playing}<button class="btn" onclick={() => (playing = false)}>Stop</button>{/if}
          <button class="btn" onclick={simulate} disabled={!analysis?.ok}>Simulate 20,000 games</button>
        </div>
        {#if analysis?.ok}
          <LineChart
            series={[{ name: "exact P(T = k)", colorIndex: 0, values: analysis.pmf.slice(0, horizon + 1), area: true }]}
            bars={sim && simKey === boardKey ? { name: "simulated", values: sim.slice(0, horizon + 1) } : null}
            markers={[{ x: Math.round(analysis.expected), label: `mean ${analysis.expected.toFixed(1)}` }]}
            xLabel="moves"
            yFormat={(v) => `${(v * 100).toFixed(1)}%`}
            valueFormat={(v) => `${(v * 100).toFixed(2)}%`}
            directLabels={false}
            height={220}
          />
        {/if}
      </div>

      <div class="card section challenge">
        <h3>Challenge: the longest game</h3>
        <p class="small">
          With at most {CHALLENGE_LIMIT} snakes and ladders and a six-sided die, how long can you make the
          <em>expected</em> game? Snakes alone aren't enough. The chain must also be unable to skip past them.
        </p>
        {#if challenge}
          <p class="score">
            Current <strong class="num">{analysis?.ok && jumps.length <= CHALLENGE_LIMIT && faces === 6 ? analysis.expected.toFixed(2) : "—"}</strong>
            · best <strong class="num">{best.toFixed(2)}</strong> moves
            <span class="muted">({jumps.length}/{CHALLENGE_LIMIT} used)</span>
          </p>
        {:else}
          <button class="btn" onclick={startChallenge}>Start on an empty board</button>
          {#if best > 0}<span class="small muted">Your best: {best.toFixed(2)} moves</span>{/if}
        {/if}
      </div>
    </div>
  </div>

  <section class="prose notes">
    <h2>How the Studio computes</h2>
    <p>
      Each edit sends the board to the engine's Web Worker. <code>mk_snakes_build</code> turns it into a 100-state
      chain using the <code>cell</code> plugin, and <code>mk_math_absorbing</code> orders the states canonically and
      inverts <code>I − Q</code> (99 × 99) by Gaussian elimination with partial pivoting. The shading reads
      <code>t = N·1</code> (moves left) or the first row of <code>N</code> (expected visits to each square in a game
      from square 1), and the chart is the exact distribution of the game length, propagated step by step through
      the sparse chain. Rules follow the original assignment: throws that overshoot square 100 are discarded, and a
      snake or ladder counts as its own move.
    </p>
  </section>
</div>

<style>
  .page {
    max-width: 1200px;
    margin: 0 auto;
    padding: 0 var(--gutter);
  }
  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1.05fr) minmax(0, 1fr);
    gap: 20px;
    align-items: start;
  }
  .boardcard {
    padding: 14px;
  }
  .toolbar {
    margin-bottom: 12px;
    justify-content: space-between;
  }
  .below {
    margin-top: 10px;
    justify-content: space-between;
  }
  .msg {
    font-size: 13.5px;
    color: var(--ink-2);
  }
  .ramp {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    color: var(--muted);
  }
  .ramp .bar {
    width: 120px;
    height: 8px;
    border-radius: 4px;
  }
  .side {
    display: flex;
    flex-direction: column;
    gap: 14px;
    min-width: 0;
  }
  .tiles {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .section {
    padding: 14px 16px;
  }
  .section .controls {
    margin-bottom: 10px;
  }
  .warn {
    padding: 14px 16px;
    border-color: var(--critical);
  }
  .warn p {
    margin: 6px 0 0;
    font-size: 14px;
  }
  .challenge h3 {
    font-size: 16px;
    margin: 0 0 6px;
  }
  .score {
    font-size: 15px;
    margin: 6px 0 0;
  }
  .small {
    font-size: 13.5px;
    margin: 0 0 10px;
  }
  .notes {
    margin-top: 40px;
  }
  @media (max-width: 960px) {
    .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
