# graphlib design — the matplotlib spec digest

Source of truth: **matplotlib 3.10.8** (interrogated from the local installation — signatures,
docstrings, and source; matplotlib.org blocks robot fetchers, use the local install as the oracle).
This document fixes what graphlib replicates and how it maps to C++. Numeric values here were
extracted from the real rcParams/source, not from memory — treat them as locked.

---

## 1. The three layers

matplotlib's architecture, which we replicate 1:1:

| Layer | matplotlib | graphlib |
|-------|-----------|----------|
| Scripting | `matplotlib.pyplot` — stateful, implicit current figure/axes | `graphlib::pyplot` (`namespace plt = graphlib::pyplot;`) |
| Artist | everything drawable is an `Artist`; containers Figure/Axes/Axis | same class names, `graphlib::` |
| Backend | `FigureCanvas` (surface) + `Renderer` (draws) + `GraphicsContext` (stroke/fill state) + `Event` | same, `graphlib::` |

The scripting layer is a thin veneer: `plt::plot(...)` ≡ `gca().plot(...)`. All real logic lives in
the artist layer. The artist layer knows nothing about concrete backends.

## 2. Object model

```
Figure                          owns everything; size in inches × dpi
├── patch (Rectangle)           figure background
├── suptitle (Text)             optional
└── Axes (1..n)                 THE central object; most plotting methods live here
    ├── patch (Rectangle)       axes background
    ├── XAxis / YAxis           each: label (Text), offset text, ticks
    │   ├── major Ticks []      Tick = tick line + grid line + label (Text)
    │   └── minor Ticks []
    ├── Spines ×4               left/right/top/bottom border lines
    ├── title (Text)            (+ optional left/right titles)
    ├── children []             Line2D | Patch | Text | Collection | AxesImage | Legend …
    └── legend (Legend)         optional
```

`Artist` base: `transform`, `zorder`, `visible`, `alpha`, `clip_box`/`clip_path`, `label`, `gid`.

**zorder defaults** (draw order within an Axes; ties broken by insertion order, stable sort):

| Artist | zorder |
|--------|--------|
| AxesImage | 0 |
| Patch / Collections | 1 |
| Line2D | 2 |
| Text | 3 |
| Legend | 5 |

## 3. Draw cycle

`figure.draw(renderer)`:
1. Draw figure patch.
2. For each Axes (by zorder):
   - **Ticks are computed lazily at draw time** from the current view limits
     (Locator → positions, Formatter → labels). Never cached across limit changes.
   - Draw: axes patch → grid → children sorted by zorder → spines → axis (ticks+labels) → title.
3. `open_group("line2d", gid)` / `close_group` bracket each artist (gives SVG its `<g id=…>` structure).

## 4. Transform framework

Coordinate frames: **data** → **axes** (0–1 inside the axes box) → **figure** (0–1 in figure) →
**display** (pixels, origin **bottom-left**, y-up).

Exact composition (from `_AxesBase._set_lim_and_transforms`, verbatim semantics):

```
transAxes   = BboxTransformTo(axes.bbox_pixels)
transLimits = BboxTransformFrom(TransformedBbox(viewLim, transScale))
transData   = transScale + (transLimits + transAxes)     // parentheses matter: affine part collapses
```

- `transScale`: identity for linear axes; `log10` per-axis for log scales — the **only nonaffine**
  transform pre-1.0.
- Blended transforms: x from one frame, y from another (x tick lines: data-x blended with axes-y).
- `Bbox` = (x0,y0,x1,y1) with union/expanded/transformed/contains; used for data limits, view
  limits, axes position, text extents.

**C++ mapping (deliberate simplification):** matplotlib has a general lazy transform *graph*.
graphlib pre-1.0 implements `Transform` = optional per-axis nonaffine functions followed by an
`Affine2D` (3×3, stored as 6 doubles a b c d e f). Composition of affines multiplies matrices;
`transData` keeps the scale part separate exactly as above. The general graph is an internal detail
we can grow later without changing artists.

