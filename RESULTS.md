# Measured cache replay results

Mean hit percentage over seeds. Timing columns use the median repetition per seed, then the median across seeds. The range covers the five seed-level throughput medians. See README.md for protocol and scope.

| KiB | Workload | Policy | Slots | Hit % | Mreq/s | Range (Mreq/s) | p99 ns | p99 TSC ticks |
|---:|---|---|---:|---:|---:|---:|---:|---:|
| 256 | Flash | AdaptiveClimb | 14976 | 73.042 | 40.04 | 38.85 to 40.82 | 100 | 178 |
| 256 | Flash | LFU | 10272 | 62.6 | 37.89 | 37.19 to 38.89 | 100 | 192 |
| 256 | Flash | LRU | 14976 | 73.944 | 46.64 | 46.15 to 46.98 | 100 | 172 |
| 256 | Flash | PCR | 14160 | 74.579 | 44.65 | 43.37 to 44.91 | 100 | 183 |
| 256 | Flash | SIEVE | 14976 | 74.312 | 50.85 | 49.28 to 51.15 | 100 | 163 |
| 256 | Flash | W-TinyLFU | 13440 | 68.21 | 34.97 | 33.69 to 35.42 | 100 | 209 |
| 256 | ScanLoop | AdaptiveClimb | 14976 | 33.076 | 42.87 | 41.89 to 43.14 | 100 | 184 |
| 256 | ScanLoop | LFU | 10272 | 37.005 | 31.92 | 31.59 to 32.26 | 100 | 208 |
| 256 | ScanLoop | LRU | 14976 | 28.105 | 60.8 | 59.86 to 62.76 | 100 | 172 |
| 256 | ScanLoop | PCR | 14160 | 42.121 | 36.83 | 36.44 to 37.25 | 100 | 183 |
| 256 | ScanLoop | SIEVE | 14976 | 46.446 | 53.71 | 52.78 to 54.67 | 100 | 124 |
| 256 | ScanLoop | W-TinyLFU | 13440 | 46.768 | 31.78 | 31.57 to 31.98 | 100 | 220 |
| 256 | Zipf | AdaptiveClimb | 14976 | 82.102 | 47.2 | 46.68 to 47.8 | 100 | 181 |
| 256 | Zipf | LFU | 10272 | 81.199 | 41.05 | 39.92 to 41.93 | 100 | 193 |
| 256 | Zipf | LRU | 14976 | 80.964 | 49.4 | 29.97 to 50.39 | 100 | 176 |
| 256 | Zipf | PCR | 14160 | 81.485 | 51.13 | 50.63 to 52.64 | 100 | 181 |
| 256 | Zipf | SIEVE | 14976 | 83.432 | 65.04 | 62.4 to 65.76 | 100 | 138 |
| 256 | Zipf | W-TinyLFU | 13440 | 82.437 | 37.07 | 29.06 to 38.18 | 100 | 215 |
| 64 | Flash | AdaptiveClimb | 3744 | 63.617 | 39.72 | 39.15 to 40.24 | 100 | 175 |
| 64 | Flash | LFU | 2560 | 50.457 | 41 | 39.72 to 41.21 | 100 | 179 |
| 64 | Flash | LRU | 3744 | 65.081 | 47.21 | 42.86 to 47.89 | 100 | 166 |
| 64 | Flash | PCR | 3536 | 66.072 | 42.38 | 42.17 to 42.93 | 100 | 181 |
| 64 | Flash | SIEVE | 3744 | 64.257 | 50.58 | 49.43 to 50.87 | 100 | 160 |
| 64 | Flash | W-TinyLFU | 3360 | 57.522 | 34.57 | 33.86 to 34.94 | 100 | 202 |
| 64 | ScanLoop | AdaptiveClimb | 3744 | 0 | 117.45 | 113.57 to 118.95 | 100 | 91 |
| 64 | ScanLoop | LFU | 2560 | 0 | 73.62 | 72.6 to 74.17 | 100 | 107 |
| 64 | ScanLoop | LRU | 3744 | 0 | 130.41 | 127.36 to 132.09 | 100 | 90 |
| 64 | ScanLoop | PCR | 3536 | 0 | 82.18 | 79.06 to 84.87 | 100 | 126 |
| 64 | ScanLoop | SIEVE | 3744 | 0 | 117.89 | 108.54 to 120.08 | 100 | 90 |
| 64 | ScanLoop | W-TinyLFU | 3360 | 15.69 | 42.78 | 41.96 to 43.11 | 100 | 177 |
| 64 | Zipf | AdaptiveClimb | 3744 | 71.103 | 43.08 | 42.78 to 43.95 | 100 | 177 |
| 64 | Zipf | LFU | 2560 | 72.199 | 39.03 | 38.17 to 39.42 | 100 | 185 |
| 64 | Zipf | LRU | 3744 | 69.027 | 49.8 | 49.1 to 49.95 | 100 | 166 |
| 64 | Zipf | PCR | 3536 | 71.275 | 45.09 | 39.14 to 46.69 | 100 | 180 |
| 64 | Zipf | SIEVE | 3744 | 74.553 | 58.04 | 55.87 to 59.55 | 100 | 128 |
| 64 | Zipf | W-TinyLFU | 3360 | 73.454 | 34.63 | 32.89 to 36.4 | 100 | 207 |
