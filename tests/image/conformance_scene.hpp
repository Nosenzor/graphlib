#pragma once
// The canonical backend-conformance scene (new-backend skill): every renderer
// must draw this correctly — strokes with dashes/caps/joins, Bézier markers,
// alpha compositing, clipping, grids, aligned + rotated text.
// Data is libm-free (pure arithmetic) so vector output is byte-stable and
// raster output is platform-stable.
#include <vector>

#include "graphlib/figure.hpp"

inline void build_conformance_scene(graphlib::Figure& fig) {
    using namespace graphlib;
    Axes& ax = fig.add_subplot();

    // Line styles with default caps/joins, colors from the cycle.
    const std::vector<double> xs{0.0, 1.0, 2.0, 3.0};
    const char* styles[] = {"-", "--", "-.", ":"};
    for (int k = 0; k < 4; ++k) {
        std::vector<double> ys;
        for (const double x : xs) {
            ys.push_back(0.25 * x + 0.4 * k);
        }
        ax.plot(xs, ys, styles[k], {.linewidth = 2.0});
    }

    // Alpha compositing: translucent wide strokes crossing.
    const std::vector<double> ax1{0.0, 3.0};
    const std::vector<double> ay1{2.0, 3.4};
    const std::vector<double> ay2{3.4, 2.0};
    ax.plot(ax1, ay1, {.color = "tab:blue", .linewidth = 8.0, .alpha = 0.5});
    ax.plot(ax1, ay2, {.color = "tab:red", .linewidth = 8.0, .alpha = 0.5});

    // Markers (Bézier circles, polygons, stroke-only crosses).
    const char* fmts[] = {"o", "s", "^", "D", "x", "*"};
    for (int k = 0; k < 6; ++k) {
        const std::vector<double> mx{0.25 + 0.5 * k};
        const std::vector<double> my{-0.6};
        ax.plot(mx, my, fmts[k], {.markersize = 9.0});
    }

    // Clipping: a line that runs beyond the manual limits on both sides.
    const std::vector<double> cx{-1.0, 4.0};
    const std::vector<double> cy{-2.0, 4.5};
    ax.plot(cx, cy, {.color = "0.3", .linewidth = 1.0});

    // Text: alignments + rotation; the ylabel exercises 90° glyph outlines.
    ax.text(1.5, 1.9, "center", {.ha = HAlign::center});
    ax.text(3.1, 0.1, "right", {.ha = HAlign::right});
    ax.text(0.4, 0.9, "rot45", {.rotation = 45.0});
    ax.set_title("backend conformance");
    ax.set_xlabel("x axis");
    ax.set_ylabel("rotated label");
    ax.grid(true);
    ax.set_xlim(-0.2, 3.2);
    ax.set_ylim(-1.0, 3.6);
}
