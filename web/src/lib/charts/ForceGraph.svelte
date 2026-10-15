<script lang="ts">
  /**
   * The neighbourhood of one state: its likeliest successors and theirs,
   * laid out with d3-force (run to rest synchronously, then drawn).
   */
  import { forceCollide, forceLink, forceManyBody, forceSimulation, forceX, forceY, type SimulationNodeDatum } from "d3";
  import type { Neighborhood } from "../engine/types";
  import { palette } from "./palette.svelte";

  let {
    graph,
    center,
    height = 360,
    labelOf = (s: string) => s,
    onselect,
  }: {
    graph: Neighborhood;
    center: number;
    height?: number;
    labelOf?: (label: string) => string;
    onselect?: (state: number) => void;
  } = $props();

  interface N extends SimulationNodeDatum {
    id: number;
    label: string;
    depth: number;
    terminal: boolean;
  }
  interface L {
    source: N;
    target: N;
    p: number;
  }

  let width = $state(600);

  const layout = $derived.by(() => {
    const W = Math.max(320, width);
    const nodes: N[] = graph.nodes.map((n) => ({ ...n, label: labelOf(n.label), x: W / 2 + (n.depth ? (Math.random() - 0.5) * 50 : 0), y: height / 2 }));
    const byId = new Map(nodes.map((n) => [n.id, n]));
    const links: L[] = graph.links
      .filter((l) => byId.has(l.source) && byId.has(l.target) && l.source !== l.target)
      .map((l) => ({ source: byId.get(l.source)!, target: byId.get(l.target)!, p: l.p }));
    const c = byId.get(center);
    if (c) {
      c.fx = W * 0.32;
      c.fy = height / 2;
    }
    const sim = forceSimulation(nodes)
      .force("link", forceLink<N, L>(links).distance((l) => (l.source.depth === 0 ? 120 : 80)).strength(0.6))
      .force("charge", forceManyBody().strength(-260))
      .force("collide", forceCollide<N>().radius((d) => 10 + Math.min(70, d.label.length * 3.6)))
      .force("x", forceX<N>((d) => W * (0.32 + 0.3 * d.depth)).strength(0.25))
      .force("y", forceY(height / 2).strength(0.06))
      .stop();
    for (let i = 0; i < 260; i++) sim.tick();
    for (const n of nodes) {
      n.x = Math.max(40, Math.min(W - 40, n.x ?? 0));
      n.y = Math.max(18, Math.min(height - 18, n.y ?? 0));
    }
    return { nodes, links, W };
  });

  function path(l: L): string {
    const sx = l.source.x ?? 0;
    const sy = l.source.y ?? 0;
    const tx = l.target.x ?? 0;
    const ty = l.target.y ?? 0;
    const mx = (sx + tx) / 2;
    return `M${sx},${sy} C${mx},${sy} ${mx},${ty} ${tx},${ty}`;
  }
</script>

<div bind:clientWidth={width}>
  <svg viewBox="0 0 {layout.W} {height}" role="img" aria-label="Likeliest continuations of the current state">
    {#each layout.links as l, i (i)}
      <path d={path(l)} fill="none" stroke={l.source.depth === 0 ? palette.value.accent : palette.value.axis} stroke-width={1 + 5 * l.p} stroke-opacity={l.source.depth === 0 ? 0.55 : 0.8} />
    {/each}
    {#snippet pill(n: N, isCenter: boolean)}
      <rect
        x={-(n.label.length * 3.6 + 10)}
        y="-12"
        width={n.label.length * 7.2 + 20}
        height="24"
        rx="12"
        fill={isCenter ? palette.value.accent : palette.value.surface}
        stroke={isCenter ? "none" : n.depth === 1 ? palette.value.accent : palette.value.axis}
        stroke-width="1.5"
      />
      <text text-anchor="middle" dy="4" class:center={isCenter} class:far={n.depth === 2}>{n.label}</text>
    {/snippet}
    {#each layout.nodes as n (n.id)}
      {#if onselect && n.depth === 1}
        <g
          class="node clickable"
          transform="translate({n.x},{n.y})"
          role="button"
          tabindex="0"
          aria-label="Choose {n.label}"
          onclick={() => onselect(n.id)}
          onkeydown={(e) => (e.key === "Enter" || e.key === " ") && onselect(n.id)}
        >
          {@render pill(n, false)}
        </g>
      {:else}
        <g class="node" transform="translate({n.x},{n.y})">{@render pill(n, n.id === center)}</g>
      {/if}
    {/each}
  </svg>
</div>

<style>
  svg {
    width: 100%;
    height: auto;
    display: block;
  }
  text {
    font: 600 12px var(--font-sans);
    fill: var(--ink-strong);
    pointer-events: none;
  }
  text.center {
    fill: #fff;
  }
  text.far {
    font-weight: 500;
    fill: var(--ink-2);
  }
  .clickable {
    cursor: pointer;
  }
  .clickable:hover rect {
    fill: var(--accent-wash);
  }
</style>
