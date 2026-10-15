<script lang="ts">
  import { tick } from "svelte";
  import PageHero from "../lib/components/PageHero.svelte";
  import { href } from "../lib/router.svelte";
  import Ch1 from "./chapters/Ch1Memoryless.svelte";
  import Ch2 from "./chapters/Ch2Counting.svelte";
  import Ch3 from "./chapters/Ch3Sampling.svelte";
  import Ch4 from "./chapters/Ch4LongRun.svelte";
  import Ch5 from "./chapters/Ch5Absorbing.svelte";
  import Ch6 from "./chapters/Ch6Ngrams.svelte";

  let { chapter }: { chapter: string | null } = $props();

  const chapters = [
    { n: 1, title: "A machine with no memory", blurb: "States, transitions and the Markov property." },
    { n: 2, title: "Counting becomes probability", blurb: "Learning P from text, and why the matrix is 99.9% empty." },
    { n: 3, title: "Rolling a weighted die", blurb: "Cumulative scans, modulo bias and Vose's alias method." },
    { n: 4, title: "The long run", blurb: "Power iteration, stationary distributions, periodicity." },
    { n: 5, title: "Snakes, ladders and absorbing states", blurb: "Canonical form and the fundamental matrix N." },
    { n: 6, title: "Remembering more", blurb: "n-gram states, entropy, and the cost of memory." },
  ];

  $effect(() => {
    const n = chapter;
    if (!n) return;
    const go = (behavior: ScrollBehavior) => document.getElementById(`ch${n}`)?.scrollIntoView({ behavior, block: "start" });
    tick().then(() => go("smooth"));
    // Figures above the target fill in as the engine answers; land again once they settle.
    const settle = setTimeout(() => go("auto"), 900);
    return () => clearTimeout(settle);
  });
</script>

<article>
  <PageHero eyebrow="An explorable explanation" title="Six chapters on a machine that forgets.">
    From the Markov property to the fundamental matrix. Every figure is live: it runs on libmarkov, a C99 engine
    compiled to WebAssembly, and each chapter shows the C that computes it.
  </PageHero>
  <nav class="toc" aria-label="Chapters">
    {#each chapters as c (c.n)}
      <a class="glass" href={href("learn", c.n)}>
        <span class="n">{c.n}</span>
        <span class="t">{c.title}</span>
        <span class="blurb">{c.blurb}</span>
      </a>
    {/each}
  </nav>

  <section id="ch1"><Ch1 /></section>
  <section id="ch2"><Ch2 /></section>
  <section id="ch3"><Ch3 /></section>
  <section id="ch4"><Ch4 /></section>
  <section id="ch5"><Ch5 /></section>
  <section id="ch6"><Ch6 /></section>

  <footer class="prose outro">
    <h2>Where next</h2>
    <p>
      Train a chain on a novel in the <a href={href("text")}>Text Lab</a>, design a board in the
      <a href={href("snakes")}>Snakes &amp; Ladders Studio</a>, test your intuition in
      <a href={href("guess")}>Guess the Chain</a>, or open the engine up in <a href={href("hood")}>Under the Hood</a>.
    </p>
  </footer>
</article>

<style>
  section {
    scroll-margin-top: 64px;
  }
  .toc {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 14px;
    width: min(var(--wide), calc(100vw - 2 * var(--gutter)));
    margin: 4px auto 0;
  }
  .toc a {
    display: grid;
    gap: 4px;
    align-content: start;
    padding: 20px 22px 22px;
    border-radius: 18px;
    color: var(--ink);
    text-decoration: none;
    transition: background-color 0.15s;
  }
  .toc a:hover {
    background: rgb(255 255 255 / 0.92);
  }
  :global(.dark) .toc a:hover {
    background: rgb(58 58 60 / 0.7);
  }
  /* The chapter numeral: this project's signature in the index. */
  .n {
    font-size: 40px;
    font-weight: 600;
    line-height: 1;
    letter-spacing: -0.04em;
    color: var(--accent);
    margin-bottom: 8px;
    font-variant-numeric: tabular-nums;
  }
  .t {
    font-size: 17px;
    font-weight: 600;
    letter-spacing: -0.01em;
  }
  .blurb {
    color: var(--muted);
    font-size: 14px;
    line-height: 1.45;
  }
  .outro {
    margin-top: 48px;
  }
  @media (max-width: 860px) {
    .toc {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
  }
  @media (max-width: 520px) {
    .toc {
      grid-template-columns: 1fr;
    }
  }
</style>
