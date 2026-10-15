/**
 * The raw C ABI exported by wasm/api.c, typed for TypeScript.
 *
 * Pointers are plain numbers (byte offsets into linear memory). Functions
 * that can fail return a negative mk_status. Linear memory can grow, which
 * replaces the HEAP* views, so always read them from the module, never cache.
 */
import createMarkovModule from "../../wasm/markov.mjs";

type Ptr = number;

export interface MarkovModule {
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
  HEAPU32: Uint32Array;
  HEAPF64: Float64Array;
  UTF8ToString(ptr: Ptr, maxBytes?: number): string;

  _mkw_malloc(size: number): Ptr;
  _mkw_free(ptr: Ptr): void;
  _mkw_status_str(status: number): Ptr;
  _mkw_alloc_stats(out: Ptr): void;
  _mkw_heap_trace(enabled: number): void;
  _mkw_heap_events(out: Ptr, max: number): number;
  _mkw_heap_events_dropped(): number;
  _mkw_block_size(ptr: Ptr): number;
  _mkw_block_tag(ptr: Ptr): number;

  _mkw_chain_new(plugin: number): Ptr;
  _mkw_chain_free(chain: Ptr): void;
  _mkw_text_build(chain: Ptr, text: Ptr, len: number, order: number, maxWords: number,
    rule: number, statsOut: Ptr): number;
  _mkw_snakes_build(chain: Ptr, size: number, faces: number, jumps: Ptr, n: number): number;
  _mkw_snakes_validate(size: number, faces: number, jumps: Ptr, n: number): number;
  _mkw_snakes_classic(nOut: Ptr): Ptr;
  _mkw_add_transition(chain: Ptr, from: number, to: number, count: number): number;
  _mkw_intern_word(chain: Ptr, word: Ptr): number;
  _mkw_intern_words(chain: Ptr, words: Ptr, terminal: number): number;
  _mkw_chain_size(chain: Ptr): number;
  _mkw_chain_edge_count(chain: Ptr): number;
  _mkw_chain_transition_total(chain: Ptr): number;
  _mkw_state_label(chain: Ptr, state: number, buf: Ptr, cap: number): number;
  _mkw_state_is_terminal(chain: Ptr, state: number): number;
  _mkw_state_is_absorbing(chain: Ptr, state: number): number;
  _mkw_state_out_degree(chain: Ptr, state: number): number;
  _mkw_state_out_total(chain: Ptr, state: number): number;
  _mkw_state_start_count(chain: Ptr, state: number): number;
  _mkw_state_edges(chain: Ptr, state: number): Ptr;
  _mkw_state_data(chain: Ptr, state: number): Ptr;

  _mkw_rng_new(kind: number, seed: number): Ptr;
  _mkw_rng_free(rng: Ptr): void;
  _mkw_random_start(chain: Ptr, rng: Ptr, policy: number): number;
  _mkw_next(chain: Ptr, rng: Ptr, from: number, sampler: number, temperature: number): number;
  _mkw_generate(chain: Ptr, rng: Ptr, start: number, policy: number, sampler: number,
    maxLength: number, temperature: number, out: Ptr): number;
  _mkw_sample_counts(chain: Ptr, rng: Ptr, from: number, sampler: number, temperature: number,
    draws: number, counts: Ptr): number;
  _mkw_alias_table(weights: Ptr, n: number, aliasOut: Ptr, thresholdOut: Ptr, totalOut: Ptr): number;
  _mkw_render(chain: Ptr, states: Ptr, n: number, style: number, buf: Ptr, cap: number): number;

  _mkw_dense(chain: Ptr, out: Ptr): number;
  _mkw_csr_new(chain: Ptr): Ptr;
  _mkw_csr_free(csr: Ptr): void;
  _mkw_power_step(csr: Ptr, x: Ptr, y: Ptr, damping: number): number;
  _mkw_stationary(chain: Ptr, damping: number, tolerance: number, maxIter: number,
    pi: Ptr, resultOut: Ptr): number;
  _mkw_absorbing_new(chain: Ptr): Ptr;
  _mkw_absorbing_free(a: Ptr): void;
  _mkw_last_status(): number;
  _mkw_abs_n_transient(a: Ptr): number;
  _mkw_abs_n_absorbing(a: Ptr): number;
  _mkw_abs_transient(a: Ptr): Ptr;
  _mkw_abs_absorbing(a: Ptr): Ptr;
  _mkw_abs_fundamental(a: Ptr): Ptr;
  _mkw_abs_expected(a: Ptr): Ptr;
  _mkw_abs_variance(a: Ptr): Ptr;
  _mkw_abs_absorption(a: Ptr): Ptr;
  _mkw_absorption_pmf(chain: Ptr, start: number, maxSteps: number, pmf: Ptr): number;

  _mkw_layout(out: Ptr, cap: number): number;
  _mkw_chain_nodes(chain: Ptr): Ptr;
  _mkw_chain_vtable(chain: Ptr): Ptr;
}

export async function loadModule(): Promise<MarkovModule> {
  const factory = createMarkovModule as unknown as () => Promise<MarkovModule>;
  return factory();
}

/** Status codes, mirroring engine/include/markov/status.h. */
export const Status = {
  OK: 0,
  NOMEM: 1,
  INVALID: 2,
  EMPTY: 3,
  NO_START: 4,
  DEAD_END: 5,
  LIMIT: 6,
  TOO_LARGE: 7,
  NOT_ABSORBING: 8,
  SINGULAR: 9,
  NOT_CONVERGED: 10,
} as const;

export const PLUGIN = { word: 0, ngram: 1, cell: 2 } as const;
export const POLICY = { compat: 0, uniform: 1, empirical: 2 } as const;
export const SAMPLER = { linear: 0, alias: 1 } as const;

/** Allocation tags, mirroring mk_alloc_tag. */
export const TAGS = ["other", "chain", "nodes", "edges", "index", "state", "alias", "math", "text"] as const;
export type Tag = (typeof TAGS)[number];
