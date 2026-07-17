#!/usr/bin/env python3
"""matplotlib twins of the graphlib gallery, regenerated from the local
matplotlib oracle (3.10.x). Run with `python3 tools/mpl_twins.py`.

Each function below is a line-by-line translation of the corresponding
examples/NN_*.cpp source (same data formulas, point counts, constants,
plot calls, titles, labels, limits, styles, colormaps). The outputs are
written to docs/conformance/mpl/<name>.svg and are the matplotlib side of
the v1.0 conformance report; the graphlib side lives in docs/gallery/.

Determinism knobs (ids/date only, no visual effect): svg.hashsalt is
pinned and the SVG Date metadata is stripped so reruns are byte-stable.
"""

from __future__ import annotations

import datetime as dt
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import numpy as np

OUT_DIR = Path(__file__).resolve().parent.parent / "docs" / "conformance" / "mpl"


def save(fig, out: Path) -> None:
    """Save one figure as deterministic SVG and close it."""
    matplotlib.rcParams["svg.hashsalt"] = "graphlib-conformance"  # stable clip-path ids
    fig.savefig(out, metadata={"Date": None})  # Date: None omits the volatile timestamp
    plt.close(fig)


# -- 01_hello_lines.cpp -> hello.svg -----------------------------------------
def hello(out: Path) -> None:
    x = np.linspace(0.0, 2.0 * np.pi, 200)
    plt.plot(x, np.sin(x), linewidth=2.0, label="sin(x)")
    plt.plot(x, np.cos(x), "--", label="cos(x)")
    plt.title("Hello graphlib")
    plt.xlabel("x [rad]")
    plt.ylabel("amplitude")
    plt.grid(True)
    save(plt.gcf(), out)


# -- 02_color_cycle.cpp -> color_cycle.svg ------------------------------------
def color_cycle(out: Path) -> None:
    x = np.linspace(0.0, 2.0 * np.pi, 300)
    for k in range(10):
        plt.plot(x, np.sin(x - 0.3 * k) + 0.4 * k)
    plt.title("The tab10 property cycle")
    save(plt.gcf(), out)


# -- 03_fmt_and_markers.cpp -> fmt_and_markers.svg ----------------------------
def fmt_and_markers(out: Path) -> None:
    x = np.linspace(0.0, 9.0, 10)

    def shifted(dy: float) -> np.ndarray:
        return 0.5 * x + dy

    plt.plot(x, shifted(0), "r--o")
    plt.plot(x, shifted(2), "g^")  # marker only: no line
    plt.plot(x, shifted(4), "C4s-")  # cycle color 4, squares, solid
    plt.plot(x, shifted(6), "k:D")
    plt.plot(shifted(9), "b*-")  # y-only overload: x = 0..N-1
    plt.title('Format strings: "r--o", "g^", "C4s-", "k:D", "b*-"')
    save(plt.gcf(), out)


# -- 04_limits_grid.cpp -> limits_grid.svg ------------------------------------
def limits_grid(out: Path) -> None:
    x = np.linspace(-6.0, 6.0, 400)
    y = np.tan(x)  # wild values: exercises clipping hard
    y[np.abs(y) > 40.0] = np.nan  # break at the poles
    plt.plot(x, y, linewidth=1.5, label="tan(x)")
    plt.xlim(-5, 5)
    plt.ylim(-4, 4)
    plt.grid(True)
    plt.text(-4.6, 3.2, "tan(x), poles as gaps")
    plt.title("Manual limits + grid")
    plt.xlabel("x")
    plt.ylabel("tan(x)")
    save(plt.gcf(), out)


