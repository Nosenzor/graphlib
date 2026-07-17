---
name: cross-platform
description: Keep graphlib green on macOS (arm64), Linux (x64, arm64), and Windows (x64, arm64) — CI matrix, compiler quirks, sanitizers, and local reproduction of remote failures. Use when CI fails on a platform you're not on, when touching CMake/toolchain/CI config, or when writing platform-sensitive code.
---

# Cross-platform playbook

## The matrix (GitHub Actions)

| runner | arch | role |
|--------|------|------|
| macos-14 (or newer arm) | arm64 | primary + ASan/UBSan job |
| ubuntu-24.04 | x64 | + Werror, clang-format check, gallery generation |
| ubuntu-24.04-arm | arm64 | full tests (free runner, **public repos only**) |
| windows-2025 | x64 | MSVC full tests |
| windows-11-arm | arm64 | MSVC arm64 tests (free runner, **public repos only**) |

macOS x86_64: build-only cross slice on the arm runner — `-DCMAKE_OSX_ARCHITECTURES=x86_64`
(tier 2, no test run). If the repo goes private, the two arm runners disappear from the free tier:
fall back to build-only cross-compilation or a self-hosted runner, and say so in AGENTS.md.

## Local reproduction

- **Linux (either arch) from the Mac:**
  `docker run --rm -v "$PWD":/src -w /src --platform linux/arm64 gcc:14 bash -c "cmake --preset ci-linux && cmake --build --preset ci-linux && ctest --preset ci-linux"`
  (swap `--platform linux/amd64` for x64.)
- **Windows:** no local repro on macOS. Read the MSVC log carefully (quirks below cover most
  cases); iterate on a `ci-debug/` branch rather than guess-committing to main.
- **Sanitizers:** `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`.
- Image diffs from CI: download the `image_failures` artifact (see `image-tests` skill).

## Compiler / platform quirks checklist

**MSVC** — `NOMINMAX` + `WIN32_LEAN_AND_MEAN` before any windows header (backends only);
`std::numbers` instead of `M_PI` (never `_USE_MATH_DEFINES`); `/utf-8` is set globally — keep
sources ASCII anyway; fix C4244/C4267 narrowing with explicit `static_cast` (keeps `-Wconversion`
parity); feature-test macros (`__cpp_…`), not compiler-id ifdefs.

**AppleClang/libc++** — `std::format` and newer chrono bits are gated by deployment target:
**banned in core** (AGENTS.md) — use `util::fmt_float`/`std::to_chars`. Check the cppreference
compiler-support table before reaching for any C++20 library feature fancier than span/ranges.

**GCC** — `-Wmaybe-uninitialized` false positives: restructure the code; don't pragma-suppress.

**ARM (both OSes)** — no x86 intrinsics outside `src/simd/` (compile-time dispatch); never
`long double` (differs across ABIs); float rounding can shift an AA pixel — that's what the PNG
tolerance is for; SVG must still be byte-identical.

**Everywhere** — little-endian assumed (`static_assert` in one TU); `std::filesystem::path` with
UTF-8 for all file paths; no locale-dependent formatting/parsing anywhere (`std::to_chars`/
`from_chars` only).

## CI job conventions

One `ci.yml`, matrix strategy. Every job: configure (`--preset ci-<os>`) → build → `ctest` →
upload `image_failures/` artifact on failure. Werror only in CI presets (`GRAPHLIB_WERROR=ON`),
so local dev never blocks on a new compiler's warnings.
