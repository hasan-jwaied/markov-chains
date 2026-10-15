<script lang="ts">
  /**
   * A 10 x 10 Snakes & Ladders board (squares numbered boustrophedon from the
   * bottom-left). Squares can be shaded by a per-square value; ladders are
   * drawn with rails and rungs, snakes as wavy curves, so the two never
   * depend on colour alone.
   */
  import { inkOn, palette } from "./palette.svelte";

  let {
    jumps,
    shade = null,
    shadeMax = 1,
    token = null,
    pending = null,
    selected = null,
    onsquare,
  }: {
    jumps: Array<[number, number]>;
    /** Per-square values (index = square - 1) for the heat shading. */
    shade?: ArrayLike<number> | null;
    shadeMax?: number;
    token?: number | null;
    pending?: number | null;
    selected?: number | null;
    onsquare?: (square: number) => void;
  } = $props();

  const S = 60;
  const N = 10;

  function centre(n: number): [number, number] {
    const r = Math.floor((n - 1) / N);
    const c = (n - 1) % N;
    const col = r % 2 === 0 ? c : N - 1 - c;
    return [col * S + S / 2, (N - 1 - r) * S + S / 2];
  }

  function ladder(from: number, to: number): { rails: string; rungs: string } {
    const [x1, y1] = centre(from);
    const [x2, y2] = centre(to);
    const len = Math.hypot(x2 - x1, y2 - y1);
    const ux = (x2 - x1) / len;
    const uy = (y2 - y1) / len;
    const nx = -uy * 7;
    const ny = ux * 7;
    const rails = `M${x1 + nx},${y1 + ny} L${x2 + nx},${y2 + ny} M${x1 - nx},${y1 - ny} L${x2 - nx},${y2 - ny}`;
    let rungs = "";
    const count = Math.max(2, Math.floor(len / 18));
    for (let i = 1; i < count; i++) {
      const t = i / count;
      const cx = x1 + (x2 - x1) * t;
      const cy = y1 + (y2 - y1) * t;
      rungs += `M${cx + nx},${cy + ny} L${cx - nx},${cy - ny}`;
    }
    return { rails, rungs };
  }

  function snake(from: number, to: number): string {
    const [x1, y1] = centre(from);
    const [x2, y2] = centre(to);
    const len = Math.hypot(x2 - x1, y2 - y1);
    const ux = (x2 - x1) / len;
    const uy = (y2 - y1) / len;
    const waves = Math.max(2, Math.round(len / 55));
    let d = `M${x1},${y1}`;
    for (let i = 0; i < waves; i++) {
      const t0 = i / waves;
      const t1 = (i + 1) / waves;
      const amp = (i % 2 ? -1 : 1) * 13;
      const mx = x1 + (x2 - x1) * ((t0 + t1) / 2) - uy * amp;
      const my = y1 + (y2 - y1) * ((t0 + t1) / 2) + ux * amp;
      d += ` Q${mx},${my} ${x1 + (x2 - x1) * t1},${y1 + (y2 - y1) * t1}`;
    }
    return d;
  }

  const squares = Array.from({ length: N * N }, (_, i) => i + 1);
  const jumpFrom = $derived(new Map(jumps.map(([f, t]) => [f, t])));
</script>

<svg viewBox="-4 -4 {N * S + 8} {N * S + 8}" role="img" aria-label="Snakes and ladders board with {jumps.length} snakes and ladders">
  {#each squares as n (n)}
    {@const [cx, cy] = centre(n)}
    {@const v = shade ? shade[n - 1] : 0}
    {@const fill = n === N * N ? palette.value.accent : shade && v > 0 ? palette.value.seq(0.04 + 0.58 * Math.min(1, v / shadeMax)) : palette.value.surface}
    {#if onsquare}
      <rect
        class="sq clickable"
        class:pending={pending === n}
        class:selected={selected === n}
        x={cx - S / 2 + 1}
        y={cy - S / 2 + 1}
        width={S - 2}
        height={S - 2}
        rx="5"
        {fill}
        role="button"
        tabindex="0"
        aria-label="Square {n}{jumpFrom.has(n) ? `, jump to ${jumpFrom.get(n)}` : ''}"
        onclick={() => onsquare(n)}
        onkeydown={(e) => (e.key === "Enter" || e.key === " ") && onsquare(n)}
      />
    {:else}
      <rect class="sq" x={cx - S / 2 + 1} y={cy - S / 2 + 1} width={S - 2} height={S - 2} rx="5" {fill} />
    {/if}
    <text x={cx - S / 2 + 6} y={cy - S / 2 + 15} class="num" fill={n === N * N ? "#fff" : inkOn(fill)} opacity={n === N * N ? 1 : 0.75}>{n}</text>
  {/each}

  {#each jumps as [from, to] (from)}
    {#if to > from}
      {@const l = ladder(from, to)}
      <g class="jump" opacity={selected !== null && selected !== from ? 0.35 : 1}>
        <path d={l.rails} stroke={palette.value.ladder} stroke-width="3.5" stroke-linecap="round" fill="none" />
        <path d={l.rungs} stroke={palette.value.ladder} stroke-width="2.5" stroke-linecap="round" fill="none" />
      </g>
    {:else}
      {@const [hx, hy] = centre(from)}
      <g class="jump" opacity={selected !== null && selected !== from ? 0.35 : 1}>
        <path d={snake(from, to)} stroke={palette.value.surface} stroke-width="9" stroke-linecap="round" fill="none" />
        <path d={snake(from, to)} stroke={palette.value.snake} stroke-width="5.5" stroke-linecap="round" fill="none" />
        <circle cx={hx} cy={hy} r="7" fill={palette.value.snake} stroke={palette.value.surface} stroke-width="2" />
      </g>
    {/if}
  {/each}

  {#if token !== null}
    {@const [tx, ty] = centre(token)}
    <g class="token" style:transform="translate({tx}px, {ty}px)">
      <circle r="13" fill={palette.value.ink} stroke={palette.value.surface} stroke-width="3" />
      <circle r="5" fill={palette.value.surface} />
    </g>
  {/if}
</svg>

<style>
  svg {
    width: 100%;
    height: auto;
    display: block;
    user-select: none;
  }
  .sq {
    stroke: var(--border);
    stroke-width: 1;
    transition: fill 250ms;
  }
  .clickable {
    cursor: pointer;
  }
  .clickable:hover {
    stroke: var(--ink-2);
    stroke-width: 2;
  }
  .pending,
  .selected {
    stroke: var(--ink-strong) !important;
    stroke-width: 3 !important;
  }
  .num {
    font: 600 11px var(--font-sans);
    pointer-events: none;
    font-variant-numeric: tabular-nums;
  }
  .jump {
    pointer-events: none;
    transition: opacity 150ms;
  }
  .token {
    pointer-events: none;
    transition: transform 260ms ease-in-out;
  }
</style>
