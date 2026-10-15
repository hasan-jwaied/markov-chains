/** Plain data types shared by the engine (in the worker) and the UI. */
import type { Tag } from "./abi";

export type PluginKind = "word" | "ngram" | "cell";
export type StartPolicy = "compat" | "uniform" | "empirical";
export type Sampler = "linear" | "alias";
export type RngKind = "pcg32" | "glibc";
export type TerminalRule = "period" | "sentence";

export interface ModelInfo {
  id: number;
  plugin: PluginKind;
  states: number;
  edges: number;
  transitions: number;
  buildMs: number;
  words?: number;
  sentences?: number;
  /** Engine bytes held by this model alone. */
  bytes: number;
}

export interface TextBuildOptions {
  order: number;
  rule: TerminalRule;
  maxWords?: number;
}

export interface EdgeInfo {
  target: number;
  count: number;
  p: number;
  label: string;
  terminal: boolean;
}

export interface StateInfo {
  id: number;
  label: string;
  terminal: boolean;
  absorbing: boolean;
  startCount: number;
  outDegree: number;
  outTotal: number;
  /** Outgoing edges, most frequent first (possibly truncated). */
  edges: EdgeInfo[];
  /** Probability mass of the edges left out of `edges`. */
  restP: number;
  restCount: number;
}

export interface GenerateOptions {
  seed: number;
  rng?: RngKind;
  policy?: StartPolicy;
  sampler?: Sampler;
  temperature?: number;
  maxLength?: number;
  count?: number;
  style?: "clean" | "compat";
  start?: number;
}

export interface Generated {
  text: string;
  states: number[];
  /** True if the sentence occurs word for word in the training text. */
  verbatim: boolean;
}

export interface TextStats {
  /** Average out-degree over states that have successors. */
  branching: number;
  /** Visit-weighted entropy of the next word, in bits. */
  entropyBits: number;
  /** Share of states with exactly one successor. */
  deterministicShare: number;
  /** edges / states^2: how empty the transition matrix is. */
  density: number;
}

export interface CustomChainSpec {
  labels: string[];
  /** Row-major integer weights, labels.length^2 entries (0 = no edge). */
  weights: number[];
}

export interface StationaryResult {
  pi: Float64Array;
  iterations: number;
  residual: number;
  converged: boolean;
  status: number;
}

export interface AbsorbingResult {
  ok: boolean;
  status: number;
  transient: Uint32Array;
  absorbing: Uint32Array;
  /** N = (I - Q)^-1, transient x transient. */
  N: Float64Array;
  expected: Float64Array;
  variance: Float64Array;
  /** B = N R, transient x absorbing. */
  B: Float64Array;
}

export interface BoardSpec {
  size: number;
  faces: number;
  jumps: Array<[number, number]>;
}

export interface BoardAnalysis {
  ok: boolean;
  error?: string;
  expected: number;
  sd: number;
  median: number;
  mode: number;
  /** P(game finishes within `limit` moves). */
  pWithin: number;
  limit: number;
  /** Expected moves still to go from each square (index = square - 1). */
  expectedBySquare: Float64Array;
  /** Expected visits to each square in a game from square 1 (row of N). */
  visits: Float64Array;
  /** P(T = k) for k = 0..horizon, then the tail P(T > horizon). */
  pmf: Float64Array;
  tail: number;
  ms: number;
}

export interface AllocStats {
  liveBytes: number;
  liveBlocks: number;
  peakBytes: number;
  allocs: number;
  frees: number;
  failed: number;
  bytesByTag: Record<Tag, number>;
  blocksByTag: Record<Tag, number>;
}

export interface HeapEvent {
  kind: "alloc" | "free";
  address: number;
  size: number;
  tag: Tag;
}

export interface FieldView {
  name: string;
  offset: number;
  size: number;
  /** Numeric value; 64-bit fields above 2^53 are approximate here... */
  value: number;
  /** ...so they also carry their exact value as hex. */
  hex: string;
  kind: "ptr" | "u32" | "u64" | "u8" | "fn";
}

export interface NodeView {
  id: number;
  address: number;
  label: string;
  raw: number[];
  fields: FieldView[];
  data: { address: number; size: number; tag: Tag; raw: number[] };
  edges: { address: number; size: number; items: Array<{ target: number; count: number }> };
}

export interface HeapSnapshot {
  pointerSize: number;
  chain: { address: number; size: number; fields: FieldView[] };
  vtable: { address: number; size: number; name: string; fields: FieldView[] };
  nodeArray: { address: number; capacity: number; length: number; stride: number; blockSize: number };
  nodes: NodeView[];
  stats: AllocStats;
}

export interface Neighborhood {
  nodes: Array<{ id: number; label: string; terminal: boolean; depth: number }>;
  links: Array<{ source: number; target: number; p: number }>;
}
