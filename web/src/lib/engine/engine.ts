/**
 * High-level, synchronous wrapper around the WebAssembly engine.
 *
 * It owns every engine object it creates (chains, RNGs) behind small integer
 * model ids, so the UI never handles raw pointers. It normally runs inside a
 * Web Worker (see worker.ts); the methods return structured-cloneable data.
 */
import {
  type MarkovModule,
  PLUGIN,
  POLICY,
  SAMPLER,
  Status,
  TAGS,
  type Tag,
  loadModule,
} from "./abi";
import { type Layout, LAYOUT_WORDS, parseLayout } from "./layout";
import type {
  AbsorbingResult,
  AllocStats,
  BoardAnalysis,
  BoardSpec,
  CustomChainSpec,
  EdgeInfo,
  FieldView,
  GenerateOptions,
  Generated,
  HeapEvent,
  HeapSnapshot,
  ModelInfo,
  Neighborhood,
  PluginKind,
  StateInfo,
  StationaryResult,
  TextBuildOptions,
  TextStats,
} from "./types";

interface Model {
  chain: number;
  plugin: PluginKind;
  /** Training text with whitespace collapsed, for verbatim checks. */
  corpus?: string;
}

const utf8 = new TextDecoder("utf-8");
const encoder = new TextEncoder();
const LABEL_CAP = 1024;

export class EngineError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
  }
}

export class Engine {
  private models = new Map<number, Model>();
  private nextId = 1;
  private scratch: number;
  readonly layout: Layout;

  private constructor(private readonly M: MarkovModule) {
    this.scratch = M._mkw_malloc(LABEL_CAP);
    const raw = M._mkw_malloc(LAYOUT_WORDS * 4);
    M._mkw_layout(raw, LAYOUT_WORDS);
    this.layout = parseLayout(this.u32(raw, LAYOUT_WORDS));
    M._mkw_free(raw);
  }

  static async create(): Promise<Engine> {
    return new Engine(await loadModule());
  }

  // memory helpers

  private u32(ptr: number, n: number): Uint32Array {
    return this.M.HEAPU32.slice(ptr >> 2, (ptr >> 2) + n);
  }

  private f64(ptr: number, n: number): Float64Array {
    return this.M.HEAPF64.slice(ptr >> 3, (ptr >> 3) + n);
  }

  private bytes(ptr: number, n: number): Uint8Array {
    return this.M.HEAPU8.slice(ptr, ptr + n);
  }

  /** Copy a JS string into engine memory (NUL-terminated) for `fn`. */
  private withString<T>(text: string, fn: (ptr: number, len: number) => T): T {
    const data = encoder.encode(text);
    const ptr = this.M._mkw_malloc(data.length + 1);
    this.M.HEAPU8.set(data, ptr);
    this.M.HEAPU8[ptr + data.length] = 0;
    try {
      return fn(ptr, data.length);
    } finally {
      this.M._mkw_free(ptr);
    }
  }

  private withBuffer<T>(bytes: number, fn: (ptr: number) => T): T {
    const ptr = this.M._mkw_malloc(bytes);
    try {
      return fn(ptr);
    } finally {
      this.M._mkw_free(ptr);
    }
  }

  private check(status: number, what: string): void {
    if (status < 0) {
      throw new EngineError(`${what}: ${this.statusText(status)}`, -status);
    }
  }

  private statusText(status: number): string {
    return this.M.UTF8ToString(this.M._mkw_status_str(status));
  }

  private model(id: number): Model {
    const m = this.models.get(id);
    if (!m) throw new EngineError(`unknown model ${id}`, Status.INVALID);
    return m;
  }

  private register(chain: number, plugin: PluginKind, corpus?: string): number {
    if (!chain) throw new EngineError("chain allocation failed", Status.NOMEM);
    const id = this.nextId++;
    this.models.set(id, { chain, plugin, corpus });
    return id;
  }

  private info(id: number, buildMs: number, bytes: number, extra: Partial<ModelInfo> = {}): ModelInfo {
    const { chain, plugin } = this.model(id);
    return {
      id,
      plugin,
      states: this.M._mkw_chain_size(chain),
      edges: this.M._mkw_chain_edge_count(chain),
      transitions: this.M._mkw_chain_transition_total(chain),
      buildMs,
      bytes,
      ...extra,
    };
  }

