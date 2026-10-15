/// <reference lib="webworker" />
/**
 * Web Worker hosting the WebAssembly engine, so building a chain from a whole
 * novel never blocks the page. Requests are {id, method, args}; replies are
 * {id, result} or {id, error}.
 */
import { Engine } from "./engine";

const ready = Engine.create();

self.onmessage = async (event: MessageEvent<{ id: number; method: string; args: unknown[] }>) => {
  const { id, method, args } = event.data;
  try {
    const engine = await ready;
    const fn = (engine as unknown as Record<string, (...a: unknown[]) => unknown>)[method];
    if (typeof fn !== "function") throw new Error(`no engine method '${method}'`);
    const result = fn.apply(engine, args);
    self.postMessage({ id, result }, { transfer: transferables(result) });
  } catch (err) {
    self.postMessage({ id, error: err instanceof Error ? err.message : String(err) });
  }
};

/** Hand typed-array buffers over instead of copying them. */
function transferables(value: unknown, out: Transferable[] = [], seen = new Set<unknown>()): Transferable[] {
  if (value === null || typeof value !== "object" || seen.has(value)) return out;
  seen.add(value);
  if (ArrayBuffer.isView(value)) {
    if (value.buffer instanceof ArrayBuffer && !out.includes(value.buffer)) out.push(value.buffer);
    return out;
  }
  for (const v of Object.values(value as Record<string, unknown>)) transferables(v, out, seen);
  return out;
}
