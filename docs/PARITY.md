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
| markers | `marker=` + fmt chars | subset v0.1, full set v0.2 | done (11 markers: `o s ^ v < > D + x . *`) |
| save SVG | `savefig("*.svg")` | same | done (72 dpi pt canvas, like mpl; deterministic) |
| save PNG (dpi) | `savefig("*.png", dpi=)` | same | planned-v0.2 |
| text on axes | `text` | same | done |
| legend | `legend` (loc, 'best', frame) | same | planned-v0.2 |
| scatter | `scatter` (s=, colors) | v0.2; `c=`+cmap v0.4 | planned-v0.2 |
| bar / barh | `bar`, `barh` (+string x) | same | planned-v0.3 |
| hist | `hist` (bins=10 default) | same | planned-v0.3 |
| fill_between | `fill_between` | same | planned-v0.3 |
| errorbar | `errorbar` | same | planned-v0.3 |
| step | `step` / drawstyle | same | planned-v0.3 |
| pie | `pie` | basic (sizes, labels, colors, startangle) | planned-v0.3 |
| h/v lines & spans | `axhline`/`axvline`/`axhspan`/`axvspan`, `hlines`/`vlines` | same | planned-v0.3 |
| subplots grid | `subplots(m,n)`, GridSpec, sharex/sharey | same | planned-v0.3 |
| twin axes | `twinx`/`twiny` | same | planned-v0.3 |
| log scales | `xscale('log')`… | same (symlog: icebox) | planned-v0.3 |
| manual ticks | `xticks`/`yticks` | same | planned-v0.3 |
| minor ticks | `AutoMinorLocator` | same | planned-v0.3 |
| tight_layout | `tight_layout` | v1 (metrics-based padding) | planned-v0.3 |
| suptitle | `suptitle` | same | planned-v0.3 |
| styles / rcParams | `rcParams`, `style.use` | `rc()`, `style::use` — default/ggplot/dark | planned-v0.3 |
| ScalarFormatter offset | offset/sci text at axis end | same algorithm | planned-v0.3 |
| colormaps + norm | `cm`, `Normalize`, `LogNorm` | viridis family + core set | planned-v0.4 |
| imshow | `imshow` (origin/extent/aspect/interp) | nearest+bilinear | planned-v0.4 |
| pcolormesh | `pcolormesh` | flat shading | planned-v0.4 |
| contour / contourf | `contour(f)` | marching squares | planned-v0.4 |
| colorbar | `colorbar` | same | planned-v0.4 |
| axes aspect | `set_aspect` auto/equal/num | same | planned-v0.4 |
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
