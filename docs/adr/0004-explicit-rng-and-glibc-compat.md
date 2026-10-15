# 4. An explicit RNG, plus a glibc-compatible mode

**Status:** accepted

## Context

The original used `srand`/`rand() % n`. That is global state, it carries
modulo bias, and its sequence depends on the C library: Emscripten's musl
`rand()` differs from glibc's, so the browser could never match the original
output.

## Decision

Every sampling function takes an `mk_rng`. Two generators are provided:

* **PCG32** (default), with rejection-based unbiased bounded draws;
* **glibc-compat**, a bit-exact re-implementation of glibc's TYPE_3 additive
  feedback generator (including its treatment of seeds ≥ 2³¹), used only to
  reproduce the original program.

## Consequences

* Golden tests compare the new CLIs and the WebAssembly build with output
  recorded from the original binaries.
* Runs are reproducible and independent, with no hidden global state.
