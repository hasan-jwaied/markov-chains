<script lang="ts">
  /** C source with lightweight, dependency-free highlighting. */
  let { code, title, highlight = [] }: { code: string; title?: string; highlight?: number[] } = $props();

  const KEYWORDS = new Set([
    "typedef", "struct", "static", "const", "return", "if", "else", "for", "while", "void",
    "int", "char", "bool", "double", "size_t", "uint8_t", "uint32_t", "uint64_t", "int32_t",
    "unsigned", "sizeof", "union", "enum", "switch", "case", "break", "continue", "true",
    "false", "NULL", "inline",
  ]);

  function escape(s: string): string {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  function highlightLine(line: string): string {
    const re = /(\/\*.*?\*\/|\/\/.*$)|("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)')|(\b0x[0-9a-fA-F]+\b|\b\d+(?:\.\d+)?[uUlLfF]*\b)|(\b[A-Za-z_]\w*\b)|(#\w+)/g;
    let out = "";
    let last = 0;
    for (const m of line.matchAll(re)) {
      out += escape(line.slice(last, m.index));
      const [text, comment, str, num, ident, pre] = m;
      if (comment) out += `<span class="c">${escape(text)}</span>`;
      else if (str) out += `<span class="s">${escape(text)}</span>`;
      else if (num) out += `<span class="n">${escape(text)}</span>`;
      else if (pre) out += `<span class="p">${escape(text)}</span>`;
      else if (ident && KEYWORDS.has(ident)) out += `<span class="k">${text}</span>`;
      else if (ident && /^mk_\w+$|^Markov\w+$/.test(ident)) out += `<span class="t">${text}</span>`;
      else out += escape(text);
      last = (m.index ?? 0) + text.length;
    }
    return out + escape(line.slice(last));
  }

  const lines = $derived(code.replace(/^\n+|\n+$/g, "").split("\n").map(highlightLine));
</script>

<div class="code">
  {#if title}<div class="title">{title}</div>{/if}
  <pre><code>{#each lines as line, i}<span class="line" class:hl={highlight.includes(i + 1)}>{@html line || " "}</span>{/each}</code></pre>
</div>

<style>
  .code {
    margin: 1.6em 0;
    border-radius: 16px;
    background: var(--surface);
    box-shadow: var(--shadow);
    overflow: hidden;
    font-size: 13px;
  }
  .title {
    font: 500 12px var(--font-mono);
    color: var(--muted);
    padding: 10px 16px;
    border-bottom: 1px solid var(--hairline);
  }
  pre {
    margin: 0;
    padding: 12px 0;
    overflow-x: auto;
    line-height: 1.55;
  }
  .line {
    display: block;
    padding: 0 16px;
  }
  .line.hl {
    background: var(--accent-wash);
    box-shadow: inset 2px 0 0 var(--accent);
  }
  pre :global(.k) {
    color: var(--accent-ink);
    font-weight: 600;
  }
  pre :global(.t) {
    color: var(--s7);
  }
  pre :global(.s) {
    color: var(--good-ink);
  }
  pre :global(.n) {
    color: var(--s2);
  }
  pre :global(.c) {
    color: var(--muted);
    font-style: italic;
  }
  pre :global(.p) {
    color: var(--s5);
  }
</style>
