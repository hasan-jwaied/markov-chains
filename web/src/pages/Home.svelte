<script lang="ts">
  import BenchChart from "../lib/charts/BenchChart.svelte";
  import ChainGraph from "../lib/charts/ChainGraph.svelte";
  import { palette } from "../lib/charts/palette.svelte";
  import PageHero from "../lib/components/PageHero.svelte";
  import { fmtInt, loadCorpus } from "../lib/corpora";
  import { engine } from "../lib/engine/client.svelte";
  import { engineModel } from "../lib/engine/model.svelte";
  import { href } from "../lib/router.svelte";
  import { REPO_URL } from "../lib/site";

  // a novel, re-written live by the engine
  let corpusText = $state<string | null>(null);
  loadCorpus("sherlock-holmes").then((t) => (corpusText = t)).catch(() => {});
  const book = engineModel(() => (corpusText ? engine.buildText(corpusText, { order: 2, rule: "sentence" }) : null));

  let shown = $state("");
  let typing = $state(false);
  let next = 0;
  async function typeSentence(id: number, alive: () => boolean) {
    const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    for (let tries = 0; tries < 20; tries++) {
      const [g] = await engine.generate(id, { seed: Math.floor(Math.random() * 1e9), maxLength: 28 });
      if (!g || g.text.length < 40 || g.text.length > 160) continue;
      typing = true;
      if (reduce) shown = g.text;
      else {
        for (let i = 1; i <= g.text.length && alive(); i++) {
          shown = g.text.slice(0, i);
          await new Promise((r) => setTimeout(r, 20));
        }
      }
      typing = false;
      return;
    }
  }
  $effect(() => {
    const id = book.id;
    if (id === null) return;
    let alive = true;
    const ticket = ++next;
    const live = () => alive && ticket === next;
    (async () => {
      while (live()) {
        await typeSentence(id, live);
        await new Promise((r) => setTimeout(r, 3400));
      }
    })().catch(() => {});
    return () => {
      alive = false;
    };
  });

  // a three-state chain, stepping
  const labels = ["Sunny", "Cloudy", "Rainy"];
  const P = [0.7, 0.2, 0.1, 0.3, 0.4, 0.3, 0.2, 0.4, 0.4];
  const weather = engineModel(() => engine.buildCustom({ labels, weights: P.map((p) => p * 1000) }));
  let current = $state(0);
  let previous = $state<number | null>(null);
  let stepKey = $state(0);
  let days = $state<number[]>([0]);
  $effect(() => {
    const id = weather.id;
    if (id === null) return;
    let seed = 1;
    const timer = setInterval(async () => {
      const n = await engine.step(id, current, seed++).catch(() => null);
      if (n === null) return;
      previous = current;
      current = n;
      stepKey++;
      days = [...days.slice(-27), n];
    }, 1500);
    return () => clearInterval(timer);
  });

  const destinations = [
    { page: "learn" as const, tag: "6 chapters", title: "Learn", body: "Transition matrices, sampling, stationary distributions, absorbing chains and n-grams, each beside the C that computes it." },
    { page: "text" as const, tag: "Lab", title: "Text Lab", body: "Learn a whole novel in milliseconds, turn the temperature, or steer the chain one word at a time." },
    { page: "snakes" as const, tag: "Lab", title: "Snakes & Ladders", body: "Edit a board and watch the exact expected game length, from the fundamental matrix, update as you click." },
    { page: "guess" as const, tag: "Game", title: "Guess the Chain", body: "Infer a hidden transition matrix from what it emits, then race the statistician's estimate." },
    { page: "hood" as const, tag: "Systems", title: "Under the Hood", body: "Function pointers as WebAssembly table indices, struct padding byte by byte, and a live allocator." },
  ];
</script>

