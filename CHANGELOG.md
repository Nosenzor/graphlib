# Changelog

All notable changes to graphlib. Format: [Keep a Changelog](https://keepachangelog.com/),
versions per [ROADMAP.md](ROADMAP.md). Pre-1.0: breaking changes allowed, always noted here.

## [Unreleased]

## [0.1.0] — 2026-07-13 — "First Light"

CI-verified on macOS arm64, Linux x86_64/arm64, Windows x86_64/arm64 (+ASan/UBSan);
golden SVGs byte-identical across all five targets.

### Added
- Core object model mirroring matplotlib: `Figure`, `Axes`, `Axis`, `Artist`, `Line2D`,
  `Text`, `Path` (mpl codes incl. NaN gaps), `Bbox`/`Affine2D` with the exact
  `transData = transScale + (transLimits + transAxes)` composition.
- Faithful ports, oracle-verified against matplotlib 3.10.8 fixtures: `MaxNLocator`
  (48/48 ranges), `ScalarFormatter` (incl. unicode minus), `nonsingular`, autoscale with
  5% margins, full color grammar (names/hex/gray/`C0`/`tab:`/`none`), tab10 cycle.
- `plot()` with matplotlib format strings (`"r--o"`), kwargs via designated initializers,
  11 markers with exact mpl geometry, dash patterns scaled by linewidth.
- Deterministic SVG backend (72 dpi pt canvas like mpl's): golden byte-equal tests,
  `<use>` marker defs, clipPath support, unicode-safe text with anchors.
- pyplot facade (`graphlib::pyplot`): figure registry, `gca`/`gcf`, `plot/title/xlabel/
  ylabel/xlim/ylim/grid/text/savefig/close/close_all`.
- Build: CMake presets (dev/asan/release/ci), Catch2 suite (32 tests), `update-baselines`
  target, GitHub Actions matrix (macOS arm64, Linux x64+arm64, Windows x64+arm64, ASan job,
  macOS x86_64 cross slice), 4 gallery examples.
- Project scaffolding: AGENTS.md, ROADMAP.md, docs/DESIGN.md (matplotlib 3.10.8 spec digest),
  docs/PARITY.md, ADR-0001/0002, repo skills.
