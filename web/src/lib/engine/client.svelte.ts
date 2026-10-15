/**
 * Main-thread handle to the engine worker. Every public Engine method is
 * available as an async function with the same signature:
 *
 *   const info = await engine.buildText(text, { order: 2, rule: "sentence" });
 */
import type { Engine } from "./engine";

type Methods = {
  [K in keyof Engine as Engine[K] extends (...args: never[]) => unknown ? K : never]: Engine[K];
};
export type EngineClient = {
  [K in keyof Methods]: Methods[K] extends (...args: infer A) => infer R ? (...args: A) => Promise<R> : never;
};

function connect(): EngineClient {
  const worker = new Worker(new URL("./worker.ts", import.meta.url), { type: "module" });
  const pending = new Map<number, { resolve: (v: unknown) => void; reject: (e: Error) => void }>();
  let nextId = 1;

  worker.onmessage = (event: MessageEvent<{ id: number; result?: unknown; error?: string }>) => {
    const { id, result, error } = event.data;
    const p = pending.get(id);
    if (!p) return;
    pending.delete(id);
    if (error !== undefined) p.reject(new Error(error));
    else p.resolve(result);
  };
  worker.onerror = (event) => {
    for (const p of pending.values()) p.reject(new Error(event.message || "engine worker crashed"));
    pending.clear();
  };

  return new Proxy({} as EngineClient, {
    get: (_target, method: string) =>
      (...args: unknown[]) =>
        new Promise((resolve, reject) => {
          const id = nextId++;
          pending.set(id, { resolve, reject });
          // Svelte $state values are Proxies, which structured clone rejects:
          // send plain snapshots instead.
          worker.postMessage({ id, method, args: $state.snapshot(args) });
        }),
  });
}

export const engine: EngineClient = connect();
