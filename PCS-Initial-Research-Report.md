# PCS research project | Initial report
**17 November 2026**

PCS is the project label used in this report. The cache eviction algorithm implemented in the repository is **pressure-clocked reuse (PCR)**. **PCR2** is its first revision. These names match the implementation and benchmark tables.

## Findings

The question behind PCR is whether a cache can recognize a durable working set by recording which keys receive hits while their neighbors are being replaced. Original PCR gave the highest hit ratio on both flash-crowd workloads. Its main weakness appeared when entries were replaced before they could earn their first reuse credit. On the 64 KiB scan/loop trace it missed every measured request.

PCR2 gave entries a wider range of residence times and reduced the cost of victim selection. It recovered an 8.107% hit ratio on that trace and improved throughput over PCR in all six workload and budget combinations. The price was substantial on flash crowds. At 64 KiB the overall hit ratio fell by 7.419 percentage points. Neither version met the original goal of leading on both hit ratio and throughput across workloads.

## Architecture and design decisions

The C++20 prototype implements a read-through cache. A hash of the 64-bit key selects one set. The cache checks at most 16 full keys in contiguous storage. A hit returns the inline 64-bit value and updates local policy state. A miss selects a free slot or a victim, installs the key and value, and returns the value. Every policy uses this lookup and fill path. There is one owner thread in the experiment.

| Decision | Reason | Cost or consequence |
|---|---|---|
| Give each set its own admission clock | A burst of hits to one key need not age unrelated sets. Local replacement activity determines when reuse evidence expires. | A key's lifetime depends on the other keys hashing to its set. |
| Use 16 slots per set | Bounds lookup and eviction work and places candidates close together in memory. | A crowded set can evict a useful key while another set has spare capacity. |
| Give new entries a trial in PCR | A new burst key can survive long enough to receive its next hit. | Successive one-use keys can displace an entire set before any resident earns credit. |
| Reuse a trial slot and store hit masks in PCR2 | Many misses can pass through one slot. Two bit masks make aging and victim selection cheap. | Newly arriving burst keys compete for the same slot. Two masks retain less information about repeated use than PCR's credit counter. |
| Compare equal resident bytes | Charges every policy for its metadata, padding and unused arena tail. | Layout changes alter capacity and set collisions as well as policy execution cost. |

**Original PCR.** Each slot has a two-bit credit and a seen flag packed in one byte. Each set also keeps admission order, the identities of the last three admissions, and a phase counter. Admission sets the seen flag and starts credit at zero. The first hit in a later epoch adds one credit, up to three. After every 16 admissions, unseen entries lose one credit and all seen flags clear. On eviction, the score is credit plus two for a recent admission. The entry with the lowest score loses, with ties broken by oldest admission. Inspecting candidates in age order permits an early exit at score zero.

**PCR2.** Two 16-bit masks mark slots hit in the current or previous epoch. After the set fills, most misses replace the same trial slot. Every eighth miss, or when the trial slot has received a hit, the policy advances to the next unmarked slot in circular order. If all are marked it advances one slot. Replacement clears the victim's marks. Every 16 full-set misses the current mask becomes the previous mask and a new current mask starts. A first use still enters the cache immediately. The change is how long other entries survive subsequent admissions.

**Bounds and memory.** For width W, lookup takes O(W) full-key comparisons. At the fixed width of 16, both variants have bounded O(1) access and eviction work and O(C) resident storage for C slots. Original PCR may score all 16 candidates and age all 16 states. PCR2 selects among cold slots using a machine-word mask. Including keys, values, bookkeeping and alignment, the measured set sizes were 296 bytes for PCR and 264 bytes for PCR2. A 64 KiB arena therefore held 3,536 and 3,968 entries respectively. The experiment measures the combined benefit of changed policy and smaller layout. [Implementation](cache.hpp)

## Evaluation

