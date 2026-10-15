<script lang="ts">
  import PageHero from "../lib/components/PageHero.svelte";
  import { untrack } from "svelte";
  import ForceGraph from "../lib/charts/ForceGraph.svelte";
  import Segmented from "../lib/components/Segmented.svelte";
  import Slider from "../lib/components/Slider.svelte";
  import StatTile from "../lib/components/StatTile.svelte";
  import { type Corpus, fmtBytes, fmtInt, listCorpora, loadCorpus } from "../lib/corpora";
  import { engine } from "../lib/engine/client.svelte";
  import { engineModel } from "../lib/engine/model.svelte";
  import type { Generated, Neighborhood, StateInfo, TerminalRule, TextStats } from "../lib/engine/types";

  // corpus & model
  let corpora = $state<Corpus[]>([]);
  let source = $state<string>("alice");
  let custom = $state("Paste any text here. Each sentence ends with a full stop. The chain learns which word follows which. Then it writes new sentences of its own.");
  let order = $state(2);
  let rule = $state<TerminalRule>("sentence");
  let text = $state<string | null>(null);
  let loadError = $state<string | null>(null);

  listCorpora().then((c) => (corpora = c)).catch((e: Error) => (loadError = e.message));

  $effect(() => {
    const id = source;
    if (id === "custom") return;
    text = null;
    let alive = true;
    loadCorpus(id)
      .then((t) => {
        if (!alive) return;
        text = t;
        const meta = corpora.find((c) => c.id === id);
        if (meta) rule = meta.rule;
      })
      .catch((e: Error) => (loadError = e.message));
    return () => {
      alive = false;
    };
  });

  const trainingText = $derived(source === "custom" ? custom : text);
  const model = engineModel(() => (trainingText ? engine.buildText(trainingText, { order, rule }) : null));

  let stats = $state<TextStats | null>(null);
  $effect(() => {
    const id = model.id;
    if (id === null) return;
    engine.textStats(id).then((s) => (stats = s)).catch(() => {});
  });

  // generation
  let temperature = $state(1);
  let policy = $state<"empirical" | "uniform" | "compat">("empirical");
  let sampler = $state<"alias" | "linear">("alias");
  let seed = $state(1);
  let sentences = $state<Generated[]>([]);
  let mode = $state<"generate" | "steer">("generate");

  async function generate() {
    if (model.id === null) return;
    try {
      sentences = await engine.generate(model.id, { seed, count: 8, maxLength: 60, temperature, policy, sampler });
      seed++;
    } catch (e) {
      sentences = [{ text: (e as Error).message, states: [], verbatim: false }];
    }
  }

  // Fresh sentences whenever the model or the sampling settings change.
  $effect(() => {
    void [model.id, temperature, policy, sampler];
    untrack(generate); // generate() reads and bumps `seed`; don't depend on it
  });

  // steering
  let path = $state<number[]>([]);
  let info = $state<StateInfo | null>(null);
  let hood = $state<Neighborhood | null>(null);
  let steerSeed = 1;

  const lastWord = (label: string) => label.split(" ").at(-1) ?? label;

  async function restart(start?: number) {
    if (model.id === null) return;
    const s = start ?? (await engine.randomStart(model.id, steerSeed++, "empirical"));
    path = s === null ? [] : [s];
  }

  $effect(() => {
    const id = model.id;
    if (id === null) return;
    untrack(() => restart());
  });

  $effect(() => {
    const id = model.id;
    const cur = path.at(-1);
    if (id === null || cur === undefined) {
      info = null;
      hood = null;
      return;
    }
    let alive = true;
    Promise.all([engine.stateInfo(id, cur, 14), engine.neighborhood(id, cur, 7, 3)])
      .then(([i, h]) => {
        if (!alive) return;
        info = i;
        hood = h;
      })
      .catch(() => {});
    return () => {
      alive = false;
    };
  });

  async function choose(next: number) {
    path = [...path, next];
  }

  async function letChainChoose() {
    const cur = path.at(-1);
    if (model.id === null || cur === undefined) return;
    const next = await engine.step(model.id, cur, steerSeed++, temperature);
    if (next !== null) path = [...path, next];
  }

  let steeredText = $state("");
  $effect(() => {
    const id = model.id;
    const p = path;
    if (id === null || !p.length) {
      steeredText = "";
      return;
    }
    engine.labels(id, p).then((ls) => {
      steeredText = [ls[0], ...ls.slice(1).map(lastWord)].join(" ");
    });
  });

  const meta = $derived(corpora.find((c) => c.id === source));
</script>

