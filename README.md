# Pressure-clocked reuse benchmark

From this directory on x86-64 Windows, with PowerShell and MSYS2 UCRT64 g++ on PATH, run:

```powershell
.\run.ps1
```

The command compiles, checks policy behavior, runs the experiment and regenerates RESULTS.md, summary.csv and raw.csv. environment.txt records CPU, OS, compiler, flags and SHA-256 of bench.cpp and cache.hpp, in that order. timing.txt records affinity and clock frequency. Replaying with the recorded toolchain reproduces the traces and hit counts. Timing varies with machine state.

## Question and unit of comparison

How much hit ratio and single-owner service capacity does each policy deliver from the same resident byte budget? The interface is read-through access to 64-bit keys with 64-bit inline values. A miss includes selection, eviction if full, and immediate insertion. There is no backing-store delay. Every implementation uses the same 16-way contiguous set lookup and full-key comparison. This isolates policy and layout effects in one cache engine. It measures neither network latency nor concurrent scalability.

The experiment assigns exactly the selected arena budget to each active policy. The number of sets is floor(budget / sizeof(Set<Policy>)). This includes resident keys, values, slot order, counters, sketch, size and alignment padding. The final partial set's bytes are unused. No policy allocates additional persistent state. The benchmark process also holds inactive arenas, trace and timing vectors, and executable code; process RSS is a different quantity. Hash range reduction uses the upper 32 bits of a fixed integer mixer. Different capacities change both the resident slot count and the specific collisions. The benchmark hash is reproducible and public.

## Policy definitions

- PCR uses the specified admission epoch of 16 misses per set, three-admission trial with a two-point bonus, two-bit credit, and oldest-admitted tie breaking. The admission itself sets seen. At an epoch boundary an untouched key loses one credit, then all seen flags clear. Slot IDs use bytes for simple rank movement. The benchmark counts this implementation's padding and larger order array.
- LRU maintains exact set-local MRU order. Strict LFU counts every resident access in 64 bits, starts at one on admission, and evicts the least recently used among equal minima. Every count fits for these traces.
- SIEVE walks from oldest toward newest, clears visited entries, stops on the first unvisited entry, and inserts the replacement at the newest end. It preserves the hand across misses. [SIEVE paper](https://www.usenix.org/conference/nsdi24/presentation/zhang-yazhuo)
- AdaptiveClimb follows Algorithm 1 with jump initially equal to associativity. Hit decrements jump and promotes by that distance with a clamp at the head. Miss increments jump, evicts the tail and inserts at zero-based rank ways minus jump, clamped during fill. [AdaptiveClimb paper](https://arxiv.org/html/2511.21235v1)
- W-TinyLFU is a fixed-window implementation with one window slot, fifteen main slots, twelve protected slots and LRU ordering within SLRU segments. Every access increments a four-row count-min sketch with sixteen four-bit counters per row. At a sample count of 160, counters halve and the count resets to 80. A displaced window entry beats the probation victim only on strictly higher estimated frequency. There is no separate Bloom doorkeeper or adaptive window tuner. Hashing, small set-local sketches, ties and window granularity influence this comparator's results. [TinyLFU paper](https://arxiv.org/abs/1512.00727)

## Workload and protocol

- **Zipf** draws independent ranks from a 131,072-key truncated Zipf distribution with exponent 1.05, using inverse CDF sampling.
- **Flash** uses that background plus a new disjoint population of 2,048 keys every 20,000 requests. For the first 6,000 requests in each phase, the generator selects a uniform flash key with probability approximately 0.8. The remaining requests use the background.
- **ScanLoop** alternates 20,000 cyclic accesses over an 8,192-key core and 20,000 single-use scan keys. It attacks retention through scans and loops. Seeds shift the loop offset. This is one deterministic stress family; it does not characterize every adversary.

The harness pre-generates traces, warms fresh state with 100,000 requests and measures the next 500,000. Each of five seeds has three repetitions. Each repetition shuffles policy order and alternates which budget runs first across repetitions. The raw trace digest and hit count allow paired comparisons. The same thread is pinned to the lowest allowed logical CPU. Frequency scaling, thermals, background load and interrupt placement are uncontrolled.

Throughput uses wall time over the complete measured loop, including trace reads and checksum accumulation. Two separate fresh replays sample one request at a pseudorandom offset in every block of 32. One uses steady_clock, the other LFENCE/RDTSC/LFENCE on a pinned thread. Each run's p99 is the nearest-rank order statistic over 15,625 individual cache operations. It includes hit and miss operations in their observed proportions. Timer overhead is included. TSC ticks are reference-counter units; no conversion to nanoseconds or core cycles is assumed. The wall timer's coarse quantization limits comparisons.

The summary reports mean hit percentage over seeds. For throughput and each p99, it takes the median repetition per seed and then the median across seeds. The throughput range is descriptive. Repetitions of a trace do not provide independent evidence about hit ratio. The adversarial seed offsets in particular should not be interpreted as independent workload populations.

## Verification and scope

Before timing, an independent timestamp-based PCR model checks victim identity under 100,000 randomized hit/admission decisions. Baseline fixtures check jump promotion, hand advancement, LRU and LFU eviction, and W-TinyLFU admission. Each timed configuration must reproduce its hit count and checksum on both latency replays. The harness also checks the checksum against expected values, and the runner checks trace pairing and completeness. These are local implementations with a shared lookup layout. The results support conclusions about this engine on these traces and hardware. Broader performance claims require other implementations, independent production traces, and concurrent tests.
