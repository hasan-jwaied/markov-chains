# 5. One worker, no WebAssembly threads

**Status:** accepted

## Context

Emscripten pthreads require `SharedArrayBuffer`, which browsers only expose to
cross-origin-isolated pages (COOP and COEP response headers). The app is served
inside the portfolio site, where `Cross-Origin-Embedder-Policy: require-corp`
would apply to the whole host and block ordinary cross-origin images.

## Decision

Build single-threaded and run the whole engine in one dedicated Web Worker,
with a small typed RPC layer to the UI.

## Consequences

* Long builds never block the main thread, and the app works on plain static
  hosting with the host's existing headers.
* The engine's global allocator statistics need no synchronisation.
* Parallel builds would need cross-origin isolation, or several independent
  workers with separate engine instances.
