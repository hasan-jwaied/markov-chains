<script lang="ts">
  import type { Snippet } from "svelte";

  /** Positioned relative to the nearest positioned ancestor. */
  let { x, y, visible, children }: { x: number; y: number; visible: boolean; children: Snippet } = $props();
</script>

{#if visible}
  <div class="tip" style:left="{x}px" style:top="{y}px" role="status">
    {@render children()}
  </div>
{/if}

<style>
  .tip {
    position: absolute;
    transform: translate(-50%, calc(-100% - 10px));
    pointer-events: none;
    z-index: 20;
    background: var(--surface);
    color: var(--ink);
    box-shadow: var(--shadow-pop);
    border-radius: 12px;
    padding: 8px 12px;
    font: 12.5px/1.45 var(--font-sans);
    white-space: nowrap;
    font-variant-numeric: tabular-nums;
  }
  .tip :global(strong) {
    color: var(--ink-strong);
    font-weight: 650;
  }
</style>