  // engine-wide

  getLayout(): Layout {
    return this.layout;
  }

  allocStats(): AllocStats {
    return this.withBuffer(24 * 8, (p) => {
      this.M._mkw_alloc_stats(p);
      const v = this.f64(p, 24);
      const bytesByTag = {} as Record<Tag, number>;
      const blocksByTag = {} as Record<Tag, number>;
      TAGS.forEach((t, i) => {
        bytesByTag[t] = v[6 + i];
        blocksByTag[t] = v[6 + TAGS.length + i];
      });
      return {
        liveBytes: v[0], liveBlocks: v[1], peakBytes: v[2],
        allocs: v[3], frees: v[4], failed: v[5], bytesByTag, blocksByTag,
      };
    });
  }

  setHeapTrace(enabled: boolean): void {
    this.M._mkw_heap_trace(enabled ? 1 : 0);
  }

  heapEvents(max = 4096): { events: HeapEvent[]; dropped: number } {
    const events: HeapEvent[] = [];
    this.withBuffer(max * 16, (p) => {
      const n = this.M._mkw_heap_events(p, max);
      const raw = this.u32(p, n * 4);
      for (let i = 0; i < n; i++) {
        events.push({
          kind: raw[i * 4] ? "free" : "alloc",
          address: raw[i * 4 + 1],
          size: raw[i * 4 + 2],
          tag: TAGS[raw[i * 4 + 3]] ?? "other",
        });
      }
    });
    return { events, dropped: this.M._mkw_heap_events_dropped() };
  }

  /** Engine bytes currently held (used to attribute memory to a model). */
  private liveBytes(): number {
    return this.withBuffer(24 * 8, (p) => {
      this.M._mkw_alloc_stats(p);
      return this.M.HEAPF64[p >> 3];
    });
  }

  free(id: number): void {
    const m = this.models.get(id);
    if (m) {
      this.M._mkw_chain_free(m.chain);
      this.models.delete(id);
    }
  }

  // building

  buildText(text: string, opts: TextBuildOptions): ModelInfo {
    const before = this.liveBytes();
    const t0 = performance.now();
    const chain = this.M._mkw_chain_new(PLUGIN.ngram);
    const id = this.register(chain, "ngram", text.replace(/\s+/g, " "));
    const stats = this.withString(text, (ptr, len) =>
      this.withBuffer(16, (out) => {
        const status = this.M._mkw_text_build(chain, ptr, len, opts.order, opts.maxWords ?? -1,
          opts.rule === "sentence" ? 1 : 0, out);
        if (status < 0) {
          this.free(id);
          this.check(status, "building the chain");
        }
        return this.f64(out, 2);
      }),
    );
    const ms = performance.now() - t0;
    return this.info(id, ms, this.liveBytes() - before, { words: stats[0], sentences: stats[1] });
  }

  /** A small hand-specified chain (word plugin): labels + weight matrix. */
  buildCustom(spec: CustomChainSpec): ModelInfo {
    const before = this.liveBytes();
    const t0 = performance.now();
    const chain = this.M._mkw_chain_new(PLUGIN.word);
    const id = this.register(chain, "word");
    const n = spec.labels.length;
    for (const label of spec.labels) {
      this.check(this.withString(label, (p) => this.M._mkw_intern_word(chain, p)), "intern");
    }
    for (let i = 0; i < n; i++) {
      for (let j = 0; j < n; j++) {
        const w = Math.round(spec.weights[i * n + j] ?? 0);
        if (w > 0) this.check(this.M._mkw_add_transition(chain, i, j, w), "transition");
      }
    }
    return this.info(id, performance.now() - t0, this.liveBytes() - before);
  }

  /** An empty chain for step-by-step demos. */
  createChain(plugin: PluginKind): ModelInfo {
    const chain = this.M._mkw_chain_new(PLUGIN[plugin]);
    return this.info(this.register(chain, plugin), 0, 0);
  }

  internWord(id: number, word: string): number {
    const { chain, plugin } = this.model(id);
    const status = this.withString(word, (p) =>
      plugin === "ngram" ? this.M._mkw_intern_words(chain, p, -1) : this.M._mkw_intern_word(chain, p));
    this.check(status, "intern");
    return status;
  }

