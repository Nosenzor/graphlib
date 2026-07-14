# Changelog

All notable changes to graphlib. Format: [Keep a Changelog](https://keepachangelog.com/),
versions per [ROADMAP.md](ROADMAP.md). Pre-1.0: breaking changes allowed, always noted here.

## [Unreleased]

## [0.5.0] — 2026-07-15 — "It Moves"

CI-verified on all five tier-1 targets + the xvfb interactive job (82 tests incl. ASan).

### Added
- **Interactive windows** (`-DGRAPHLIB_INTERACTIVE=ON`, fetches GLFW — first tier-2
  optional dependency): one window per figure, retina-aware AGG rendering blitted via
  loader-free GL, live resize relayout; `plt::show()` / `plt::pause()`.
- **Navigation**: drag to pan, scroll to zoom at the cursor (log-correct, share-group
  aware), `h` home / `s` save PNG / `q` quit.
- **matplotlib-named events**: `fig.mpl_connect("button_press_event", ...)` with
  `xdata`/`ydata`/`inaxes` hit-testing (log axes inverse-mapped); reentrant-safe registry;
  headless-testable core (backends only translate native input).
- **`FuncAnimation`**: live frame-driven animation (~30 fps comfortably).
- CI: interactive smoke job under xvfb; window path verified on real macOS.
- Without the option, `plt::show()` throws a ValueError explaining how to enable it.

## [0.4.0] — 2026-07-14 — "Images & Fields"

CI-verified on all five tier-1 targets (76 tests incl. ASan).

### Added
- **Colormap engine**: 11 maps with oracle-exact LUTs generated from matplotlib
  (viridis family, coolwarm, RdBu, gray, jet, tab10/20), `Normalize`/`LogNorm`,
  under/over/bad semantics, `get_cmap` registry.
- **`imshow`**: origin 'upper' (inverted y) / 'lower', extents, nearest + bilinear,
  through the new `Renderer::draw_image` (AGG resampling blit; SVG embeds base64 PNG,
  like matplotlib). Full aspect control: `set_aspect('auto'|'equal'|ratio)`.
- **`pcolormesh`** (flat-shaded quads, non-uniform edges, log-axis aware, sticky edges).
- **`contour`/`contourf`** via marching squares (saddle-resolved) with MaxNLocator level
  selection; filled bands as ascending stacked fills (D13).
- **`colorbar`**: space-stealing axes with a gradient strip, right-side ticks + label.
- **scatter `c_array`**: value arrays through cmap/norm for per-point colors.
- Examples 11–14; fields golden scene; 77 oracle colormap/norm cases.

## [0.3.0] — 2026-07-14 — "Everyday Plots"

CI-verified on all five tier-1 targets (66 tests incl. ASan); ~180 oracle fixture cases.

### Added
- **The everyday plot types**: `bar`/`barh` (categorical labels, sticky-zero autoscale),
  `hist` (np.histogram-compatible), `fill_between`, `errorbar` (caps as `_`/`|` markers),
  `step` + drawstyles, `pie` (equal-aspect, frameless), `axh/axvline`, `axh/axvspan`,
  `hlines`/`vlines` — all through new Patch artists (Rectangle/Polygon/Wedge).
- **Layout**: `Figure::subplots(m, n)` + `GridSpec`, mpl-style `add_subplot(r, c, i)`,
  `sharex/sharey` (shared limits, hidden inner labels), `twinx`/`twiny` (far-side axes),
  `suptitle`, metrics-based `tight_layout` v1.
- **Log scales**: `set_x/yscale("log")` with oracle-exact `LogLocator` (including the
  `log(v)/log(b)` rounding subtlety), minor decade subs, minpos clipping, log-space margins,
  unicode-superscript decade labels (D10). Linear minor ticks via `AutoMinorLocator`.
- **rcParams + styles**: typed `rc()` store (~55 keys, KeyError on typos), `style::use`
  with `default`/`ggplot`/`dark_background`; artists capture rc at creation like mpl.
- **Full ScalarFormatter**: offset + scientific notation with the axis-end offset text
  (`1e6`, `+1e8`, `1e−9`), oracle-exact against all fixture cases.
- `set_xticks`/`set_yticks`, `minorticks_on`, `set_axis_off`, `set_aspect_equal`.
- Examples 07–10 (everyday plots, dashboard, log scales, style gallery); 88 new oracle
  fixture cases; 66 tests total.

## [0.2.0] — 2026-07-13 — "Raster & Type"

CI-verified on all five tier-1 targets; glyph-outline SVG goldens byte-identical and PNG
baselines within RMS ≤ 1.0 across macOS/Linux/Windows on x64 and arm64.

### Added
- **PNG output**: vendored AGG 2.4 (matplotlib's own rasterizer, ADR-0002) drives
  `savefig("*.png", {.dpi})` with `savefig.dpi='figure'` semantics and transparency.
- **Real typography**: stb_truetype over embedded DejaVu Sans/Bold — exact metrics,
  kerning, UTF-8, multi-line; text renders as glyph outlines on every backend
  (matplotlib's `svg.fonttype='path'` default); axis/label layout uses true extents.
- **`scatter()`** with pt² sizes (broadcast or per-point), cycle colors, `edgecolors='face'`.
- **`legend()`** with all matplotlib loc codes including a ported `'best'` placement scan,
  translucent frame, line/marker/scatter handles.
- Full matplotlib string-marker set (25 markers).
- PNG image-comparison harness (RMS ≤ 1.0, diff artifacts) + a backend conformance scene
  pinned as SVG byte-golden and AGG PNG baseline.

### Changed
- SVG no longer emits native `<text>` (matches mpl's default); goldens regenerated.

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