**Units:** figure size **inches** (default 6.4 × 4.8), `dpi` = 100 ⇒ 640×480 px canvas.
Everything styled is in **points**: `px = pt × dpi / 72`. Display origin is bottom-left throughout
the artist layer; each backend converts to its native convention (SVG/PNG are y-down) **exactly
once**, at emission — the flip site carries a comment, and there is only one per backend.

## 5. Path model

The single geometry currency (mirrors `matplotlib.path.Path`):

- `vertices`: N×2 doubles. `codes`: per-vertex, or absent ⇒ implicit polyline (MOVETO then LINETOs).

| code | value | consumes |
|------|-------|----------|
| STOP | 0 | (unused in practice) |
| MOVETO | 1 | 1 pt |
| LINETO | 2 | 1 pt |
| CURVE3 | 3 | 2 pts (quadratic Bézier) |
| CURVE4 | 4 | 3 pts (cubic Bézier) |
| CLOSEPOLY | 79 | 1 (ignored pt) |

- Fill rule: nonzero winding.
- **NaN vertex ⇒ gap** (subpath break) — matplotlib's masked-data behavior for lines.
- Markers are small Paths defined in **points space** around (0,0), stamped at positions
  (`draw_markers`). Unit shapes: circle via 4 CURVE4 arcs (k = 0.5523), square, triangles, etc.
- Bézier flattening is a shared utility (`path_iter`), not per-backend code.

## 6. GraphicsContext — stroke/fill state handed to the renderer

Fields (mirrors `GraphicsContextBase`): `rgba` color, `alpha` (+`forced_alpha`), `linewidth` (pt),
`dashes` (offset + on/off pattern in pt), `capstyle`, `joinstyle`, `antialiased`, `cliprect`,
`clippath`, `hatch` (+color/linewidth, v0.6+), `snap`, `gid`/`url` (vector metadata).

**Dash patterns** (pt at linewidth 1.0, **scaled by linewidth**; from `lines._get_dash_pattern`):

| style | pattern |
|-------|---------|
| `'-'` | solid |
| `'--'` | (3.7, 1.6) |
| `'-.'` | (6.4, 1.6, 1.0, 1.6) |
| `':'` | (1.0, 1.65) |

Cap/join defaults: solid lines cap=`projecting`, dashed cap=`butt`; joinstyle `round` for both.

## 7. Renderer interface — the backend contract

Distilled from `RendererBase` (the actual method set of 3.10.8). graphlib C++ form:

```cpp
class Renderer {
public:
    virtual ~Renderer() = default;

    // ---- required ----
    virtual void draw_path(const GraphicsContext& gc, const Path& path,
                           const Transform& tf,
                           const std::optional<Color>& face = std::nullopt) = 0;
    virtual void draw_text(const GraphicsContext& gc, double x, double y,
                           std::string_view s, const FontProperties& prop,
                           double angle_deg) = 0;
    virtual void draw_image(const GraphicsContext& gc, double x, double y,
                            const ImageBuffer& rgba) = 0;
    virtual Size   get_canvas_size() const = 0;                    // px
    virtual double points_to_pixels(double pt) const;              // pt * dpi/72 (72-dpi vector: pt)

    // ---- text metrics come from the shared FontManager, NOT from backends ----
    // (get_text_width_height_descent lives on FontManager so layout is backend-independent)

    // ---- optional fast paths; defaults loop over draw_path ----
    virtual void draw_markers(const GraphicsContext&, const Path& marker,
                              const Transform& marker_tf, const Path& positions,
                              const Transform& tf, const std::optional<Color>& face);
    virtual void draw_path_collection(/* scatter fast path, v0.2 */);
    virtual void draw_quad_mesh(/* pcolormesh fast path, v0.4 */);

    // ---- structure hooks (vector formats) ----
    virtual void open_group(std::string_view tag, std::string_view id) {}
    virtual void close_group(std::string_view tag) {}
};
```

Dropped from matplotlib's contract on purpose (recorded in PARITY deviations): `draw_tex` (no TeX),
`draw_gouraud_triangles` (post-1.0), filter/rasterizing hooks (post-1.0), `get_texmanager`.

