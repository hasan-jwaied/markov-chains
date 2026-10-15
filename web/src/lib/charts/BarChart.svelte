<script lang="ts">
  /**
   * Columns for a probability distribution over a few named states, with an
   * optional reference tick per column (e.g. the stationary distribution the
   * bars are converging to). Value on each cap; hover for details.
   */
  import Tooltip from "../components/Tooltip.svelte";
  import { palette } from "./palette.svelte";

  let {
    labels,
    values,
    reference = null,
    referenceLabel = "target",
    colorIndex,
    max = 1,
    height = 190,
    format = (v: number) => `${(v * 100).toFixed(1)}%`,
  }: {
    labels: string[];
    values: ArrayLike<number>;
    reference?: ArrayLike<number> | null;
    referenceLabel?: string;
    colorIndex?: number[];
    max?: number;
    height?: number;
    format?: (v: number) => string;
  } = $props();

  let width = $state(420);
  const W = $derived(Math.max(240, width));
  const pad = { top: 22, right: 8, bottom: 26, left: 8 };
  const band = $derived((W - pad.left - pad.right) / Math.max(1, labels.length));
  const bw = $derived(Math.min(24, band * 0.5));
  const y = (v: number) => pad.top + (height - pad.top - pad.bottom) * (1 - Math.min(v, max) / max);
  const base = $derived(height - pad.bottom);

  let hover = $state<number | null>(null);
  let wrap: HTMLDivElement;
  let tip = $state({ x: 0, y: 0 });

  /** Column with a 4px rounded top and a square foot on the baseline. */
  function column(x: number, top: number, w: number): string {
    const h = base - top;
    if (h <= 0.5) return "";
    const r = Math.min(4, h, w / 2);
    return `M${x},${base} V${top + r} Q${x},${top} ${x + r},${top} H${x + w - r} Q${x + w},${top} ${x + w},${top + r} V${base} Z`;
  }
</script>

<div class="wrap" bind:this={wrap} bind:clientWidth={width}>
  <svg viewBox="0 0 {W} {height}" role="img" aria-label="Bar chart of a probability distribution">
    <line class="baseline" x1={pad.left} x2={W - pad.right} y1={base} y2={base} />
    {#each labels as label, i (i)}
      {@const cx = pad.left + band * i + band / 2}
      {@const v = values[i]}
      <!-- hit target covers the whole band, not just the painted bar -->
      <rect
        x={pad.left + band * i}
        y={pad.top - 18}
        width={band}
        height={base - pad.top + 18}
        fill="transparent"
        role="presentation"
        onpointermove={(ev) => {
          const b = wrap.getBoundingClientRect();
          tip = { x: ev.clientX - b.left, y: ev.clientY - b.top };
          hover = i;
        }}
        onpointerleave={() => (hover = null)}
      />
      <path
        d={column(cx - bw / 2, y(v), bw)}
        fill={palette.value.series[(colorIndex?.[i] ?? i) % 8]}
        opacity={hover === null || hover === i ? 1 : 0.55}
        pointer-events="none"
      />
      {#if reference}
        <line class="ref" x1={cx - bw / 2 - 6} x2={cx + bw / 2 + 6} y1={y(reference[i])} y2={y(reference[i])} />
      {/if}
      <text class="value" x={cx} y={y(v) - 6} text-anchor="middle">{format(v)}</text>
      <text class="label" x={cx} y={height - 8} text-anchor="middle">{label}</text>
    {/each}
  </svg>
  {#if reference}
    <div class="legend"><span><span class="key line" style:background="var(--ink-strong)"></span>{referenceLabel}</span></div>
  {/if}
  <Tooltip x={tip.x} y={tip.y} visible={hover !== null}>
    {#if hover !== null}
      <strong>{format(values[hover])}</strong> <span class="muted">· {labels[hover]}</span>
      {#if reference}<br /><span class="muted">{referenceLabel}: {format(reference[hover])}</span>{/if}
    {/if}
  </Tooltip>
</div>

<style>
  .wrap {
    position: relative;
  }
  svg {
    width: 100%;
    height: auto;
    display: block;
  }
  .baseline {
    stroke: var(--axis);
  }
  .ref {
    stroke: var(--ink-strong);
    stroke-width: 2;
    stroke-linecap: round;
    pointer-events: none;
  }
  .value {
    font: 600 11.5px var(--font-sans);
    fill: var(--ink-strong);
    font-variant-numeric: tabular-nums;
    pointer-events: none;
  }
  .label {
    font: 12px var(--font-sans);
    fill: var(--ink-2);
    pointer-events: none;
  }
  path {
    transition: d 260ms ease, opacity 120ms;
  }
  .legend {
    margin-top: 6px;
  }
</style>