The experiment used two arena sizes (64 and 256 KiB), five trace seeds (41 through 45), and three timing repetitions per seed. Each replay warmed the cache with 100,000 accesses and measured the following 500,000. All seven policies received the same generated key stream for a given workload and seed. The full comparison contains 630 runs on a pinned thread of an Intel Core i5-12450H under Windows, compiled with GCC 15.2.0 at -O3. [Protocol](README.md) · [Build and host record](environment.txt)

| Workload | Access pattern | Question |
|---|---|---|
| Zipf | Independent requests over 131,072 keys with exponent 1.05 | Can the cache retain a stable, skewed working set? |
| Flash crowd | Zipf background plus a fresh 2,048-key population every 20,000 requests. In the first 6,000 requests of each phase, roughly 80% select a burst key. | How quickly can new keys become useful without displacing the background? |
| Scan/loop | Alternate 20,000 accesses cycling through 8,192 core keys with 20,000 unique scan keys. | Can core entries survive enough intervening admissions to receive a second hit? |

**Comparators.** LRU keeps exact within-set recency. SIEVE uses a visited bit and a moving eviction hand. Strict LFU counts resident uses and breaks ties by recency. AdaptiveClimb adjusts promotion and insertion distance in an ordered set. W-TinyLFU combines a one-slot window, segmented LRU and a decaying frequency sketch. Its window is fixed for this experiment. All are local implementations in the same engine. [Policy definitions and references](README.md#policy-definitions)

**Metrics.** Hit ratio is measured hits divided by requests, averaged over seeds. Throughput is millions of complete accesses per second, including immediate fills. Each per-run p99 is the nearest-rank latency of 15,625 sampled operations in a separate replay. Throughput and p99 use the median repetition within each seed and then the median across seeds. The wall-clock p99 rounded to the same 100 ns timer bin throughout. The tables therefore also give p99 in directly measured timestamp-counter ticks, including timer overhead. The ticks are not calibrated to nanoseconds. [Raw measurements](raw.csv) · [Timing details](timing.txt)

## Results

Each cell below gives **hit percentage / million requests per second / p99 timestamp-counter ticks**. A larger hit percentage or throughput is better. A smaller p99 is better. [Seed-level throughput ranges](RESULTS.md)

### 64 KiB

| Policy | Zipf | Flash crowd | Scan/loop |
|---|---:|---:|---:|
| PCR | 71.275 / 44.32 / 175 | 66.072 / 41.74 / 181 | 0 / 78.1 / 123 |
| PCR2 | 72.726 / 66.65 / 120 | 58.653 / 66.27 / 116 | 8.107 / 90.98 / 118 |
| LRU | 69.027 / 47.74 / 170 | 65.081 / 47.56 / 169 | 0 / 112.47 / 92 |
| SIEVE | 74.553 / 57.2 / 132 | 64.257 / 50.03 / 160 | 0 / 112.78 / 92 |
| LFU | 72.199 / 37.39 / 187 | 50.457 / 40.37 / 183 | 0 / 73.05 / 108 |
| AdaptiveClimb | 71.103 / 40.05 / 181 | 63.617 / 39.54 / 182 | 0 / 113.5 / 103 |
| W-TinyLFU | 73.454 / 33.82 / 211 | 57.522 / 34.83 / 203 | 15.69 / 42.72 / 182 |

### 256 KiB

| Policy | Zipf | Flash crowd | Scan/loop |
|---|---:|---:|---:|
| PCR | 81.485 / 49.54 / 180 | 74.579 / 41.91 / 180 | 42.121 / 35.88 / 187 |
| PCR2 | 83.089 / 73.05 / 132 | 69.935 / 60.55 / 128 | 47.888 / 86.7 / 130 |
| LRU | 80.964 / 49.03 / 185 | 73.944 / 46.06 / 175 | 28.105 / 59.79 / 182 |
| SIEVE | 83.432 / 63.38 / 146 | 74.312 / 50.68 / 166 | 46.446 / 54.22 / 130 |
| LFU | 81.199 / 38.54 / 186 | 62.6 / 37.96 / 183 | 37.005 / 31.42 / 211 |
| AdaptiveClimb | 82.102 / 45.64 / 195 | 73.042 / 39.74 / 177 | 33.076 / 42.85 / 187 |
| W-TinyLFU | 82.437 / 36.7 / 223 | 68.21 / 34.33 / 217 | 46.768 / 31.66 / 221 |

## What explains the differences?

**The 64 KiB loop exceeded the capacity of every PCR set.** Even the least crowded set had 21 distinct loop keys competing for 16 slots. With zero credit, entries lost protection after their three-admission trial and the oldest entry was evicted first. Within a full loop, at least 20 distinct same-set competitors intervened before a return. The one-use scan added more pressure between loop phases. For seed 41, all 240,000 measured core requests missed. PCR2 let more of the scan pass through one slot, restoring some core hits. W-TinyLFU still retained more at this budget (15.690% overall versus 8.107%). At 256 KiB, PCR2 led all seven policies on ScanLoop for both hit ratio and throughput. [Diagnostic replay](diagnostics.csv)

**Victim selection was expensive when there were useful residents to distinguish.** On 64 KiB Zipf, seed 41, PCR inspected 970,406 candidates across 143,251 misses. SIEVE inspected 140,422 across 127,335 misses. On 64 KiB ScanLoop both inspected one candidate per miss, but PCR additionally swept its state array 31,239 times and maintained its recent-admission list. These observations explain why removing candidate scoring and per-slot aging was worth testing. They do not assign a cycle cost to each operation. PCR2 exceeded SIEVE's median throughput on five of six configurations. SIEVE remained faster on 64 KiB ScanLoop (112.78 versus 90.98 million requests/s). [Diagnostic counters](diagnostics.csv) · [Full timing results](RESULTS.md)

**Flash regressions came from the new population itself.** Pooled across seeds, burst-key hit ratio fell from 58.88% to 28.36% at 64 KiB and from 61.42% to 45.98% at 256 KiB. Including background traffic, the corresponding losses were 7.419 and 4.644 percentage points. A new key in PCR2 can be overwritten by the very next miss before its second request. Faster fills in the microbenchmark do not tell us whether the throughput gain would pay for those additional misses against a real backing store. [Flash subset counts](diagnostics.csv)

## Research status and next experiments

Original PCR is the stronger of the two for hit ratio on these flash traces. PCR2 is the stronger for scan retention and measured service throughput. SIEVE led stationary Zipf hit ratio at both budgets. The experiments establish a workload-dependent trade-off. The evidence comes from synthetic traces, one machine, a single owner thread and immediate fills. Seed ranges describe variation within this setup. They do not establish production or distributed performance.

Three follow-up experiments would resolve the main open questions:

1. Hold slot count and set mapping constant alongside the equal-byte comparison. This would separate changes in eviction behavior from the extra capacity purchased by smaller metadata.
2. Vary the trial-slot rotation interval on training traces, then fix the choice and evaluate on held-out bursts of different lengths and sizes. Track burst and background hits separately so an aggregate improvement cannot hide slower adoption of new keys.
3. Replay independent production traces with measured backing-store miss costs. Measure end-to-end latency under concurrency before drawing deployment conclusions.

## Reproduction and audit trail

From this folder on x86-64 Windows with MSYS2 UCRT64 g++ on PATH, run:

```powershell
.\run.ps1
```

The runner builds the C++ benchmark, checks policy fixtures, executes the paired replays and regenerates the measurements. Trace digests and hit counts support deterministic comparisons with the recorded toolchain. Timing depends on machine state. This report summarizes the recorded seven-policy run. [Runner](run.ps1) · [Harness](bench.cpp) · [Summary data](summary.csv) · [Per-run data](raw.csv)
