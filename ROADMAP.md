# ROADMAP — graphlib

## Principles

1. **Every milestone ends in a tag you can build and play with in under 5 minutes.** Each release
   below has a "Play with it" section — that's the acceptance test.
2. **Vertical slices over horizontal layers** — a feature lands with API + tests + example, never
   "framework now, usable later".
3. **matplotlib parity drives scope** — [docs/PARITY.md](docs/PARITY.md) is the shopping list;
   [docs/DESIGN.md](docs/DESIGN.md) is the spec.
4. **Static-image quality before interactivity** — a plotting library that saves beautiful files is
   useful on day one; a window is v0.5.
5. Versioning: `0.MINOR.PATCH`, minor = milestone. Breaking changes allowed pre-1.0 (changelogged).

**Cadence assumption:** AI-assisted, part-time ⇒ roughly 1–3 weeks per milestone, v1.0 horizon
≈ 6–9 months. Sizes: S ≈ days, M ≈ 1–2 weeks, L ≈ 2–3 weeks part-time.

## The ladder

| ver | codename | size | you get to play with |
|-----|----------|------|----------------------|
| 0.1 | **First Light** | L | line plots — ticks, labels, grid, markers, colors → SVG |
| 0.2 | **Raster & Type** | L | PNG at any dpi, real text, legend, scatter, image-diff tests |
| 0.3 | **Everyday Plots** | L | bar/hist/fill/errorbar/pie, subplots, log scales, styles |
| 0.4 | **Images & Fields** | M–L | imshow, pcolormesh, contour, colormaps, colorbar |
| 0.5 | **It Moves** | L | a window: pan/zoom/keys, events, live animation |
| 0.6 | **Publication Grade** | L | PDF output, mathtext, date axes, annotations |
| 0.7 | **Fast Path** | M | 10M-point lines, benchmarks, docs site |
| 1.0 | **Parity Core** | M | API freeze, vcpkg packaging, conformance report |

---

## v0.1.0 — "First Light" (SVG line plots, the object model end-to-end) ✅ RELEASED 2026-07-13

**Goal:** prove the whole architecture with the thinnest complete slice: `plt::plot` → correct
ticks/layout → SVG file. If this milestone is right, everything after is additive.

> Shipped as scoped (multi-pair `plot(x1,y1,x2,y2)` deferred to v0.3). Exit criteria met:
> CI green on all 5 targets, golden SVGs byte-identical cross-platform, hero example verified
> side-by-side against python matplotlib.

**Bootstrap (first tasks):**
- CMake + presets (dev/asan/release) + Ninja; Catch2 wired; `.clang-format`; `version.hpp` generation.
- GitHub Actions matrix: macos-14 (arm64), ubuntu-24.04, ubuntu-24.04-arm, windows-2025,
  windows-11-arm (+ macOS x86_64 cross-compile build-only). Werror + ASan jobs.
- LICENSE (proposed: MIT — confirm), README stub with the hero example.
- `tests/fixtures/generate.py` (oracle fixture generator — see `mpl-parity` skill).

**In scope (library):**
- Core types: `Bbox`, `Affine2D`/`Transform` (+ the exact `transData` composition, DESIGN §4),
  `Path` (+codes, NaN gaps), `Color` (full spec grammar §9), `GraphicsContext`, `Renderer` interface.
- Artists: `Artist` base (zorder/visible/alpha/transform), `Figure`, `Axes` (single, default rect),
  `XAxis`/`YAxis` + major `Tick`s, `Spine`s, `Line2D` (solid + dashed styles, dash patterns §6),
  basic markers (`o s ^ v D + x . *` as Paths), `Text` (native SVG text; estimated metrics OK
  because the default layout uses fixed margins, like mpl).
- Ticking: `MaxNLocator` (ported `_raw_ticks`, steps [1,2,2.5,5,10]) + `ScalarFormatter`
  (fixed notation only) + autoscale with 5% margins.
- SVG backend: deterministic output (DESIGN §13), `<g>` structure, dashes, caps/joins.
- pyplot facade: `figure, plot, title, xlabel, ylabel, xlim, ylim, grid, savefig, close`.
  Format shorthand `"r--o"` fully parsed.
- Tests: unit (transforms, ticker-vs-fixtures, color parsing, path), SVG golden tests.
  Examples: `01_hello_lines`, `02_color_cycle`, `03_fmt_and_markers`, `04_limits_grid`.

**Out (explicitly):** legend (0.2), PNG (0.2), any second Axes (0.3), log scale (0.3).

**Play with it:**
```bash
git clone <repo> && cd graphlib
cmake --preset release && cmake --build --preset release
./build/release/examples/01_hello_lines     # → hello.svg, open in any browser
```
Also consumable via `FetchContent` from your own CMake project (smoke-tested in CI).

**Exit criteria:** CI green on all 5 targets; the 4 examples' SVGs are visually indistinguishable
(side by side) from the same script run in python matplotlib, modulo fonts; golden tests
byte-stable across platforms.

---

