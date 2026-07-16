#!/usr/bin/env python3
"""Benchmark regression gate (CI). Thresholds are ~5x the M-series baselines
in benchmarks/BASELINES.md — generous enough for shared CI runners, tight
enough to catch a lost fast path (the regressions v0.7 fixed were 5-30x).

Usage: check_thresholds.py <benchmark_json>
"""
import json
import sys

# benchmark name -> ceiling in ms
THRESHOLDS = {
    "BM_line_draw/1000000": 120.0,               # baseline 22 ms
    "BM_line_savefig_png/10000000/min_warmup_time:0.500": 1500.0,  # 268 ms
    "BM_scatter_draw/1000000": 1000.0,           # 187 ms
    "BM_line_svg/100000": 15.0,                  # 1.4 ms
    "BM_text_mathtext": 5.0,                     # 0.75 ms
}

data = json.load(open(sys.argv[1]))
failures = []
for bench in data["benchmarks"]:
    name = bench["name"]
    if name in THRESHOLDS:
        ms = bench["real_time"]  # unit is ms (all gated benches use kMillisecond)
        limit = THRESHOLDS[name]
        status = "FAIL" if ms > limit else "ok"
        print(f"{status:4} {name}: {ms:.1f} ms (limit {limit:.0f} ms)")
        if ms > limit:
            failures.append(name)

if failures:
    sys.exit(f"benchmark regression: {', '.join(failures)}")
print("all gated benchmarks within thresholds")
