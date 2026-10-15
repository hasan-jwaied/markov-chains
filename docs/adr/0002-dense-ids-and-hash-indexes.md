# 2. Dense state ids with hash indexes for states and edges

**Status:** accepted

## Context

The original stored states in a linked list and found each one by a linear
scan, and grew each frequency array with `realloc(len + 1)`. Learning a corpus
was O(words × vocabulary): 18 s for *Moby-Dick*.

## Decision

* States live in one dense array; a state is its index (`mk_state`).
* An open-addressing table (linear probing, load ≤ ½, cached hashes) maps
  payloads to ids.
* A second table maps `(from << 32 | to)` to the edge's position, so repeated
  transitions are found without scanning.
* Every array grows by doubling.

## Consequences

* Interning and transition updates are O(1) expected; *Moby-Dick* builds in
  about 47 ms (≈380×).
* Edges keep insertion order, so the original linear sampler, and therefore
  the original output, is reproduced exactly.
* Integer ids make matrix export and the absorbing analysis straightforward.
* Cost: about 12 extra bytes per distinct transition for the edge index.