  addTransition(id: number, from: number, to: number, count = 1): void {
    this.check(this.M._mkw_add_transition(this.model(id).chain, from, to, count), "add transition");
  }

  buildBoard(board: BoardSpec): ModelInfo {
    const t0 = performance.now();
    const chain = this.M._mkw_chain_new(PLUGIN.cell);
    const id = this.register(chain, "cell");
    const status = this.withJumps(board, (p, n) =>
      this.M._mkw_snakes_build(chain, board.size, board.faces, p, n));
    if (status < 0) {
      this.free(id);
      this.check(status, "building the board");
    }
    return this.info(id, performance.now() - t0, 0);
  }

  classicBoard(): BoardSpec {
    return this.withBuffer(4, (nPtr) => {
      const ptr = this.M._mkw_snakes_classic(nPtr);
      const n = this.M.HEAPU32[nPtr >> 2];
      const raw = this.M.HEAP32.slice(ptr >> 2, (ptr >> 2) + 2 * n);
      const jumps: Array<[number, number]> = [];
      for (let i = 0; i < n; i++) jumps.push([raw[2 * i], raw[2 * i + 1]]);
      return { size: 100, faces: 6, jumps };
    });
  }

  private withJumps<T>(board: BoardSpec, fn: (ptr: number, n: number) => T): T {
    return this.withBuffer(Math.max(8, board.jumps.length * 8), (p) => {
      board.jumps.forEach(([from, to], i) => {
        this.M.HEAP32[(p >> 2) + 2 * i] = from;
        this.M.HEAP32[(p >> 2) + 2 * i + 1] = to;
      });
      return fn(p, board.jumps.length);
    });
  }

  // inspection

  private label(id: number, state: number): string {
    const { chain } = this.model(id);
    const n = this.M._mkw_state_label(chain, state, this.scratch, LABEL_CAP);
    return utf8.decode(this.M.HEAPU8.subarray(this.scratch, this.scratch + Math.min(n, LABEL_CAP - 1)));
  }

  labels(id: number, states: number[]): string[] {
    return states.map((s) => this.label(id, s));
  }

  stateInfo(id: number, state: number, limit = 24): StateInfo {
    const { chain } = this.model(id);
    const degree = this.M._mkw_state_out_degree(chain, state);
    const total = this.M._mkw_state_out_total(chain, state);
    const raw = degree ? this.u32(this.M._mkw_state_edges(chain, state), degree * 2) : new Uint32Array();
    const order = Array.from({ length: degree }, (_, i) => i).sort((a, b) => raw[2 * b + 1] - raw[2 * a + 1]);
    const edges: EdgeInfo[] = [];
    let restCount = total;
    for (const i of order.slice(0, limit)) {
      const target = raw[2 * i];
      const count = raw[2 * i + 1];
      restCount -= count;
      edges.push({ target, count, p: count / total, label: this.label(id, target),
        terminal: !!this.M._mkw_state_is_terminal(chain, target) });
    }
    return {
      id: state,
      label: this.label(id, state),
      terminal: !!this.M._mkw_state_is_terminal(chain, state),
      absorbing: !!this.M._mkw_state_is_absorbing(chain, state),
      startCount: this.M._mkw_state_start_count(chain, state),
      outDegree: degree,
      outTotal: total,
      edges,
      restCount,
      restP: total ? restCount / total : 0,
    };
  }

  /** Where the chain can go from `center`: top successors, two levels deep. */
  neighborhood(id: number, center: number, first = 8, second = 3): Neighborhood {
    const nodes = new Map<number, Neighborhood["nodes"][number]>();
    const links: Neighborhood["links"] = [];
    const { chain } = this.model(id);
    const add = (s: number, depth: number) => {
      if (!nodes.has(s)) {
        nodes.set(s, { id: s, label: this.label(id, s), depth,
          terminal: !!this.M._mkw_state_is_terminal(chain, s) });
      }
    };
    add(center, 0);
    const level1 = this.stateInfo(id, center, first).edges;
    for (const e of level1) {
      add(e.target, 1);
      links.push({ source: center, target: e.target, p: e.p });
    }
    for (const e of level1) {
      for (const f of this.stateInfo(id, e.target, second).edges) {
        add(f.target, 2);
        links.push({ source: e.target, target: f.target, p: f.p });
      }
    }
    return { nodes: [...nodes.values()], links };
  }

