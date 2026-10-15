<script lang="ts">
  import type { Snippet } from "svelte";

  /** The centred page opening shared with Pixel Lab: eyebrow, headline, lede. */
  let {
    eyebrow,
    title,
    children,
    actions,
    compact = false,
  }: { eyebrow: string; title: string; children?: Snippet; actions?: Snippet; compact?: boolean } = $props();
</script>

<header class="hero" class:compact>
  <p class="eyebrow">{eyebrow}</p>
  <h1>{title}</h1>
  {#if children}<p class="body">{@render children()}</p>{/if}
  {#if actions}<div class="actions">{@render actions()}</div>{/if}
</header>

<style>
  .hero {
    max-width: var(--page);
    margin: 0 auto;
    padding: 72px var(--gutter) 40px;
    text-align: center;
  }
  .hero.compact {
    padding-top: 56px;
    padding-bottom: 28px;
  }
  .eyebrow {
    margin: 0;
    font-size: 17px;
    font-weight: 600;
    color: var(--muted);
  }
  h1 {
    margin: 10px 0 0;
    font-size: clamp(36px, 5.6vw, 64px);
    line-height: 1.05;
    letter-spacing: -0.03em;
  }
  .compact h1 {
    font-size: clamp(34px, 4.6vw, 52px);
  }
  .body {
    max-width: 740px;
    margin: 18px auto 0;
    color: var(--muted);
    font-size: clamp(17px, 1.7vw, 21px);
    line-height: 1.45;
  }
  .actions {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 10px;
    margin-top: 28px;
  }
  @media (max-width: 640px) {
    .hero {
      padding-top: 40px;
      padding-bottom: 24px;
    }
  }
</style>