**Text rule:** metrics (width/height/descent, kerning) come from the shared `FontManager`
(stb_truetype over embedded DejaVu). Backends only choose how to *emit* text: native element (SVG),
glyph rasterization (AGG), embedded subset font (PDF). This is what makes layout identical across
backends and platforms.

## 8. Ticking pipeline

`view limits → Locator → positions → Formatter → labels`, run lazily at draw time.

- **AutoLocator = MaxNLocator(nbins='auto', steps=[1, 2, 2.5, 5, 10])** — candidate steps are
  decade multiples of the steps list; `'auto'` derives max tick count from axis length (capped 9).
  MaxNLocator defaults: `nbins=10, steps=None, integer=False, symmetric=False, prune=None, min_n_ticks=2`.
  Port `MaxNLocator._raw_ticks` faithfully — do not invent a "nice numbers" algorithm.
- Others (in scope order): FixedLocator, NullLocator, MultipleLocator (v0.1 if trivial),
  LogLocator (v0.3), AutoMinorLocator (v0.3 — divides major step into 4 or 5),
  AutoDateLocator + ConciseDateFormatter (v0.6).
- **ScalarFormatter:** fixed notation while values are "reasonable"; switches to scientific with an
  **offset/scale text at the axis end** (e.g. `1e6`, `+2.02e3`) otherwise. v0.1 ships fixed-notation
  only; the offset algorithm ports in v0.3.
- **Autoscale:** dataLim ∪ per-artist sticky edges → add margins (`axes.xmargin = ymargin = 0.05`)
  → `autolimit_mode='data'` (margins only; `'round_numbers'` also expands to locator ticks).
  Sticky edges (v0.3): bar/hist bottom sticks to 0, imshow sticks to extent — no margin on that side.
- Tick geometry: major size 3.5 pt, minor 2.0 pt, width 0.8 pt, direction `out`, label pad 3.5 pt.
  Grid lines are drawn at major (and optionally minor) locator positions: `#b0b0b0`, lw 0.8.

## 9. Color & style pipeline

**Color spec grammar** — accepted anywhere a color is (parse once, store RGBA doubles 0–1):

| form | example | notes |
|------|---------|-------|
| single letter | `"r"` | bgrcmykw (8) |
| name | `"rebeccapurple"` | CSS4, 148 names (table vendored from mpl) |
| tab palette | `"tab:blue"` | Tableau 10 |
| hex | `"#1f77b4"`, `"#1f77b480"`, `"#abc"` | 3/4/6/8 digits |
| gray string | `"0.8"` | 0=black … 1=white |
| cycle ref | `"C0"`…`"C9"` | nth color of the property cycle |
| none | `"none"` | fully transparent |
| struct | `Color{r,g,b,a}` | C++ side |

**Default property cycle** (tab10): `#1f77b4 #ff7f0e #2ca02c #d62728 #9467bd #8c564b #e377c2
#7f7f7f #bcbd22 #17becf`.

**Format shorthand** (`plot` only): `fmt = [marker][line][color]` in any order, max one each.
Markers `.,ov^<>12348spP*hH+xXDd|_` · lines `- -- -. :` · colors `bgrcmykw`. E.g. `"r--o"`, `"g^"`.

**Colormaps (v0.4):** `ListedColormap` 256-entry tables (viridis, plasma, inferno, magma, cividis —
tables vendored from mpl) + a few segmented (coolwarm, RdBu, gray, jet-for-compat); `Normalize`
[vmin,vmax]→[0,1], `LogNorm`; under/over/bad colors. Default image cmap: `viridis`.

**rcParams:** typed key→value store, `graphlib::rc()`; style sheets are rc overlays:
`plt::style::use("ggplot")`. Ship `default`, `ggplot`, `dark_background` (v0.3).

## 10. Text pipeline

- `FontProperties`: family (DejaVu Sans/Bold/Mono only pre-0.6), size in pt (default 10) or named
  size, weight, style (italic synthesized as oblique).
- Named size factors (× `font.size`): xx-small 0.579 · x-small 0.694 · small 0.833 · medium 1.0 ·
  large 1.2 · x-large 1.44 · xx-large 1.728.
