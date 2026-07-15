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

enum class FaceId { regular = 0, bold = 1, oblique = 2 }; // oblique: mathtext italics

struct GlyphMetrics {
    double advance = 0.0; // px
    double xmin = 0.0;    // ink box, px, y-up, relative to the pen position
    double xmax = 0.0;
    double ymin = 0.0;
    double ymax = 0.0;
    bool valid = false; // codepoint present in the face
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

    // ---- per-glyph access (mathtext layout) ----
    [[nodiscard]] GlyphMetrics glyph_metrics(char32_t cp, double em_px, FaceId face) const;
    /// Append the glyph outline with its origin at pen (x, y), y-up.
    void append_glyph(Path& out, char32_t cp, double x, double y, double em_px,
                      FaceId face) const;
    /// Kerning adjustment between two codepoints of the same face, px.
    [[nodiscard]] double kern(char32_t a, char32_t b, double em_px, FaceId face) const;
    /// DejaVu Sans x-height (PCLT table value mpl's mathtext layout keys on).
    [[nodiscard]] static double x_height(double em_px) { return 0.5578125 * em_px; }

private:
    FontManager();
    struct Face;
    [[nodiscard]] const Face& face(bool bold) const;
    [[nodiscard]] const Face& face_by_id(FaceId id) const;

    std::vector<Face> faces_;
};

/// Lenient UTF-8 decode (invalid bytes become U+FFFD) — tick labels carry U+2212.
std::vector<char32_t> decode_utf8(std::string_view s);

} // namespace graphlib::detail
