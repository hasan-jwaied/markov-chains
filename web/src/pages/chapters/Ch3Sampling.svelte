<script lang="ts">
  import BarChart from "../../lib/charts/BarChart.svelte";
  import { inkOn, palette } from "../../lib/charts/palette.svelte";
  import CodeBlock from "../../lib/components/CodeBlock.svelte";
  import Figure from "../../lib/components/Figure.svelte";
  import Segmented from "../../lib/components/Segmented.svelte";
  import Slider from "../../lib/components/Slider.svelte";
  import Tex from "../../lib/components/Tex.svelte";
  import { engine } from "../../lib/engine/client.svelte";
  import { engineModel } from "../../lib/engine/model.svelte";

  const names = ["cat", "dog", "mat", "hat"];
  let weights = $state([5, 2, 2, 1]);
  const total = $derived(weights.reduce((a, b) => a + b, 0));

  // linear scan
  let r = $state<number | null>(null);
  let scanned = $state(0);
  let found = $state<number | null>(null);

  async function drawLinear() {
    if (total === 0) return;
    r = Math.floor(Math.random() * total);
    found = null;
    let cumulative = 0;
    for (let i = 0; i < weights.length; i++) {
      scanned = i + 1;
      await new Promise((res) => setTimeout(res, 260));
      cumulative += weights[i];
      if (r < cumulative) {
        found = i;
        return;
      }
    }
  }

  // alias table (built by the C engine)
  let table = $state<{ alias: number[]; threshold: number[]; total: number } | null>(null);
  $effect(() => {
    const w = [...weights];
    if (w.every((x) => x === 0)) {
      table = null;
      return;
    }
    let alive = true;
    engine.aliasTable(w).then((t) => alive && (table = t)).catch(() => {});
    return () => {
      alive = false;
    };
  });
  let pick = $state<{ column: number; height: number; result: number } | null>(null);

  function drawAlias() {
    if (!table) return;
    const column = Math.floor(Math.random() * weights.length);
    const height = Math.floor(Math.random() * table.total);
    const result = height < table.threshold[column] ? column : table.alias[column];
    pick = { column, height, result };
  }

  // 100,000 draws in the engine
  const hub = engineModel(() => {
    const w = [...weights];
    const n = names.length + 1;
    const matrix = new Array(n * n).fill(0);
    w.forEach((x, i) => (matrix[i + 1] = x));
    return engine.buildCustom({ labels: ["from", ...names.map((s) => `${s}.`)], weights: matrix });
  });
  let sampler = $state<"alias" | "linear">("alias");
  let empirical = $state<number[] | null>(null);
  let exact = $state<number[]>([]);
  let draws = 100000;

  $effect(() => {
    const id = hub.id;
    const s = sampler;
    const w = [...weights];
    if (id === null) return;
    let alive = true;
    engine
      .sampleCounts(id, 0, draws, 2026, s)
      .then((res) => {
        if (!alive) return;
        // map engine targets (1..n) back to the fixed name order
        const byTarget = new Map(res.map((e) => [e.target, e]));
        empirical = names.map((_, i) => (byTarget.get(i + 1)?.count ?? 0) / draws);
        const t = w.reduce((a, b) => a + b, 0);
        exact = w.map((x) => x / t);
      })
      .catch(() => {});
    return () => {
      alive = false;
    };
  });

  const W = 600;
  const segX = $derived(weights.map((_, i) => (weights.slice(0, i).reduce((a, b) => a + b, 0) / Math.max(1, total)) * W));
</script>

