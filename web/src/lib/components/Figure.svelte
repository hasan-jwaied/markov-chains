<script lang="ts">
  import type { Snippet } from "svelte";

  let {
    caption,
    wide = false,
    controls,
    children,
    label,
  }: { caption?: string; wide?: boolean; controls?: Snippet; children: Snippet; label?: string } = $props();
</script>

<figure class:wide aria-label={label}>
  <div class="card body">
    {#if controls}
      <div class="figure-controls controls">{@render controls()}</div>
    {/if}
    {@render children()}
  </div>
  {#if caption}
    <figcaption>{caption}</figcaption>
  {/if}
</figure>

<style>
  figure {
    margin: 2.2em 0 2.4em;
    font-family: var(--font-sans);
    font-size: 15px;
    line-height: 1.5;
    color: var(--ink);
  }
  figure:not(.wide) {
    margin-left: -12px;
    margin-right: -12px;
  }
  /* Break out of the reading column, centred on it. */
  figure.wide {
    --w: min(var(--wide), calc(100vw - 2 * var(--gutter)));
    width: var(--w);
    margin-left: calc((100% - var(--w)) / 2);
    margin-right: 0;
  }
  .body {
    padding: 22px;
    overflow: hidden;
  }
  .figure-controls {
    padding-bottom: 16px;
    margin-bottom: 16px;
    border-bottom: 1px solid var(--hairline);
  }
  figcaption {
    font-size: 13.5px;
    color: var(--muted);
    margin: 12px 6px 0;
    line-height: 1.5;
  }
  @media (max-width: 640px) {
    .body {
      padding: 14px;
    }
    figure:not(.wide) {
      margin-left: 0;
      margin-right: 0;
    }
  }
</style>
