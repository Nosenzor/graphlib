// Deterministic PDF writer (PARITY D20). Structure: Catalog / Pages / Page /
// Flate content stream, then per-face Type0 + CIDFontType2 + FontDescriptor +
// FontFile2 + ToUnicode, image XObjects (+ SMask), ExtGStates. Identity-H:
// the text strings are glyph ids; /W carries the real advances so extraction
// and reflow space correctly; TJ offsets reapply our kerning so PDF text
// lands exactly where the other backends put it.
#include "graphlib/backend/pdf.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <numbers>

#include <stb_truetype.h>

// stb_image_write's zlib compressor (defined in tests-independent stb_impl.cpp);
// its prototype only exists in the implementation section of the header.
extern "C" unsigned char* stbi_zlib_compress(unsigned char* data, int data_len,
                                             int* out_len, int quality);

#include "../../core/fmt.hpp"
#include "graphlib/errors.hpp"
#include "text/font_manager.hpp"
#include "text/mathtext.hpp"
#include "core/path_simplify.hpp"
#include "graphlib/rc.hpp"

// Embedded by cmake/EmbedResource.cmake from third_party/fonts/.
extern const unsigned char graphlib_font_dejavu_sans[];
extern const std::size_t graphlib_font_dejavu_sans_size;
extern const unsigned char graphlib_font_dejavu_sans_bold[];
extern const std::size_t graphlib_font_dejavu_sans_bold_size;
extern const unsigned char graphlib_font_dejavu_sans_oblique[];
extern const std::size_t graphlib_font_dejavu_sans_oblique_size;

namespace graphlib {

namespace {

using detail::fmt_trim;

std::string num(double v, int decimals = 2) { return fmt_trim(v, decimals); }

std::string flate(const unsigned char* data, size_t len) {
    int out_len = 0;
    unsigned char* z = stbi_zlib_compress(const_cast<unsigned char*>(data),
                                          static_cast<int>(len), &out_len, 8);
    if (z == nullptr) {
        throw Error("pdf: zlib compression failed");
    }
    std::string out(reinterpret_cast<char*>(z), static_cast<size_t>(out_len));
    std::free(z); // stbi's default allocator
    return out;
}

std::string flate(const std::string& s) {
    return flate(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

std::string stream_obj(const std::string& dict_extra, const std::string& raw) {
    const std::string z = flate(raw);
    return "<< /Filter /FlateDecode /Length " + std::to_string(z.size()) + dict_extra +
           " >>\nstream\n" + z + "\nendstream";
}

std::string hex4(unsigned v) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string s(4, '0');
    s[0] = digits[(v >> 12) & 0xF];
    s[1] = digits[(v >> 8) & 0xF];
    s[2] = digits[(v >> 4) & 0xF];
    s[3] = digits[v & 0xF];
    return s;
}

} // namespace

struct PdfRenderer::FontSlot {
    bool bold = false;
    bool italic = false;
    std::string res_name; // /F1, /F2, ...
    const unsigned char* data = nullptr;
    size_t size = 0;
    stbtt_fontinfo info{};
    int upem = 2048;
    std::map<int, char32_t> used_gids; // gid -> codepoint (ToUnicode, /W)