<div class="page">
  <PageHero compact eyebrow="Lab" title="Text Lab">
    Train an order-k word chain on a book, entirely in your browser, then let it write, or steer it yourself one word at a time.
  </PageHero>

  <div class="layout">
    <aside class="card panel" aria-label="Model settings">
      <h2>Corpus</h2>
      <select bind:value={source} aria-label="Corpus">
        {#each corpora as c (c.id)}
          <option value={c.id}>{c.title} ({fmtInt(c.words)} words)</option>
        {/each}
        <option value="custom">Your own text…</option>
      </select>
      {#if meta}<p class="small muted">{meta.author}</p>{/if}
      {#if source === "custom"}
        <textarea bind:value={custom} rows="7" aria-label="Your training text"></textarea>
      {/if}
      {#if loadError}<p class="error">{loadError}</p>{/if}

      <h2>Model</h2>
      <Segmented label="Order k (words of memory)" options={[1, 2, 3, 4].map((k) => ({ value: k, label: String(k) }))} bind:value={order} />
      <Segmented
        label="Sentences end at"
        options={[{ value: "sentence", label: ". ! ?" }, { value: "period", label: "a final '.'" }]}
        bind:value={rule}
      />

      <h2>Sampling</h2>
      <Slider label="Temperature" bind:value={temperature} min={0.1} max={3} step={0.05} width={230} format={(v) => v.toFixed(2)} />
      <p class="small muted">Below 1 sharpens toward the likeliest word; above 1 flattens toward uniform.</p>
      <Segmented
        label="Start"
        options={[{ value: "empirical", label: "Real openings", title: "Weighted by how often each state begins a sentence" }, { value: "uniform", label: "Uniform" }, { value: "compat", label: "Original", title: "The assignment's rejection sampling" }]}
        bind:value={policy}
      />
      <Segmented label="Sampler" options={[{ value: "alias", label: "Alias O(1)" }, { value: "linear", label: "Linear O(d)" }]} bind:value={sampler} />

      {#if model.info}
        <h2>The chain</h2>
        <div class="tiles">
          <StatTile label="States" value={fmtInt(model.info.states)} />
          <StatTile label="Transitions" value={fmtInt(model.info.edges)} />
          <StatTile label="Built in" value="{model.info.buildMs.toFixed(0)} ms" note="WebAssembly" />
          <StatTile label="Memory" value={fmtBytes(model.info.bytes)} />
          {#if stats}
            <StatTile label="Avg. successors" value={stats.branching.toFixed(2)} />
            <StatTile label="Entropy" value="{stats.entropyBits.toFixed(2)} bits" note="per word" />
          {/if}
        </div>
      {/if}
    </aside>

    <section class="main">
      <div class="modebar">
        <Segmented options={[{ value: "generate", label: "Generate" }, { value: "steer", label: "Steer" }]} bind:value={mode} />
        {#if model.loading}<span class="small muted">learning…</span>{/if}
        {#if model.error}<span class="error">{model.error}</span>{/if}
      </div>

      {#if mode === "generate"}
        <div class="card out">
          <div class="outhead">
            <button class="btn primary" onclick={generate} disabled={model.id === null}>Generate</button>
            <span class="small muted">8 sentences · up to 60 words · seed {seed - 1}</span>
          </div>
          <ol class="sentences">
            {#each sentences as g, i (i)}
              <li>
                <span>{g.text}</span>
                {#if g.verbatim}<span class="tag" title="This exact sentence appears in the training text">verbatim</span>{/if}
                {#if g.states.length}
                  <button class="btn ghost small" onclick={() => { mode = "steer"; restart(g.states[0]); }}>steer from here</button>
                {/if}
              </li>
            {/each}
          </ol>
        </div>
      {:else}
        <div class="card out">
          <p class="steered">{steeredText}<span class="caret" aria-hidden="true"></span></p>
          <div class="outhead">
            <button class="btn primary" onclick={letChainChoose} disabled={!info || info.outDegree === 0}>Let the chain choose</button>
            <button class="btn" onclick={() => (path = path.slice(0, -1))} disabled={path.length <= 1}>Undo</button>
            <button class="btn ghost" onclick={() => restart()}>New start</button>
          </div>
          {#if info}
            {#if info.outDegree === 0}
              <p class="muted">{info.terminal ? "The sentence is complete: this state is terminal." : "Dead end: this context never continued in the text."}</p>
            {:else}
              <p class="small muted">
                After “{info.label}” the text continued {fmtInt(info.outTotal)} time{info.outTotal === 1 ? "" : "s"},
                in {fmtInt(info.outDegree)} different way{info.outDegree === 1 ? "" : "s"}. Click a word to choose it.
              </p>
              <div class="dist">
                {#each info.edges as e (e.target)}
                  <button class="drow" onclick={() => choose(e.target)} aria-label="Choose {lastWord(e.label)}, probability {(e.p * 100).toFixed(1)}%">
                    <span class="w">{lastWord(e.label)}</span>
                    <span class="track"><span class="fill" style:width="{(e.p / info.edges[0].p) * 100}%"></span></span>
                    <span class="p num">{(e.p * 100).toFixed(1)}%</span>
                  </button>
                {/each}
                {#if info.restP > 0}
                  <div class="drow rest"><span class="w muted">{fmtInt(info.outDegree - info.edges.length)} more</span><span></span><span class="p num muted">{(info.restP * 100).toFixed(1)}%</span></div>
                {/if}
              </div>
            {/if}
          {/if}
        </div>
        {#if hood && path.length}
          <div class="card out">
            <p class="small muted">Where the chain can go from here: the likeliest next words, and theirs. Click a blue one to take it.</p>
            <ForceGraph graph={hood} center={path[path.length - 1]} labelOf={lastWord} onselect={choose} />
          </div>
        {/if}
      {/if}
    </section>
  </div>
</div>

<style>
  .page {
    max-width: 1200px;
    margin: 0 auto;
    padding: 0 var(--gutter);
  }
  .layout {
    display: grid;
    grid-template-columns: 320px minmax(0, 1fr);
    gap: 20px;
    align-items: start;
  }
  .panel {
    padding: 16px 18px 20px;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .panel h2 {
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 0.07em;
    color: var(--muted);
    margin: 10px 0 0;
  }
  .panel h2:first-child {
    margin-top: 0;
  }
  select {
    width: 100%;
  }
  .tiles {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .tiles :global(.value) {
    font-size: 19px;
  }
  .main {
    display: flex;
    flex-direction: column;
    gap: 14px;
    min-width: 0;
  }
  .modebar {
    display: flex;
    gap: 14px;
    align-items: center;
  }
  .out {
    padding: 16px 18px;
  }
  .outhead {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    align-items: center;
    margin-bottom: 8px;
  }
  .sentences {
    margin: 8px 0 0;
    padding-left: 24px;
    font-family: var(--font-serif);
    font-size: 18px;
    line-height: 1.55;
  }
  .sentences li {
    margin-bottom: 12px;
  }
  .sentences .tag {
    margin-left: 6px;
    vertical-align: 2px;
  }
  .sentences .btn {
    margin-left: 4px;
    vertical-align: 1px;
    font-size: 12px;
    padding: 3px 7px;
    color: var(--accent-ink);
  }
  .steered {
    font-family: var(--font-serif);
    font-size: 22px;
    line-height: 1.5;
    min-height: 1.5em;
    margin: 0 0 14px;
    color: var(--ink-strong);
  }
  .caret {
    display: inline-block;
    width: 2px;
    height: 1.05em;
    background: var(--accent);
    margin-left: 4px;
    vertical-align: -3px;
    animation: blink 1.1s steps(2) infinite;
  }
  @keyframes blink {
    50% {
      opacity: 0;
    }
  }
  .dist {
    display: flex;
    flex-direction: column;
    gap: 3px;
    margin-top: 6px;
  }
  .drow {
    display: grid;
    grid-template-columns: minmax(90px, 180px) 1fr 58px;
    gap: 12px;
    align-items: center;
    border: none;
    background: transparent;
    padding: 5px 8px;
    border-radius: 6px;
    text-align: left;
    font: 14px var(--font-sans);
    color: var(--ink);
    cursor: pointer;
  }
  .drow:hover {
    background: var(--accent-wash);
  }
  .drow.rest {
    cursor: default;
  }
  .drow.rest:hover {
    background: transparent;
  }
  .w {
    font-family: var(--font-serif);
    font-size: 16px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .track {
    height: 10px;
    border-radius: 0 4px 4px 0;
    overflow: hidden;
  }
  .fill {
    display: block;
    height: 100%;
    background: var(--accent);
    border-radius: 0 4px 4px 0;
  }
  .p {
    text-align: right;
    font-weight: 600;
    color: var(--ink-strong);
  }
  .small {
    font-size: 13px;
    margin: 0;
  }
  .error {
    color: var(--critical);
    font-size: 13px;
  }
  @media (max-width: 900px) {
    .layout {
      grid-template-columns: 1fr;
    }
  }
</style>
