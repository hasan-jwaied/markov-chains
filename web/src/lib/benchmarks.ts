/**
 * Native benchmark results (bench/bench_main.c, Release build, single
 * thread). Regenerate with:
 *   cmake --preset release && cmake --build --preset release --target benchmark
 * The "original" column is the assignment's own engine (legacy/ex3b).
 */
export interface BuildRow {
  corpus: string;
  words: number;
  states: number;
  originalS: number | null;
  libmarkovS: number;
}

export const BUILD: BuildRow[] = [
  { corpus: "Alice in Wonderland", words: 26525, states: 5267, originalS: 0.2865, libmarkovS: 0.0038 },
  { corpus: "Tweet corpus (assignment)", words: 78831, states: 16166, originalS: 2.51, libmarkovS: 0.0126 },
  { corpus: "Frankenstein", words: 75042, states: 11597, originalS: 1.65, libmarkovS: 0.0105 },
  { corpus: "Pride and Prejudice", words: 127360, states: 14151, originalS: 3.35, libmarkovS: 0.0167 },
  { corpus: "Moby-Dick", words: 212796, states: 33090, originalS: 17.96, libmarkovS: 0.0472 },
  { corpus: "Synthetic 300k words", words: 300000, states: 68059, originalS: 140.3, libmarkovS: 0.0906 },
  { corpus: "Synthetic 3M words", words: 3000000, states: 227696, originalS: null, libmarkovS: 2.64 },
];

export interface SampleRow {
  degree: number;
  originalNs: number;
  aliasNs: number;
}

export const SAMPLING: SampleRow[] = [
  { degree: 4, originalNs: 24.8, aliasNs: 21.3 },
  { degree: 16, originalNs: 33.6, aliasNs: 24.4 },
  { degree: 64, originalNs: 59.9, aliasNs: 23.9 },
  { degree: 256, originalNs: 200.6, aliasNs: 24.5 },
  { degree: 1024, originalNs: 742.2, aliasNs: 23.9 },
  { degree: 4096, originalNs: 3545.7, aliasNs: 26.6 },
];

export function fmtSeconds(s: number): string {
  if (s < 1e-3) return `${(s * 1e6).toFixed(0)} µs`;
  if (s < 1) return `${(s * 1e3).toFixed(1)} ms`;
  return `${s.toFixed(2)} s`;
}