- Layout in `FontManager`: advance widths + kerning from stb_truetype; multi-line via `\n`
  (line spacing 1.2×); `ha` left|center|right, `va` top|center|baseline|bottom; rotation CCW degrees.
- Emission: SVG native `<text>` (font-family DejaVu Sans + generic fallback), AGG rasterized glyphs
  (cached per (glyph, size)), PDF embedded TrueType subset (v0.6).
- mathtext `$...$` (v0.6): recursive-descent parser for a documented subset — sub/superscripts,
  greek, `\frac`, `\sqrt`, common operators. Everything else ⇒ ValueError, never silent garbage.

## 11. The pyplot facade

Implicit state (mirrors pyplot): figure registry (int → Figure), current figure, current axes.
`plt::plot(...)` → `gca().plot(...)`; `plt::figure(n)` creates/activates; `plt::savefig` → `gcf()`;
`plt::close(n)` / `plt::close_all()`; `plt::show()` (v0.5) blocks on the interactive event loop.

```cpp
#include <graphlib/pyplot.hpp>
namespace plt = graphlib::pyplot;

int main() {
    auto x = graphlib::linspace(0.0, 2 * std::numbers::pi, 200);
    auto y = x; for (auto& v : y) v = std::sin(v);
    plt::plot(x, y, "b-", {.label = "sin"});
    plt::title("Hello graphlib");
    plt::grid(true);
    plt::savefig("hello.svg");
}
```

## 12. Canonical defaults — locked from matplotlib 3.10.8 rcParams

| key | value |
|-----|-------|
| figure.figsize / dpi / facecolor | 6.4 × 4.8 in / 100 / white → 640×480 px |
| savefig.dpi | 'figure' (inherit) |
| axes box (subplot params) | left .125, right .900, bottom .110, top .880 (wspace .2, hspace .2) |
| font | DejaVu Sans, 10 pt |
| lines | lw 1.5, ls '-', no marker, markersize 6, antialiased, solid cap projecting, dash cap butt, join round |
| prop cycle | tab10 (see §9) |
| axes | edge black lw 0.8, face white, grid off, xmargin/ymargin 0.05, autolimit_mode 'data' |
| title / labels | 'large' (12 pt) pad 6 / 'medium' (10 pt) pad 4 |
| ticks | out, major 3.5 pt, minor 2.0 pt, width 0.8, pad 3.5, labels 'medium' |
| grid | #b0b0b0, lw 0.8, ls '-', alpha 1.0 |
| legend | loc 'best', frameon, framealpha 0.8, edgecolor '0.8', borderpad 0.4 |
| patch | lw 1.0, face C0, edge black |
| image | cmap viridis, origin 'upper', aspect 'equal' (⇒ imshow forces equal axes aspect) |
| hist.bins / scatter.marker / errorbar.capsize | 10 / 'o' / 0.0 |

## 13. Determinism rules (make golden tests possible)

1. Floats in vector output via `std::to_chars` shortest-round-trip, then a fixed max precision
   (util helper) — never `%f`/locale-dependent formatting.
2. Attribute order fixed; element ids from a per-canvas counter (never pointer values).
3. All iteration over containers with deterministic order (no unordered_map in render paths — or
   sort at the boundary).
4. Fonts: embedded DejaVu only; no system font lookup pre-0.6 (and after: opt-in only).
5. PNG: fixed encoder settings; pixel content is what's compared (RMS), not bytes.
6. No clock, no randomness, no environment reads inside layout/render code paths.

## 14. Non-goals pre-1.0 (icebox — revisit after)

3D (mplot3d) · widgets · TeX integration (`usetex`) · HarfBuzz shaping & i18n text · a units
framework (datetime arrives v0.6 as a concrete type, not a framework) · pandas-style data adapters ·
animation export to gif/mp4 (live animation IS in scope, v0.5) · WebAgg-style browser backend ·
polar & map projections (polar is the first post-1.0 candidate — the transform design keeps the
door open) · pick events · path effects · hatching (v0.6 stretch) · quiver/streamplot/sankey/table ·
boxplot/violinplot (post-1.0 candidates) · Gouraud shading.
