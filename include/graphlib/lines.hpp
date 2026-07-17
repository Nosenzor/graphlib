#pragma once
// Mirrors matplotlib.lines.Line2D. Created via Axes::plot().
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/color.hpp"
#include "graphlib/graphics_context.hpp"
#include "graphlib/markers.hpp"

namespace graphlib {

class Axes;

/// kwargs of Axes::plot — field names/defaults mirror matplotlib rcParams;
/// an unset optional/empty string means "use the rc default / format string".
struct LineOpts {
    std::string_view color{};                 // "" -> fmt color or property cycle
    std::optional<double> linewidth{};        // lines.linewidth = 1.5
    std::string_view linestyle{};             // lines.linestyle = '-'
    std::string_view marker{};                // lines.marker = none
    std::optional<double> markersize{};       // lines.markersize = 6
    std::string_view markerfacecolor{};       // default: line color
    std::string_view markeredgecolor{};       // default: line color
    std::optional<double> markeredgewidth{};  // lines.markeredgewidth = 1
    std::optional<double> alpha{};
    std::optional<double> zorder{};           // Line2D default 2
    std::string_view label{};
};

/// Line2D.drawstyle (mirrors mpl): steps hold the previous/mid/next value.
enum class DrawStyle { normal, steps_pre, steps_mid, steps_post };

class Line2D final : public Artist {
public:
    Line2D() { zorder = 2.0; }

    std::vector<double> xdata;
    std::vector<double> ydata;
    DrawStyle drawstyle = DrawStyle::normal;
    Color color = colors::tab10[0];
    double linewidth = 1.5;
    LineStyle linestyle{};
    const Marker* marker = nullptr; // nullptr == no marker
    double markersize = 6.0;
    Color markerfacecolor = colors::tab10[0];
    Color markeredgecolor = colors::tab10[0];
    double markeredgewidth = 1.0;

    Axes* axes = nullptr; // set by Axes::plot

    void draw(Renderer& renderer) override;

    /// Extent of the raw data (NaN-aware) — the Axes dataLim contribution.
    [[nodiscard]] Bbox data_extents() const;

private:
    friend class Axes; // axhline/axvline set the blended-transform flags
    // Interpret that coordinate as axes fraction (0..1) — mpl's blended
    // transform for reference lines/spans.
    bool x_axes_fraction = false;
    bool y_axes_fraction = false;
};

namespace detail {
/// Port of matplotlib.axes._base._process_plot_format ('ro--', 'C2^', 'g', …).
/// Unset optionals mean "not specified in fmt"; the defaulting rules
/// (line if neither, 'None' otherwise) are applied by Axes::plot.
struct ParsedFormat {
    std::optional<std::string> color;
    std::optional<std::string> linestyle;
    std::optional<std::string> marker;
};
ParsedFormat parse_plot_format(std::string_view fmt); // throws ValueError like mpl
} // namespace detail

} // namespace graphlib
