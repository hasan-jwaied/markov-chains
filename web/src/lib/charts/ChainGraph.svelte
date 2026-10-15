<script lang="ts">
  /**
   * A small Markov chain drawn as a state diagram: nodes, curved weighted
   * arrows labelled with probabilities, self-loops, an optional highlighted
   * current state, a token that travels along the edge taken, and an
   * optional probability mass shown as each node's filled area.
   */
  import { fmtP, palette } from "./palette.svelte";

  type Point = [number, number];

  let {
    labels,
    P,
    positions,
    active = null,
    previous = null,
    stepKey = 0,
    mass = null,
    colorIndex,
    width = 520,
    height = 340,
    radius = 30,
    showProbabilities = true,
    onselect,
  }: {
    labels: string[];
    /** Row-major n x n transition probabilities. */
    P: ArrayLike<number>;
    /** Node centres in viewBox units; defaults to a circle. */
    positions?: Point[];
    active?: number | null;
    previous?: number | null;
    /** Changes whenever a new step is taken (re-triggers the token). */
    stepKey?: number;
    mass?: ArrayLike<number> | null;
    colorIndex?: number[];
    width?: number;
    height?: number;
    radius?: number;
    showProbabilities?: boolean;
    onselect?: (state: number) => void;
  } = $props();

  const uid = Math.random().toString(36).slice(2, 8);
  const n = $derived(labels.length);
  const pts = $derived<Point[]>(
    positions ??
      labels.map((_, i) => {
        const a = -Math.PI / 2 + (2 * Math.PI * i) / labels.length;
        const r = Math.min(width, height) / 2 - radius - 34;
        return [width / 2 + r * Math.cos(a), height / 2 + r * Math.sin(a)];
      }),
  );
  const centre = $derived<Point>([
    pts.reduce((s, p) => s + p[0], 0) / Math.max(1, pts.length),
    pts.reduce((s, p) => s + p[1], 0) / Math.max(1, pts.length),
  ]);

  interface EdgeGeom {
    i: number;
    j: number;
    p: number;
    d: string;
    lx: number;
    ly: number;
  }

  /** Curved edge i -> j, bent to the right so i->j and j->i separate. */
  function edgePath(i: number, j: number): { d: string; lx: number; ly: number } {
    const [x1, y1] = pts[i];
    const [x2, y2] = pts[j];
    if (i === j) {
      // Self-loop: a teardrop pointing away from the diagram's centre.
      const ang = Math.atan2(y1 - centre[1], x1 - centre[0]) || -Math.PI / 2;
      const spread = 0.5;
      const sx = x1 + radius * Math.cos(ang - spread);
      const sy = y1 + radius * Math.sin(ang - spread);
      const ex = x1 + (radius + 5) * Math.cos(ang + spread);
      const ey = y1 + (radius + 5) * Math.sin(ang + spread);
      const far = radius * 2.5;
      const c1x = x1 + far * Math.cos(ang - 0.75);
      const c1y = y1 + far * Math.sin(ang - 0.75);
      const c2x = x1 + far * Math.cos(ang + 0.75);
      const c2y = y1 + far * Math.sin(ang + 0.75);
      return {
        d: `M${sx},${sy} C${c1x},${c1y} ${c2x},${c2y} ${ex},${ey}`,
        lx: x1 + (radius * 2.2) * Math.cos(ang),
        ly: y1 + (radius * 2.2) * Math.sin(ang),
      };
    }
    const dx = x2 - x1;
    const dy = y2 - y1;
    const len = Math.hypot(dx, dy) || 1;
    const ux = dx / len;
    const uy = dy / len;
    const bend = 0.18 * len;
    const mx = (x1 + x2) / 2 - uy * bend;
    const my = (y1 + y2) / 2 + ux * bend;
    // Start/end on the circles, aimed at the control point.
    const a1 = Math.atan2(my - y1, mx - x1);
    const a2 = Math.atan2(my - y2, mx - x2);
    const sx = x1 + radius * Math.cos(a1);
    const sy = y1 + radius * Math.sin(a1);
    const ex = x2 + (radius + 5) * Math.cos(a2);
    const ey = y2 + (radius + 5) * Math.sin(a2);
    return { d: `M${sx},${sy} Q${mx},${my} ${ex},${ey}`, lx: (sx + 2 * mx + ex) / 4, ly: (sy + 2 * my + ey) / 4 };
  }

  const edges = $derived.by<EdgeGeom[]>(() => {
    const out: EdgeGeom[] = [];
    for (let i = 0; i < n; i++) {
      for (let j = 0; j < n; j++) {
        const p = P[i * n + j];
        if (p > 1e-9) out.push({ i, j, p, ...edgePath(i, j) });
      }
    }
    return out;
  });

  const color = (i: number) => palette.value.series[(colorIndex?.[i] ?? i) % 8];
  const maxMass = $derived(mass ? Math.max(1e-9, ...Array.from(mass)) : 1);

  // ---- the travelling token -------------------------------------------------
  let token = $state<{ x: number; y: number } | null>(null);
  let pathEls: Record<string, SVGPathElement> = $state({});

  $effect(() => {
    void stepKey;
    if (previous === null || active === null) return;
    const el = pathEls[`${previous}-${active}`];
    if (!el) return;
    const total = el.getTotalLength();
    const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    const duration = reduce ? 1 : 420;
    let start: number | null = null;
    let raf = 0;
    const frame = (t: number) => {
      start ??= t;
      const k = Math.min(1, (t - start) / duration);
      const e = k < 0.5 ? 2 * k * k : 1 - (-2 * k + 2) ** 2 / 2;
      const pt = el.getPointAtLength(e * total);
      token = { x: pt.x, y: pt.y };
      if (k < 1) raf = requestAnimationFrame(frame);
      else token = null;
    };
    raf = requestAnimationFrame(frame);
    return () => cancelAnimationFrame(raf);
  });
