#pragma once
// Internal: the mathtext subset (docs/DESIGN.md §10, PARITY D19) — a TeX-lite
// typesetter over the embedded DejaVu faces, porting the layout constants and
// algorithms of matplotlib._mathtext (SHRINK_FACTOR 0.7, FontConstantsBase,
// subsuper/_genfrac/sqrt). Coordinates px, y-up, origin at baseline-left.
#include <string>
#include <string_view>
#include <vector>

#include "graphlib/path.hpp"

namespace graphlib::detail {

/// Ink box of a typeset math run.
struct MathBox {
    Path path;           // glyph outlines + rules
    double width = 0.0;  // px
    double height = 0.0; // px above the baseline
    double depth = 0.0;  // px below the baseline
};

/// True if the string holds at least one unescaped '$' (mathtext candidate).
bool contains_math(std::string_view utf8);

/// One line split into alternating plain/math runs; plain runs have "\$"
/// unescaped, math runs carry the content between the '$'s. Throws
/// graphlib::ValueError on an unmatched '$'.
struct TextRun {
    std::string text;
    bool math = false;
};
std::vector<TextRun> split_math_runs(std::string_view line);

/// Typeset the inner content of a "$...$" run at the given em size.
/// Throws graphlib::ValueError naming any unsupported command.
MathBox layout_math(std::string_view inner, double em_px);

} // namespace graphlib::detail
