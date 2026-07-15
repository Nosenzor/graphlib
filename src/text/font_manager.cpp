#include "text/font_manager.hpp"

#include <algorithm>
#include <cstddef>

#include <stb_truetype.h>

#include "graphlib/errors.hpp"
#include "text/mathtext.hpp"

// Embedded by cmake/EmbedResource.cmake from third_party/fonts/.
extern const unsigned char graphlib_font_dejavu_sans[];
extern const std::size_t graphlib_font_dejavu_sans_size;
extern const unsigned char graphlib_font_dejavu_sans_bold[];
extern const std::size_t graphlib_font_dejavu_sans_bold_size;
extern const unsigned char graphlib_font_dejavu_sans_oblique[];
extern const std::size_t graphlib_font_dejavu_sans_oblique_size;

namespace graphlib::detail {

namespace {
constexpr double kLineSpacing = 1.2; // mpl multi-line spacing, x font size
constexpr char32_t kReplacement = 0xFFFD;
} // namespace

std::vector<char32_t> decode_utf8(std::string_view s) {
    std::vector<char32_t> out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        char32_t cp = kReplacement;
        size_t len = 1;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 >> 5) == 0x6 && i + 1 < s.size()) {
            cp = static_cast<char32_t>((b0 & 0x1F) << 6 |
                                       (static_cast<unsigned char>(s[i + 1]) & 0x3F));
            len = 2;
        } else if ((b0 >> 4) == 0xE && i + 2 < s.size()) {
            cp = static_cast<char32_t>((b0 & 0x0F) << 12 |
                                       (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6 |
                                       (static_cast<unsigned char>(s[i + 2]) & 0x3F));
            len = 3;
        } else if ((b0 >> 3) == 0x1E && i + 3 < s.size()) {
            cp = static_cast<char32_t>((b0 & 0x07) << 18 |
                                       (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12 |
                                       (static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6 |
                                       (static_cast<unsigned char>(s[i + 3]) & 0x3F));
            len = 4;
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

struct FontManager::Face {
    stbtt_fontinfo info{};
    int ascent_units = 0;
    int descent_units = 0; // negative in font units, stored as-is
    int line_gap_units = 0;

    [[nodiscard]] double scale(double em_px) const {
        return stbtt_ScaleForMappingEmToPixels(&info, static_cast<float>(em_px));
    }
};

FontManager::FontManager() {
    auto load = [this](const unsigned char* data) {
        Face f;
        const int offset = stbtt_GetFontOffsetForIndex(data, 0);
        if (stbtt_InitFont(&f.info, data, offset) == 0) {
            throw Error("FontManager: failed to parse embedded DejaVu font");
        }
        stbtt_GetFontVMetrics(&f.info, &f.ascent_units, &f.descent_units, &f.line_gap_units);
        faces_.push_back(f);
    };
    load(graphlib_font_dejavu_sans);
    load(graphlib_font_dejavu_sans_bold);
    load(graphlib_font_dejavu_sans_oblique); // mathtext italics
}

const FontManager& FontManager::instance() {
    static const FontManager fm;
    return fm;
}

const FontManager::Face& FontManager::face(bool bold) const { return faces_[bold ? 1 : 0]; }

const FontManager::Face& FontManager::face_by_id(FaceId id) const {
    return faces_[static_cast<size_t>(id)];
}

GlyphMetrics FontManager::glyph_metrics(char32_t cp, double em_px, FaceId fid) const {
    const Face& f = face_by_id(fid);
    const double sc = f.scale(em_px);
    GlyphMetrics m;
    m.valid = stbtt_FindGlyphIndex(&f.info, static_cast<int>(cp)) != 0;
    int adv = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&f.info, static_cast<int>(cp), &adv, &lsb);
    m.advance = adv * sc;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (stbtt_GetCodepointBox(&f.info, static_cast<int>(cp), &x0, &y0, &x1, &y1) != 0) {
        m.xmin = x0 * sc;
        m.xmax = x1 * sc;
        m.ymin = y0 * sc;
        m.ymax = y1 * sc;
    }
    return m;
}

double FontManager::kern(char32_t a, char32_t b, double em_px, FaceId fid) const {
    const Face& f = face_by_id(fid);
    return stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(a), static_cast<int>(b)) *
           f.scale(em_px);
}

void FontManager::append_glyph(Path& out, char32_t cp, double x, double y, double em_px,
                               FaceId fid) const {
    const Face& f = face_by_id(fid);
    const double sc = f.scale(em_px);
    stbtt_vertex* verts = nullptr;
    const int n = stbtt_GetCodepointShape(&f.info, static_cast<int>(cp), &verts);
    for (int k = 0; k < n; ++k) {
        const stbtt_vertex& v = verts[k];
        const double vx = x + v.x * sc;
        const double vy = y + v.y * sc; // TrueType is y-up, like display coords
        switch (v.type) {
        case STBTT_vmove:
            out.move_to(vx, vy);
            break;
        case STBTT_vline:
            out.line_to(vx, vy);
            break;
        case STBTT_vcurve:
            out.curve3_to({x + v.cx * sc, y + v.cy * sc}, {vx, vy});
            break;
        case STBTT_vcubic:
            out.curve4_to({x + v.cx * sc, y + v.cy * sc}, {x + v.cx1 * sc, y + v.cy1 * sc},
                          {vx, vy});
            break;
        default:
            break;
        }
    }
    stbtt_FreeShape(&f.info, verts);
    out.close_subpath();
}

double FontManager::ascent(double em_px, bool bold) const {
    const Face& f = face(bold);
    return f.ascent_units * f.scale(em_px);
}

double FontManager::descent(double em_px, bool bold) const {
    const Face& f = face(bold);
    return -f.descent_units * f.scale(em_px); // positive px below baseline
}

TextExtent FontManager::text_extent(std::string_view utf8, double em_px, bool bold) const {
    const Face& f = face(bold);
    const double sc = f.scale(em_px);

    double max_width = 0.0;
    size_t n_lines = 1;
    double extra_top = 0.0;    // math boxes taller than the font box (first line)
    double extra_bottom = 0.0; // deeper than the font descent (last line)
    const double font_asc = f.ascent_units * sc;
    const double font_desc = -f.descent_units * sc;

    size_t line_start = 0;
    for (size_t pos = 0; pos <= utf8.size(); ++pos) {
        if (pos != utf8.size() && utf8[pos] != '\n') {
            continue;
        }
        const std::string_view line = utf8.substr(line_start, pos - line_start);
        const bool first_line = line_start == 0;
        const bool last_line = pos == utf8.size();
        double width = 0.0;
        if (contains_math(line)) {
            for (const TextRun& run : split_math_runs(line)) {
                if (run.math) {
                    const MathBox box = layout_math(run.text, em_px);
                    width += box.width;
                    if (first_line) {
                        extra_top = std::max(extra_top, box.height - font_asc);
                    }
                    if (last_line) {
                        extra_bottom = std::max(extra_bottom, box.depth - font_desc);
                    }
                } else {
                    width += text_extent(run.text, em_px, bold).width;
                }
            }
        } else {
            const auto cps = decode_utf8(line);
            for (size_t i = 0; i < cps.size(); ++i) {
                int adv = 0;
                int lsb = 0;
                stbtt_GetCodepointHMetrics(&f.info, static_cast<int>(cps[i]), &adv, &lsb);
                width += adv * sc;
                if (i + 1 < cps.size()) {
                    width += stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cps[i]),
                                                           static_cast<int>(cps[i + 1])) *
                             sc;
                }
            }
        }
        max_width = std::max(max_width, width);
        if (pos != utf8.size()) {
            ++n_lines;
            line_start = pos + 1;
        }
    }

    TextExtent ext;
    ext.width = max_width;
    ext.descent = font_desc + std::max(0.0, extra_bottom);
    ext.height = font_asc + std::max(0.0, extra_top) + ext.descent +
                 static_cast<double>(n_lines - 1) * kLineSpacing * em_px;
    return ext;
}