  textStats(id: number): TextStats {
    const { chain } = this.model(id);
    const n = this.M._mkw_chain_size(chain);
    let withSucc = 0;
    let edgeSum = 0;
    let deterministic = 0;
    let weighted = 0;
    let weight = 0;
    for (let s = 0; s < n; s++) {
      const degree = this.M._mkw_state_out_degree(chain, s);
      if (!degree) continue;
      withSucc++;
      edgeSum += degree;
      if (degree === 1) deterministic++;
      const total = this.M._mkw_state_out_total(chain, s);
      const base = this.M._mkw_state_edges(chain, s) >> 2;
      let h = 0;
      for (let k = 0; k < degree; k++) {
        const p = this.M.HEAPU32[base + 2 * k + 1] / total;
        h -= p * Math.log2(p);
      }
      weighted += total * h;
      weight += total;
    }
    return {
      branching: withSucc ? edgeSum / withSucc : 0,
      entropyBits: weight ? weighted / weight : 0,
      deterministicShare: withSucc ? deterministic / withSucc : 0,
      density: n ? this.M._mkw_chain_edge_count(chain) / (n * n) : 0,
    };
  }

  // sampling

  generate(id: number, opts: GenerateOptions): Generated[] {
    const { chain, plugin, corpus } = this.model(id);
    const maxLength = opts.maxLength ?? 30;
    const count = opts.count ?? 1;
    const rng = this.M._mkw_rng_new(opts.rng === "glibc" ? 1 : 0, opts.seed);
    const out: Generated[] = [];
    try {
      this.withBuffer(maxLength * 4, (states) =>
        this.withBuffer(16384, (buf) => {
          for (let i = 0; i < count; i++) {
            const n = this.M._mkw_generate(chain, rng, opts.start ?? -1,
              POLICY[opts.policy ?? "empirical"], SAMPLER[opts.sampler ?? "alias"], maxLength,
              opts.temperature ?? 1, states);
            this.check(n, "generating");
            const ids = Array.from(this.u32(states, n));
            let text: string;
            if (plugin === "cell") {
              text = ids.map((s) => this.label(id, s)).join(" → ");
            } else {
              const len = this.M._mkw_render(chain, states, n, opts.style === "compat" ? 0 : 1, buf, 16384);
              text = utf8.decode(this.M.HEAPU8.subarray(buf, buf + Math.min(len, 16383)));
            }
            out.push({ text, states: ids, verbatim: !!corpus && n > 1 && corpus.includes(text) });
          }
        }),
      );
    } finally {
      this.M._mkw_rng_free(rng);
    }
    return out;
  }

  /** One random step from `from` (null at a dead end). */
  step(id: number, from: number, seed: number, temperature = 1, sampler: "linear" | "alias" = "alias"): number | null {
    const rng = this.M._mkw_rng_new(0, seed);
    try {
      const s = this.M._mkw_next(this.model(id).chain, rng, from, SAMPLER[sampler], temperature);
      return s >= 0 ? s : null;
    } finally {
      this.M._mkw_rng_free(rng);
    }
  }

  randomStart(id: number, seed: number, policy: "compat" | "uniform" | "empirical" = "empirical"): number | null {
    const rng = this.M._mkw_rng_new(0, seed);
    try {
      const s = this.M._mkw_random_start(this.model(id).chain, rng, POLICY[policy]);
      return s >= 0 ? s : null;
    } finally {
      this.M._mkw_rng_free(rng);
    }
  }

  /**
   * Draw `draws` successors of `from` in the engine and count them per edge.
   * Returned in the same order as stateInfo(...).edges (most frequent first).
   */
  sampleCounts(id: number, from: number, draws: number, seed: number,
    sampler: "linear" | "alias" = "alias", temperature = 1): Array<{ target: number; count: number; p: number }> {
    const { chain } = this.model(id);
    const degree = this.M._mkw_state_out_degree(chain, from);
    const total = this.M._mkw_state_out_total(chain, from);
    const edges = this.u32(this.M._mkw_state_edges(chain, from), degree * 2);
    const rng = this.M._mkw_rng_new(0, seed);
    try {
      return this.withBuffer(Math.max(4, degree * 4), (counts) => {
        this.check(this.M._mkw_sample_counts(chain, rng, from, SAMPLER[sampler], temperature, draws, counts),
          "sampling");
        const c = this.u32(counts, degree);
        return Array.from({ length: degree }, (_, k) => ({ target: edges[2 * k], count: c[k], p: edges[2 * k + 1] / total }))
          .sort((a, b) => b.p - a.p);
      });
    } finally {
      this.M._mkw_rng_free(rng);
    }
  }

