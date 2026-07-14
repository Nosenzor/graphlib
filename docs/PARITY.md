# PARITY.md — matplotlib coverage map

The shopping list and scoreboard. Every feature that exists in matplotlib gets a row **before**
implementation (see the `mpl-parity` skill). Update the Status column as work lands.

Status: `planned-vX.Y` → `in-progress` → `done` (or `deviates` — must link a Deviations row, or
`icebox` — post-1.0).

## pyplot surface

| Feature | matplotlib | graphlib target | Status |
|---------|-----------|-----------------|--------|
| line plots | `plt.plot` (+fmt strings) | `plt::plot` | done (single x/y pair; multi-pair args: v0.3) |
| figure/axes creation | `figure`, `subplots`, `add_subplot` | same | done (single axes; grid: v0.3) |
| title / xlabel / ylabel | `title`… | same | done (fixed-offset layout until v0.2 metrics) |
| limits | `xlim`/`ylim`, autoscale+margins | same | done (oracle-verified; sticky edges: v0.3) |
| grid | `grid` | same | done (major ticks; minor: v0.3) |
| markers | `marker=` + fmt chars | full string set | done (all 25 string markers) |
| save SVG | `savefig("*.svg")` | same | done (72 dpi pt canvas; text as paths = svg.fonttype 'path' default) |
| save PNG (dpi) | `savefig("*.png", dpi=)` | AGG raster, savefig.dpi='figure' semantics | done |
| text on axes | `text` | same | done (real DejaVu metrics; rotation anchor-style — see D8) |
| legend | `legend` (loc, 'best', frame) | ported _find_best_position | done (rect frame — D9; ncols/bbox_to_anchor later) |
| scatter | `scatter` (s=, colors, c=+cmap) | `c_array` field for mpl's array form | done (sizes pt², per-point cmap colors) |
| bar / barh | `bar`, `barh` (+string x) | same | done (transparent edges, sticky 0, categorical labels) |
| hist | `hist` (bins=10 default) | same | done (np.histogram-compatible; 'auto' bins later) |
| fill_between | `fill_between` | same | done (where-mask later) |
| errorbar | `errorbar` | same | done (caps as _/| markers) |
| step | `step` / drawstyle | same | done (pre/mid/post) |
| pie | `pie` | basic (sizes, labels, colors, startangle) | done (equal-aspect box, axis off) |
| h/v lines & spans | `axhline`/`axvline`/`axhspan`/`axvspan`, `hlines`/`vlines` | same | done (blended fraction transforms) |
| subplots grid | `subplots(m,n)`, GridSpec, sharex/sharey | same | done (+add_subplot(r,c,i)) |
| twin axes | `twinx`/`twiny` | same | done |
| log scales | `xscale('log')`… | same (symlog: icebox) | done (oracle-exact LogLocator; labels: D10) |
| manual ticks | `xticks`/`yticks` | same | done |
| minor ticks | `AutoMinorLocator` | same | done (oracle-exact) |
| tight_layout | `tight_layout` | v1 (metrics-based, per-cell) | done (D11: per-cell, not global) |
| suptitle | `suptitle` | same | done |
| styles / rcParams | `rcParams`, `style.use` | `rc()`, `style::use` — default/ggplot/dark | done (~55 keys; grows on demand) |
| ScalarFormatter offset | offset/sci text at axis end | same algorithm | done (oracle-exact) |
| colormaps + norm | `cm`, `Normalize`, `LogNorm` | 11 maps, oracle-exact LUTs | done |
| imshow | `imshow` (origin/extent/aspect/interp) | nearest+bilinear | done (origin 'upper' inverts y; 'auto' interp -> nearest, D12) |
| pcolormesh | `pcolormesh` | flat shading, 1D edge arrays | done (sticky edges; works on log axes) |
| contour / contourf | `contour(f)` | marching squares | done (stacked-fill bands, D13; MaxNLocator levels) |
| colorbar | `colorbar` | space-stealing cax + gradient | done (vertical only; ticks/label right) |
| axes aspect | `set_aspect` auto/equal/num | adjustable='box' | done |
| show / interactive | `show`, pan/zoom, keymap | GLFW window | planned-v0.5 |
| events | `mpl_connect` (button/motion/scroll/key/resize/draw/close) | same names | planned-v0.5 |
| animation | `FuncAnimation` + blitting | live only (no file export) | planned-v0.5 |
| save PDF | `savefig("*.pdf")` | vector + font subset | planned-v0.6 |
| mathtext | `$...$` subset | sub/sup, greek, frac, sqrt | planned-v0.6 |
| date axis | date2num, AutoDateLocator, ConciseDateFormatter | `std::chrono` based | planned-v0.6 |
| annotate | `annotate` + arrowprops subset | same | planned-v0.6 |
| path simplification / chunking | `path.simplify` etc. | same algorithm | planned-v0.7 |
| 3D, widgets, TeX, quiver, streamplot, boxplot, violin, polar, pick, patheffects, sankey, table | — | — | icebox |

