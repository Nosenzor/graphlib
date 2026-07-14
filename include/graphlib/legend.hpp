#pragma once
// Mirrors matplotlib.legend.Legend — v0.2: line/marker handles, loc strings
// including the 'best' candidate scan (port of Legend._find_best_position).
// Frame is a plain rectangle (mpl uses a slightly rounded FancyBbox — PARITY D8).
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/color.hpp"
#include "graphlib/graphics_context.hpp"
#include "graphlib/markers.hpp"

namespace graphlib {

class Axes;

/// kwargs of Axes::legend.
struct LegendOpts {
    std::string_view loc{};             // rc legend.loc = 'best'
    std::optional<double> fontsize{};   // rc legend.fontsize 'medium' = 10
    std::optional<bool> frameon{};      // rc legend.frameon = true
    std::optional<double> framealpha{}; // rc legend.framealpha = 0.8
};

class Legend final : public Artist {
public:
    Legend() { zorder = 5.0; }

    /// One row: a handle sample (line, marker, or patch swatch) + the label.
    struct Entry {
        std::string label;
        Color color;
        LineStyle linestyle{};
        double linewidth = 1.5;
        const Marker* marker = nullptr;
        Color markerfacecolor;
        Color markeredgecolor;
        double markersize = 6.0;
        double markeredgewidth = 1.0;
        double marker_scale = 1.0; // rc legend.markerscale
        bool patch_swatch = false; // bar/hist/fill handles: a filled rectangle
        Color patch_facecolor;
        Color patch_edgecolor;
    };

    std::vector<Entry> entries;
    int loc_code = 0; // matplotlib Legend.codes; 0 == 'best'
    double fontsize = 10.0;
    bool frameon = true;
    double framealpha = 0.8;

    Axes* axes = nullptr;

    void draw(Renderer& renderer) override;

    /// matplotlib Legend.codes: 'best' 0, 'upper right' 1, 'upper left' 2,
    /// 'lower left' 3, 'lower right' 4, 'right' 5, 'center left' 6,
    /// 'center right' 7, 'lower center' 8, 'upper center' 9, 'center' 10.
    /// Throws graphlib::ValueError on unknown strings.
    static int parse_loc(std::string_view loc);
};

} // namespace graphlib
