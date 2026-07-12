#include "graphlib/graphics_context.hpp"

#include "graphlib/errors.hpp"

namespace graphlib {

LineStyle LineStyle::parse(std::string_view token) {
    using K = LineStyle::Kind;
    // Token spellings mirror matplotlib.lines.lineStyles + the long names.
    if (token == "-" || token == "solid") {
        return {K::solid};
    }
    if (token == "--" || token == "dashed") {
        return {K::dashed};
    }
    if (token == "-." || token == "dashdot") {
        return {K::dashdot};
    }
    if (token == ":" || token == "dotted") {
        return {K::dotted};
    }
    if (token.empty() || token == "None" || token == "none" || token == " ") {
        return {K::none};
    }
    throw ValueError("Unrecognized linestyle '" + std::string(token) + "'");
}

DashPattern LineStyle::dash_pattern(double linewidth) const {
    // Patterns at lw=1 from mpl lines._get_dash_pattern, scaled by linewidth
    // (rc lines.scale_dashes) — docs/DESIGN.md §6.
    switch (kind) {
    case Kind::dashed:
        return {0.0, {3.7 * linewidth, 1.6 * linewidth}};
    case Kind::dashdot:
        return {0.0, {6.4 * linewidth, 1.6 * linewidth, 1.0 * linewidth, 1.6 * linewidth}};
    case Kind::dotted:
        return {0.0, {1.0 * linewidth, 1.65 * linewidth}};
    case Kind::solid:
    case Kind::none:
        return {};
    }
    return {};
}

} // namespace graphlib
