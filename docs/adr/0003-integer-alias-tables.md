# 3. Exact integer alias tables

**Status:** accepted

## Context

Vose's alias method gives O(1) sampling. The textbook version uses floating
point probabilities, which round differently across platforms and cannot be
verified exactly.

## Decision

Build the table over integer counts. Scale each weight by n, so every column
holds exactly `W = Σ w_i` units. Draw a column uniformly and a threshold
uniformly in `[0, W)`.

## Consequences

* `P(i) = w_i / W` exactly; a unit test recomputes each outcome's share from
  the table and compares integers.
* Identical results natively and in WebAssembly.
* Requires `n · max(w)` to fit in 64 bits (checked; `MK_ERR_LIMIT` otherwise).