## v0.2.0 — "Raster & Type" (PNG, real text metrics, legend, scatter) — ✅ feature-complete 2026-07-13

**Goal:** pixel output and typography — the release where output looks *professional*.

> Shipped as scoped (the compare.py side-by-side script folded into the gallery workflow).
> Bonus beyond scope: SVG text-as-paths (mpl's svg.fonttype default) so output renders
> identically in every viewer.

**In scope:**
- Vendor AGG 2.4 (ADR-0002 re-check first) → `AggRenderer`: AA strokes/fills, dashes, clipping;
  PNG via stb_image_write; `savefig("*.png", {.dpi=…})`, `figure.dpi`, transparent option.
- Text stack: stb_truetype + embedded DejaVu Sans/Bold/Mono → `FontManager` with real
  width/height/descent + kerning for **all** backends (SVG layout improves too); multi-line; rotation.
- Full marker set (all string markers, DESIGN §5), markeredge/face/width.
- `Legend` v1: loc strings incl. `'best'` (port mpl's candidate-scan), frame, line+marker handles.
- `scatter()` — PathCollection with per-point sizes, uniform/per-point colors (cmap comes in 0.4).
- **Image-comparison test harness** + `update-baselines` target (activates the `image-tests` skill);
  backend conformance scene (activates `new-backend` checklist for AGG).

**Play with it:** same examples now in crisp PNG at any dpi + `05_scatter`, `06_legend`;
`compare.py` script renders the twin python figure next to ours.

**Exit criteria:** PNG baselines stable across all tier-1 platforms within RMS ≤ 1.0; a text-heavy
figure (long title, rotated labels) lays out correctly; legend 'best' avoids data like mpl does.

---

## v0.3.0 — "Everyday Plots" (the workhorse release) — ✅ tagged 2026-07-14

**Goal:** cover ~80% of day-to-day plotting scripts.

> Shipped as scoped. Deviations logged: D10 (log labels as unicode superscripts until
> mathtext), D11 (tight_layout per-cell v1). Multi-pair plot(x1,y1,x2,y2) deferred again
> (v0.4 candidate); minorticks default off exactly like mpl.

**In scope:**
- New artists/methods: `bar`/`barh` (string x-labels via fixed positions), `hist` (bins=int/edges),
  `fill_between`, `errorbar` (caps), `step`/drawstyle, `pie` (basic), `axhline/axvline/axhspan/
  axvspan`, `hlines/vlines`. Sticky edges so bars/hists sit on 0 (DESIGN §8).
- Layout: `subplots(m, n)` + `GridSpec`, `sharex/sharey`, `twinx/twiny`, `suptitle`,
  `tight_layout` v1 (metrics-based padding).
- Scales: log x/y (nonaffine transScale), `LogLocator`, log tick labels; minor ticks +
  `AutoMinorLocator`; `xticks/yticks` manual control; `ScalarFormatter` offset/scientific text.
- Config: `rc()` params store, `style::use` + `default`/`ggplot`/`dark_background` sheets;
  property cycle configurable.

**Play with it:** `07_bar_hist`, `08_subplots_dashboard` (2×2 mixed panel), `09_log_scales`,
`10_styles_gallery` — a one-file "dashboard" script that looks like a matplotlib gallery page.

**Exit criteria:** the dashboard example reproduces its python twin's layout; log ticks match
oracle fixtures; styles switch without re-plotting.

---

## v0.4.0 — "Images & Fields" (2D data) — ✅ tagged 2026-07-14

**Goal:** heatmaps and scalar fields — science/engineering credibility.

**In scope:**
- Colormap engine: viridis/plasma/inferno/magma/cividis tables + coolwarm/RdBu/gray (+jet for
  compat); `Normalize`, `LogNorm`; under/over/bad.
- `scatter(c=…, cmap=…)`; `imshow` (origin, extent, aspect, nearest+bilinear); `pcolormesh`
  (quad mesh fast path); `contour`/`contourf` (marching squares); `colorbar` (own Axes + locator).
- Axes aspect control (`'auto'`/`'equal'`/numeric) — imshow defaults to equal like mpl.
- Alpha compositing correctness pass across AGG + SVG.

**Play with it:** `11_heatmap_colorbar`, `12_contour_gaussian`, `13_imshow_photo` (load a PNG,
display it), `14_scatter_cmap`.

**Exit criteria:** colormap LUTs byte-match mpl's tables; contour lines match oracle fixture
polylines within tolerance; colorbar ticks = axis ticks rules.

---

## v0.5.0 — "It Moves" (interactive window) — ✅ tagged 2026-07-15

**Goal:** `plt::show()` opens a real window; the figure becomes an instrument, not just a file.

**In scope:**
- Event system with mpl's names (`button_press_event`, `motion_notify_event`, `scroll_event`,
  `key_press_event`, `resize_event`, `draw_event`, `close_event`) + `mpl_connect`-style API.
- GLFW + OpenGL 3.3 canvas: AGG framebuffer blitted as a texture; damage-based redraw; resize →
  re-layout; multiple figures; `plt::show()` / `plt::pause()`.
- Navigation matching mpl's keymap: scroll-zoom at cursor, drag-pan, `h` home, `s` save, box zoom.
- Animation: `FuncAnimation`-lite with blitting.
- CI: interactive smoke tests on Linux via xvfb; unit-testable event core (fake canvas).

**Play with it:** `15_interactive_signal` (pan/zoom a 100k-point trace), `16_live_sine`
(animated), `17_events_probe` (click to annotate).

**Exit criteria:** 60 fps pan on 100k points on the dev MacBook; window closes cleanly (ASan green);
same figure code renders identically to file when no window is requested.

---

## v0.6.0 — "Publication Grade" (PDF, mathtext, dates, annotate) — ✅ tagged 2026-07-16

**Goal:** the outputs you put in a paper or a report.

**In scope:**
- PDF backend: vector paths + embedded TrueType subset; content parity with SVG on the
  conformance scene.
- mathtext subset: `$…$` sub/superscripts, greek, `\frac`, `\sqrt`, operators (DESIGN §10);
  clean ValueError on unsupported commands.
- Date axes: `std::chrono` overloads, date2num (1970 epoch), `AutoDateLocator`,
  `ConciseDateFormatter`.
- `annotate` with arrowprops subset (`'->'`, `'-|>'`…), text bbox (round box style).
  *(shipped without the visible round-box style — moved to v0.7)*
- Optional FreeType path for hinted glyph quality (CMake option, default OFF).
  *(v0.7 decision: moved to the icebox — stb+DejaVu output has been visually
  indistinguishable from matplotlib's through six releases of side-by-side
  comparison, and an optional FreeType would fork the text stack the golden
  tests depend on; revisit only on concrete rendering-quality reports)*

**Play with it:** `20_paper_figure` — a two-panel publication figure with math labels, dated
x-axis, annotations → one PDF + one SVG; drop it straight into LaTeX.

**Exit criteria:** the PDF opens correctly in Preview/Acrobat/Chrome; text is selectable; the
paper figure is accepted by `pdflatex` without warnings.

---

## v0.7.0 — "Fast Path" (performance + docs) — ✅ tagged 2026-07-17

**Goal:** be fast enough that nobody asks; be documented enough that nobody's blocked.

**In scope:**
- Port mpl's path simplification + chunking; fast-marker stamping; `draw_path_collection` fast
  path; optional tiled multithreaded rasterization; NEON/SSE review behind `src/simd/` dispatch.
  *(shipped: simplification + chunking + stamping — 5-30x; MT/SIMD rasterization not needed to
  meet the savefig target and stays icebox until a use case demands it)*
- Rounded annotate text box (deferred from v0.6) — shipped: boxstyle 'round'/'square'.
- Benchmarks (Google Benchmark) + tracked targets: 10M-point line `savefig` < 1 s;
  1M-point scatter pan at 60 fps (M-series).
- Memory/lifetime audit (ASan/LSan), figure teardown, peak RSS budget.
- Docs: Doxygen + gallery generator → static site; quick-start tutorial mirroring mpl's.

**Exit criteria:** benchmark suite in CI with regression thresholds; docs site builds from a
clean checkout with one command.

---

## v1.0.0 — "Parity Core" (freeze & package) — ✅ tagged 2026-07-18

**In scope:**
- API review sweep and freeze; semver from here; deprecations removed.
- Packaging: CMake install/export config, CPack archives, **vcpkg port** (conan recipe stretch).
- Windows arm64 & Linux arm64 formally tier-1 with full test runs.
- Conformance report published: PARITY.md finalized with a side-by-side gallery (ours vs
  matplotlib) for every covered feature.

**Exit criteria:** an external user can `find_package(graphlib)` or vcpkg-install and reproduce
the gallery without reading anything but the README.

---

## Risk register

| Risk | Mitigation |
|------|------------|
| Text metrics/shaping rabbit hole | stb+DejaVu only pre-0.6; no i18n pre-1.0 (D2) |
| Stroke/AA quality disappoints | vendor AGG — matplotlib's own engine — instead of writing a rasterizer (ADR-0002) |
| `legend('best')`, `tight_layout`, `MaxNLocator` subtleties | port mpl's actual algorithms from source, with oracle fixtures — never reinvent |
| Interactive platform variance | GPU only blits an AGG framebuffer; drawing code identical everywhere |
| Windows/Linux arm CI needs public repo (free runners) | keep repo public, or accept build-only cross for arm until self-hosted runner |
| contour / mathtext are mini-projects | isolated modules; each may slip one milestone without blocking the rest |
| Scope creep ("can it do violin plots?") | PARITY.md is the gate: not on the list ⇒ icebox row first, discussion second |

## Icebox (post-1.0 candidates, in rough order)

polar projection · boxplot/violinplot · quiver/streamplot · hatching · path effects · pick events ·
animation export (gif/mp4) · Blend2D speed backend · Qt/ImGui embedding · HarfBuzz shaping ·
symlog/logit scales · 3D (mplot3d) — big enough to be its own roadmap.
