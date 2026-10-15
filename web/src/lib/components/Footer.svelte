<script lang="ts">
  import { href } from "../router.svelte";
  import { AUTHOR, HOME_URL, REPO_URL } from "../site";
</script>

<footer>
  <a class="back" href={HOME_URL}>
    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m15 18-6-6 6-6" /></svg>
    Back to the portfolio
  </a>
  <p>
    Every number and sentence here is computed live by <strong>libmarkov</strong>, a C99 engine compiled to
    WebAssembly, in a Web Worker on your device. Nothing is sent anywhere.
  </p>
  <p class="links">
    <a href={href("learn")}>Learn</a> · <a href={href("text")}>Text Lab</a> ·
    <a href={href("snakes")}>Snakes &amp; Ladders</a> · <a href={href("guess")}>Guess the Chain</a> ·
    <a href={href("hood")}>Under the Hood</a> · <a href={REPO_URL} target="_blank" rel="noopener noreferrer">Source</a>
  </p>
  <p>Built by {AUTHOR}. Books from Project Gutenberg (public domain).</p>
</footer>

<style>
  footer {
    display: grid;
    gap: 6px;
    max-width: var(--page);
    margin: 96px auto 0;
    padding: 28px max(var(--gutter), env(safe-area-inset-right)) max(48px, calc(env(safe-area-inset-bottom) + 24px)) max(var(--gutter), env(safe-area-inset-left));
    border-top: 1px solid var(--hairline);
    font-size: 14px;
    color: var(--muted);
  }
  p {
    margin: 0;
  }
  strong {
    color: var(--ink-body);
    font-weight: 600;
  }
  .back {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    width: max-content;
    margin-bottom: 8px;
    color: var(--muted);
    text-decoration: none;
  }
  .back:hover {
    color: var(--ink);
  }
  .links a {
    color: var(--muted);
    text-decoration: none;
  }
  .links a:hover {
    color: var(--ink);
    text-decoration: underline;
  }
</style>
