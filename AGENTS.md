# AGENTS.md — graphlib

**graphlib** is a modern C++20 plotting library that mimics [matplotlib](https://matplotlib.org/stable/)'s
API, object model, and rendering semantics. A matplotlib user must feel at home within minutes; a C++
user must get publication-quality output with zero mandatory dependencies.

This file is the operating manual for anyone — human or agent — working in this repo. Read it before
changing code. Keep it accurate and under ~250 lines; deep material lives in `docs/` and `.claude/skills/`.

**New session bootstrap:** read §Status below, then [ROADMAP.md](ROADMAP.md) for the current milestone,
then [docs/PARITY.md](docs/PARITY.md) for what's in scope. Run `ctest --preset dev` before starting work.

## Ground rules

1. **matplotlib is the spec.** Its documented behavior *at default settings* is our requirement
   document. Before designing or implementing any feature that exists in matplotlib, use the
   `mpl-parity` skill: interrogate the **local matplotlib installation** (3.10.x, the oracle — the
   website blocks non-browser fetchers), then record the decision in [docs/PARITY.md](docs/PARITY.md).
   Deviations are allowed but must be deliberate and logged there.
2. **Artists never talk to backends.** All drawing flows through the `Renderer` interface
   ([docs/DESIGN.md](docs/DESIGN.md) §7). Need a new primitive? Extend the interface deliberately —
   never special-case a backend inside an artist.
3. **The core stays dependency-free.** See §Dependencies. Anything heavier than a vendored
   single-file library must be optional and OFF by default.
4. **Determinism is a feature.** Same input ⇒ byte-identical SVG on every platform, PNG within RMS
   tolerance. Embedded fonts only; stable float formatting (`std::to_chars`); no locale, no clock,
   no randomness anywhere in a render path.
5. **Every user-visible feature ships as a vertical slice:** API + unit tests + image test + one
   gallery example + PARITY.md row + changelog fragment. No horizontal "framework-only" PRs.
6. **Keep this file and ROADMAP.md current** — update §Status when a milestone lands.

## Platforms

| Tier | Target | Promise |
|------|--------|---------|
| 1 | macOS arm64 *(primary dev)* | CI build + full test suite (+ sanitizers) |
| 1 | Linux x86_64, Linux arm64 | CI build + full test suite |
| 1 | Windows x86_64 | CI build + full test suite (MSVC) |
| 1* | Windows arm64 | CI build + tests — `windows-11-arm` runner (free for public repos only) |
| 2 | macOS x86_64 | cross-compiled slice (`CMAKE_OSX_ARCHITECTURES=x86_64`), no runtime CI |

## Architecture — mirrors matplotlib's three layers

```
scripting   graphlib::pyplot     stateful facade: plt::plot(...), implicit current figure/axes
artist      Figure ⊃ Axes ⊃ (Axis ⊃ Ticks) · Line2D · Text · Patch · Collections · Legend …
backend     Renderer + GraphicsContext + FigureCanvas + Event    (SVG, AGG→PNG, GLFW, PDF)
```

- Everything visible is an `Artist`: `draw(Renderer&)`, `zorder`, `visible`, `alpha`, a `Transform`.
- Coordinates: data → `transData = transScale + (transLimits + transAxes)` → display pixels.
  Figure size in **inches**; line widths & font sizes in **points**; `px = pt × dpi / 72`.
- The full spec digest (extracted from matplotlib 3.10.8 source) is [docs/DESIGN.md](docs/DESIGN.md).
  Feature coverage and deviations: [docs/PARITY.md](docs/PARITY.md).

## Repository map (planned — grows per ROADMAP.md)

```
include/graphlib/       public headers (installed); pyplot.hpp is the facade
src/core/               artists, transforms, path, colors, ticker, scale, text layout
src/backends/           svg/  agg/ (raster→PNG)  glfw/ (interactive)  pdf/
third_party/            vendored: agg/, stb/, fonts/ (DejaVu) — patches documented, never silent
examples/               gallery sources; each file builds to a tiny executable
tests/unit/             Catch2 unit tests
tests/image/            image-comparison tests + baselines/{svg,png}/
tests/fixtures/         oracle fixtures generated from python matplotlib (committed JSON)
benchmarks/             Google Benchmark (v0.7+)
docs/                   DESIGN.md, PARITY.md, decisions/ (ADRs), gallery/
cmake/                  presets includes, toolchain helpers
.claude/skills/         repo skills (see §Skills)
```

## Toolchain & build

- **C++20**, no compiler extensions. Minimum: AppleClang 15, Clang 16, GCC 12, MSVC 19.38.
- **CMake ≥ 3.24** + presets + Ninja. Library target: `graphlib::graphlib` (static by default).
- Commands:
  ```
  cmake --preset dev && cmake --build --preset dev   # Debug, tests+examples ON
  ctest --preset dev                                 # -R unit / -R image to filter
  cmake --preset asan                                # ASan+UBSan variant
  cmake --preset release                             # optimized, examples ON
  ```
- Options: `GRAPHLIB_BUILD_TESTS`, `GRAPHLIB_BUILD_EXAMPLES`, `GRAPHLIB_INTERACTIVE` (GLFW),
  `GRAPHLIB_WERROR` (ON in CI).
- CLion opens the folder directly and picks up CMake presets natively.

## API conventions — the C++ ↔ matplotlib mapping

- Namespaces: core in `graphlib::`; facade in `graphlib::pyplot`. Examples always alias:
  `namespace plt = graphlib::pyplot;` (the C++ homage to `import matplotlib.pyplot as plt`).
- **Names mirror matplotlib exactly.** Classes `Figure`, `Axes`, `Line2D`…; methods snake_case:
  `set_xlim`, `add_subplot`, `savefig`. Don't "improve" names — familiarity is the product.
- **kwargs → aggregate options structs** with designated initializers. Field names = matplotlib
  kwarg names; defaults = matplotlib rcParams defaults (cite the rc key in a comment):
  ```cpp
  ax.plot(x, y, "r--o");                                  // fmt shorthand, like mpl
  ax.plot(x, y, {.linewidth = 2.0, .label = "phase"});    // kwargs style
  plt::savefig("out.png", {.dpi = 200});
  ```
- **Stringly-typed at the boundary, strongly-typed inside.** Public API accepts matplotlib string
  tokens (`"tab:blue"`, `"--"`, `"upper right"`, `"o"`); parsed once into strong enums/structs.
  Unsupported-but-valid-in-mpl token ⇒ `graphlib::ValueError` naming the milestone that adds it.
- Data in: `std::span<const double>` is canonical; overloads accept contiguous ranges. `double` only.
- Errors: exceptions — `graphlib::ValueError` (bad user input, mirrors Python's), `graphlib::KeyError`
  (unknown rc key). No error codes in the public API. Internal invariants use `assert`.
- Ownership: a `Figure` owns its artist tree (`unique_ptr` children); factory methods
  (`add_subplot`, `plot`) return references, stable until the figure is cleared/destroyed. Figures
  are owned by the pyplot registry (`plt::figure()`) or held by value (`graphlib::Figure fig;`).
- Headers mirror mpl modules: umbrella `<graphlib/graphlib.hpp>`, fine-grained `<graphlib/axes.hpp>`,
  `<graphlib/ticker.hpp>`, … (map in PARITY.md).

## Coding standards

- Style enforced by `.clang-format` (LLVM base, 100 cols, 4-space indent). Run before committing.
- Naming: `PascalCase` types, `snake_case` functions/variables, `trailing_underscore_` private
  members, macros avoided.
- Warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` / MSVC `/W4 /permissive- /utf-8`;
  CI adds `-Werror`.
- No raw `new`/`delete`, no owning raw pointers. Global mutable state only in the pyplot registry
  and rcParams (thread-confined; graphlib is not thread-safe per figure — same as matplotlib).
- Platform `#ifdef`s only under `src/backends/` and `src/platform/`. Little-endian assumed
  (one `static_assert`). UTF-8 everywhere.
- `std::format`/newer chrono are **banned in core** (AppleClang availability) — use the
  `util::fmt_float`/`to_chars` helpers.
- Comments state constraints and invariants, not narration. Public APIs carry Doxygen `///` whose
  first line matches the matplotlib docstring summary where applicable.

## Dependencies

| Tier | Policy | Set |
|------|--------|-----|
| 0 — core | C++20 STL only, forever | — |
| 1 — vendored | tiny, permissive license, committed under `third_party/` | stb_truetype, stb_image_write, DejaVu fonts (v0.2+), AGG 2.4 — matplotlib's own rasterizer (v0.2+) |
| 2 — optional | FetchContent, behind a CMake option, OFF by default | GLFW (v0.5+), FreeType (v0.6+, optional quality text) |
| dev-only | FetchContent | Catch2 v3, Google Benchmark |

Never: Boost, Qt, a Python runtime. Any new dependency requires an ADR in `docs/decisions/`.

## Testing policy

- Catch2 v3. Layout per §Repository map.
- **Oracle fixtures:** numeric expectations (tick positions, transforms, color parsing) are generated
  from local python matplotlib by `tests/fixtures/generate.py` and committed as JSON — CI has no
  Python. See the `mpl-parity` skill.
- **Image tests:** every artist/backend feature gets baselines — see the `image-tests` skill.
  SVG: byte-equal after normalization. PNG: RMS ≤ 1.0 (0–255 scale) default tolerance.
- The ASan+UBSan job stays green; no suppressions without an ADR.
- A PR that changes rendered output must include regenerated baselines and explain why.

## Examples & docs

- Every user-visible feature ⇒ one runnable example in `examples/` (short, matplotlib-gallery style,
  ends in `savefig`). Compiled in CI on all tier-1 platforms; executed on Linux x64 to regenerate
  `docs/gallery/` at release time.
- Public API gets Doxygen comments; the docs site is assembled from v0.7.

## Git conventions

- Conventional commits: `feat(axes): …`, `fix(svg): …`, `test(ticker): …`, `perf:`, `docs:`,
  `build:`, `ci:`, `refactor:`.
- Branches: `feat/v0.X-slug`. One vertical slice per PR. CI green on tier-1 before merge.
- Never commit build dirs; never hand-edit baselines (use the `update-baselines` target).

## Skills (in `.claude/skills/`)

| Skill | Use when |
|-------|----------|
| `mpl-parity` | before designing anything that exists in matplotlib; resolving "what would mpl do"; generating oracle fixtures |
| `new-artist` | adding a plot type / drawable (bar, hist, imshow, …) end-to-end |
| `new-backend` | implementing or extending a Renderer backend (SVG, AGG, PDF, GLFW…) |
| `image-tests` | writing / triaging / regenerating image-comparison tests |
| `cross-platform` | CI red on one platform; toolchain/CMake changes; platform-sensitive code |
| `release` | cutting a vX.Y.Z milestone release |

## Decisions

ADRs live in `docs/decisions/`. Current: **ADR-0001** language & toolchain, **ADR-0002** rendering
stack. A new dependency, a public-API breaking change, or a determinism exception ⇒ new ADR.

## Status

- **v0.1.0 "First Light" — RELEASED 2026-07-13** ([tag](https://github.com/Nosenzor/graphlib/releases/tag/v0.1.0)).
- **v0.2.0 "Raster & Type" — tagged 2026-07-13, CI green on all 5 targets**: AGG raster→PNG
  at any dpi, embedded-DejaVu text stack (metrics + glyph outlines on all backends), full
  marker set, scatter, legend with ported 'best', PNG image-test harness + conformance
  scene. 47 tests incl. ASan; goldens byte-identical cross-platform.
- **v0.3.0 "Everyday Plots" — tagged 2026-07-14, CI green on all 5 targets**: bar/barh/hist/
  fill_between/errorbar/step/pie/spans, subplots+GridSpec+sharing+twins+tight_layout v1,
  log scales + minor ticks (oracle-exact), rcParams + 3 style sheets, full ScalarFormatter
  offset machinery. 66 tests incl. ASan; ~180 oracle fixture cases.
- **v0.4.0 "Images & Fields" — tagged 2026-07-14, CI green on all 5 targets**: colormap
  engine (11 oracle-exact LUTs) + Normalize/LogNorm, draw_image on both backends, imshow
  (inverted-y origin, aspect control), pcolormesh, contour/contourf (marching squares),
  colorbar, scatter c-arrays. 76 tests incl. ASan.
- **v0.5.0 "It Moves" — tagged 2026-07-15, CI green on all targets + xvfb window job**:
  GLFW window backend (GRAPHLIB_INTERACTIVE, tier-2 optional dep), mpl-named events +
  mpl_connect, log-correct drag-pan/scroll-zoom + h/s/q keys, FuncAnimation-lite.
  82 tests incl. ASan.
- **v0.6.0 "Publication Grade" — tagged 2026-07-16, CI green on all targets**: deterministic
  PDF backend (selectable CID text, golden-testable), mathtext subset ($...$ everywhere,
  TeX-lite over DejaVu incl. new Oblique face), date axes (chrono date2num +
  AutoDateLocator/ConciseDateFormatter, oracle-exact), annotate with arrow styles.
  100 tests incl. ASan; PDF verified with Preview + pypdf extraction.
- **v0.7.0 "Fast Path" — tagged 2026-07-17, CI green on all targets**: PathSimplifier
  port (oracle-exact; 10M-pt savefig 1379->268 ms), AGG marker stamping (1M scatter
  5648->187 ms), Google Benchmark suite + CI regression gate, ASan/LSan Linux job
  (0 leaks), one-command docs site (Doxygen + gallery + quick start), rounded annotate
  boxes; FreeType option iceboxed with rationale. 103 tests.
- **Current milestone: v1.0.0 "Parity Core"** ([ROADMAP.md](ROADMAP.md)) — API freeze,
  packaging (install/export, CPack, vcpkg port), arm64 formally tier-1, conformance report.
