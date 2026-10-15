<script lang="ts">
  import Nav from "./lib/components/Nav.svelte";
  import Footer from "./lib/components/Footer.svelte";
  import { router } from "./lib/router.svelte";
  import Home from "./pages/Home.svelte";
  import Learn from "./pages/Learn.svelte";
  import TextLab from "./pages/TextLab.svelte";
  import SnakesStudio from "./pages/SnakesStudio.svelte";
  import GuessChain from "./pages/GuessChain.svelte";
  import UnderTheHood from "./pages/UnderTheHood.svelte";

  const titles = {
    home: "Markov Chains, from C to the browser",
    learn: "Learn · Markov Chains",
    text: "Text Lab · Markov Chains",
    snakes: "Snakes & Ladders Studio · Markov Chains",
    guess: "Guess the Chain · Markov Chains",
    hood: "Under the Hood · Markov Chains",
  } as const;

  $effect(() => {
    document.title = titles[router.current.page];
  });
</script>

<Nav />
<main id="content">
  {#if router.current.page === "home"}
    <Home />
  {:else if router.current.page === "learn"}
    <Learn chapter={router.current.param} />
  {:else if router.current.page === "text"}
    <TextLab />
  {:else if router.current.page === "snakes"}
    <SnakesStudio />
  {:else if router.current.page === "guess"}
    <GuessChain />
  {:else if router.current.page === "hood"}
    <UnderTheHood />
  {/if}
</main>
<Footer />

<style>
  main {
    min-height: calc(100vh - 160px);
  }
</style>
