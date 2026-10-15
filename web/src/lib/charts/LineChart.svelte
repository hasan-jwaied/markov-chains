<script lang="ts" module>
  export interface Series {
    name: string;
    values: ArrayLike<number>;
    colorIndex: number;
    area?: boolean;
  }
</script>

<script lang="ts">
  /**
   * Multi-series line chart over an integer x (steps, moves...). Optional
   * bars (a histogram behind the lines), vertical markers, a crosshair that
   * snaps to the nearest x with one tooltip listing every series.
   */
  import { max as d3max, scaleLinear } from "d3";
  import Tooltip from "../components/Tooltip.svelte";
  import { palette } from "./palette.svelte";

  let {
    series,
    bars = null,
    markers = [],
    xLabel = "",
    yFormat = (v: number) => `${(v * 100).toFixed(0)}%`,
    valueFormat = (v: number) => `${(v * 100).toFixed(2)}%`,
    yMax,
    height = 240,
    xStart = 0,
    directLabels = true,
    xFormat = (v: number) => String(v),
  }: {
    series: Series[];
    bars?: { name: string; values: ArrayLike<number>; colorIndex?: number } | null;
    markers?: Array<{ x: number; label: string }>;
    xLabel?: string;
    yFormat?: (v: number) => string;
    valueFormat?: (v: number) => string;
    yMax?: number;
    height?: number;
    xStart?: number;
    directLabels?: boolean;
    /** Label for an x position (tick labels and the tooltip header). */
    xFormat?: (v: number) => string;
  } = $props();

  let width = $state(640);
  const W = $derived(Math.max(300, width));
  const pad = $derived({ top: 14, right: directLabels && series.length <= 4 ? 78 : 24, bottom: 34, left: 44 });
  const len = $derived(Math.max(bars?.values.length ?? 0, ...series.map((s) => s.values.length)));
  const top = $derived(
    yMax ?? (d3max([...series.flatMap((s) => Array.from(s.values)), ...(bars ? Array.from(bars.values) : [])]) || 1) * 1.08,
  );
  const x = $derived(scaleLinear().domain([xStart, xStart + Math.max(1, len - 1)]).range([pad.left, W - pad.right]));
  const y = $derived(scaleLinear().domain([0, top]).range([height - pad.bottom, pad.top]).nice());
  const yTicks = $derived(y.ticks(4));
  const xTicks = $derived(
    len <= 12 ? Array.from({ length: len }, (_, i) => xStart + i) : x.ticks(Math.min(8, len)).filter((t) => Number.isInteger(t)),
  );

  function line(values: ArrayLike<number>): string {
    let d = "";
    for (let i = 0; i < values.length; i++) d += `${i ? "L" : "M"}${x(xStart + i).toFixed(2)},${y(values[i]).toFixed(2)}`;
    return d;
  }
  function area(values: ArrayLike<number>): string {
    if (!values.length) return "";
    return `${line(values)}L${x(xStart + values.length - 1)},${y(0)}L${x(xStart)},${y(0)}Z`;
  }

  const barW = $derived(Math.max(1, Math.min(24, (W - pad.left - pad.right) / Math.max(1, len) - 2)));

  let hoverIndex = $state<number | null>(null);
  let wrap: HTMLDivElement;
  let svg: SVGSVGElement;
  let tip = $state({ x: 0, y: 0 });

  function move(ev: PointerEvent) {
    const box = svg.getBoundingClientRect();
    const sx = ((ev.clientX - box.left) / box.width) * W;
    const i = Math.round(x.invert(sx) - xStart);
    if (i < 0 || i >= len) {
      hoverIndex = null;
      return;
    }
    hoverIndex = i;
    const wb = wrap.getBoundingClientRect();
    tip = { x: ev.clientX - wb.left, y: ev.clientY - wb.top };
  }
</script>