  /** The exact Vose alias table the engine builds for integer weights. */
  aliasTable(weights: number[]): { alias: number[]; threshold: number[]; total: number } {
    const n = weights.length;
    return this.withBuffer(n * 8, (w) =>
      this.withBuffer(n * 4, (a) =>
        this.withBuffer(n * 8, (t) =>
          this.withBuffer(8, (tot) => {
            this.M.HEAPF64.set(weights, w >> 3);
            this.check(this.M._mkw_alias_table(w, n, a, t, tot), "alias table");
            return { alias: Array.from(this.u32(a, n)), threshold: Array.from(this.f64(t, n)), total: this.M.HEAPF64[tot >> 3] };
          }),
        ),
      ),
    );
  }

  /** Lengths of `games` independent walks from `start` (capped at maxSteps). */
  simulate(id: number, start: number, seed: number, games: number, maxSteps: number): Uint32Array {
    const { chain } = this.model(id);
    const rng = this.M._mkw_rng_new(0, seed);
    const lengths = new Uint32Array(games);
    try {
      this.withBuffer((maxSteps + 1) * 4, (buf) => {
        for (let g = 0; g < games; g++) {
          const n = this.M._mkw_generate(chain, rng, start, 1, 1, maxSteps + 1, 1, buf);
          this.check(n, "simulating");
          lengths[g] = n - 1; // moves = states visited - 1
        }
      });
    } finally {
      this.M._mkw_rng_free(rng);
    }
    return lengths;
  }

  // mathematics

  matrix(id: number): Float64Array {
    const { chain } = this.model(id);
    const n = this.M._mkw_chain_size(chain);
    if (n > 600) throw new EngineError("too many states for a dense matrix", Status.TOO_LARGE);
    return this.withBuffer(Math.max(8, n * n * 8), (p) => {
      this.check(this.M._mkw_dense(chain, p), "dense matrix");
      return this.f64(p, n * n);
    });
  }

  stationary(id: number, damping = 1, tolerance = 1e-12, maxIterations = 10000): StationaryResult {
    const { chain } = this.model(id);
    const n = this.M._mkw_chain_size(chain);
    return this.withBuffer(Math.max(8, n * 8), (pi) =>
      this.withBuffer(24, (res) => {
        const status = this.M._mkw_stationary(chain, damping, tolerance, maxIterations, pi, res);
        const r = this.f64(res, 3);
        return { pi: this.f64(pi, n), iterations: r[0], residual: r[1], converged: r[2] === 1, status: -status };
      }),
    );
  }

  /** The distributions x_0, x_1 = x_0 P, ..., x_steps (power iteration). */
  powerTrajectory(id: number, x0: number[], steps: number, damping = 1): Float64Array[] {
    const { chain } = this.model(id);
    const n = this.M._mkw_chain_size(chain);
    const csr = this.M._mkw_csr_new(chain);
    if (!csr) throw new EngineError("csr", this.M._mkw_last_status());
    const out: Float64Array[] = [Float64Array.from(x0)];
    try {
      this.withBuffer(n * 8, (x) =>
        this.withBuffer(n * 8, (y) => {
          this.M.HEAPF64.set(x0, x >> 3);
          for (let k = 0; k < steps; k++) {
            this.M._mkw_power_step(csr, x, y, damping);
            const next = this.f64(y, n);
            out.push(next);
            this.M.HEAPF64.set(next, x >> 3);
          }
        }),
      );
    } finally {
      this.M._mkw_csr_free(csr);
    }
    return out;
  }

