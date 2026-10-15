<script lang="ts">
  import { href, router, type Page } from "../router.svelte";
  import { HOME_URL, REPO_URL } from "../site";
  import { theme } from "../theme.svelte";
  import Logo from "./Logo.svelte";

  const links: Array<{ page: Page; label: string }> = [
    { page: "learn", label: "Learn" },
    { page: "text", label: "Text Lab" },
    { page: "snakes", label: "Snakes & Ladders" },
    { page: "guess", label: "Guess the Chain" },
    { page: "hood", label: "Under the Hood" },
  ];

  let menuOpen = $state(false);
  $effect(() => {
    void router.current.page; // close the sheet on navigation
    menuOpen = false;
  });
</script>

<header class="topbar">
  <a class="skip-link" href="#content">Skip to content</a>
  <div class="glass-nav">
    <nav class="nav-inner" aria-label="Primary">
      <a class="nav-back" href={HOME_URL} aria-label="Back to the portfolio">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m15 18-6-6 6-6" /></svg>
        <span>Portfolio</span>
      </a>
      <a class="brand" href={href("home")} aria-label="Markov Chains, home">
        <Logo />
        <span class="brand-name">Markov Chains</span>
      </a>
      <ul class="topnav" role="list">
        {#each links as link (link.page)}
          <li>
            <a href={href(link.page)} aria-current={router.current.page === link.page ? "page" : undefined}>{link.label}</a>
          </li>
        {/each}
      </ul>
      <div class="actions">
        <button
          class="icon-btn"
          onclick={() => theme.toggle()}
          aria-label={theme.effective === "dark" ? "Switch to light mode" : "Switch to dark mode"}
          title="Toggle theme"
        >
          {#if theme.effective === "dark"}
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="4" /><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M6.34 17.66l-1.41 1.41M19.07 4.93l-1.41 1.41" /></svg>
          {:else}
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z" /></svg>
          {/if}
        </button>
        <a class="icon-btn" href={REPO_URL} target="_blank" rel="noopener noreferrer" aria-label="Source code on GitHub" title="Source on GitHub">
          <svg width="18" height="18" viewBox="0 0 16 16" aria-hidden="true"><path fill="currentColor" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z" /></svg>
        </a>
        <button
          class="icon-btn menu-btn"
          onclick={() => (menuOpen = !menuOpen)}
          aria-expanded={menuOpen}
          aria-controls="nav-sheet"
          aria-label={menuOpen ? "Close menu" : "Open menu"}
        >
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true">
            {#if menuOpen}<path d="M6 6l12 12M18 6 6 18" />{:else}<path d="M4 8h16M4 16h16" />{/if}
          </svg>
        </button>
      </div>
    </nav>
    <div class="nav-progress" aria-hidden="true"></div>
  </div>
  <!-- A sibling of the bar, not a child: a blurred child of a blurred parent
       would not see the page behind it. -->
  {#if menuOpen}
    <div class="glass-menu" id="nav-sheet">
      <ul role="list">
        {#each links as link (link.page)}
          <li><a href={href(link.page)} aria-current={router.current.page === link.page ? "page" : undefined}>{link.label}</a></li>
        {/each}
      </ul>
    </div>
  {/if}
</header>

<style>
  .topbar {
    position: sticky;
    top: 0;
    z-index: 50;
  }
  .glass-nav {
    position: relative;
    background: rgb(245 245 247 / 0.72);
    -webkit-backdrop-filter: saturate(180%) blur(24px);
    backdrop-filter: saturate(180%) blur(24px);
    border-bottom: 1px solid rgb(0 0 0 / 0.1);
  }
  :global(.dark) .glass-nav {
    background: rgb(22 22 23 / 0.72);
    border-bottom-color: rgb(255 255 255 / 0.12);
  }
  .nav-inner {
    display: flex;
    align-items: center;
    gap: 8px;
    height: var(--nav-h);
    max-width: var(--page);
    margin: 0 auto;
    padding: 0 max(var(--gutter), env(safe-area-inset-left)) 0 max(var(--gutter), env(safe-area-inset-right));
  }
  .nav-back {
    display: inline-flex;
    align-items: center;
    gap: 2px;
    padding: 6px 12px 6px 8px;
    border-radius: 999px;
    color: var(--ink-2);
    font-size: 14px;
    font-weight: 500;
    text-decoration: none;
    transition: background-color 0.15s;
  }
  .nav-back:hover {
    background: var(--hover);
    color: var(--ink);
  }
  .brand {
    display: flex;
    align-items: center;
    gap: 8px;
    padding-inline: 4px;
    color: var(--ink);
    text-decoration: none;
    font-weight: 600;
    letter-spacing: -0.01em;
    white-space: nowrap;
  }
  .nav-back + .brand {
    padding-inline-start: 12px;
    border-inline-start: 1px solid var(--separator);
  }
  .topnav {
    display: flex;
    gap: 2px;
    margin: 0 0 0 8px;
    padding: 0;
    list-style: none;
    min-width: 0;
  }
  .topnav a {
    display: block;
    padding: 6px 11px;
    border-radius: 999px;
    color: var(--ink-2);
    font-size: 14px;
    font-weight: 500;
    text-decoration: none;
    white-space: nowrap;
    transition: background-color 0.15s;
  }
  .topnav a:hover {
    background: var(--hover);
    color: var(--ink);
  }
  .topnav a[aria-current="page"] {
    color: var(--ink);
    background: var(--fill);
  }
  .actions {
    display: flex;
    align-items: center;
    gap: 2px;
    margin-left: auto;
  }
  .icon-btn {
    display: inline-grid;
    place-items: center;
    width: 36px;
    height: 36px;
    border: 0;
    border-radius: 999px;
    background: transparent;
    color: var(--ink-2);
    transition: background-color 0.15s, color 0.15s;
  }
  .icon-btn:hover {
    background: var(--hover);
    color: var(--ink);
  }
  .menu-btn {
    display: none;
  }
  .nav-progress {
    position: absolute;
    inset: auto 0 -1px 0;
    height: 2px;
    background: var(--accent);
    transform: scaleX(0);
    transform-origin: left;
    pointer-events: none;
  }
  @keyframes nav-progress {
    to {
      transform: scaleX(1);
    }
  }
  @supports (animation-timeline: scroll()) {
    @media (prefers-reduced-motion: no-preference) {
      .nav-progress {
        animation: nav-progress linear both;
        animation-timeline: scroll(root block);
      }
    }
  }
  .glass-menu {
    position: absolute;
    left: 0;
    right: 0;
    top: calc(100% + 0px);
    background: rgb(245 245 247 / 0.86);
    -webkit-backdrop-filter: saturate(180%) blur(24px);
    backdrop-filter: saturate(180%) blur(24px);
    border-bottom: 1px solid rgb(0 0 0 / 0.1);
    padding: 8px var(--gutter) 16px;
  }
  :global(.dark) .glass-menu {
    background: rgb(22 22 23 / 0.9);
    border-bottom-color: rgb(255 255 255 / 0.12);
  }
  .glass-menu ul {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .glass-menu a {
    display: block;
    padding: 12px 4px;
    border-bottom: 1px solid var(--hairline);
    color: var(--ink);
    font-size: 17px;
    font-weight: 500;
    text-decoration: none;
  }
  .glass-menu a[aria-current="page"] {
    color: var(--accent-ink);
  }
  .skip-link {
    position: absolute;
    left: 12px;
    top: 6px;
    z-index: 100;
    padding: 8px 14px;
    border-radius: 999px;
    background: var(--accent);
    color: var(--on-accent);
    font-weight: 500;
    font-size: 14px;
    text-decoration: none;
  }
  .skip-link:not(:focus) {
    clip-path: inset(50%);
    width: 1px;
    height: 1px;
    padding: 0;
    overflow: hidden;
    white-space: nowrap;
  }
  @media (prefers-reduced-transparency: reduce) {
    .glass-nav,
    .glass-menu {
      -webkit-backdrop-filter: none;
      backdrop-filter: none;
      background: #f5f5f7;
    }
    :global(.dark) .glass-nav,
    :global(.dark) .glass-menu {
      background: #161617;
    }
  }
  @media (max-width: 1000px) {
    .topnav {
      display: none;
    }
    .menu-btn {
      display: inline-grid;
    }
  }
  @media (max-width: 420px) {
    .brand-name {
      display: none;
    }
    .nav-back + .brand {
      border: 0;
      padding: 0;
    }
  }
</style>
