# Benchmarks

`bench/bench_main.c` links the **original assignment engine**
(`legacy/ex3b/markov_chain.c` + `linked_list.c`, bug-fixed but with its
original data structures) and **libmarkov** into one binary, and times both on
the same input with the same compiler flags.

```sh
make bench        # = cmake --preset release && cmake --build --preset release --target benchmark
```

* **Build** = tokenising an in-memory corpus and learning the order-1 chain
  (I/O excluded). The original engine runs the original `fill_database` logic;
  libmarkov runs `mk_text_build`. Both must produce the same number of states,
  or the benchmark aborts.
* **Sampling** = drawing a successor from one state with *d* outgoing
  transitions (Zipf-like counts): the original `get_next_random_node` (linear
  scan + `rand() %`), against `mk_chain_next` with the alias sampler.
* Times are medians of repeated runs; the original is run once above 100k words
  and skipped above 300k (it would take many minutes).
* Synthetic corpora draw words from a 200,000-word Zipf vocabulary in
  sentences of about 12 words, so the vocabulary grows with corpus size as it
  does in real text.

## Results

Native Release build, single thread. Absolute times depend on the machine; the ratios are what matter.

### Building the chain (order 1)

| corpus | words | states | transitions | original | libmarkov | speedup | libmarkov memory |
|---|---:|---:|---:|---:|---:|---:|---:|
| justdoit_tweets.txt | 78831 | 16166 | 44397 | 2.51 s | 12.6 ms | **199×** | 3.4 MiB |
| alice.txt | 26525 | 5267 | 16720 | 286.5 ms | 3.8 ms | **75×** | 1.6 MiB |
| frankenstein.txt | 75042 | 11597 | 44727 | 1.65 s | 10.5 ms | **158×** | 3.3 MiB |
| pride-and-prejudice.txt | 127360 | 14151 | 65453 | 3.35 s | 16.7 ms | **200×** | 3.6 MiB |
| moby-dick.txt | 212796 | 33090 | 128161 | 17.96 s | 47.2 ms | **380×** | 9.2 MiB |
| synthetic 10k words | 10000 | 4881 | 8516 | 181.2 ms | 1.6 ms | **116×** | 1.1 MiB |
| synthetic 30k words | 30000 | 12108 | 24532 | 1.16 s | 6.0 ms | **196×** | 2.3 MiB |
| synthetic 100k words | 100000 | 31034 | 77973 | 13.12 s | 26.6 ms | **494×** | 6.6 MiB |
| synthetic 300k words | 300000 | 68059 | 220156 | 140.30 s | 90.6 ms | **1548×** | 18.0 MiB |
| synthetic 1000k words | 1000000 | 141492 | 678182 | (skipped) | 327.0 ms | — | 50.4 MiB |
| synthetic 3000k words | 3000000 | 227696 | 1858359 | (skipped) | 2.64 s | — | 88.8 MiB |

### Sampling a successor (ns per draw, median of 7 × 400,000 draws)

| out-degree | original (linear scan, `rand()`) | linear scan (PCG32) | Vose alias (PCG32) | speedup |
|---:|---:|---:|---:|---:|
| 4 | 24.8 | 18.4 | 21.3 | 1.2× |
| 16 | 33.6 | 25.4 | 24.4 | 1.4× |
| 64 | 59.9 | 39.6 | 23.9 | 2.5× |
| 256 | 200.6 | 112.9 | 24.5 | 8.2× |
| 1024 | 742.2 | 415.5 | 23.9 | 31.0× |
| 4096 | 3545.7 | 1747.2 | 26.6 | 133.2× |

The alias sampler's cost is flat in the out-degree; the scan's grows linearly.
Part of the original's extra cost at small degrees is glibc's `rand()` itself.
The build speedup grows with the corpus because the original is
O(words × vocabulary): from ~75× on *Alice* to ~1,500× at 300,000 words.
Timings vary by roughly ±15% between runs.
