# Benchmark baselines

Reference machine: Apple M-series MacBook (macOS, AppleClang, `--preset bench` = Release).
Regenerate with:

```
cmake --preset bench && cmake --build --preset bench
./build/bench/benchmarks/graphlib_bench
```

## v0.6.0 (pre-"Fast Path" — the numbers v0.7 exists to beat)

| Benchmark | Time | Roadmap target |
|---|---|---|
| line_draw/1k | 0.53 ms | |
| line_draw/100k | 8.9 ms | |
| line_draw/1M | 88.6 ms | |
| line_savefig_png/1M | 115 ms | |
| line_savefig_png/10M | **1379 ms** | **< 1000 ms** |
| scatter_draw/10k | 58.4 ms | |
| scatter_draw/100k | 579 ms | |
| scatter_draw/1M | **5648 ms** | **16.7 ms redraw (60 fps)** |
| line_svg/100k | 12.8 ms | |
| pcolormesh_draw/100² | 5.6 ms | |
| pcolormesh_draw/300² | 42.2 ms | |
| text_mathtext | 0.75 ms | |
| figure_lifecycle | 2.4 µs | |

Reading: the scatter path re-rasterizes the marker outline per point — marker
stamping is the big win. The 10M line is stroke-bound — path simplification
(mpl's PathSimplifier) collapses collinear/sub-threshold vertices before AGG.

## v0.7.0 (after "Fast Path")

| Benchmark | v0.6 | v0.7 | |
|---|---|---|---|
| line_draw/1M | 88.6 ms | **22 ms** | 4.0x |
| line_savefig_png/1M | 115 ms | **34 ms** | 3.4x |
| line_savefig_png/10M | 1379 ms | **268 ms** | 5.1x — target < 1 s met |
| scatter_draw/10k | 58.4 ms | **2.7 ms** | 21x |
| scatter_draw/100k | 579 ms | **19 ms** | 31x |
| scatter_draw/1M | 5648 ms | **187 ms** | 30x |
| line_svg/100k | 12.8 ms | **1.4 ms** | 9.3x |

The 60 fps 1M-scatter pan target lands at ~187 ms/frame — a further 10x
needs banded multithreaded blending; parked in the icebox with the MT/SIMD
item until a use case demands it (matplotlib itself is far slower here).

## Memory (v0.7 audit)

- `leaks --atExit` on the full test suite and a 3-round all-backends
  figure-lifecycle probe: **0 leaks** (macOS; LSan runs in the Linux CI job).
- Peak RSS for the 10M-point line savefig: **~650 MB** (~4x the raw data:
  artist copy + path points + display-space transform copy). Budget: stay
  within 5x input data for line plots.