</script>

<svg viewBox="0 0 {width} {height}" role="img" aria-label="State diagram of a Markov chain with {n} states">
  <defs>
    <marker id="arrow-{uid}" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0,1 L8,5 L0,9 z" fill={palette.value.ink2} />
    </marker>
    <marker id="arrow-active-{uid}" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0,1 L8,5 L0,9 z" fill={palette.value.accent} />
    </marker>
  </defs>

  {#each edges as e (`${e.i}-${e.j}`)}
    {@const hot = active === e.i}
    <path
      bind:this={pathEls[`${e.i}-${e.j}`]}
      d={e.d}
      fill="none"
      stroke={hot ? palette.value.accent : palette.value.ink2}
      stroke-opacity={hot ? 0.95 : 0.45}
      stroke-width={1.2 + 3.2 * e.p}
      stroke-linecap="round"
      marker-end={hot ? `url(#arrow-active-${uid})` : `url(#arrow-${uid})`}
    />
  {/each}

  {#if showProbabilities}
    {#each edges as e (`l${e.i}-${e.j}`)}
      {@const hot = active === e.i}
      <g transform="translate({e.lx},{e.ly})">
        <rect x="-17" y="-10" width="34" height="20" rx="10" fill={palette.value.surface} stroke={hot ? palette.value.accent : "none"} stroke-width="1" />
        <text class="plabel" class:hot text-anchor="middle" dy="4">{fmtP(e.p)}</text>
      </g>
    {/each}
  {/if}

  {#snippet nodeBody(label: string, i: number)}
    {#if active === i}
      <circle r={radius + 7} fill="none" stroke={palette.value.accent} stroke-width="2" stroke-opacity="0.35" />
    {/if}
    <circle r={radius} fill={palette.value.surface} stroke={color(i)} stroke-width={active === i ? 3.5 : 2} />
    {#if mass}
      <circle r={Math.max(0, (radius - 4) * Math.sqrt(mass[i] / maxMass))} fill={color(i)} fill-opacity="0.32" />
    {/if}
    <text class="nlabel" text-anchor="middle" dy="4">{label}</text>
    {#if mass}
      <text class="mlabel" text-anchor="middle" y={radius + 16}>{(mass[i] * 100).toFixed(1)}%</text>
    {/if}
  {/snippet}

  {#each labels as label, i (i)}
    {@const [x, y] = pts[i]}
    {#if onselect}
      <g
        class="node clickable"
        transform="translate({x},{y})"
        role="button"
        tabindex="0"
        aria-label="Jump to {label}"
        onclick={() => onselect(i)}
        onkeydown={(ev) => (ev.key === "Enter" || ev.key === " ") && onselect(i)}
      >
        {@render nodeBody(label, i)}
      </g>
    {:else}
      <g class="node" transform="translate({x},{y})">{@render nodeBody(label, i)}</g>
    {/if}
  {/each}

  {#if token}
    <circle cx={token.x} cy={token.y} r="7" fill={palette.value.accent} stroke={palette.value.surface} stroke-width="2" />
  {/if}
</svg>

<style>
  svg {
    width: 100%;
    max-width: 620px;
    height: auto;
    display: block;
    margin: 0 auto;
    overflow: visible;
  }
  .plabel {
    font: 600 11px var(--font-sans);
    fill: var(--ink-2);
    font-variant-numeric: tabular-nums;
  }
  .plabel.hot {
    fill: var(--accent-ink);
  }
  .nlabel {
    font: 600 13px var(--font-sans);
    fill: var(--ink-strong);
    pointer-events: none;
  }
  .mlabel {
    font: 600 11.5px var(--font-sans);
    fill: var(--ink-2);
    font-variant-numeric: tabular-nums;
  }
  .node.clickable {
    cursor: pointer;
  }
  .node:focus-visible {
    outline: none;
  }
  .node:focus-visible circle {
    stroke-width: 4;
  }
</style>
