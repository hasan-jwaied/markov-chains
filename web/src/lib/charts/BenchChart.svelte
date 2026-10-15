<script lang="ts">
  /**
   * Build time per corpus, original engine vs libmarkov, on a log axis:
   * two thin bars per row (one per engine), values at the bar ends, a legend,
   * and a hover tooltip with the exact numbers and the speedup.
   */
  import { scaleLog } from "d3";
  import Tooltip from "../components/Tooltip.svelte";
  import { BUILD, fmtSeconds } from "../benchmarks";
  import { palette } from "./palette.svelte";

  const rows = BUILD.filter((r) => r.originalS !== null);
  let width = $state(640);
  const W = $derived(Math.max(320, width));
  const labelW = $derived(W < 520 ? 118 : 190);
  const rowH = 46;
  const top = 8;
  const H = $derived(top + rows.length * rowH + 30);
  const x = $derived(scaleLog().domain([0.001, 300]).range([labelW, W - 64]).clamp(true));
  const ticks = [0.001, 0.01, 0.1, 1, 10, 100];
  const tickLabel = (t: number) => (t < 1 ? `${Math.round(t * 1000)} ms` : `${t} s`);

  let hover = $state<number | null>(null);
  let wrap: HTMLDivElement;
  let tip = $state({ x: 0, y: 0 });
</script>

<div class="wrap" bind:this={wrap} bind:clientWidth={width}>
  <div class="legend">
    <span><span class="key" style:background={palette.value.muted}></span>Original assignment engine</span>
    <span><span class="key" style:background={palette.value.series[0]}></span>libmarkov</span>
  </div>
  <svg viewBox="0 0 {W} {H}" role="img" aria-label="Time to learn each corpus, original engine versus libmarkov, log scale">
    <g class="grid">
      {#each ticks as t (t)}<line x1={x(t)} x2={x(t)} y1={top} y2={H - 26} />{/each}
    </g>
    <g class="axis">
      {#each ticks as t (t)}<text x={x(t)} y={H - 8} text-anchor="middle">{tickLabel(t)}</text>{/each}
    </g>
    {#each rows as r, i (r.corpus)}
      {@const y = top + i * rowH}
      <g
        role="presentation"
        onpointermove={(ev) => {
          const b = wrap.getBoundingClientRect();
          tip = { x: ev.clientX - b.left, y: ev.clientY - b.top };
          hover = i;
        }}
        onpointerleave={() => (hover = null)}
      >
        <rect x="0" y={y} width={W} height={rowH} fill="transparent" />
        <text class="name" x={labelW - 12} y={y + rowH / 2 + 4} text-anchor="end">{r.corpus}</text>
        <rect x={labelW} y={y + 9} width={Math.max(2, x(r.originalS ?? 0) - labelW)} height="10" rx="3" fill={palette.value.muted} opacity={hover === null || hover === i ? 0.55 : 0.25} />
        <rect x={labelW} y={y + 23} width={Math.max(2, x(r.libmarkovS) - labelW)} height="10" rx="3" fill={palette.value.series[0]} opacity={hover === null || hover === i ? 1 : 0.4} />
        <text class="val" x={x(r.originalS ?? 0) + 6} y={y + 18}>{fmtSeconds(r.originalS ?? 0)}</text>
        <text class="val strong" x={x(r.libmarkovS) + 6} y={y + 32}>{fmtSeconds(r.libmarkovS)}</text>
      </g>
    {/each}
  </svg>
  <Tooltip x={tip.x} y={tip.y} visible={hover !== null}>
    {#if hover !== null}
      {@const r = rows[hover]}
      <strong>{Math.round((r.originalS ?? 0) / r.libmarkovS).toLocaleString("en-US")}× faster</strong>
      <span class="muted"> · {r.corpus}</span><br />
      <span class="muted">{r.words.toLocaleString("en-US")} words: {fmtSeconds(r.originalS ?? 0)} → {fmtSeconds(r.libmarkovS)}</span>
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
    overflow: visible;
  }
  .legend {
    margin-bottom: 10px;
  }
  .name {
    font: 500 13px var(--font-sans);
    fill: var(--ink-body);
  }
  .val {
    font: 12px var(--font-sans);
    fill: var(--muted);
    font-variant-numeric: tabular-nums;
  }
  .val.strong {
    fill: var(--ink);
    font-weight: 600;
  }
</style>
