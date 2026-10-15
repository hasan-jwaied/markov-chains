// Runs golden cases through the WebAssembly build of the engine (Node 18+).
//
//   node tests/wasm/golden_wasm.mjs [path/to/markov.mjs]
//
// The engine carries its own glibc-compatible RNG, so the browser build must
// reproduce the original program's output byte for byte, like the native CLI.
import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const modulePath = resolve(process.argv[2] ?? `${root}/web/src/wasm/markov.mjs`);
const { default: createMarkovModule } = await import(pathToFileURL(modulePath).href);
const M = await createMarkovModule();

// Corpora and outputs are handled as raw bytes, exactly like the C code.
const latin1 = new TextDecoder("latin1");
const bytesIn = (bytes) => {
  const p = M._mkw_malloc(bytes.length);
  M.HEAPU8.set(bytes, p);
  return [p, bytes.length];
};
const bytesOut = (p, n) => latin1.decode(M.HEAPU8.subarray(p, p + n));

function tweets(seed, count, file, maxWords = -1) {
  const chain = M._mkw_chain_new(1);
  const [text, len] = bytesIn(readFileSync(resolve(root, file)));
  if (M._mkw_text_build(chain, text, len, 1, maxWords, 0, 0) < 0) throw new Error("build failed");
  M._mkw_free(text);
  const rng = M._mkw_rng_new(1, seed);
  const states = M._mkw_malloc(20 * 4);
  const buf = M._mkw_malloc(65536);
  let out = "";
  for (let i = 1; i <= count; i++) {
    const n = M._mkw_generate(chain, rng, -1, 0, 0, 20, 1.0, states);
    if (n < 0) throw new Error(M.UTF8ToString(M._mkw_status_str(n)));
    const size = M._mkw_render(chain, states, n, 0, buf, 65536);
    out += `Tweet ${i}: ${bytesOut(buf, size)}\n`;
  }
  [states, buf].forEach((p) => M._mkw_free(p));
  M._mkw_rng_free(rng);
  M._mkw_chain_free(chain);
  return out;
}

function snakes(seed, count) {
  const chain = M._mkw_chain_new(2);
  const nPtr = M._mkw_malloc(4);
  const jumps = M._mkw_snakes_classic(nPtr);
  if (M._mkw_snakes_build(chain, 100, 6, jumps, M.HEAPU32[nPtr >> 2]) < 0) throw new Error("board");
  M._mkw_free(nPtr);
  const rng = M._mkw_rng_new(1, seed);
  const walk = M._mkw_malloc(60 * 4);
  let out = "";
  for (let i = 1; i <= count; i++) {
    const n = M._mkw_generate(chain, rng, 0, 0, 0, 60, 1.0, walk);
    out += `Random Walk ${i}: `;
    for (let k = 0; k < n; k++) {
      const s = M.HEAPU32[(walk >> 2) + k];
      const cell = M._mkw_state_data(chain, s) >> 2; // mk_cell: number, ladder_to, snake_to, is_last
      const [num, ladder, snake, last] = M.HEAP32.subarray(cell, cell + 4);
      out += `[${num}]`;
      if (!last) out += ladder !== -1 ? " -ladder to-> " : snake !== -1 ? " -snake to-> " : " -> ";
    }
    out += "\n";
  }
  M._mkw_free(walk);
  M._mkw_rng_free(rng);
  M._mkw_chain_free(chain);
  return out;
}

const cases = readFileSync(`${root}/tests/golden/cases.txt`, "utf8")
  .split("\n")
  .map((l) => l.trim())
  .filter((l) => l && !l.startsWith("#"))
  .map((l) => l.split("|").map((x) => x.trim()));

let pass = 0, fail = 0;
for (const [name, prog, args] of cases) {
  if (prog === "tweets_a") continue;
  const a = args.split(/\s+/);
  const got = prog === "tweets"
    ? tweets(Number(a[0]), Number(a[1]), a[2], a[3] !== undefined ? Number(a[3]) : -1)
    : snakes(Number(a[0]), Number(a[1]));
  const expected = readFileSync(`${root}/tests/golden/expected/${name}.out`, "latin1");
  if (got === expected) pass++;
  else { fail++; console.log(`FAIL ${name}`); }
}

const stats = M._mkw_malloc(24 * 8);
M._mkw_alloc_stats(stats);
const liveBlocks = M.HEAPF64[(stats >> 3) + 1];
M._mkw_free(stats);
if (liveBlocks !== 0) { fail++; console.log(`FAIL leak: ${liveBlocks} engine block(s) alive`); }

console.log(`wasm golden: ${pass} passed, ${fail} failed`);
process.exit(fail === 0 ? 0 : 1);
