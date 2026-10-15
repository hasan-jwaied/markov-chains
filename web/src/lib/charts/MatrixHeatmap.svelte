<script lang="ts">
  /**
   * A matrix as a heat map (sequential blue), e.g. the transition matrix P
   * or the fundamental matrix N. Cells are separated by a 2px surface gap;
   * small matrices print their values, every cell has a hover tooltip.
   */
  import Tooltip from "../components/Tooltip.svelte";
  import { inkOn, palette } from "./palette.svelte";

  let {
    rows,
    cols = rows,
    values,
    max,
    format = (v: number) => v.toFixed(2),
    describe = (r: string, c: string, v: string) => `${r} → ${c}: ${v}`,
    highlightRow = null,
    cell = 44,
    showValues,
    rowTitle,
    colTitle,
  }: {
    rows: string[];
    cols?: string[];
    values: ArrayLike<number>;
    max?: number;
    format?: (v: number) => string;
    describe?: (row: string, col: string, value: string) => string;
    highlightRow?: number | null;
    cell?: number;
    showValues?: boolean;
    rowTitle?: string;
    colTitle?: string;
  } = $props();

  const nr = $derived(rows.length);
  const nc = $derived(cols.length);
  const vmax = $derived(max ?? Math.max(1e-12, ...Array.from(values)));
  const labelW = $derived(Math.min(110, 12 + 7.2 * Math.max(...rows.map((r) => r.length))));
  const headH = $derived(nc <= 14 ? 26 : 8);
  const printValues = $derived(showValues ?? (cell >= 34 && nc <= 14));
  let hover = $state<{ r: number; c: number } | null>(null);
  let wrap: HTMLDivElement;
  let tip = $state({ x: 0, y: 0 });

  function move(ev: PointerEvent, r: number, c: number) {
    const box = wrap.getBoundingClientRect();
    tip = { x: ev.clientX - box.left, y: ev.clientY - box.top };
    hover = { r, c };
  }
</script>

<div class="wrap" bind:this={wrap}>
  {#if colTitle}<div class="ctitle" style:padding-left="{labelW}px">{colTitle}</div>{/if}
  <svg
    viewBox="0 0 {labelW + nc * cell} {headH + nr * cell}"
    style:max-width="{labelW + nc * cell}px"
    role="img"
    aria-label="Matrix heat map with {nr} rows and {nc} columns"
    onpointerleave={() => (hover = null)}
  >
    {#if headH > 10}
      {#each cols as c, j (j)}
        <text class="head" class:on={hover?.c === j} x={labelW + j * cell + cell / 2} y={headH - 9} text-anchor="middle">{c}</text>
      {/each}
    {/if}
    {#each rows as r, i (i)}
      {#if nr <= 40}
        <text
          class="head"
          class:on={hover?.r === i || highlightRow === i}
          x={labelW - 8}
          y={headH + i * cell + cell / 2 + 4}
          text-anchor="end">{r}</text
        >
      {/if}
      {#each cols as c, j (j)}
        {@const v = values[i * nc + j]}
        {@const fill = v === 0 ? palette.value.surface : palette.value.seq(0.12 + 0.88 * (v / vmax))}
        <g>
          <rect
            x={labelW + j * cell + 1}
            y={headH + i * cell + 1}
            width={cell - 2}
            height={cell - 2}
            rx={cell > 16 ? 4 : 1}
            {fill}
            stroke={v === 0 ? palette.value.hairline : "none"}
            class:dim={highlightRow !== null && highlightRow !== i}
            class:hovered={hover?.r === i && hover?.c === j}
            role="presentation"
            onpointermove={(ev) => move(ev, i, j)}
            aria-label={describe(r, c, format(v))}
          />
          {#if printValues && v !== 0}
            <text
              class="val"
              x={labelW + j * cell + cell / 2}
              y={headH + i * cell + cell / 2 + 4}
              text-anchor="middle"
              fill={inkOn(fill)}>{format(v)}</text
            >
          {/if}
        </g>
      {/each}
    {/each}
  </svg>
  {#if rowTitle}<div class="rtitle">{rowTitle}</div>{/if}
  <Tooltip x={tip.x} y={tip.y} visible={hover !== null}>
    {#if hover}
      <strong>{format(values[hover.r * nc + hover.c])}</strong>
      <span class="muted"> · {describe(rows[hover.r], cols[hover.c], "").replace(/:\s*$/, "")}</span>
    {/if}
  </Tooltip>
</div>

<style>
  .wrap {
    position: relative;
    overflow-x: auto;
  }
  svg {
    width: 100%;
    height: auto;
    display: block;
    margin: 0 auto;
  }
  .head {
    font: 600 11.5px var(--font-sans);
    fill: var(--ink-2);
  }
  .head.on {
    fill: var(--accent-ink);
  }
  .val {
    font: 600 11px var(--font-sans);
    font-variant-numeric: tabular-nums;
    pointer-events: none;
  }
  rect {
    transition: opacity 150ms;
  }
  rect.dim {
    opacity: 0.35;
  }
  rect.hovered {
    stroke: var(--ink-strong);
    stroke-width: 1.5;
  }
  .ctitle,
  .rtitle {
    font: 12px var(--font-sans);
    color: var(--muted);
  }
  .ctitle {
    margin-bottom: 2px;
  }
</style>
