#pragma once
// Mirrors matplotlib.figure.Figure — owns the artist tree, savefig dispatch.
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "graphlib/axes.hpp"
#include "graphlib/color.hpp"

namespace graphlib {

struct FigureOpts {
    std::array<double, 2> figsize{6.4, 4.8}; // rc figure.figsize, inches
    double dpi = 100.0;                      // rc figure.dpi (SVG renders at 72, like mpl)
    std::string_view facecolor = "white";    // rc figure.facecolor
};

struct SaveOpts {
    std::optional<double> dpi; // raster backends only (v0.2); ignored by SVG
    bool transparent = false;  // suppress figure/axes face patches
};

class Figure {
public:
    Figure() : Figure(FigureOpts{}) {}
    explicit Figure(const FigureOpts& opts);

    /// Single axes at the default rect (grid layouts arrive v0.3).
    Axes& add_subplot();
    Axes& add_axes(Bbox position_fraction);
    /// Current (last) axes; creates one if none exist — mirrors pyplot.gca.
    Axes& gca();

    /// Render to file; the format comes from the extension (.svg in v0.1).
    /// Unknown extensions throw ValueError naming the milestone that adds them.
    void savefig(const std::string& filename, const SaveOpts& opts = {}) const;

    void draw(Renderer& renderer) const;

    std::array<double, 2> figsize;
    double dpi;
    Color facecolor;

    [[nodiscard]] const std::vector<std::unique_ptr<Axes>>& axes_list() const { return axes_; }
    /// True while a savefig(transparent=true) render is in flight — the axes
    /// patch checks this to skip its background.
    [[nodiscard]] bool transparent_render() const { return transparent_render_; }

private:
    std::vector<std::unique_ptr<Axes>> axes_;
    mutable bool transparent_render_ = false; // savefig(transparent=true) scope flag
};

} // namespace graphlib
