#include "text/font_manager.hpp"

#include <cstddef>

#include <stb_truetype.h>

#include "graphlib/errors.hpp"

// Embedded by cmake/EmbedResource.cmake from third_party/fonts/.
extern const unsigned char graphlib_font_dejavu_sans[];
extern const std::size_t graphlib_font_dejavu_sans_size;
extern const unsigned char graphlib_font_dejavu_sans_bold[];
extern const std::size_t graphlib_font_dejavu_sans_bold_size;

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
}

const FontManager& FontManager::instance() {
    static const FontManager fm;
    return fm;
}

const FontManager::Face& FontManager::face(bool bold) const { return faces_[bold ? 1 : 0]; }

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
    double width = 0.0;
    const auto cps = decode_utf8(utf8);
    for (size_t i = 0; i < cps.size(); ++i) {
        if (cps[i] == U'\n') {
            max_width = std::max(max_width, width);
            width = 0.0;
            ++n_lines;
            continue;
        }
        int adv = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&f.info, static_cast<int>(cps[i]), &adv, &lsb);
        width += adv * sc;
        if (i + 1 < cps.size() && cps[i + 1] != U'\n') {
            width += stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cps[i]),
                                                   static_cast<int>(cps[i + 1])) *
                     sc;
        }
    }
    max_width = std::max(max_width, width);

    TextExtent ext;
    ext.width = max_width;
    ext.descent = -f.descent_units * sc;
    ext.height = f.ascent_units * sc + ext.descent +
                 static_cast<double>(n_lines - 1) * kLineSpacing * em_px;
    return ext;
}

Path FontManager::text_path(std::string_view utf8, double em_px, bool bold) const {
    const Face& f = face(bold);
    const double sc = f.scale(em_px);

    Path out;
    double pen_x = 0.0;
    double pen_y = 0.0; // first baseline; subsequent lines go down (y-up: negative)
    const auto cps = decode_utf8(utf8);
    for (size_t i = 0; i < cps.size(); ++i) {
        const char32_t cp = cps[i];
        if (cp == U'\n') {
            pen_x = 0.0;
            pen_y -= kLineSpacing * em_px;
            continue;
        }
        stbtt_vertex* verts = nullptr;
        const int n =
            stbtt_GetCodepointShape(&f.info, static_cast<int>(cp), &verts);
        for (int k = 0; k < n; ++k) {
            const stbtt_vertex& v = verts[k];
            const double x = pen_x + v.x * sc;
            const double y = pen_y + v.y * sc; // TrueType is y-up, like display coords
            switch (v.type) {
            case STBTT_vmove:
                out.move_to(x, y);
                break;
            case STBTT_vline:
                out.line_to(x, y);
                break;
            case STBTT_vcurve:
                out.curve3_to({pen_x + v.cx * sc, pen_y + v.cy * sc}, {x, y});
                break;
            case STBTT_vcubic:
                out.curve4_to({pen_x + v.cx * sc, pen_y + v.cy * sc},
                              {pen_x + v.cx1 * sc, pen_y + v.cy1 * sc}, {x, y});
                break;
            default:
                break;
            }
        }
        stbtt_FreeShape(&f.info, verts);
        out.close_subpath();

        int adv = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&f.info, static_cast<int>(cp), &adv, &lsb);
        pen_x += adv * sc;
        if (i + 1 < cps.size() && cps[i + 1] != U'\n') {
            pen_x += stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cp),
                                                   static_cast<int>(cps[i + 1])) *
                     sc;
        }
    }
    return out;
}

} // namespace graphlib::detail