# -- 05_scatter.cpp -> scatter.svg ---------------------------------------------
def scatter(out: Path) -> None:
    # A deterministic swirl (no RNG in examples: reproducible gallery).
    i1 = np.arange(60)
    t1 = 0.18 * i1
    x1 = 0.05 * t1 * np.cos(2.2 * t1)
    y1 = 0.05 * t1 * np.sin(2.2 * t1)
    sizes = 9.0 + i1 * 2.5  # pt^2, grows along the swirl
    plt.scatter(x1, y1, s=sizes, label="swirl")

    i2 = np.arange(40)
    t2 = 0.16 * i2
    x2 = -0.35 + 0.04 * t2 * np.cos(2.9 * t2 + 1.5)
    y2 = 0.25 + 0.04 * t2 * np.sin(2.9 * t2 + 1.5)
    plt.scatter(x2, y2, marker="^", alpha=0.7, label="second cluster")

    plt.title("scatter: sizes in points^2")
    plt.grid(True)
    plt.legend(loc="lower right")
    save(plt.gcf(), out)


# -- 06_legend.cpp -> legend.svg -----------------------------------------------
def legend(out: Path) -> None:
    x = np.linspace(0.0, 2.0 * np.pi, 200)
    envelope = np.exp(-0.35 * x)
    damped = envelope * np.cos(4.0 * x)
    plt.plot(x, damped, linewidth=1.5, label="e^{-t/3} cos 4t")
    plt.plot(x, envelope, "--", label="envelope")

    t = 0.25 + 0.75 * np.arange(8)
    px = t
    py = np.exp(-0.35 * t) * np.cos(4.0 * t)
    plt.scatter(px, py, c="tab:red", label="samples")

    plt.title('legend(loc="best") avoids the data')
    plt.xlabel("t [s]")
    plt.ylabel("signal")
    plt.grid(True)
    plt.legend()
    save(plt.gcf(), out)


# -- 07_bar_hist.cpp -> bar_hist.svg -------------------------------------------
def bar_hist(out: Path) -> None:
    fig, axs = plt.subplots(2, 2, figsize=(10.0, 7.5))

    axs[0, 0].bar(["mon", "tue", "wed", "thu", "fri"],
                  [4.2, 7.1, 6.4, 8.3, 5.9], label="sessions")
    axs[0, 0].set_title("bar")
    axs[0, 0].set_ylabel("count")

    i = np.arange(300)
    data = 2.0 * np.sin(0.7 * i) + np.sin(0.13 * i + 1.0)
    axs[0, 1].hist(data, bins=15, alpha=0.85)
    axs[0, 1].set_title("hist")

    x = [1, 2, 3, 4, 5, 6]
    y = [2.0, 3.5, 3.1, 4.6, 4.2, 5.1]
    err = [0.4, 0.3, 0.5, 0.25, 0.45, 0.3]
    axs[1, 0].errorbar(x, y, yerr=err, fmt="s-", capsize=3.0)
    axs[1, 0].set_title("errorbar")
    axs[1, 0].grid(True)

    axs[1, 1].pie([35, 25, 20, 12, 8],
                  labels=["core", "text", "backends", "tests", "docs"],
                  startangle=90)
    axs[1, 1].set_title("pie")

    fig.suptitle("graphlib everyday plots")
    fig.tight_layout()
    save(fig, out)


# -- 08_subplots_dashboard.cpp -> subplots_dashboard.svg -----------------------
def subplots_dashboard(out: Path) -> None:
    fig, axs = plt.subplots(2, 1, sharex=True, figsize=(9.0, 6.5))  # inner row loses x labels

    t = np.linspace(0.0, 4.0 * np.pi, 400)
    env = np.exp(-0.18 * t)
    sig = env * np.sin(3.0 * t)
    env_hi = env
    env_lo = -env
    axs[0].fill_between(t, env_hi, env_lo, color="tab:blue", alpha=0.2)
    axs[0].plot(t, sig, label="signal")
    axs[0].axhline(0.0, 0, 1, color="0.4", linewidth=0.8)
    axs[0].set_title("damped oscillator")
    axs[0].set_ylabel("amplitude")
    rate = axs[0].twinx()
    energy = np.exp(-0.36 * t)
    rate.plot(t, energy, "C3--")
    rate.set_ylabel("energy")

    steps = np.floor(2.0 + 1.8 * np.sin(0.9 * t))
    axs[1].step(t, steps, color="tab:green", label="quantized")
    axs[1].axvspan(4.0, 6.0, color="tab:orange", alpha=0.25)
    axs[1].set_xlabel("t [s]")
    axs[1].set_ylabel("level")
    axs[1].grid(True)

    fig.suptitle("layout: sharex + twinx + spans")
    fig.tight_layout()
    save(fig, out)


