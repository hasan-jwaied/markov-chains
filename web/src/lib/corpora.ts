/** The corpora shipped with the site (see scripts/copy-corpora.mjs). */
export interface Corpus {
  id: string;
  file: string;
  title: string;
  author: string;
  rule: "period" | "sentence";
  words: number;
  bytes: number;
}

let manifest: Promise<Corpus[]> | null = null;

/** Relative URLs would break under /markov (no trailing slash): use the base. */
const BASE = import.meta.env.BASE_URL;
const texts = new Map<string, Promise<string>>();

export function listCorpora(): Promise<Corpus[]> {
  manifest ??= fetch(`${BASE}corpora/manifest.json`).then((r) => {
    if (!r.ok) throw new Error(`could not load the corpus list (${r.status})`);
    return r.json() as Promise<Corpus[]>;
  });
  return manifest;
}

export function loadCorpus(id: string): Promise<string> {
  let p = texts.get(id);
  if (!p) {
    p = fetch(`${BASE}corpora/${id}.txt`).then((r) => {
      if (!r.ok) throw new Error(`could not load ${id} (${r.status})`);
      return r.text();
    });
    texts.set(id, p);
  }
  return p;
}

export const fmtInt = (n: number): string => Math.round(n).toLocaleString("en-US");

export function fmtBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 ** 2) return `${(n / 1024).toFixed(1)} KiB`;
  if (n < 1024 ** 3) return `${(n / 1024 ** 2).toFixed(1)} MiB`;
  return `${(n / 1024 ** 3).toFixed(2)} GiB`;
}