## Module map (headers mirror mpl modules)

| matplotlib module | graphlib header |
|-------------------|-----------------|
| pyplot | `<graphlib/pyplot.hpp>` |
| figure / axes / axis / spines | `<graphlib/figure.hpp>` `<graphlib/axes.hpp>` `<graphlib/axis.hpp>` |
| artist / lines / patches / text / image / collections / legend | `<graphlib/artist.hpp>` `<graphlib/lines.hpp>` … |
| path / markers / transforms | `<graphlib/path.hpp>` `<graphlib/markers.hpp>` `<graphlib/transforms.hpp>` |
| ticker / scale | `<graphlib/ticker.hpp>` `<graphlib/scale.hpp>` |
| colors / cm / colorbar | `<graphlib/colors.hpp>` `<graphlib/colormaps.hpp>` `<graphlib/colorbar.hpp>` |
| rcsetup / style | `<graphlib/rc.hpp>` `<graphlib/style.hpp>` |
| font_manager / mathtext | `<graphlib/text/font_manager.hpp>` (internal until stable) |
| backend_bases / backends.* | `<graphlib/backend/renderer.hpp>`, `<graphlib/backend/{svg,agg,glfw,pdf}.hpp>` |
| gridspec | `<graphlib/gridspec.hpp>` |
| dates | `<graphlib/dates.hpp>` |
| animation | `<graphlib/animation.hpp>` |

## Deviations log (deliberate differences — keep short, keep honest)

| # | Deviation | Why |
|---|-----------|-----|
| D1 | kwargs are aggregate option structs (designated initializers), not dynamic dicts | C++; compile-time checked; field names/defaults still mirror mpl |
| D2 | Fonts: embedded DejaVu family only pre-0.6; system fonts opt-in later | determinism across 5 platforms beats font choice |
| D3 | No units framework; data is `double` (datetime arrives v0.6 as chrono overloads) | mpl's units registry is Python-dynamic; not worth it pre-1.0 |
| D4 | Renderer contract drops `draw_tex`, gouraud, filter hooks | out of scope pre-1.0 (see DESIGN §7) |
| D5 | `plt::show()` before v0.5 raises with a clear message | interactivity lands as one coherent milestone |
| D6 | NaN in data = gap (mpl behavior) but no masked-array type | NaN is the C++ idiom |
| D7 | Figure numbers are `int` only (no string labels) | keep registry simple |
| D8 | Text rotation uses anchor-style alignment (mpl rotation_mode='anchor'), not mpl's default rotated-bbox mode | simpler; visually close for axis labels — revisit if annotations need it |
| D9 | Legend frame is a plain rectangle (mpl: slightly rounded FancyBbox) | rounded boxstyle arrives with Patch work (v0.4+) |
| D10 | Log decade labels use unicode superscripts (10⁻³), not mathtext | mathtext arrives v0.6; visually equivalent |
| D11 | tight_layout fits decorations per grid cell, not via mpl's global redistribution | covers the common cases; revisit with constrained layout |
| D12 | imshow interpolation 'auto' maps to 'nearest' (mpl picks by zoom) | antialiased downsampling is a v0.7 perf topic |
| D13 | contourf paints ascending stacked threshold fills (opaque bands) | identical pixels to mpl bands; semi-transparent contourf overlaps differ |
