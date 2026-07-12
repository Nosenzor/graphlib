#pragma once
// Marker geometry (mirrors matplotlib.markers) — paths are in "marker units":
// 1 unit == markersize in points; scale by points_to_pixels(markersize) at draw.
// Tables are generated from matplotlib by tools/gen_tables.py.
#include <string_view>

#include "graphlib/path.hpp"

namespace graphlib {

struct Marker {
    std::string_view name;
    Path path;   // marker units, centered on (0,0)
    bool filled; // unfilled markers (+, x, ...) are stroke-only
};

/// Look up a marker token ('o', 's', '^', …). Throws graphlib::ValueError for
/// unknown tokens; a token matplotlib knows but v0.1 doesn't ship yet gets an
/// error naming the milestone (mpl-parity rule).
const Marker& get_marker(std::string_view token);

/// True if `c` is any matplotlib marker character (".,ov^<>12348spP*hH+xXDd|_") —
/// used by the plot() format-string parser, including not-yet-shipped ones.
bool is_marker_char(char c);

} // namespace graphlib
