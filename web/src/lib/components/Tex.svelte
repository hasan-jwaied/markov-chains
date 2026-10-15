<script lang="ts">
  import katex from "katex";

  let { tex, display = false }: { tex: string; display?: boolean } = $props();
  const html = $derived(katex.renderToString(tex, { displayMode: display, throwOnError: false, output: "html" }));
</script>

{#if display}
  <div class="display">{@html html}</div>
{:else}
  <span>{@html html}</span>
{/if}

<style>
  .display {
    overflow-x: auto;
    overflow-y: hidden;
    margin: 1em 0 1.3em;
    padding: 2px 0;
  }
  .display :global(.katex) {
    font-size: 1.12em;
  }
  span :global(.katex) {
    font-size: 1.02em;
  }
</style>
