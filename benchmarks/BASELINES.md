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
