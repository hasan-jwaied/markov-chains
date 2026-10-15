// Runs the real WebAssembly engine under Node (vitest).
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { beforeAll, describe, expect, it } from "vitest";
import { Engine } from "./engine";

let engine: Engine;
const root = resolve(__dirname, "../../../..");

beforeAll(async () => {
  engine = await Engine.create();
});

describe("Engine (WebAssembly)", () => {
  it("reports the wasm32 struct layout", () => {
    expect(engine.getLayout().pointerSize).toBe(4);
    expect(engine.getLayout().edge.size).toBe(8);
  });

  it("builds a text chain and generates reproducibly", () => {
    const text = readFileSync(resolve(root, "data/corpora/gutenberg/alice.txt"), "utf8");
    const info = engine.buildText(text, { order: 2, rule: "sentence" });
    expect(info.states).toBeGreaterThan(10000);
    const a = engine.generate(info.id, { seed: 7, count: 3 });
    const b = engine.generate(info.id, { seed: 7, count: 3 });
    expect(a.map((g) => g.text)).toEqual(b.map((g) => g.text));
    const start = a[0].states[0];
    const s = engine.stateInfo(info.id, start, 5);
    expect(s.edges.length).toBeGreaterThan(0);
    expect(s.edges.reduce((sum, e) => sum + e.p, 0) + s.restP).toBeCloseTo(1, 12);
    const stats = engine.textStats(info.id);
    expect(stats.entropyBits).toBeGreaterThan(0);
    engine.free(info.id);
  });

  it("solves the classic board exactly", () => {
    const board = engine.classicBoard();
    expect(board.jumps).toHaveLength(20);
    const a = engine.analyzeBoard(board);
    expect(a.ok).toBe(true);
    expect(a.expected).toBeCloseTo(47.5712, 4);
    expect(a.pWithin).toBeCloseTo(0.7431, 4);
  });

  it("detects boards that trap the player", () => {
    const a = engine.analyzeBoard({ size: 20, faces: 1, jumps: [[5, 10], [10, 5]] });
    expect(a.ok).toBe(false);
    expect(a.error).toMatch(/loop/);
  });

  it("computes the stationary distribution of a custom chain", () => {
    const info = engine.buildCustom({ labels: ["A", "B"], weights: [7, 3, 2, 8] });
    const r = engine.stationary(info.id);
    expect(r.converged).toBe(true);
    expect(r.pi[0]).toBeCloseTo(0.4, 10);
    const traj = engine.powerTrajectory(info.id, [1, 0], 50);
    expect(traj[50][0]).toBeCloseTo(0.4, 6);
    engine.free(info.id);
  });

  it("exposes real memory and frees everything", () => {
    const before = engine.allocStats().liveBlocks;
    const info = engine.createChain("word");
    const a = engine.internWord(info.id, "hello");
    const b = engine.internWord(info.id, "world.");
    engine.addTransition(info.id, a, b, 3);
    const snap = engine.heapSnapshot(info.id);
    expect(snap.vtable.name).toBe("word");
    expect(snap.nodes[0].label).toBe("hello");
    expect(snap.nodes[0].edges.items).toEqual([{ target: b, count: 3 }]);
    expect(String.fromCharCode(...snap.nodes[0].data.raw.slice(0, 5))).toBe("hello");
    // 64-bit fields must be exact: compare the decoded hash with its raw bytes.
    const off = engine.getLayout().node.fields.hash;
    const bytes = snap.nodes[0].raw.slice(off, off + 8).reverse();
    const fromBytes = bytes.map((b) => b.toString(16).padStart(2, "0")).join("").replace(/^0+/, "");
    expect(snap.nodes[0].fields.find((f) => f.name === "hash")?.hex).toBe(fromBytes);
    engine.free(info.id);
    expect(engine.allocStats().liveBlocks).toBe(before);
  });
});