<div class="wrap" bind:this={wrap} bind:clientWidth={width}>
  {#if series.length + (bars ? 1 : 0) >= 2}
    <div class="legend">
      {#if bars}<span><span class="key" style:background={palette.value.muted}></span>{bars.name}</span>{/if}
      {#each series as s (s.name)}
        <span><span class="key line" style:background={palette.value.series[s.colorIndex % 8]}></span>{s.name}</span>
      {/each}
    </div>
  {/if}
  <svg
    bind:this={svg}
    viewBox="0 0 {W} {height}"
    role="img"
    aria-label="Line chart{xLabel ? ` against ${xLabel}` : ''}"
    onpointermove={move}
    onpointerleave={() => (hoverIndex = null)}
  >
    <g class="grid">
      {#each yTicks as t (t)}
        <line x1={pad.left} x2={W - pad.right} y1={y(t)} y2={y(t)} />
      {/each}
    </g>
    <g class="axis">
      {#each yTicks as t (t)}
        <text x={pad.left - 8} y={y(t) + 4} text-anchor="end">{yFormat(t)}</text>
      {/each}
      {#each xTicks as t (t)}
        <text x={x(t)} y={height - pad.bottom + 16} text-anchor="middle">{xFormat(t)}</text>
      {/each}
      <line x1={pad.left} x2={W - pad.right} y1={y(0)} y2={y(0)} />
      {#if xLabel}<text x={(pad.left + W - pad.right) / 2} y={height - 3} text-anchor="middle">{xLabel}</text>{/if}
    </g>

    {#if bars}
      {#each Array.from(bars.values) as v, i (i)}
        {#if v > 0}
          <rect
            x={x(xStart + i) - barW / 2}
            y={y(v)}
            width={barW}
            height={Math.max(0, y(0) - y(v))}
            fill={bars.colorIndex === undefined ? palette.value.muted : palette.value.series[bars.colorIndex]}
            opacity="0.45"
          />
        {/if}
      {/each}
    {/if}

    {#each series as s (s.name)}
      {@const c = palette.value.series[s.colorIndex % 8]}
      {#if s.area}<path d={area(s.values)} fill={c} opacity="0.1" />{/if}
      <path d={line(s.values)} fill="none" stroke={c} stroke-width="2" stroke-linejoin="round" stroke-linecap="round" />
      {#if directLabels && series.length <= 4 && s.values.length}
        {@const last = s.values.length - 1}
        <circle cx={x(xStart + last)} cy={y(s.values[last])} r="4" fill={c} stroke={palette.value.surface} stroke-width="2" />
        <text class="end" x={x(xStart + last) + 8} y={y(s.values[last]) + 4}>{s.name}</text>
      {/if}
    {/each}

    {#each markers as m, i (m.label)}
      <g class="marker">
        <line x1={x(m.x)} x2={x(m.x)} y1={pad.top} y2={y(0)} />
        <!-- stagger labels so neighbouring markers never collide -->
        <text x={x(m.x) + 5} y={pad.top + 10 + i * 15}>{m.label}</text>
      </g>
    {/each}

    {#if hoverIndex !== null}
      <line class="crosshair" x1={x(xStart + hoverIndex)} x2={x(xStart + hoverIndex)} y1={pad.top} y2={y(0)} />
      {#each series as s (s.name)}
        {#if hoverIndex < s.values.length}
          <circle cx={x(xStart + hoverIndex)} cy={y(s.values[hoverIndex])} r="4" fill={palette.value.series[s.colorIndex % 8]} stroke={palette.value.surface} stroke-width="2" />
        {/if}
      {/each}
    {/if}
  </svg>
  <Tooltip x={tip.x} y={tip.y} visible={hoverIndex !== null}>
    {#if hoverIndex !== null}
      <div class="muted">{xLabel || "x"}: {xFormat(xStart + hoverIndex)}</div>
      {#each series as s (s.name)}
        {#if hoverIndex < s.values.length}
          <div><span class="tkey" style:background={palette.value.series[s.colorIndex % 8]}></span><strong>{valueFormat(s.values[hoverIndex])}</strong> <span class="muted">{s.name}</span></div>
        {/if}
      {/each}
      {#if bars && hoverIndex < bars.values.length}
        <div><span class="tkey" style:background={palette.value.muted}></span><strong>{valueFormat(bars.values[hoverIndex])}</strong> <span class="muted">{bars.name}</span></div>
      {/if}
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
    touch-action: pan-y;
  }
  .end {
    font: 600 11.5px var(--font-sans);
    fill: var(--ink-2);
  }
  .crosshair {
    stroke: var(--ink-2);
    stroke-width: 1;
  }
  .marker line {
    stroke: var(--ink-2);
    stroke-width: 1;
    stroke-dasharray: 3 3;
  }
  .marker text {
    font: 600 11px var(--font-sans);
    fill: var(--ink-2);
  }
  .legend {
    margin-bottom: 6px;
  }
  .tkey {
    display: inline-block;
    width: 12px;
    height: 2px;
    margin-right: 6px;
    vertical-align: 3px;
  }
</style>
