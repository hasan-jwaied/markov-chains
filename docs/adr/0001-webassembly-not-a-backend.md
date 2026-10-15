# 1. Run the C engine in the browser as WebAssembly

**Status:** accepted

## Context

The portfolio site must show the C engine working live. The options were a
server running the native engine behind an API, or the engine compiled to
WebAssembly and running client-side.

## Decision

Compile `libmarkov` with Emscripten and run it in a Web Worker. Host the site
statically, as part of the portfolio site (at `/markov`).

## Consequences

* The code a visitor runs *is* the C engine, not a reimplementation, and the
  golden tests prove the browser build matches the native one byte for byte.
* No server cost, no cold starts, and the site works offline once loaded.
* The engine must stay free of filesystem and threads (see ADR 5), and its
  boundary must be a flat C ABI.
