#pragma once
// Internal text stack (docs/DESIGN.md §10): stb_truetype over the embedded
// DejaVu Sans/Bold. All layout metrics come from here — never from a backend —
// so text measures identically on every platform and output format.
// Header is internal (not installed); the public surface goes through Renderer.
#include <string_view>
#include <vector>

#include "graphlib/path.hpp"

namespace graphlib::detail {

struct TextExtent {
    double width = 0.0;   // px
    double height = 0.0;  // px — ascent + descent (font metrics, like mpl)
    double descent = 0.0; // px below the baseline
};

class FontManager {
public:
    static const FontManager& instance();

    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    /// Extent of (possibly multi-line) text at an em size in pixels
    /// (em_px = points_to_pixels(size_pt); line spacing 1.2 em, like mpl).
    [[nodiscard]] TextExtent text_extent(std::string_view utf8, double em_px,
                                         bool bold = false) const;

    /// The text as filled outlines: one Path in pixels, y-up, origin at the
    /// first line's baseline-left pen position. Quadratic TrueType contours
    /// become CURVE3 segments.
    [[nodiscard]] Path text_path(std::string_view utf8, double em_px, bool bold = false) const;

    [[nodiscard]] double ascent(double em_px, bool bold = false) const;
    [[nodiscard]] double descent(double em_px, bool bold = false) const;

private:
    FontManager();
    struct Face;
    [[nodiscard]] const Face& face(bool bold) const;

    std::vector<Face> faces_;
};

/// Lenient UTF-8 decode (invalid bytes become U+FFFD) — tick labels carry U+2212.
std::vector<char32_t> decode_utf8(std::string_view s);

} // namespace graphlib::detail
