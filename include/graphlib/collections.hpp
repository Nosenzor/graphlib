#pragma once
// Mirrors matplotlib.collections.PathCollection as produced by Axes::scatter():
// one marker path stamped at data offsets with per-point sizes.
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/color.hpp"
#include "graphlib/markers.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

class Axes;

/// kwargs of Axes::scatter — names/defaults mirror matplotlib.
struct ScatterOpts {
    std::span<const double> s{};       // sizes in pt^2 (broadcasts); default 36 (markersize^2)
    std::string_view c{};              // uniform color; "" -> property cycle
    std::span<const double> c_array{}; // mpl's array form of c=: values through cmap/norm
    std::string_view cmap{};           // rc image.cmap = 'viridis' (with c_array)
    std::optional<double> vmin{};      // default: c_array min/max
    std::optional<double> vmax{};
    std::string_view marker{};         // rc scatter.marker = 'o'
    std::optional<double> alpha{};
    std::string_view edgecolors{};     // rc scatter.edgecolors = 'face' (follow the face color)
    std::optional<double> linewidths{}; // edge width, mpl default 1.0
    std::optional<double> zorder{};    // Collection default 1
    std::string_view label{};
};

class PathCollection final : public Artist {
public:
    PathCollection() { zorder = 1.0; } // matplotlib Collection zorder

    std::vector<double> xdata;
    std::vector<double> ydata;
    std::vector<double> sizes; // pt^2; size() == 1 broadcasts, else per point
    const Marker* marker = nullptr;
    Color facecolor = colors::tab10[0];
    Color edgecolor = colors::tab10[0];
    std::vector<Color> facecolors; // per-point (c_array through cmap); empty == uniform
    bool edge_follows_face = true; // rc scatter.edgecolors = 'face'
    double linewidth = 1.0;

    Axes* axes = nullptr; // set by Axes::scatter

    void draw(Renderer& renderer) override;
    [[nodiscard]] Bbox data_extents() const;
};

} // namespace graphlib