# -- 09_log_scales.cpp -> log_scales.svg ---------------------------------------
def log_scales(out: Path) -> None:
    fig, axs = plt.subplots(1, 2, figsize=(10.0, 4.2))

    x = np.linspace(0.1, 100.0, 300)
    power = 3.0 * x * x
    root = 20.0 * np.sqrt(x)
    axs[0].plot(x, power, label="3x²")
    axs[0].plot(x, root, "--", label="20√x")
    axs[0].set_xscale("log")
    axs[0].set_yscale("log")
    axs[0].grid(True)
    axs[0].set_title("log-log")
    axs[0].legend(loc="upper left")

    # Large magnitudes: the axis-end offset text takes over.
    t = np.linspace(0.0, 10.0, 100)
    drift = 1.0e8 + 12.0 * t + 3.0 * np.sin(2.0 * t)
    axs[1].plot(t, drift, "C2")
    axs[1].set_title("offset text (+1e8)")
    axs[1].grid(True)

    fig.tight_layout()
    save(fig, out)


# -- 10_styles_gallery.cpp -> style_{default,ggplot,dark_background}.svg -------
def _style_twin(style_name: str, out: Path) -> None:
    # graphlib::style::use(name) starts from clean defaults each time ->
    # matplotlib spelling: a style context with after_reset=True.
    with plt.style.context(style_name, after_reset=True):
        fig = plt.figure(figsize=(6.4, 4.0))
        x = np.linspace(0.0, 2.0 * np.pi, 200)
        for k in range(4):
            y = np.sin(x + 0.6 * k) * (1.0 - 0.15 * k)
            plt.plot(x, y, linewidth=2.0, label=f"wave {k}")
        plt.title(f"style: {style_name}")
        plt.legend(loc="lower left")
        save(fig, out)


def style_default(out: Path) -> None:
    _style_twin("default", out)


def style_ggplot(out: Path) -> None:
    _style_twin("ggplot", out)


def style_dark_background(out: Path) -> None:
    _style_twin("dark_background", out)


# -- 11_heatmap_colorbar.cpp -> heatmap_colorbar.svg ---------------------------
def heatmap_colorbar(out: Path) -> None:
    R, C = 60, 90
    # graphlib imshow(z, R, C) takes a flat row-major buffer + dims ->
    # matplotlib spelling: a 2D (R, C) array.
    cc, rr = np.meshgrid(np.arange(C), np.arange(R))
    x = (cc - C / 2.0) / 12.0
    y = (rr - R / 2.0) / 9.0
    z = np.sin(1.4 * x) * np.cos(1.1 * y) * np.exp(-0.08 * (x * x + y * y))
    fig = plt.figure()
    im = plt.imshow(z, cmap="coolwarm")
    plt.title("interference pattern")
    fig.colorbar(im, label="amplitude")
    save(fig, out)


# -- 12_contour_gaussian.cpp -> contour_gaussian.svg ---------------------------
def contour_gaussian(out: Path) -> None:
    x = np.linspace(-3.0, 3.0, 120)
    y = np.linspace(-2.2, 2.2, 90)
    xx, yy = np.meshgrid(x, y)  # z is row-major over y (rows) then x (cols)
    z = (np.exp(-((xx + 1.0) ** 2 + yy**2))
         + 0.8 * np.exp(-((xx - 1.2) ** 2 + (yy - 0.4) ** 2) / 0.6))
    plt.contourf(x, y, z)
    plt.contour(x, y, z, colors="k", linewidths=0.5)
    plt.title("two Gaussians (contourf + contour)")
    plt.xlabel("x")
    plt.ylabel("y")
    save(plt.gcf(), out)


