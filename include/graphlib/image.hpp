#pragma once
// Mirrors matplotlib.image.AxesImage — 2D data through norm + colormap.
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/colormaps.hpp"

namespace graphlib {

class Axes;

/// kwargs of Axes::imshow.
struct ImshowOpts {
    std::string_view cmap{};                        // rc image.cmap = 'viridis'
    std::optional<double> vmin{};                   // default: finite data min
    std::optional<double> vmax{};                   // default: finite data max
    std::string_view origin{};                      // rc image.origin = 'upper'
    std::optional<std::array<double, 4>> extent{};  // left, right, bottom, top
    std::string_view interpolation{};               // 'nearest' (mpl 'auto' -> nearest, D12)
    std::optional<double> alpha{};
    std::string_view aspect{};                      // rc image.aspect = 'equal'
};

class AxesImage final : public Artist {
public:
    AxesImage() { zorder = 0.0; } // mpl AxesImage zorder

    std::vector<double> data; // row-major rows x cols
    int rows = 0;
    int cols = 0;
    const Colormap* cmap = nullptr;
    double vmin = 0.0;
    double vmax = 1.0;
    bool origin_upper = true;             // data[0][0] at the top-left
    std::array<double, 4> extent{};       // data coords: left, right, bottom, top
    Interp interpolation = Interp::nearest;

    Axes* axes = nullptr;

    void draw(Renderer& renderer) override;
    [[nodiscard]] Bbox data_extents() const;
};

} // namespace graphlib