Path FontManager::text_path(std::string_view utf8, double em_px, bool bold) const {
    Path out;
    double pen_y = 0.0; // first baseline; subsequent lines go down (y-up: negative)
    size_t line_start = 0;
    for (size_t pos = 0; pos <= utf8.size(); ++pos) {
        if (pos != utf8.size() && utf8[pos] != '\n') {
            continue;
        }
        const std::string_view line = utf8.substr(line_start, pos - line_start);
        double pen_x = 0.0;
        auto emit_plain = [&](std::string_view run) {
            const auto cps = decode_utf8(run);
            const FaceId fid = bold ? FaceId::bold : FaceId::regular;
            for (size_t i = 0; i < cps.size(); ++i) {
                append_glyph(out, cps[i], pen_x, pen_y, em_px, fid);
                pen_x += glyph_metrics(cps[i], em_px, fid).advance;
                if (i + 1 < cps.size()) {
                    pen_x += kern(cps[i], cps[i + 1], em_px, fid);
                }
            }
        };
        if (contains_math(line)) {
            for (const TextRun& run : split_math_runs(line)) {
                if (run.math) {
                    const MathBox box = layout_math(run.text, em_px);
                    out.append(box.path, pen_x, pen_y);
                    pen_x += box.width;
                } else {
                    emit_plain(run.text);
                }
            }
        } else {
            emit_plain(line);
        }
        if (pos != utf8.size()) {
            pen_y -= kLineSpacing * em_px;
            line_start = pos + 1;
        }
    }
    return out;
}

} // namespace graphlib::detail