    [[nodiscard]] int to_thousandths(int font_units) const {
        return static_cast<int>(std::lround(font_units * 1000.0 / upem));
    }
};

struct PdfRenderer::PdfImage {
    int width = 0;
    int height = 0;
    bool interpolate = false;
    std::string rgb_flate;
    std::string alpha_flate; // empty when fully opaque
};

PdfRenderer::PdfRenderer(double width_pt, double height_pt)
    : Renderer(/*dpi=*/72.0), width_(width_pt), height_(height_pt) {}

PdfRenderer::~PdfRenderer() = default;

PdfRenderer::FontSlot& PdfRenderer::slot_for(bool bold, bool italic) {
    for (const auto& f : fonts_) {
        if (f->bold == bold && f->italic == italic) {
            return *f;
        }
    }
    auto slot = std::make_unique<FontSlot>();
    slot->bold = bold;
    slot->italic = italic;
    slot->res_name = "F" + std::to_string(fonts_.size() + 1);
    if (bold) {
        slot->data = graphlib_font_dejavu_sans_bold;
        slot->size = graphlib_font_dejavu_sans_bold_size;
    } else if (italic) {
        slot->data = graphlib_font_dejavu_sans_oblique;
        slot->size = graphlib_font_dejavu_sans_oblique_size;
    } else {
        slot->data = graphlib_font_dejavu_sans;
        slot->size = graphlib_font_dejavu_sans_size;
    }
    const int offset = stbtt_GetFontOffsetForIndex(slot->data, 0);
    if (stbtt_InitFont(&slot->info, slot->data, offset) == 0) {
        throw Error("pdf: cannot parse embedded font");
    }
    fonts_.push_back(std::move(slot));
    return *fonts_.back();
}

std::string PdfRenderer::gstate_name(double stroke_alpha, double fill_alpha) {
    const std::string key = num(stroke_alpha, 3) + "," + num(fill_alpha, 3);
    auto it = gstates_.find(key);
    if (it == gstates_.end()) {
        it = gstates_.emplace(key, "Gs" + std::to_string(++next_gs_)).first;
    }
    return it->second;
}

std::string PdfRenderer::set_color_ops(const GraphicsContext& gc,
                                       const std::optional<Color>& face) {
    std::string ops;
    const double sa = gc.color.a;
    const double fa = face ? face->a : 1.0;
    if (sa < 1.0 || fa < 1.0) {
        ops += "/" + gstate_name(sa, fa) + " gs\n";
    }
    if (face && face->a > 0.0) {
        ops += num(face->r, 3) + " " + num(face->g, 3) + " " + num(face->b, 3) + " rg\n";
    }
    if (gc.color.a > 0.0 && gc.linewidth > 0.0) {
        ops += num(gc.color.r, 3) + " " + num(gc.color.g, 3) + " " + num(gc.color.b, 3) +
               " RG\n" + num(points_to_pixels(gc.linewidth), 3) + " w\n";
        if (!gc.dashes.is_solid()) {
            ops += "[";
            for (size_t i = 0; i < gc.dashes.on_off.size(); ++i) {
                if (i > 0) {
                    ops += " ";
                }
                ops += num(points_to_pixels(gc.dashes.on_off[i]), 3);
            }
            ops += "] " + num(points_to_pixels(gc.dashes.offset), 3) + " d\n";
        }
        if (gc.capstyle == CapStyle::round) {
            ops += "1 J\n";
        } else if (gc.capstyle == CapStyle::projecting) {
            ops += "2 J\n";
        }
        if (gc.joinstyle == JoinStyle::round) {
            ops += "1 j\n";
        } else if (gc.joinstyle == JoinStyle::bevel) {
            ops += "2 j\n";
        }
    }
    return ops;
}

void PdfRenderer::draw_path(const GraphicsContext& gc, const Path& path,
                            const Affine2D& transform, const std::optional<Color>& face) {
    if (path.empty()) {
        return;
    }
    const bool fill = face && face->a > 0.0;
    const bool stroke = gc.color.a > 0.0 && gc.linewidth > 0.0;
    if (!fill && !stroke) {
        return;
    }
    content_ += "q\n";
    if (gc.clip_rect) {
        const Bbox& r = *gc.clip_rect;
        content_ += num(r.x0()) + " " + num(r.y0()) + " " + num(r.width()) + " " +
                    num(r.height()) + " re W n\n";
    }
    content_ += set_color_ops(gc, face);

    // mpl backends simplify unfilled stroke paths in display space
    // (rc path.simplify / path.simplify_threshold; Path.should_simplify gate).
    Path simplified_storage;
    const Path* src = &path;
    Affine2D tf = transform;
    if ((!face || face->a <= 0.0) && detail::should_simplify(path)) {
        simplified_storage = detail::simplify_path(path.transformed(transform),
                                                   rc().number("path.simplify_threshold"));
        src = &simplified_storage;
        tf = Affine2D::identity();
    }

    const auto verts = src->vertices();
    Point cur{0, 0};
    size_t i = 0;
    while (i < verts.size()) {
        switch (src->code_at(i)) {
        case PathCode::moveto: {
            cur = tf.apply(verts[i]);
            content_ += num(cur.x) + " " + num(cur.y) + " m\n";
            i += 1;
            break;
        }
        case PathCode::lineto: {
            cur = tf.apply(verts[i]);
            content_ += num(cur.x) + " " + num(cur.y) + " l\n";
            i += 1;
            break;
        }
        case PathCode::curve3: { // PDF has no quadratics: elevate to cubic
            const Point q = tf.apply(verts[i]);
            const Point p1 = tf.apply(verts[i + 1]);
            const Point c1{cur.x + 2.0 / 3.0 * (q.x - cur.x), cur.y + 2.0 / 3.0 * (q.y - cur.y)};
            const Point c2{p1.x + 2.0 / 3.0 * (q.x - p1.x), p1.y + 2.0 / 3.0 * (q.y - p1.y)};
            content_ += num(c1.x) + " " + num(c1.y) + " " + num(c2.x) + " " + num(c2.y) + " " +
                        num(p1.x) + " " + num(p1.y) + " c\n";
            cur = p1;
            i += 2;
            break;
        }
        case PathCode::curve4: {
            const Point c1 = tf.apply(verts[i]);
            const Point c2 = tf.apply(verts[i + 1]);
            const Point p1 = tf.apply(verts[i + 2]);
            content_ += num(c1.x) + " " + num(c1.y) + " " + num(c2.x) + " " + num(c2.y) + " " +
                        num(p1.x) + " " + num(p1.y) + " c\n";
            cur = p1;
            i += 3;
            break;
        }
        case PathCode::closepoly:
            content_ += "h\n";
            i += 1;
            break;
        case PathCode::stop:
            i = verts.size();
            break;
        }
    }
    if (fill && stroke) {
        content_ += "B\n";
    } else if (fill) {
        content_ += "f\n";
    } else {
        content_ += "S\n";
    }
    content_ += "Q\n";
}

void PdfRenderer::draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                            const FontProperties& font, double angle_deg, HAlign ha,
                            VAlign va) {
    if (text.empty()) {
        return;
    }
    if (detail::contains_math(text)) { // formulas stay outlines (D20)
        Renderer::draw_text(gc, pos, text, font, angle_deg, ha, va);
        return;
    }
    const auto& fm = detail::FontManager::instance();
    const double em = points_to_pixels(font.size_pt);
    const detail::TextExtent ext = fm.text_extent(text, em, font.bold);
    const double asc = fm.ascent(em, font.bold);
    const double desc = fm.descent(em, font.bold);

    // Anchor offsets in the unrotated frame — same math as Renderer::draw_text.
    double dx = 0.0;
    if (ha == HAlign::center) {
        dx = -ext.width / 2.0;
    } else if (ha == HAlign::right) {
        dx = -ext.width;
    }
    double dy = 0.0;
    switch (va) {
    case VAlign::top:
        dy = -asc;
        break;
    case VAlign::center:
        dy = -(asc - desc) / 2.0;
        break;
    case VAlign::bottom:
        dy = ext.height - asc;
        break;
    case VAlign::baseline:
        dy = 0.0;
        break;
    }

    FontSlot& slot = slot_for(font.bold, font.italic);
    content_ += "q\n";
    if (gc.clip_rect) {
        const Bbox& r = *gc.clip_rect;
        content_ += num(r.x0()) + " " + num(r.y0()) + " " + num(r.width()) + " " +
                    num(r.height()) + " re W n\n";
    }
    if (gc.color.a < 1.0) {
        content_ += "/" + gstate_name(gc.color.a, gc.color.a) + " gs\n";
    }
    content_ += num(gc.color.r, 3) + " " + num(gc.color.g, 3) + " " + num(gc.color.b, 3) +
                " rg\nBT\n/" + slot.res_name + " " + num(em, 3) + " Tf\n";

    const double t = angle_deg * std::numbers::pi / 180.0;
    const double c = std::cos(t);
    const double s = std::sin(t);

    size_t line_start = 0;
    int line_no = 0;
    for (size_t p = 0; p <= text.size(); ++p) {
        if (p != text.size() && text[p] != '\n') {
            continue;
        }
        const std::string_view line = text.substr(line_start, p - line_start);
        // Line origin in the rotated frame, then to page space about the anchor.
        const double lx = dx;
        const double ly = dy - line_no * 1.2 * em; // mpl line spacing
        const double px = pos.x + c * lx - s * ly;
        const double py = pos.y + s * lx + c * ly;
        content_ += num(c, 4) + " " + num(s, 4) + " " + num(-s, 4) + " " + num(c, 4) + " " +
                    num(px) + " " + num(py) + " Tm\n[<";
        const auto cps = detail::decode_utf8(line);
        std::string tj;
        for (size_t k = 0; k < cps.size(); ++k) {
            const int gid = stbtt_FindGlyphIndex(&slot.info, static_cast<int>(cps[k]));
            slot.used_gids.emplace(gid, cps[k]);
            tj += hex4(static_cast<unsigned>(gid));
            if (k + 1 < cps.size()) { // reapply our kerning via TJ offsets
                const int kern_units = stbtt_GetCodepointKernAdvance(
                    &slot.info, static_cast<int>(cps[k]), static_cast<int>(cps[k + 1]));
                if (kern_units != 0) {
                    tj += "> " + std::to_string(-slot.to_thousandths(kern_units)) + " <";
                }
            }
        }
        content_ += tj + ">] TJ\n";
        if (p != text.size()) {
            ++line_no;
            line_start = p + 1;
        }
    }
    content_ += "ET\nQ\n";
}

void PdfRenderer::draw_image(const GraphicsContext& gc, const Bbox& dest,
                             const ImageBuffer& image, Interp interpolation) {
    if (image.width <= 0 || image.height <= 0) {
        return;
    }
    PdfImage img;
    img.width = image.width;
    img.height = image.height;
    img.interpolate = interpolation == Interp::bilinear;
    const size_t n = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    std::vector<unsigned char> rgb(n * 3);
    std::vector<unsigned char> alpha(n);
    bool any_alpha = false;
    for (size_t i = 0; i < n; ++i) {
        rgb[i * 3 + 0] = image.rgba[i * 4 + 0];
        rgb[i * 3 + 1] = image.rgba[i * 4 + 1];
        rgb[i * 3 + 2] = image.rgba[i * 4 + 2];
        alpha[i] = image.rgba[i * 4 + 3];
        any_alpha |= alpha[i] != 255;
    }
    img.rgb_flate = flate(rgb.data(), rgb.size());
    if (any_alpha) {
        img.alpha_flate = flate(alpha.data(), alpha.size());
    }
    images_.push_back(std::move(img));
    const std::string name = "Im" + std::to_string(images_.size());

    content_ += "q\n";
    if (gc.clip_rect) {
        const Bbox& r = *gc.clip_rect;
        content_ += num(r.x0()) + " " + num(r.y0()) + " " + num(r.width()) + " " +
                    num(r.height()) + " re W n\n";
    }
    content_ += num(dest.width()) + " 0 0 " + num(dest.height()) + " " + num(dest.x0()) + " " +
                num(dest.y0()) + " cm\n/" + name + " Do\nQ\n";
}

std::string PdfRenderer::finalize() const {
    // Fixed object numbering: 1 Catalog, 2 Pages, 3 Page, 4 Contents, then
    // 5 per font (Type0, CIDFont, Descriptor, FontFile2, ToUnicode), then
    // per image (XObject [+ SMask]), then one per ExtGState.
    std::vector<std::string> objs; // objs[i] = body of object i+1
    objs.resize(4);

    std::string font_res;
    int next = 5;
    for (const auto& f : fonts_) {
        const int type0 = next;
        const int cidfont = next + 1;
        const int descriptor = next + 2;
        const int fontfile = next + 3;
        const int tounicode = next + 4;
        next += 5;
        font_res += " /" + f->res_name + " " + std::to_string(type0) + " 0 R";

        const std::string base = std::string("DejaVuSans") +
                                 (f->bold ? "-Bold" : f->italic ? "-Oblique" : "");
        objs.push_back("<< /Type /Font /Subtype /Type0 /BaseFont /" + base +
                       " /Encoding /Identity-H /DescendantFonts [" + std::to_string(cidfont) +
                       " 0 R] /ToUnicode " + std::to_string(tounicode) + " 0 R >>");

        std::string w = "[";
        for (const auto& [gid, cp] : f->used_gids) {
            int adv = 0;
            int lsb = 0;
            stbtt_GetGlyphHMetrics(&f->info, gid, &adv, &lsb);
            w += " " + std::to_string(gid) + " [" + std::to_string(f->to_thousandths(adv)) +
                 "]";
        }
        w += " ]";
        objs.push_back("<< /Type /Font /Subtype /CIDFontType2 /BaseFont /" + base +
                       " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) "
                       "/Supplement 0 >> /FontDescriptor " +
                       std::to_string(descriptor) + " 0 R /CIDToGIDMap /Identity /W " + w +
                       " >>");

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetFontBoundingBox(&f->info, &x0, &y0, &x1, &y1);
        int ascent = 0;
        int descent = 0;
        int gap = 0;
        stbtt_GetFontVMetrics(&f->info, &ascent, &descent, &gap);
        objs.push_back(
            "<< /Type /FontDescriptor /FontName /" + base + " /Flags " +
            (f->italic ? "96" : "32") + " /FontBBox [" + std::to_string(f->to_thousandths(x0)) +
            " " + std::to_string(f->to_thousandths(y0)) + " " +
            std::to_string(f->to_thousandths(x1)) + " " + std::to_string(f->to_thousandths(y1)) +
            "] /ItalicAngle " + (f->italic ? "-11" : "0") + " /Ascent " +
            std::to_string(f->to_thousandths(ascent)) + " /Descent " +
            std::to_string(f->to_thousandths(descent)) + " /CapHeight " +
            std::to_string(f->to_thousandths(ascent)) + " /StemV 80 /FontFile2 " +
            std::to_string(fontfile) + " 0 R >>");

        objs.push_back(stream_obj(" /Length1 " + std::to_string(f->size),
                                  std::string(reinterpret_cast<const char*>(f->data), f->size)));

        // ToUnicode CMap: CID (== GID) -> UTF-16BE codepoint, for extraction.
        std::string cmap =
            "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
            "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
            "/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n"
            "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n" +
            std::to_string(f->used_gids.size()) + " beginbfchar\n";
        for (const auto& [gid, cp] : f->used_gids) {
            cmap += "<" + hex4(static_cast<unsigned>(gid)) + "> <" +
                    hex4(static_cast<unsigned>(cp)) + ">\n"; // BMP only (DejaVu)
        }
        cmap += "endbfchar\nendcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n";
        objs.push_back(stream_obj("", cmap));
    }

    std::string xobj_res;
    for (size_t i = 0; i < images_.size(); ++i) {
        const PdfImage& im = images_[i];
        const int img_obj = next++;
        const bool has_smask = !im.alpha_flate.empty();
        const int smask_obj = has_smask ? next++ : 0;
        xobj_res += " /Im" + std::to_string(i + 1) + " " + std::to_string(img_obj) + " 0 R";
        std::string dict = "<< /Type /XObject /Subtype /Image /Width " +
                           std::to_string(im.width) + " /Height " + std::to_string(im.height) +
                           " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Interpolate " +
                           (im.interpolate ? "true" : "false") +
                           " /Filter /FlateDecode /Length " +
                           std::to_string(im.rgb_flate.size());
        if (has_smask) {
            dict += " /SMask " + std::to_string(smask_obj) + " 0 R";
        }
        dict += " >>\nstream\n" + im.rgb_flate + "\nendstream";
        objs.push_back(dict);
        if (has_smask) {
            objs.push_back("<< /Type /XObject /Subtype /Image /Width " +
                           std::to_string(im.width) + " /Height " + std::to_string(im.height) +
                           " /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /FlateDecode"
                           " /Length " +
                           std::to_string(im.alpha_flate.size()) + " >>\nstream\n" +
                           im.alpha_flate + "\nendstream");
        }
    }

    std::string gs_res;
    for (const auto& [key, name] : gstates_) {
        const size_t comma = key.find(',');
        gs_res += " /" + name + " " + std::to_string(next++) + " 0 R";
        objs.push_back("<< /Type /ExtGState /CA " + key.substr(0, comma) + " /ca " +
                       key.substr(comma + 1) + " >>");
    }

    objs[0] = "<< /Type /Catalog /Pages 2 0 R >>";
    objs[1] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
    std::string resources = "<< /ProcSet [/PDF /Text /ImageC]";
    if (!font_res.empty()) {
        resources += " /Font <<" + font_res + " >>";
    }
    if (!xobj_res.empty()) {
        resources += " /XObject <<" + xobj_res + " >>";
    }
    if (!gs_res.empty()) {
        resources += " /ExtGState <<" + gs_res + " >>";
    }
    resources += " >>";
    objs[2] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + num(width_) + " " + num(height_) +
              "] /Contents 4 0 R /Resources " + resources + " >>";
    objs[3] = stream_obj("", content_);

    // Assemble with exact byte offsets (no timestamps, no /ID: deterministic).
    std::string out = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<size_t> offsets(objs.size() + 1, 0);
    for (size_t i = 0; i < objs.size(); ++i) {
        offsets[i + 1] = out.size();
        out += std::to_string(i + 1) + " 0 obj\n" + objs[i] + "\nendobj\n";
    }
    const size_t xref_pos = out.size();
    out += "xref\n0 " + std::to_string(objs.size() + 1) + "\n0000000000 65535 f \n";
    for (size_t i = 1; i <= objs.size(); ++i) {
        std::string o = std::to_string(offsets[i]);
        out += std::string(10 - o.size(), '0') + o + " 00000 n \n";
    }
    out += "trailer\n<< /Size " + std::to_string(objs.size() + 1) +
           " /Root 1 0 R >>\nstartxref\n" +
           std::to_string(xref_pos) + "\n%%EOF\n";
    return out;
}

} // namespace graphlib
