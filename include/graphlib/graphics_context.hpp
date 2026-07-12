#pragma once
// Stroke/fill state handed to the renderer — mirrors GraphicsContextBase
// (docs/DESIGN.md §6). Artists build one per draw call.
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graphlib/color.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

enum class CapStyle { butt, round, projecting };
enum class JoinStyle { miter, round, bevel };

/// Dash sequence in points, already scaled by linewidth (mpl scales dashes).
struct DashPattern {
    double offset = 0.0;
    std::vector<double> on_off; // empty == solid
    [[nodiscard]] bool is_solid() const { return on_off.empty(); }
};

struct GraphicsContext {
    Color color{0, 0, 0, 1};
    double linewidth = 1.0; // points
    DashPattern dashes{};
    CapStyle capstyle = CapStyle::butt;
    JoinStyle joinstyle = JoinStyle::round;
    bool antialiased = true;
    std::optional<Bbox> clip_rect; // device pixels, y-up display coords
};

/// Line style tokens: '-', '--', '-.', ':', 'None'/''/' ' (accepted spellings
/// mirror matplotlib.lines.lineStyles; 'solid'/'dashed'/'dashdot'/'dotted' too).
/// Throws graphlib::ValueError on unknown tokens.
struct LineStyle {
    enum class Kind { solid, dashed, dashdot, dotted, none };
    Kind kind = Kind::solid;

    static LineStyle parse(std::string_view token);
    [[nodiscard]] bool drawn() const { return kind != Kind::none; }
    /// Pattern in points at the given linewidth (mpl: dashes scale with lw):
    /// '--' (3.7,1.6) · '-.' (6.4,1.6,1.0,1.6) · ':' (1.0,1.65), all × lw.
    [[nodiscard]] DashPattern dash_pattern(double linewidth) const;
    /// rc default capstyle: solid -> projecting, dashed -> butt.
    [[nodiscard]] CapStyle default_capstyle() const {
        return kind == Kind::solid ? CapStyle::projecting : CapStyle::butt;
    }
};

} // namespace graphlib
