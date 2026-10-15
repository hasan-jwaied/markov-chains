/**
 * A reactive handle to an engine model. `build` runs inside an effect, so any
 * state it reads synchronously becomes a dependency: change the state and the
 * model is rebuilt, the previous one freed in the worker, and stale results
 * from slower builds are discarded.
 */
import { engine } from "./client.svelte";
import type { ModelInfo } from "./types";

export interface ModelHandle {
  readonly info: ModelInfo | null;
  readonly id: number | null;
  readonly error: string | null;
  readonly loading: boolean;
}

export function engineModel(build: () => Promise<ModelInfo> | null): ModelHandle {
  let info = $state<ModelInfo | null>(null);
  let error = $state<string | null>(null);
  let loading = $state(false);
  let ticket = 0;
  let destroyed = false;

  $effect(() => {
    const pending = build();
    if (!pending) return;
    const mine = ++ticket;
    loading = true;
    pending.then(
      (result) => {
        if (mine !== ticket || destroyed) {
          void engine.free(result.id); // superseded by a newer build
          return;
        }
        const previous = info;
        info = result; // swap first, so the figures never go blank
        error = null;
        loading = false;
        if (previous) void engine.free(previous.id);
      },
      (err: unknown) => {
        if (mine !== ticket) return;
        error = err instanceof Error ? err.message : String(err);
        loading = false;
      },
    );
  });

  $effect(() => () => {
    destroyed = true;
    if (info) void engine.free(info.id);
  });

  return {
    get info() {
      return info;
    },
    get id() {
      return info?.id ?? null;
    },
    get error() {
      return error;
    },
    get loading() {
      return loading;
    },
  };
}
