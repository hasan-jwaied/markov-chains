import { svelte } from "@sveltejs/vite-plugin-svelte";
import { defineConfig, type Plugin } from "vitest/config";

/**
 * KaTeX declares every font as woff2, woff and ttf. Every browser that can run
 * this app (module workers, WebAssembly) uses woff2, so the other formats
 * would never be downloaded: drop them instead of shipping 40 unused files.
 */
const katexWoff2Only: Plugin = {
  name: "katex-woff2-only",
  enforce: "pre",
  transform(code, id) {
    if (!/katex[^/]*\.css$/.test(id.split("?")[0])) return;
    return code.replace(/,\s*url\([^)]+\.(?:woff|ttf)\)\s*format\("(?:woff|truetype)"\)/g, "");
  },
};

export default defineConfig({
  // Served by the portfolio at /markov (no trailing slash), so URLs are absolute.
  base: "/markov/",
  plugins: [katexWoff2Only, svelte()],
  worker: { format: "es" },
  build: {
    target: "es2022",
    chunkSizeWarningLimit: 1200,
    // No data: URIs: the portfolio's CSP allows fonts and images from 'self'
    // only, so every asset must be a real file (Vite inlines < 4 KB by default).
    assetsInlineLimit: 0,
  },
  test: { environment: "node", include: ["src/**/*.test.ts"] },
});