  absorbing(id: number): AbsorbingResult {
    const { chain } = this.model(id);
    const a = this.M._mkw_absorbing_new(chain);
    if (!a) {
      const empty = new Float64Array();
      return { ok: false, status: this.M._mkw_last_status(), transient: new Uint32Array(),
        absorbing: new Uint32Array(), N: empty, expected: empty, variance: empty, B: empty };
    }
    try {
      const t = this.M._mkw_abs_n_transient(a);
      const k = this.M._mkw_abs_n_absorbing(a);
      return {
        ok: true,
        status: 0,
        transient: this.u32(this.M._mkw_abs_transient(a), t),
        absorbing: this.u32(this.M._mkw_abs_absorbing(a), k),
        N: this.f64(this.M._mkw_abs_fundamental(a), t * t),
        expected: this.f64(this.M._mkw_abs_expected(a), t),
        variance: this.f64(this.M._mkw_abs_variance(a), t),
        B: this.f64(this.M._mkw_abs_absorption(a), t * k),
      };
    } finally {
      this.M._mkw_absorbing_free(a);
    }
  }

  private absorptionPmf(id: number, start: number, horizon: number): { pmf: Float64Array; tail: number } {
    const { chain } = this.model(id);
    return this.withBuffer((horizon + 2) * 8, (p) => {
      this.check(this.M._mkw_absorption_pmf(chain, start, horizon, p), "pmf");
      const all = this.f64(p, horizon + 2);
      return { pmf: all.slice(0, horizon + 1), tail: all[horizon + 1] };
    });
  }

  /** Everything the Snakes & Ladders Studio shows, in one call. */
  analyzeBoard(board: BoardSpec, limit = 60, horizon = 400): BoardAnalysis {
    const t0 = performance.now();
    const fail = (error: string): BoardAnalysis => ({
      ok: false, error, expected: NaN, sd: NaN, median: NaN, mode: NaN, pWithin: NaN, limit,
      expectedBySquare: new Float64Array(board.size), visits: new Float64Array(board.size),
      pmf: new Float64Array(), tail: NaN, ms: performance.now() - t0,
    });
    const valid = this.withJumps(board, (p, n) => this.M._mkw_snakes_validate(board.size, board.faces, p, n));
    if (valid < 0) return fail("This board breaks a rule (overlapping or out-of-range jumps).");
    const info = this.buildBoard(board);
    try {
      const a = this.absorbing(info.id);
      if (!a.ok) {
        return fail(a.status === Status.NOT_ABSORBING
          ? "Some squares can never reach the finish: a snake and a ladder form a loop."
          : this.statusText(-a.status));
      }
      const t = a.transient.length;
      const pos = new Map<number, number>();
      a.transient.forEach((s, i) => pos.set(s, i));
      const expectedBySquare = new Float64Array(board.size);
      const visits = new Float64Array(board.size);
      const startRow = pos.get(0) ?? 0;
      a.transient.forEach((s, i) => {
        expectedBySquare[s] = a.expected[i];
        visits[s] = a.N[startRow * t + i];
      });
      const { pmf, tail } = this.absorptionPmf(info.id, 0, horizon);
      let cdf = 0;
      let median = 0;
      let mode = 0;
      let pWithin = 0;
      for (let k = 0; k < pmf.length; k++) {
        cdf += pmf[k];
        if (k <= limit) pWithin = cdf;
        if (!median && cdf >= 0.5) median = k;
        if (pmf[k] > pmf[mode]) mode = k;
      }
      return {
        ok: true,
        expected: a.expected[startRow],
        sd: Math.sqrt(Math.max(0, a.variance[startRow])),
        median, mode, pWithin, limit, expectedBySquare, visits, pmf, tail,
        ms: performance.now() - t0,
      };
    } finally {
      this.free(info.id);
    }
  }

  // introspection (Under the Hood)

