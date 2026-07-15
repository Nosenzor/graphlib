#pragma once
// Internal: the FancyArrowPatch geometry pipeline used by Annotation
// (mpl ConnectionStyle.__call__ + ArrowStyle._Curve.transmute, specialized to
// the straight arc3,rad=0 connector). White-box tested against oracle
// fixtures; all coordinates are display pixels, y-up.
#include <optional>
#include <string_view>
#include <vector>

#include "graphlib/transforms.hpp"

namespace graphlib::detail {

struct ArrowStyleSpec {
    bool head = false;   // "->" or "-|>"
    bool filled = false; // "-|>"
};

/// Parse the supported arrowstyle subset ("-", "->", "-|>").
/// Throws graphlib::ValueError on other (mpl-valid or not) styles.
ArrowStyleSpec parse_arrowstyle(std::string_view style);

struct ArrowGeometry {
    Point tail_begin;         // after patchA clip + shrinkA
    Point tail_end;           // posB after shrinkB (and head overshoot pad)
    std::vector<Point> head;  // empty, or 3 barb points (open) / triangle (filled)
    bool head_filled = false;
};

/// posA/posB in display px; patchA is the (already padded) text box the tail
/// is clipped out of; shrink/mutation/linewidth in px (points x dpi/72).
ArrowGeometry build_arrow(Point posA, Point posB, const std::optional<Bbox>& patchA,
                          double shrinkA_px, double shrinkB_px, const ArrowStyleSpec& style,
                          double mutation_px, double linewidth_px);

} // namespace graphlib::detail
