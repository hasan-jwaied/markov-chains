/**
 * Struct layouts as reported by the compiled engine (mkw_layout in
 * wasm/api.c). The order of `raw` must match that function exactly.
 */
export interface StructLayout {
  size: number;
  fields: Record<string, number>;
}

export interface Layout {
  pointerSize: number;
  chain: StructLayout;
  node: StructLayout;
  edge: StructLayout;
  vtable: StructLayout;
  ngram: StructLayout;
  cell: StructLayout;
  alias: StructLayout;
}

const SPEC: Array<[keyof Omit<Layout, "pointerSize">, string[]]> = [
  ["chain", ["vt", "nodes", "n_nodes", "cap_nodes", "slots", "n_slots", "edge_keys",
    "edge_pos", "n_edge_slots", "n_edges"]],
  ["node", ["data", "edges", "n_edges", "cap_edges", "out_total", "hash", "alias",
    "start_count", "terminal", "is_starter"]],
  ["edge", ["target", "count"]],
  ["vtable", ["name", "hash", "compare", "copy", "destroy", "is_terminal", "format"]],
  ["ngram", ["order", "terminal", "size", "text"]],
  ["cell", ["number", "ladder_to", "snake_to", "is_last"]],
  ["alias", ["n", "total", "threshold", "alias"]],
];

export const LAYOUT_WORDS = 1 + SPEC.reduce((n, [, f]) => n + 1 + f.length, 0);

export function parseLayout(raw: ArrayLike<number>): Layout {
  if (raw.length < LAYOUT_WORDS) {
    throw new Error(`layout table too short: ${raw.length} < ${LAYOUT_WORDS}`);
  }
  const out = { pointerSize: raw[0] } as Layout;
  let i = 1;
  for (const [name, fields] of SPEC) {
    const s: StructLayout = { size: raw[i++], fields: {} };
    for (const f of fields) s.fields[f] = raw[i++];
    out[name] = s;
  }
  return out;
}

/** mk_node as compiled natively on x86-64 Linux (LP64), for the size comparison. */
export const NATIVE_X86_64 = {
  node: { size: 56, fields: { data: 0, edges: 8, n_edges: 16, cap_edges: 20, out_total: 24,
    hash: 32, alias: 40, start_count: 48, terminal: 52, is_starter: 53 } },
};