  heapSnapshot(id: number, maxNodes = 12): HeapSnapshot {
    const { chain } = this.model(id);
    const L = this.layout;
    const P = L.pointerSize;
    const H8 = this.M.HEAPU8;
    const read = (addr: number, size: number): number => {
      if (size === 8) {
        const lo = this.M.HEAPU32[addr >> 2];
        const hi = this.M.HEAPU32[(addr >> 2) + 1];
        return hi * 2 ** 32 + lo;
      }
      if (size === 4) return this.M.HEAPU32[addr >> 2];
      return H8[addr];
    };
    /** Exact hex, assembled from 32-bit halves (a double only has 53 bits). */
    const exactHex = (addr: number, size: number): string => {
      if (size !== 8) return read(addr, size).toString(16);
      const lo = this.M.HEAPU32[addr >> 2];
      const hi = this.M.HEAPU32[(addr >> 2) + 1];
      return hi ? hi.toString(16) + lo.toString(16).padStart(8, "0") : lo.toString(16);
    };
    const fields = (base: number, spec: Array<[string, number, number, FieldView["kind"]]>): FieldView[] =>
      spec.map(([name, offset, size, kind]) => ({
        name, offset, size, kind, value: read(base + offset, size), hex: exactHex(base + offset, size),
      }));

    const cf = L.chain.fields;
    const chainFields = fields(chain, [
      ["vt", cf.vt, P, "ptr"], ["nodes", cf.nodes, P, "ptr"], ["n_nodes", cf.n_nodes, 4, "u32"],
      ["cap_nodes", cf.cap_nodes, 4, "u32"], ["slots", cf.slots, P, "ptr"], ["n_slots", cf.n_slots, 4, "u32"],
      ["edge_keys", cf.edge_keys, P, "ptr"], ["edge_pos", cf.edge_pos, P, "ptr"],
      ["n_edge_slots", cf.n_edge_slots, 8, "u64"], ["n_edges", cf.n_edges, 8, "u64"],
    ]);
    const vt = this.M._mkw_chain_vtable(chain);
    const vf = L.vtable.fields;
    const vtFields = fields(vt, [
      ["name", vf.name, P, "ptr"], ["hash", vf.hash, P, "fn"], ["compare", vf.compare, P, "fn"],
      ["copy", vf.copy, P, "fn"], ["destroy", vf.destroy, P, "fn"],
      ["is_terminal", vf.is_terminal, P, "fn"], ["format", vf.format, P, "fn"],
    ]);

    const nodesPtr = this.M._mkw_chain_nodes(chain);
    const nNodes = this.M._mkw_chain_size(chain);
    const capNodes = read(chain + cf.cap_nodes, 4);
    const nf = L.node.fields;
    const stride = L.node.size;
    const nodes: HeapSnapshot["nodes"] = [];
    for (let s = 0; s < Math.min(nNodes, maxNodes); s++) {
      const addr = nodesPtr + s * stride;
      const nodeFields = fields(addr, [
        ["data", nf.data, P, "ptr"], ["edges", nf.edges, P, "ptr"], ["n_edges", nf.n_edges, 4, "u32"],
        ["cap_edges", nf.cap_edges, 4, "u32"], ["out_total", nf.out_total, 8, "u64"],
        ["hash", nf.hash, 8, "u64"], ["alias", nf.alias, P, "ptr"],
        ["start_count", nf.start_count, 4, "u32"], ["terminal", nf.terminal, 1, "u8"],
        ["is_starter", nf.is_starter, 1, "u8"],
      ]);
      const dataPtr = read(addr + nf.data, P);
      const dataSize = this.M._mkw_block_size(dataPtr);
      const edgesPtr = read(addr + nf.edges, P);
      const nEdges = read(addr + nf.n_edges, 4);
      const rawEdges = nEdges ? this.u32(edgesPtr, 2 * nEdges) : new Uint32Array();
      nodes.push({
        id: s,
        address: addr,
        label: this.label(id, s),
        raw: Array.from(this.bytes(addr, stride)),
        fields: nodeFields,
        data: { address: dataPtr, size: dataSize, tag: TAGS[this.M._mkw_block_tag(dataPtr)] ?? "other",
          raw: Array.from(this.bytes(dataPtr, Math.min(dataSize, 64))) },
        edges: {
          address: edgesPtr,
          size: this.M._mkw_block_size(edgesPtr),
          items: Array.from({ length: nEdges }, (_, k) => ({ target: rawEdges[2 * k], count: rawEdges[2 * k + 1] })),
        },
      });
    }
    return {
      pointerSize: P,
      chain: { address: chain, size: L.chain.size, fields: chainFields },
      vtable: { address: vt, size: L.vtable.size, name: this.M.UTF8ToString(read(vt + vf.name, P)), fields: vtFields },
      nodeArray: { address: nodesPtr, capacity: capNodes, length: nNodes, stride,
        blockSize: this.M._mkw_block_size(nodesPtr) },
      nodes,
      stats: this.allocStats(),
    };
  }
}