<div class="home">
  <PageHero eyebrow="Markov chains from first principles" title="A machine that forgets, explained down to the byte.">
    A Markov chain engine written in C99 and compiled to WebAssembly. Learn the mathematics, write with a novel's
    statistics, solve Snakes &amp; Ladders exactly, and watch the memory it uses. Everything runs on your device.
    {#snippet actions()}
      <a class="btn primary big" href={href("learn")}>Start reading</a>
      <a class="btn big" href={href("text")}>Open the Text Lab</a>
    {/snippet}
  </PageHero>

  <!-- The live showcase: this project's signature, where Pixel Lab has its image workspace. -->
  <section class="showcase glass" aria-label="The engine, running">
    <div class="pane graph-pane">
      <p class="pane-label"><span class="dot" aria-hidden="true"></span>A three-state chain, stepping every 1.5 s</p>
      <div class="graph">
        <ChainGraph {labels} {P} active={current} {previous} {stepKey} positions={[[260, 70], [110, 270], [410, 270]]} />
      </div>
      <div class="days" aria-label="Recent states">
        {#each days as d, i (i)}<span class="day" style:background={palette.value.series[d]} title={labels[d]}></span>{/each}
      </div>
    </div>
    <div class="pane text-pane">
      <p class="pane-label"><span class="dot" aria-hidden="true"></span>A novel, rewritten word by word</p>
      <p class="sentence" aria-live="polite">{shown}<span class="caret" class:still={!typing} aria-hidden="true"></span></p>
      <p class="source">
        <em>The Adventures of Sherlock Holmes</em>, learned as an order-2 chain
        {#if book.info}({fmtInt(book.info.states)} states, built in {book.info.buildMs.toFixed(0)} ms){/if}
      </p>
    </div>
  </section>

  <section class="explore" aria-label="Explore">
    {#each destinations as d (d.page)}
      <a class="dest glass" href={href(d.page)}>
        <span class="dest-tag">{d.tag}</span>
        <span class="dest-title">{d.title}</span>
        <span class="dest-body">{d.body}</span>
        <span class="dest-go" aria-hidden="true">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m9 18 6-6-6-6" /></svg>
        </span>
      </a>
    {/each}
  </section>

  <article class="article" id="how">
    <header class="article-head">
      <h2>How it's built</h2>
      <p class="lead">
        It began as two university exercises in C: a tweet generator, and a generic engine that also played Snakes
        &amp; Ladders. Here is how it became fast, exact and explainable.
      </p>
    </header>

    <ul class="highlights" role="list">
      <li class="highlight glass"><div class="hv">2.51 s → 12.6 ms</div><div class="hl">to learn the original tweet corpus</div></li>
      <li class="highlight glass"><div class="hv">133×</div><div class="hl">faster sampling with alias tables</div></li>
      <li class="highlight glass"><div class="hv">18 / 18</div><div class="hl">golden outputs identical in WebAssembly</div></li>
      <li class="highlight glass"><div class="hv">0</div><div class="hl">leaks, even when every allocation fails</div></li>
    </ul>

    <section class="case">
      <p class="case-label">Performance</p>
      <div class="case-body">
        <h3>The lookup that took 18 seconds</h3>
        <p>
          The original engine found every word with a linear scan of a linked list, so learning a corpus cost
          O(words × vocabulary): 18 seconds for <em>Moby-Dick</em>. Open-addressing hash tables for states and for
          transitions make each word O(1). The same book now takes 47 ms, and the gap grows with the corpus.
        </p>
        <div class="figure glass"><BenchChart /></div>
        <p class="caption">Native build, same compiler flags, log scale. Hover a row for the speedup.</p>
      </div>
    </section>

    <section class="case">
      <p class="case-label">Verification</p>
      <div class="case-body">
        <h3>Byte for byte, in two runtimes</h3>
        <p>
          The new engine had to reproduce the original program exactly, including its random choices. That meant
          re-implementing glibc's <code>rand()</code> bit for bit, because the browser's C library (musl) would
          otherwise produce different tweets. Output recorded from the untouched original now matches the native
          build and the WebAssembly build alike.
        </p>
      </div>
    </section>

    <section class="case">
      <p class="case-label">Architecture</p>
      <div class="case-body">
        <h3>One engine, four targets</h3>
        <p>
          The same C sources build the command-line tools, the sanitizer and Valgrind test builds, the benchmarks,
          and a 55 KB WebAssembly module that runs in a Web Worker behind a typed interface.
        </p>
        <div class="flow glass" aria-label="Build pipeline">
          <div class="lane"><span class="node">libmarkov (C99)</span><span class="arrow"></span><span class="node file">wasm/api.c</span><span class="arrow"></span><span class="node">Emscripten</span><span class="arrow"></span><span class="node">Web Worker</span><span class="arrow"></span><span class="node accent">This page</span></div>
          <div class="lane"><span class="node">libmarkov (C99)</span><span class="arrow"></span><span class="node">CLIs · tests · benchmarks</span><span class="arrow"></span><span class="node accent">ASan · UBSan · Valgrind</span></div>
        </div>
      </div>
    </section>

    <section class="case">
      <p class="case-label">Memory</p>
      <div class="case-body">
        <h3>Zero leaks, checked at every allocation</h3>
        <p>
          Every byte goes through a tracked allocator. The tests fail if anything is still allocated after a test,
          and a fault-injection sweep makes each allocation in a full pipeline fail in turn, then checks that the
          engine reports the error and frees everything. <a href={href("hood")}>Watch it happen in Under the Hood.</a>
        </p>
      </div>
    </section>

    <section class="case">
      <p class="case-label">Trust</p>
      <div class="case-body">
        <h3>Built to be checked</h3>
        <ul class="checks" role="list">
          <li>Private: everything runs on your device; nothing is uploaded.</li>
          <li>Strict: gcc and clang with every warning as an error.</li>
          <li>Tested: unit, golden, fuzz, sanitizer and Valgrind runs on every change.</li>
          <li>Exact: the maths is checked against closed forms and an independent solver.</li>
        </ul>
      </div>
    </section>

    <section class="cta glass">
      <h2>Start with a machine that forgets</h2>
      <p>Six short chapters, from the Markov property to the fundamental matrix, each beside the C that computes it.</p>
      <div class="cta-actions">
        <a class="btn primary big" href={href("learn")}>Start reading</a>
        <a class="btn big" href={REPO_URL} target="_blank" rel="noopener noreferrer">View the source</a>
      </div>
    </section>
  </article>
</div>

<style>
  .home {
    padding-bottom: 8px;
  }
  .showcase {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    width: min(var(--wide), calc(100vw - 2 * var(--gutter)));
    margin: 8px auto 0;
    overflow: hidden;
  }
  .pane {
    padding: 22px 26px 26px;
    min-width: 0;
  }
  .graph-pane {
    border-right: 1px solid var(--hairline);
  }
  .pane-label {
    display: flex;
    align-items: center;
    gap: 8px;
    margin: 0;
    font-size: 13px;
    font-weight: 500;
    color: var(--muted);
  }
  .dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--mint);
    box-shadow: 0 0 0 3px rgb(0 199 190 / 0.18);
  }
  .graph {
    max-width: 400px;
    margin: 8px auto 0;
  }
  .days {
    display: flex;
    justify-content: center;
    gap: 3px;
    margin-top: 10px;
    min-height: 10px;
  }
  .day {
    width: 10px;
    height: 10px;
    border-radius: 3px;
  }
  .text-pane {
    display: flex;
    flex-direction: column;
  }
  .sentence {
    flex: 1;
    margin: 18px 0;
    font-family: var(--font-serif);
    font-size: clamp(22px, 2.5vw, 30px);
    line-height: 1.35;
    color: var(--ink-strong);
    letter-spacing: -0.01em;
    min-height: 4.2em;
  }
  .caret {
    display: inline-block;
    width: 2px;
    height: 1em;
    margin-left: 3px;
    vertical-align: -0.12em;
    background: var(--accent);
  }
  .caret.still {
    animation: blink 1.1s steps(2) infinite;
  }
  @keyframes blink {
    50% {
      opacity: 0;
    }
  }
  .source {
    margin: 0;
    font-size: 13.5px;
    color: var(--muted);
  }
  .explore {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(196px, 1fr));
    gap: 14px;
    width: min(var(--wide), calc(100vw - 2 * var(--gutter)));
    margin: 28px auto 0;
  }
  .dest {
    position: relative;
    display: flex;
    flex-direction: column;
    gap: 6px;
    padding: 20px 20px 50px;
    border-radius: 18px;
    color: var(--ink);
    text-decoration: none;
    transition: background-color 0.15s;
  }
  .dest:hover {
    background: rgb(255 255 255 / 0.92);
  }
  :global(.dark) .dest:hover {
    background: rgb(58 58 60 / 0.7);
  }
  .dest-tag {
    font-size: 12.5px;
    font-weight: 600;
    color: var(--accent-ink);
  }
  .dest-title {
    font-size: 19px;
    font-weight: 600;
    letter-spacing: -0.015em;
  }
  .dest-body {
    font-size: 14px;
    line-height: 1.45;
    color: var(--muted);
  }
  .dest-go {
    position: absolute;
    left: 20px;
    bottom: 16px;
    display: grid;
    place-items: center;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: var(--fill);
    color: var(--ink-2);
  }
  .dest:hover .dest-go {
    background: var(--accent);
    color: #fff;
  }

  /* "How it's built", in the same form as Pixel Lab's */
  .article {
    max-width: var(--page);
    margin: 96px auto 0;
    padding: 0 var(--gutter);
  }
  .article-head {
    display: grid;
    gap: 14px;
    padding-bottom: 32px;
  }
  .article-head h2 {
    margin: 0;
    font-size: clamp(40px, 6vw, 64px);
    letter-spacing: -0.03em;
    line-height: 1.04;
  }
  .lead {
    max-width: 760px;
    margin: 0;
    font-size: clamp(19px, 2vw, 24px);
    line-height: 1.4;
    color: var(--muted);
  }
  .highlights {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 14px;
    margin: 8px 0 56px;
    padding: 0;
    list-style: none;
  }
  .highlight {
    padding: 22px;
    border-radius: 18px;
  }
  .hv {
    font-size: clamp(24px, 2.6vw, 32px);
    font-weight: 600;
    letter-spacing: -0.02em;
    line-height: 1.1;
    font-variant-numeric: tabular-nums;
  }
  .hl {
    margin-top: 6px;
    font-size: 14px;
    color: var(--muted);
  }
  .case {
    display: grid;
    grid-template-columns: 11rem minmax(0, 1fr);
    gap: 48px;
    padding: 64px 0;
    border-top: 1px solid var(--hairline);
  }
  .case-label {
    margin: 0;
    padding-top: 8px;
    font-size: 14px;
    font-weight: 500;
    color: var(--muted);
  }
  .case-body {
    display: grid;
    gap: 20px;
    min-width: 0;
  }
  .case-body h3 {
    margin: 0;
    font-size: clamp(24px, 2.6vw, 32px);
    letter-spacing: -0.02em;
  }
  .case-body p {
    margin: 0;
    font-size: clamp(17px, 1.6vw, 19px);
    line-height: 1.6;
    color: var(--ink-body);
  }
  .case-body .caption {
    font-size: 13px;
    color: var(--muted);
  }
  .figure {
    padding: 20px 22px;
    border-radius: 18px;
  }
  .flow {
    display: grid;
    gap: 12px;
    padding: 20px;
    border-radius: 18px;
  }
  .lane {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 8px;
  }
  .node {
    padding: 6px 12px;
    border-radius: 999px;
    background: var(--fill);
    font-size: 13px;
    font-weight: 500;
    white-space: nowrap;
  }
  .node.file {
    font-family: var(--font-mono);
    font-weight: 450;
    background: transparent;
    box-shadow: inset 0 0 0 1px var(--separator);
  }
  .node.accent {
    background: var(--accent-wash);
    color: var(--accent-ink);
  }
  .arrow {
    position: relative;
    width: 16px;
    height: 1.5px;
    background: var(--muted);
  }
  .arrow::after {
    content: "";
    position: absolute;
    right: -1px;
    top: -4px;
    border: 5px solid transparent;
    border-left-color: var(--muted);
    border-right-width: 0;
  }
  .checks {
    display: grid;
    gap: 12px;
    margin: 0;
    padding: 0;
    list-style: none;
  }
  .checks li {
    position: relative;
    padding-left: 36px;
    font-size: 17px;
    line-height: 1.5;
    color: var(--ink-body);
  }
  .checks li::before {
    content: "";
    position: absolute;
    left: 0;
    top: 1px;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: var(--accent);
  }
  .checks li::after {
    content: "";
    position: absolute;
    left: 8px;
    top: 7px;
    width: 8px;
    height: 5px;
    border-left: 2.5px solid #fff;
    border-bottom: 2.5px solid #fff;
    transform: rotate(-45deg);
  }
  .cta {
    display: grid;
    justify-items: center;
    gap: 12px;
    margin-top: 24px;
    padding: 72px 24px;
    text-align: center;
  }
  .cta h2 {
    margin: 0;
    font-size: clamp(28px, 3.5vw, 40px);
  }
  .cta p {
    max-width: 560px;
    margin: 0;
    color: var(--muted);
    font-size: 17px;
  }
  .cta-actions {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 10px;
    margin-top: 16px;
  }
  @media (max-width: 899px) {
    .showcase {
      grid-template-columns: 1fr;
    }
    .graph-pane {
      border-right: 0;
      border-bottom: 1px solid var(--hairline);
    }
    .highlights {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }
  }
  @media (max-width: 767px) {
    .case {
      grid-template-columns: 1fr;
      gap: 16px;
      padding: 44px 0;
    }
    .case-label {
      padding: 0;
    }
  }
</style>
