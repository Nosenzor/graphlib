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
         "#pragma once", "#include <string>", "#include <vector>", "",
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
    }
    for name, cases in data.items():
        (HERE / f"{name}.json").write_text(json.dumps({**META, "cases": cases}, indent=1),
                                           encoding="utf-8")
        print(f"{name}.json: {len(cases)} cases")
    emit_inc(data)
    print("fixtures.inc written")
