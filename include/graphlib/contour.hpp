#pragma once
// Mirrors matplotlib.contour.ContourSet — iso-lines and filled bands over a
// regular grid, extracted with marching squares.
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/colormaps.hpp"
#include "graphlib/path.hpp"

namespace graphlib {

class Axes;

/// kwargs of Axes::contour / contourf.
struct ContourOpts {
    std::span<const double> levels{}; // explicit; default: MaxNLocator over the data range
    std::string_view cmap{};          // rc image.cmap = 'viridis'
    std::string_view colors{};        // single color for all levels (overrides cmap)
    std::optional<double> linewidths{}; // contour lines; rc lines.linewidth
    std::optional<double> alpha{};
};

class ContourSet final : public Artist {
public:
    ContourSet() { zorder = 2.0; } // mpl contour zorder

    std::vector<double> x; // column coordinates (size cols)
    std::vector<double> y; // row coordinates (size rows)
    std::vector<double> z; // rows x cols, row-major
    std::vector<double> levels;
    bool filled = false;
    const Colormap* cmap = nullptr;
    std::optional<Color> single_color;
    double linewidth = 1.5;

    Axes* axes = nullptr;

    void draw(Renderer& renderer) override;
    [[nodiscard]] Bbox data_extents() const;

private:
    [[nodiscard]] size_t cols() const { return x.size(); }
    [[nodiscard]] size_t rows() const { return y.size(); }
    [[nodiscard]] double at(size_t r, size_t c) const { return z[r * cols() + c]; }
    /// Iso-lines for one level: marching-squares segments (MOVETO pairs).
    [[nodiscard]] Path level_lines(double level) const;
    /// Filled region {z >= level}: per-cell boundary-walk polygons. Bands are
    /// painted by stacking these fills ascending (painter's algorithm).
    [[nodiscard]] Path threshold_fill(double level) const;
};

} // namespace graphlib