# -- 13_pcolormesh.cpp -> pcolormesh.svg ---------------------------------------
def pcolormesh(out: Path) -> None:
    # Non-uniform y edges: logarithmically spaced.
    x_edges = np.linspace(0.0, 10.0, 41)
    y_edges = 10.0 ** (-1.0 + 3.0 * np.arange(25) / 24.0)
    xm = (x_edges[:-1] + x_edges[1:]) / 2.0
    ym = np.sqrt(y_edges[:-1] * y_edges[1:])
    c = np.sin(xm)[None, :] * np.log10(ym)[:, None]
    fig = plt.figure()
    mesh = plt.pcolormesh(x_edges, y_edges, c, cmap="viridis")
    plt.gca().set_yscale("log")
    plt.title("pcolormesh, log y")
    fig.colorbar(mesh)
    save(fig, out)


# -- 14_scatter_cmap.cpp -> scatter_cmap.svg -----------------------------------
def scatter_cmap(out: Path) -> None:
    i = np.arange(220)
    t = 0.055 * i
    x = t * np.cos(3.1 * t)
    y = 0.8 * t * np.sin(3.1 * t)
    speed = np.abs(np.sin(2.0 * t)) * t
    size = 12.0 + 3.0 * t
    # graphlib spells the value array `c_array` (kwargs struct); mpl spells it `c`.
    plt.scatter(x, y, s=size, c=speed, cmap="plasma", alpha=0.85)
    plt.title('scatter(c=speed, cmap="plasma")')
    plt.grid(True)
    save(plt.gcf(), out)


# -- 17_annotations.cpp -> annotations.svg -------------------------------------
def annotations(out: Path) -> None:
    t = np.linspace(0.0, 4.0 * np.pi, 400)
    s = np.exp(-0.15 * t) * np.cos(2.0 * t)
    plt.plot(t, s, linewidth=1.5)
    plt.ylim(-1.2, 1.4)

    t_peak = np.pi  # near the second crest
    plt.annotate("local max", (t_peak, np.exp(-0.15 * t_peak)),
                 xytext=(t_peak + 1.5, 1.1),
                 arrowprops=dict(arrowstyle="->"))
    plt.annotate("decay envelope", (7.0, np.exp(-0.15 * 7.0)),
                 xytext=(0.62, 0.82),
                 textcoords="axes fraction",
                 arrowprops=dict(arrowstyle="-|>"))
    plt.annotate("sampled here", (2.0 * np.pi, np.exp(-0.15 * 2.0 * np.pi)),
                 xytext=(-60.0, -35.0),
                 textcoords="offset points",
                 arrowprops=dict(arrowstyle="-"))

    plt.title("Damped oscillation")
    plt.xlabel("t")
    plt.ylabel("amplitude")
    save(plt.gcf(), out)


# -- 18_dates.cpp -> dates.svg ---------------------------------------------------
def dates(out: Path) -> None:
    # Ninety days of a noisy-looking (but deterministic) series.
    d0 = mdates.date2num(dt.date(2026, 4, 16))
    di = np.arange(90.0)
    day_num = d0 + di
    value = 0.05 * di + np.sin(di / 6.0) + 0.4 * np.sin(di / 1.7)
    plt.plot(day_num, value, linewidth=1.5, label="daily index")
    # graphlib xaxis_date() installs AutoDateLocator + ConciseDateFormatter;
    # spell that explicitly here.
    ax = plt.gca()
    locator = mdates.AutoDateLocator()
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    plt.legend()
    plt.title("Spring 2026")
    plt.ylabel("index")
    save(plt.gcf(), out)


