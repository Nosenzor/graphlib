#!/usr/bin/env python3
"""Generate oracle fixtures from the local matplotlib installation (mpl-parity skill §4).

Run from the repo root:  python3 tests/fixtures/generate.py

Outputs, all COMMITTED (CI has no Python):
  *.json        — human-readable/diffable record of the oracle values
  fixtures.inc  — the same data as C++ tables, consumed by the unit tests
Each output records the matplotlib version it was generated from.
"""

import json
import pathlib

import matplotlib

matplotlib.use("svg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

HERE = pathlib.Path(__file__).resolve().parent
META = {"matplotlib": matplotlib.__version__}


def ticker_raw():
    """MaxNLocator.tick_values for ranges; nbins -1 encodes 'auto' (=> 9 w/o axis)."""
    ranges = [
        (0.0, 1.0), (0.0, 0.9999), (-1.0, 1.0), (0.0, 10.0), (0.0, 6.283185307179586),
        (-2.5, 7.5), (0.001, 0.008), (123456.0, 123789.0), (-1e-9, 1e-9),
        (2.0, 2.0000001), (5.0, 95.0), (-500.0, 12345.0), (0.1, 0.11),
        (1e6, 2e6), (-0.05, 1.05), (3.0, 17.0),
    ]
    cases = []
    for vmin, vmax in ranges:
        for nbins in ("auto", 5, 9):
            loc = mticker.MaxNLocator(nbins=nbins, steps=[1, 2, 2.5, 5, 10])
            cases.append({
                "vmin": vmin, "vmax": vmax,
                "nbins": -1 if nbins == "auto" else nbins,
                "ticks": [float(t) for t in loc.tick_values(vmin, vmax)],
            })
    return cases


def axis_ticks():
    """Default single-axes figure (6.4x4.8 @100dpi): limits -> locator ticks + label text.

    NOTE: get_xticks() returns the locator output BEFORE trimming to the view
    interval — the C++ test compares untrimmed locator output; trimming is a
    separate unit test.
    """
    lims = [
        (0.0, 1.0), (0.0, 6.283185307179586), (-1.05, 1.05), (0.0, 100.0),
        (-2.5, 7.5), (0.001, 0.008), (5.0, 95.0), (0.0, 0.35), (3.0, 17.0),
        (-40.0, 40.0),
    ]
    cases = []
    for vmin, vmax in lims:
        fig, ax = plt.subplots()
        ax.set_xlim(vmin, vmax)
        ax.set_ylim(vmin, vmax)
        fig.canvas.draw()
        cases.append({
            "vmin": vmin, "vmax": vmax,
            "xticks": [float(t) for t in ax.get_xticks()],
            "xlabels": [t.get_text() for t in ax.get_xticklabels()],
            "yticks": [float(t) for t in ax.get_yticks()],
            "ylabels": [t.get_text() for t in ax.get_yticklabels()],
        })
        plt.close(fig)
    return cases


def autoscale():
    """Data extent -> autoscaled view limits (margins 0.05, autolimit_mode='data')."""
    datasets = [
        (0.0, 1.0), (0.0, 10.0), (-3.2, 5.7), (2.0, 2.0),  # incl. degenerate span
        (0.0, 6.283185307179586), (-1.0, -0.2), (100.0, 101.0),
    ]
    cases = []
    for lo, hi in datasets:
        fig, ax = plt.subplots()
        ax.plot([lo, hi], [lo, hi])
        fig.canvas.draw()
        cases.append({
            "dlo": lo, "dhi": hi,
            "xlim": [float(v) for v in ax.get_xlim()],
            "ylim": [float(v) for v in ax.get_ylim()],
        })
        plt.close(fig)
    return cases


def colors():
    """Color spec -> RGBA for every grammar form supported in v0.1."""
    specs = [
        "r", "g", "b", "c", "m", "y", "k", "w",
        "red", "RED", "rebeccapurple", "aliceblue", "tab:blue", "tab:orange", "tab:cyan",
        "#1f77b4", "#1f77b480", "#abc", "#abcd",
        "0.5", "0.8", "0", "1",
        "C0", "C1", "C9", "C11",
        "none", "None",
    ]
    return [{"spec": s, "rgba": [float(v) for v in matplotlib.colors.to_rgba(s)]}
            for s in specs]




def log_ticks():
    """Log-scale axis: major tick locs + labels + minor locs (LogLocator oracle)."""
    lims = [(1.0, 1000.0), (0.001, 100.0), (5.0, 50000.0), (0.5, 80.0),
            (1e-6, 1e-2), (2.0, 9.0), (1.0, 1e7)]
    cases = []
    for vmin, vmax in lims:
        fig, ax = plt.subplots()
        ax.set_xscale("log")
        ax.set_xlim(vmin, vmax)
        fig.canvas.draw()
        cases.append({
            "vmin": vmin, "vmax": vmax,
            "major": [float(v) for v in ax.get_xticks()],
            "labels": [t.get_text() for t in ax.get_xticklabels()],
            "minor": [float(v) for v in ax.get_xticks(minor=True)],
        })
        plt.close(fig)
    return cases


def minor_ticks():
    """AutoMinorLocator on a default linear axis."""
    lims = [(0.0, 1.0), (0.0, 10.0), (-2.5, 7.5), (3.0, 17.0), (0.0, 0.35)]
    cases = []
    for vmin, vmax in lims:
        fig, ax = plt.subplots()
        ax.set_xlim(vmin, vmax)
        ax.minorticks_on()
        fig.canvas.draw()
        cases.append({
            "vmin": vmin, "vmax": vmax,
            "minor": [float(v) for v in ax.get_xticks(minor=True)],
        })
        plt.close(fig)
    return cases


def offset_labels():
    """ScalarFormatter offset/scientific: labels + the axis-end offset text."""
    lims = [(1e6, 2e6), (123456.0, 123789.0), (-1e-9, 1e-9), (99999990.0, 100000010.0),
            (0.0001, 0.0008), (1e10, 5e10), (0.0, 1.0)]
    cases = []
    for vmin, vmax in lims:
        fig, ax = plt.subplots()
        ax.set_xlim(vmin, vmax)
        fig.canvas.draw()
        cases.append({
            "vmin": vmin, "vmax": vmax,
            "ticks": [float(v) for v in ax.get_xticks()],
            "labels": [t.get_text() for t in ax.get_xticklabels()],
            "offset_text": ax.xaxis.get_offset_text().get_text(),
        })
        plt.close(fig)
    return cases


def hist_bins():
    """np.histogram-compatible binning (bins=int over data range)."""
    import numpy as np
    datasets = [
        [float(v) for v in np.linspace(0, 10, 47) ** 1.3],
        [float(v) for v in np.sin(np.linspace(0, 6.283, 100))],
        [1.0, 1.0, 2.0, 3.5, 3.5, 3.5, 9.0],
    ]
    cases = []
    for data in datasets:
        for bins in (10, 5):
            counts, edges = np.histogram(data, bins=bins)
            cases.append({"data": data, "bins": bins,
                          "counts": [int(c) for c in counts],
                          "edges": [float(e) for e in edges]})
    return cases


def bar_autoscale():
    """bar(): sticky-zero bottom + margined x (autoscale oracle)."""
    cases = []
    for heights in ([1.0, 2.0, 3.0], [3.0, -1.0, 2.0], [0.5, 0.9]):
        fig, ax = plt.subplots()
        ax.bar(range(len(heights)), heights)
        fig.canvas.draw()
        cases.append({"heights": heights,
                      "xlim": [float(v) for v in ax.get_xlim()],
                      "ylim": [float(v) for v in ax.get_ylim()]})
        plt.close(fig)
    return cases




def colormap_samples():
    """Colormap lookups incl. under/over/bad + norm values (v0.4 oracle)."""
    import math
    import numpy as np
    xs = [-0.5, 0.0, 0.1, 0.25, 0.5, 0.75, 0.99, 1.0, 1.5, float("nan")]
    cases = []
    for name in ["viridis", "plasma", "coolwarm", "gray", "jet", "tab10"]:
        cmap = matplotlib.colormaps[name]
        for x in xs:
            r, g, b, a = cmap(np.float64(x))
            cases.append({"cmap": name, "x": x if not math.isnan(x) else "nan",
                          "rgba": [float(r), float(g), float(b), float(a)]})
    return cases


def norm_cases():
    from matplotlib.colors import Normalize, LogNorm
    cases = []
    for vmin, vmax, vals in [(0.0, 10.0, [-5.0, 0.0, 2.5, 10.0, 20.0]),
                             (-1.0, 1.0, [-1.0, 0.0, 0.5, 1.0])]:
        n = Normalize(vmin, vmax)
        for v in vals:
            cases.append({"kind": "linear", "vmin": vmin, "vmax": vmax, "v": v,
                          "out": float(n(v))})
    for vmin, vmax, vals in [(1.0, 1000.0, [1.0, 10.0, 100.0, 1000.0, 5000.0]),
                             (0.1, 10.0, [0.1, 1.0, 10.0])]:
        n = LogNorm(vmin, vmax)
        for v in vals:
            cases.append({"kind": "log", "vmin": vmin, "vmax": vmax, "v": v,
                          "out": float(n(v))})
    return cases




def date_ticks():
    """Date axis oracle: xlim (datenums, 1970 epoch days) -> ticks/labels/offset
    via AutoDateLocator + ConciseDateFormatter."""
    import matplotlib.dates as mdates
    from datetime import datetime
    spans = [
        (datetime(2019, 3, 1), datetime(2026, 7, 1)),     # years
        (datetime(2026, 1, 15), datetime(2026, 11, 3)),   # months
        (datetime(2026, 7, 1), datetime(2026, 7, 11)),    # days
        (datetime(2026, 7, 14, 6), datetime(2026, 7, 15, 18)),  # hours
        (datetime(2026, 7, 15, 9, 0), datetime(2026, 7, 15, 10, 30)),  # minutes
        (datetime(2026, 7, 15, 9, 0, 0), datetime(2026, 7, 15, 9, 2, 0)),  # seconds-ish
    ]
    cases = []
    for d0, d1 in spans:
        fig, ax = plt.subplots()
        ax.set_xlim(mdates.date2num(d0), mdates.date2num(d1))
        loc = mdates.AutoDateLocator()
        fmt = mdates.ConciseDateFormatter(loc)
        ax.xaxis.set_major_locator(loc)
        ax.xaxis.set_major_formatter(fmt)
        fig.canvas.draw()
        cases.append({
            "vmin": float(mdates.date2num(d0)), "vmax": float(mdates.date2num(d1)),
            "ticks": [float(t) for t in ax.get_xticks()],
            "labels": [t.get_text() for t in ax.get_xticklabels()],
            "offset": ax.xaxis.get_offset_text().get_text(),
        })
        plt.close(fig)
    return cases


def annotate_arrows():
    """Annotation arrow pipeline oracle: FancyArrowPatch posA->posB with
    optional patchA clip, shrink circles, and '-'/'->'/'-|>' head transmute.
    All inputs/outputs in display px (IdentityTransform, dpi_cor=1)."""
    from matplotlib.patches import FancyArrowPatch, Rectangle
    from matplotlib.transforms import IdentityTransform

    cases_in = [
        # (posA, posB, style, lw, patchA rect or None)
        ((20.0, 30.0), (180.0, 140.0), "-", 1.0, None),
        ((20.0, 30.0), (180.0, 140.0), "->", 1.0, None),
        ((20.0, 30.0), (180.0, 140.0), "-|>", 1.0, None),
        ((70.0, 32.0), (180.0, 140.0), "->", 1.0, (40.0, 20.0, 60.0, 24.0)),
        ((120.0, 88.0), (30.0, 20.0), "-|>", 1.0, (90.0, 76.0, 60.0, 24.0)),
        ((20.0, 30.0), (180.0, 60.0), "->", 2.0, None),
    ]
    cases = []
    for posA, posB, style, lw, rect in cases_in:
        fap = FancyArrowPatch(posA, posB, arrowstyle=style, mutation_scale=10.0,
                              shrinkA=2.0, shrinkB=2.0, transform=IdentityTransform())
        fap.set_linewidth(lw)
        if rect is not None:
            fap.set_patchA(Rectangle(rect[:2], rect[2], rect[3],
                                     transform=IdentityTransform()))
        paths, fillable = fap._get_path_in_displaycoord()
        tail = paths[0].vertices
        head = paths[1].vertices if len(paths) > 1 else []
        cases.append({
            "posA": list(posA), "posB": list(posB), "style": style, "lw": lw,
            "patchA": list(rect) if rect else [],
            "tail": [float(tail[0][0]), float(tail[0][1]),
                     float(tail[-1][0]), float(tail[-1][1])],
            # first three head vertices (the CLOSEPOLY vertex of filled heads
            # is a dummy (0,0) in mpl)
            "head": [float(v) for xy in head[:3] for v in xy],
            "filled": bool(fillable[1]) if len(paths) > 1 else False,
        })
    return cases


# ---------------------------------------------------------------- emission

def cxx_str(s: str) -> str:
    out = s.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{out}"'


def cxx_num(v: float) -> str:
    r = repr(float(v))
    return r if ("." in r or "e" in r or "inf" in r or "nan" in r) else r + ".0"


def cxx_vec(vals, fn) -> str:
    return "{" + ", ".join(fn(v) for v in vals) + "}"


def emit_inc(data: dict) -> None:
    L = [f"// Generated by tests/fixtures/generate.py from matplotlib {matplotlib.__version__}"
         " — do not edit.",
         "#pragma once", "#include <cmath>", "#include <string>", "#include <vector>", "",
         "// clang-format off", "namespace fixtures {", ""]

    L.append("struct TickerRawCase { double vmin; double vmax; int nbins; "
             "std::vector<double> ticks; };")
    L.append("inline const std::vector<TickerRawCase> ticker_raw = {")
    for c in data["ticker_raw"]:
        L.append(f"    {{{cxx_num(c['vmin'])}, {cxx_num(c['vmax'])}, {c['nbins']}, "
                 f"{cxx_vec(c['ticks'], cxx_num)}}},")
    L.append("};\n")

    L.append("struct AxisTicksCase { double vmin; double vmax; std::vector<double> xticks; "
             "std::vector<std::string> xlabels; std::vector<double> yticks; "
             "std::vector<std::string> ylabels; };")
    L.append("inline const std::vector<AxisTicksCase> axis_ticks = {")
    for c in data["axis_ticks"]:
        L.append(f"    {{{cxx_num(c['vmin'])}, {cxx_num(c['vmax'])}, "
                 f"{cxx_vec(c['xticks'], cxx_num)}, {cxx_vec(c['xlabels'], cxx_str)}, "
                 f"{cxx_vec(c['yticks'], cxx_num)}, {cxx_vec(c['ylabels'], cxx_str)}}},")
    L.append("};\n")

    L.append("struct AutoscaleCase { double dlo; double dhi; double xlo; double xhi; "
             "double ylo; double yhi; };")
    L.append("inline const std::vector<AutoscaleCase> autoscale = {")
    for c in data["autoscale"]:
        L.append(f"    {{{cxx_num(c['dlo'])}, {cxx_num(c['dhi'])}, "
                 f"{cxx_num(c['xlim'][0])}, {cxx_num(c['xlim'][1])}, "
                 f"{cxx_num(c['ylim'][0])}, {cxx_num(c['ylim'][1])}}},")
    L.append("};\n")

    L.append("struct ColorCase { std::string spec; double r; double g; double b; double a; };")
    L.append("inline const std::vector<ColorCase> colors = {")
    for c in data["colors"]:
        r, g, b, a = c["rgba"]
        L.append(f"    {{{cxx_str(c['spec'])}, {cxx_num(r)}, {cxx_num(g)}, {cxx_num(b)}, "
                 f"{cxx_num(a)}}},")
    L.append("};\n")


    L.append("struct LogTicksCase { double vmin; double vmax; std::vector<double> major; "
             "std::vector<std::string> labels; std::vector<double> minor; };")
    L.append("inline const std::vector<LogTicksCase> log_ticks = {")
    for c in data["log_ticks"]:
        L.append(f"    {{{cxx_num(c['vmin'])}, {cxx_num(c['vmax'])}, "
                 f"{cxx_vec(c['major'], cxx_num)}, {cxx_vec(c['labels'], cxx_str)}, "
                 f"{cxx_vec(c['minor'], cxx_num)}}},")
    L.append("};\n")

    L.append("struct MinorTicksCase { double vmin; double vmax; std::vector<double> minor; };")
    L.append("inline const std::vector<MinorTicksCase> minor_ticks = {")
    for c in data["minor_ticks"]:
        L.append(f"    {{{cxx_num(c['vmin'])}, {cxx_num(c['vmax'])}, "
                 f"{cxx_vec(c['minor'], cxx_num)}}},")
    L.append("};\n")

    L.append("struct OffsetLabelsCase { double vmin; double vmax; std::vector<double> ticks; "
             "std::vector<std::string> labels; std::string offset_text; };")
    L.append("inline const std::vector<OffsetLabelsCase> offset_labels = {")
    for c in data["offset_labels"]:
        L.append(f"    {{{cxx_num(c['vmin'])}, {cxx_num(c['vmax'])}, "
                 f"{cxx_vec(c['ticks'], cxx_num)}, {cxx_vec(c['labels'], cxx_str)}, "
                 f"{cxx_str(c['offset_text'])}}},")
    L.append("};\n")

    L.append("struct HistBinsCase { std::vector<double> data; int bins; "
             "std::vector<int> counts; std::vector<double> edges; };")
    L.append("inline const std::vector<HistBinsCase> hist_bins = {")
    for c in data["hist_bins"]:
        L.append(f"    {{{cxx_vec(c['data'], cxx_num)}, {c['bins']}, "
                 f"{{{', '.join(str(v) for v in c['counts'])}}}, "
                 f"{cxx_vec(c['edges'], cxx_num)}}},")
    L.append("};\n")

    L.append("struct BarAutoscaleCase { std::vector<double> heights; double xlo; double xhi; "
             "double ylo; double yhi; };")
    L.append("inline const std::vector<BarAutoscaleCase> bar_autoscale = {")
    for c in data["bar_autoscale"]:
        L.append(f"    {{{cxx_vec(c['heights'], cxx_num)}, "
                 f"{cxx_num(c['xlim'][0])}, {cxx_num(c['xlim'][1])}, "
                 f"{cxx_num(c['ylim'][0])}, {cxx_num(c['ylim'][1])}}},")
    L.append("};\n")


    L.append("struct CmapSampleCase { std::string cmap; double x; "
             "double r; double g; double b; double a; };")
    L.append("inline const std::vector<CmapSampleCase> colormap_samples = {")
    for c in data["colormap_samples"]:
        x = 'std::nan("")' if c["x"] == "nan" else cxx_num(c["x"])
        r, g, b, a = c["rgba"]
        L.append(f'    {{{cxx_str(c["cmap"])}, {x}, {cxx_num(r)}, {cxx_num(g)}, '
                 f'{cxx_num(b)}, {cxx_num(a)}}},')
    L.append("};\n")

    L.append("struct NormCase { std::string kind; double vmin; double vmax; double v; "
             "double out; };")
    L.append("inline const std::vector<NormCase> norm_cases = {")
    for c in data["norm_cases"]:
        L.append(f'    {{{cxx_str(c["kind"])}, {cxx_num(c["vmin"])}, {cxx_num(c["vmax"])}, '
                 f'{cxx_num(c["v"])}, {cxx_num(c["out"])}}},')
    L.append("};\n")


    L.append("struct DateTicksCase { double vmin; double vmax; std::vector<double> ticks; "
             "std::vector<std::string> labels; std::string offset; };")
    L.append("inline const std::vector<DateTicksCase> date_ticks = {")
    for c in data["date_ticks"]:
        L.append(f'    {{{cxx_num(c["vmin"])}, {cxx_num(c["vmax"])}, '
                 f'{cxx_vec(c["ticks"], cxx_num)}, {cxx_vec(c["labels"], cxx_str)}, '
                 f'{cxx_str(c["offset"])}}},')
    L.append("};\n")

    L.append("struct AnnotateArrowCase { double ax; double ay; double bx; double by; "
             "std::string style; double lw; std::vector<double> patchA; "
             "double tail_x0; double tail_y0; double tail_x1; double tail_y1; "
             "std::vector<double> head; bool filled; };")
    L.append("inline const std::vector<AnnotateArrowCase> annotate_arrows = {")
    for c in data["annotate_arrows"]:
        L.append(f'    {{{cxx_num(c["posA"][0])}, {cxx_num(c["posA"][1])}, '
                 f'{cxx_num(c["posB"][0])}, {cxx_num(c["posB"][1])}, '
                 f'{cxx_str(c["style"])}, {cxx_num(c["lw"])}, '
                 f'{cxx_vec(c["patchA"], cxx_num)}, '
                 f'{cxx_num(c["tail"][0])}, {cxx_num(c["tail"][1])}, '
                 f'{cxx_num(c["tail"][2])}, {cxx_num(c["tail"][3])}, '
                 f'{cxx_vec(c["head"], cxx_num)}, {"true" if c["filled"] else "false"}}},')
    L.append("};\n")

    L += ["} // namespace fixtures", "// clang-format on", ""]
    (HERE / "fixtures.inc").write_text("\n".join(L), encoding="utf-8")


if __name__ == "__main__":
    data = {
        "ticker_raw": ticker_raw(),
        "axis_ticks": axis_ticks(),
        "autoscale": autoscale(),
        "colors": colors(),
        "log_ticks": log_ticks(),
        "minor_ticks": minor_ticks(),
        "offset_labels": offset_labels(),
        "hist_bins": hist_bins(),
        "bar_autoscale": bar_autoscale(),
        "colormap_samples": colormap_samples(),
        "norm_cases": norm_cases(),
        "date_ticks": date_ticks(),
        "annotate_arrows": annotate_arrows(),
    }
    for name, cases in data.items():
        (HERE / f"{name}.json").write_text(json.dumps({**META, "cases": cases}, indent=1),
                                           encoding="utf-8")
        print(f"{name}.json: {len(cases)} cases")
    emit_inc(data)
    print("fixtures.inc written")
