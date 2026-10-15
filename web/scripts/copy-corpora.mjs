// Copies the corpora shipped in data/corpora into public/corpora so the
// Text Lab can fetch them, and writes a manifest with word counts.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const web = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const data = resolve(web, "../data/corpora");
const out = resolve(web, "public/corpora");
mkdirSync(out, { recursive: true });

const presets = [
  { id: "alice", file: "gutenberg/alice.txt", title: "Alice's Adventures in Wonderland", author: "Lewis Carroll", rule: "sentence" },
  { id: "sherlock-holmes", file: "gutenberg/sherlock-holmes.txt", title: "The Adventures of Sherlock Holmes", author: "Arthur Conan Doyle", rule: "sentence" },
  { id: "pride-and-prejudice", file: "gutenberg/pride-and-prejudice.txt", title: "Pride and Prejudice", author: "Jane Austen", rule: "sentence" },
  { id: "frankenstein", file: "gutenberg/frankenstein.txt", title: "Frankenstein", author: "Mary Shelley", rule: "sentence" },
  { id: "grimm", file: "gutenberg/grimm.txt", title: "Grimms' Fairy Tales", author: "Jacob & Wilhelm Grimm", rule: "sentence" },
  { id: "sonnets", file: "gutenberg/sonnets.txt", title: "The Sonnets", author: "William Shakespeare", rule: "sentence" },
  { id: "tale-of-two-cities", file: "gutenberg/tale-of-two-cities.txt", title: "A Tale of Two Cities", author: "Charles Dickens", rule: "sentence" },
  { id: "moby-dick", file: "gutenberg/moby-dick.txt", title: "Moby-Dick", author: "Herman Melville", rule: "sentence" },
  { id: "tweets", file: "justdoit_tweets.txt", title: "#JustDoIt tweets", author: "the original assignment corpus", rule: "period" },
];

const manifest = presets.map((p) => {
  let text = readFileSync(resolve(data, p.file), "utf8");
  // Project Gutenberg marks italics as _word_; drop the markers for the demo.
  if (p.file.startsWith("gutenberg/")) text = text.replace(/_([^_\n]+)_/g, "$1");
  writeFileSync(resolve(out, `${p.id}.txt`), text);
  return { ...p, file: `corpora/${p.id}.txt`, words: text.split(/\s+/).filter(Boolean).length, bytes: Buffer.byteLength(text) };
});
writeFileSync(resolve(out, "manifest.json"), JSON.stringify(manifest, null, 2));
console.log(`copied ${manifest.length} corpora to public/corpora`);