# -- 19_mathtext.cpp -> mathtext.svg ---------------------------------------------
def mathtext(out: Path) -> None:
    x = np.linspace(0.0, 2.0, 200)
    y = x * x * np.exp(-2.0 * x)
    plt.plot(x, y, linewidth=2.0, label=r"$x^2 e^{-2x}$")
    plt.title(r"$f(x) = x^2 e^{-2x}$")
    plt.xlabel(r"$x$")
    plt.ylabel(r"$f(x)$")
    plt.legend()
    plt.annotate(r"$\max f = e^{-2}$", (1.0, np.exp(-2.0)),
                 xytext=(1.35, 0.125),
                 arrowprops=dict(arrowstyle="->"))
    plt.text(0.98, 0.04, r"$\sigma = \sqrt{\frac{1}{n}\sum_{i=1}^{n}(x_i-\mu)^2}$",
             fontsize=14.0)
    save(plt.gcf(), out)


# -- 20_paper_figure.cpp -> paper_figure.svg -------------------------------------
def paper_figure(out: Path) -> None:
    fig, (left, right) = plt.subplots(1, 2, figsize=(8.0, 3.6))

    # Left: model vs theory, annotated.
    x = np.linspace(0.0, 4.0, 300)
    theory = np.exp(-x) * np.sin(6.0 * x) / (1.0 + x)
    model = theory + 0.02 * np.sin(40.0 * x)
    left.plot(x, model, linewidth=1.0, label="measured")
    left.plot(x, theory, "--", linewidth=1.5, label=r"$\frac{e^{-t}\sin 6t}{1+t}$")
    left.set_title(r"$\Gamma(t)$ response")
    left.set_xlabel(r"$t$ (s)")
    left.set_ylabel(r"$\Gamma$")
    left.legend()
    left.annotate("ringing", (0.26, theory[20]),
                  xytext=(0.42, 0.18),
                  textcoords="axes fraction",
                  arrowprops=dict(arrowstyle="->"))

    # Right: a month of daily measurements on a date axis.
    d0 = mdates.date2num(dt.date(2026, 6, 15))
    di = np.arange(30.0)
    day_num = d0 + di
    level = 5.0 + 0.08 * di + 0.6 * np.sin(di / 3.1)
    right.plot(day_num, level, "o-", markersize=3.0, label=r"$\mu_{daily}$")
    # graphlib xaxis_date() == AutoDateLocator + ConciseDateFormatter.
    locator = mdates.AutoDateLocator()
    right.xaxis.set_major_locator(locator)
    right.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    right.set_title(r"field data, $\sigma < 0.5$")
    right.set_ylabel("level (m)")
    right.legend()

    fig.tight_layout()
    save(fig, out)  # SVG twin only; the C++ example also emits PDF/PNG


TWINS = {
    "hello": hello,
    "color_cycle": color_cycle,
    "fmt_and_markers": fmt_and_markers,
    "limits_grid": limits_grid,
    "scatter": scatter,
    "legend": legend,
    "bar_hist": bar_hist,
    "subplots_dashboard": subplots_dashboard,
    "log_scales": log_scales,
    "style_default": style_default,
    "style_ggplot": style_ggplot,
    "style_dark_background": style_dark_background,
    "heatmap_colorbar": heatmap_colorbar,
    "contour_gaussian": contour_gaussian,
    "pcolormesh": pcolormesh,
    "scatter_cmap": scatter_cmap,
    "annotations": annotations,
    "dates": dates,
    "mathtext": mathtext,
    "paper_figure": paper_figure,
}


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    failed = []
    for name, twin in TWINS.items():
        out = OUT_DIR / f"{name}.svg"
        try:
            twin(out)
        except Exception as exc:  # keep going: report every failure at once
            failed.append((name, exc))
            plt.close("all")
            continue
        print(f"wrote {out.relative_to(OUT_DIR.parent.parent.parent)}")
    if failed:
        for name, exc in failed:
            print(f"FAILED {name}: {exc!r}", file=sys.stderr)
        return 1
    print(f"{len(TWINS)} twins OK (matplotlib {matplotlib.__version__})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
