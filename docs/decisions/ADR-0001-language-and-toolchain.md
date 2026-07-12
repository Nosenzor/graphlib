# ADR-0001 — Language & toolchain

Date: 2026-07-12 · Status: **accepted**

## Decision

- **C++20** (no extensions). Minimum compilers: AppleClang 15, Clang 16, GCC 12, MSVC 19.38.
  Designated initializers are load-bearing (they are our "kwargs"). C++23 rejected for now:
  MSVC/AppleClang library support still uneven across our 5 targets.
- **CMake ≥ 3.24 + presets + Ninja**; single library target `graphlib::graphlib`, static by
  default (`BUILD_SHARED_LIBS` respected later; symbol-visibility work deferred until requested).
- **FetchContent** for optional/dev dependencies; tiny libs vendored under `third_party/`.
  No package manager required to build. vcpkg/conan are *packaging outputs* (v1.0), not inputs.
- **Catch2 v3** for tests; **Google Benchmark** (v0.7) for perf.
- `std::format`/new chrono banned in core until every tier-1 toolchain ships them without
  deployment-target traps (AppleClang availability annotations) — `util::fmt_float` wraps
  `std::to_chars`.

## Alternatives rejected

- Meson/Bazel: CMake is what C++ plotting-library consumers expect (and what CLion drives natively).
- Header-only design: rasterizer + fonts make compile times explode; classic lib + clean headers.
- Exceptions-free API: mpl raises; we mirror with `graphlib::ValueError`. A no-exceptions build
  profile can come post-1.0 if demanded.