<div class="prose">
  <p class="eyebrow">Chapter 3</p>
  <h2>Rolling a weighted die</h2>
  <p>
    Simulating a chain means, over and over, drawing a successor with probability proportional to its count. The
    original code did it the obvious way: pick a random integer <Tex tex="r" /> below the row total, then walk the
    successors adding up counts until the running sum passes <Tex tex="r" />.
  </p>

  <Figure wide caption="The cumulative scan. Each successor owns a stretch of [0, total) as wide as its count. The scan visits stretches left to right, so a row with d successors costs up to d comparisons per draw.">
    {#snippet controls()}
      {#each names as name, i (name)}
        <Slider label="count({name})" bind:value={weights[i]} min={0} max={12} width={110} />
      {/each}
      <button class="btn primary" onclick={drawLinear} disabled={total === 0}>Draw r</button>
    {/snippet}
    <svg viewBox="-10 -34 {W + 20} 110" class="strip" role="img" aria-label="Cumulative weights as a strip">
      {#each weights as w, i (i)}
        {#if w > 0}
          <rect
            x={segX[i] + 1}
            y="0"
            width={Math.max(0, (w / total) * W - 2)}
            height="40"
            rx="4"
            fill={palette.value.series[i]}
            opacity={found === null ? (i < scanned ? 1 : 0.35) : i === found ? 1 : 0.25}
          />
          <text x={segX[i] + (w / total) * W / 2} y="62" text-anchor="middle" class="seg">{names[i]} · {w}</text>
        {/if}
      {/each}
      {#if r !== null && total > 0}
        {@const rx = ((r + 0.5) / total) * W}
        <line x1={rx} x2={rx} y1="-10" y2="46" class="rline" />
        <text x={rx} y="-16" text-anchor="middle" class="rlabel">r = {r}</text>
      {/if}
    </svg>
    <p class="note">
      {#if found !== null}
        <strong>{names[found]}</strong> after {scanned} comparison{scanned === 1 ? "" : "s"}.
      {:else if r !== null}
        scanning…
      {:else}
        Press <em>Draw r</em> to sample once.
      {/if}
    </p>
  </Figure>

  <p>
    That is fine for four successors. But a common word like <em>the</em> has thousands, and the scan cost grows
    with them. <strong>Vose's alias method</strong> trades a one-off <Tex tex="O(d)" /> setup for
    <Tex tex="O(1)" /> draws. Cut the probability mass into <Tex tex="d" /> columns of equal height. Each column
    holds at most two outcomes: its own, and one "alias" that tops it up. To sample, pick a column uniformly, then a
    height; one comparison decides which of the two you got.
  </p>

  <Figure wide caption="The alias table that libmarkov builds for the counts above, computed by the C engine. The engine's version is exact integer arithmetic: every column holds exactly `total` units, so P(outcome) = count / total with no rounding.">
    {#snippet controls()}
      <button class="btn primary" onclick={drawAlias} disabled={!table}>Draw (column, height)</button>
      {#if pick}
        <span class="note">column <strong>{pick.column + 1}</strong>, height <strong>{pick.height}</strong>
          {pick.height < (table?.threshold[pick.column] ?? 0) ? "≤ threshold → keep the column's own outcome" : "> threshold → take the alias"}: <strong>{names[pick.result]}</strong></span>
      {/if}
    {/snippet}
    {#if table}
      {@const n = names.length}
      {@const cw = 90}
      {@const ch = 150}
      <svg viewBox="0 -10 {n * (cw + 24)} {ch + 50}" class="alias" role="img" aria-label="Alias table columns">
        {#each table.threshold as t, i (i)}
          {@const x = i * (cw + 24) + 12}
          {@const own = (t / table.total) * ch}
          <rect x={x} y={ch - own} width={cw} height={Math.max(0, own - 1)} rx="4" fill={palette.value.series[i]} opacity={pick && pick.column !== i ? 0.3 : 1} />
          {#if own < ch - 1}
            <rect x={x} y="0" width={cw} height={ch - own - 1} rx="4" fill={palette.value.series[table.alias[i]]} opacity={pick && pick.column !== i ? 0.3 : 1} />
            <text x={x + cw / 2} y={(ch - own) / 2 + 4} text-anchor="middle" class="in" fill={inkOn(palette.value.series[table.alias[i]])}>alias: {names[table.alias[i]]}</text>
          {/if}
          {#if own > 18}
            <text x={x + cw / 2} y={ch - own / 2 + 4} text-anchor="middle" class="in" fill={inkOn(palette.value.series[i])}>{names[i]}</text>
          {/if}
          <text x={x + cw / 2} y={ch + 18} text-anchor="middle" class="seg">column {i + 1}</text>
          <text x={x + cw / 2} y={ch + 34} text-anchor="middle" class="seg muted">threshold {t}</text>
          {#if pick && pick.column === i}
            {@const py = ch - (pick.height / table.total) * ch}
            <line x1={x - 6} x2={x + cw + 6} y1={py} y2={py} class="rline" />
          {/if}
        {/each}
      </svg>
    {/if}
  </Figure>

  <p>
    Does the trick really give the right probabilities? Here are one hundred thousand draws made by the engine, with
    each sampler, against the exact values (the ticks):
  </p>
  <Figure caption="Empirical frequencies from 100,000 draws in the engine (bars) against count / total (ticks). Both samplers converge to the same distribution; only their cost differs.">
    {#snippet controls()}
      <Segmented options={[{ value: "alias", label: "Alias, O(1)" }, { value: "linear", label: "Linear scan, O(d)" }]} bind:value={sampler} />
    {/snippet}
    {#if empirical}
      <BarChart labels={names} values={empirical} reference={exact} referenceLabel="exact probability" max={Math.max(0.05, ...empirical, ...exact) * 1.15} />
    {/if}
  </Figure>

  <h3>The bias hiding in <code>rand() % n</code></h3>
  <p>
    One more subtlety. The original code reduced a random number with <code>rand() % n</code>. If the generator
    produces <Tex tex="M" /> equally likely values and <Tex tex="n" /> does not divide <Tex tex="M" />, the first
    <Tex tex="M \bmod n" /> remainders get one extra chance. With an 8-bit generator and <Tex tex="n = 3" />, for
    example, outcome 0 has probability <Tex tex="86/256" /> rather than <Tex tex="1/3" />. The engine's PCG32
    generator rejects that biased tail. For the golden tests it also carries a bit-exact re-implementation of
    glibc's <code>rand()</code>, so the C command-line tools still reproduce the original program's output
    byte for byte.
  </p>
  <CodeBlock
    title="engine/src/rng.c"
    code={`uint32_t threshold = (0u - bound) % bound;  /* = 2^32 mod bound */
for (;;)
{
    uint32_t r = pcg32_next(rng);
    if (r >= threshold)                     /* reject the biased tail */
        return r % bound;
}`}
  />
  <p>
    On a successor list of 4,096 entries the alias table draws in about 27&nbsp;ns, where the original scan needed
    about 3.5&nbsp;µs: over a hundred times faster. See <a href="#/hood">Under the Hood</a> for the full benchmarks.
  </p>
</div>

<style>
  .strip,
  .alias {
    width: 100%;
    height: auto;
    display: block;
    margin: 0 auto;
  }
  .alias {
    max-width: 560px;
  }
  .strip rect {
    transition: opacity 200ms;
  }
  .seg {
    font: 12px var(--font-sans);
    fill: var(--ink-2);
  }
  .in {
    font: 600 12px var(--font-sans);
  }
  .rline {
    stroke: var(--ink-strong);
    stroke-width: 2;
  }
  .rlabel {
    font: 600 12px var(--font-sans);
    fill: var(--ink-strong);
  }
  .note {
    font: 13.5px var(--font-sans);
    color: var(--ink-2);
    margin: 6px 0 0;
  }
</style>
