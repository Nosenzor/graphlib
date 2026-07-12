#include "graphlib/backend/svg.hpp"

#include <cmath>

#include "../../core/fmt.hpp"

namespace graphlib {

namespace {

using detail::fmt_trim;

std::string xml_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string opacity_attr(const char* name, double a) {
    if (a >= 1.0) {
        return {};
    }
    return std::string(" ") + name + "=\"" + fmt_trim(a, 3) + "\"";
}

} // namespace

// SVG is rendered at 72 dpi (canvas in pt), like matplotlib's SVG backend.
SvgRenderer::SvgRenderer(double width_px, double height_px)
    : Renderer(/*dpi=*/72.0), width_(width_px), height_(height_px) {}

void SvgRenderer::open_group(std::string_view tag) {
    const std::string name(tag);
    const int n = ++group_counters_[name];
    body_ += "<g id=\"" + name + "_" + std::to_string(n) + "\">\n";
}

void SvgRenderer::close_group() { body_ += "</g>\n"; }

std::string SvgRenderer::path_data(const Path& path, const Affine2D& transform) {
    std::string d;
    auto emit = [&](Point v) {
        const Point p = flip(transform.apply(v));
        d += fmt_trim(p.x, 2) + " " + fmt_trim(p.y, 2);
    };
    const auto verts = path.vertices();
    size_t i = 0;
    while (i < verts.size()) {
        switch (path.code_at(i)) {
        case PathCode::moveto:
            d += "M ";
            emit(verts[i]);
            i += 1;
            break;
        case PathCode::lineto:
            d += "L ";
            emit(verts[i]);
            i += 1;
            break;
        case PathCode::curve3: // control + endpoint
            d += "Q ";
            emit(verts[i]);
            d += " ";
            emit(verts[i + 1]);
            i += 2;
            break;
        case PathCode::curve4: // two controls + endpoint
            d += "C ";
            emit(verts[i]);
            d += " ";
            emit(verts[i + 1]);
            d += " ";
            emit(verts[i + 2]);
            i += 3;
            break;
        case PathCode::closepoly:
            d += "z";
            i += 1;
            break;
        case PathCode::stop:
            i = verts.size();
            break;
        }
        if (i < verts.size()) {
            d += " ";
        }
    }
    return d;
}

std::string SvgRenderer::clip_attr(const GraphicsContext& gc) {
    if (!gc.clip_rect) {
        return {};
    }
    const Bbox& r = *gc.clip_rect;
    // y-down rect: top edge is the flipped y1
    const std::string key = fmt_trim(r.x0(), 2) + "," + fmt_trim(height_ - r.y1(), 2) + "," +
                            fmt_trim(r.width(), 2) + "," + fmt_trim(r.height(), 2);
    auto it = clip_ids_.find(key);
    if (it == clip_ids_.end()) {
        const std::string id = "p" + std::to_string(++clip_counter_);
        defs_ += "<clipPath id=\"" + id + "\"><rect x=\"" + fmt_trim(r.x0(), 2) + "\" y=\"" +
                 fmt_trim(height_ - r.y1(), 2) + "\" width=\"" + fmt_trim(r.width(), 2) +
                 "\" height=\"" + fmt_trim(r.height(), 2) + "\"/></clipPath>\n";
        it = clip_ids_.emplace(key, id).first;
    }
    return " clip-path=\"url(#" + it->second + ")\"";
}

std::string SvgRenderer::fill_attrs(const std::optional<Color>& face) {
    if (!face || face->a <= 0.0) {
        return " fill=\"none\"";
    }
    return " fill=\"" + to_hex(*face) + "\"" + opacity_attr("fill-opacity", face->a);
}

std::string SvgRenderer::stroke_attrs(const GraphicsContext& gc) const {
    if (gc.color.a <= 0.0 || gc.linewidth <= 0.0) {
        return {};
    }
    std::string s = " stroke=\"" + to_hex(gc.color) + "\"" +
                    opacity_attr("stroke-opacity", gc.color.a) + " stroke-width=\"" +
                    fmt_trim(points_to_pixels(gc.linewidth), 3) + "\"";
    if (!gc.dashes.is_solid()) {
        s += " stroke-dasharray=\"";
        for (size_t i = 0; i < gc.dashes.on_off.size(); ++i) {
            if (i > 0) {
                s += ",";
            }
            s += fmt_trim(points_to_pixels(gc.dashes.on_off[i]), 3);
        }
        s += "\"";
        if (gc.dashes.offset != 0.0) {
            s += " stroke-dashoffset=\"" + fmt_trim(points_to_pixels(gc.dashes.offset), 3) + "\"";
        }
    }
    if (gc.capstyle == CapStyle::round) {
        s += " stroke-linecap=\"round\"";
    } else if (gc.capstyle == CapStyle::projecting) {
        s += " stroke-linecap=\"square\"";
    } // butt is the SVG default
    if (gc.joinstyle == JoinStyle::round) {
        s += " stroke-linejoin=\"round\"";
    } else if (gc.joinstyle == JoinStyle::bevel) {
        s += " stroke-linejoin=\"bevel\"";
    } // miter is the SVG default
    return s;
}

void SvgRenderer::draw_path(const GraphicsContext& gc, const Path& path,
                            const Affine2D& transform, const std::optional<Color>& face) {
    if (path.empty()) {
        return;
    }
    body_ += "<path" + clip_attr(gc) + " d=\"" + path_data(path, transform) + "\"" +
             fill_attrs(face) + stroke_attrs(gc) + "/>\n";
}

void SvgRenderer::draw_markers(const GraphicsContext& gc, const Path& marker,
                               const Affine2D& marker_transform, const Path& positions,
                               const Affine2D& transform, const std::optional<Color>& face) {
    if (marker.empty() || positions.empty()) {
        return;
    }
    // The <use> def needs relative marker geometry with y negated (y-down doc).
    // path_data applies the canvas flip (y -> height - y); pre-translating by
    // +height cancels it: height - (s*my + height) = -s*my.
    const Affine2D def_tf = marker_transform.then(Affine2D::translation(0, height_));
    const std::string d = path_data(marker, def_tf);
    auto it = marker_ids_.find(d);
    if (it == marker_ids_.end()) {
        const std::string id = "m" + std::to_string(++marker_counter_);
        defs_ += "<path id=\"" + id + "\" d=\"" + d + "\"/>\n";
        it = marker_ids_.emplace(d, id).first;
    }
    body_ += "<g" + clip_attr(gc) + fill_attrs(face) + stroke_attrs(gc) + ">\n";
    for (const Point& v : positions.vertices()) {
        const Point p = flip(transform.apply(v));
        body_ += "<use href=\"#" + it->second + "\" x=\"" + fmt_trim(p.x, 2) + "\" y=\"" +
                 fmt_trim(p.y, 2) + "\"/>\n";
    }
    body_ += "</g>\n";
}

void SvgRenderer::draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                            const FontProperties& font, double angle_deg, HAlign ha, VAlign va) {
    if (text.empty()) {
        return;
    }
    const Point p = flip(pos);
    std::string attrs = " x=\"" + fmt_trim(p.x, 2) + "\" y=\"" + fmt_trim(p.y, 2) + "\"";
    // Baseline shims until FontManager metrics land (TODO(v0.2)).
    if (va == VAlign::top) {
        attrs += " dy=\"0.8em\"";
    } else if (va == VAlign::center) {
        attrs += " dy=\"0.35em\"";
    } else if (va == VAlign::bottom) {
        attrs += " dy=\"-0.2em\"";
    }
    attrs += " font-family=\"DejaVu Sans, sans-serif\" font-size=\"" +
             fmt_trim(points_to_pixels(font.size_pt), 2) + "\"";
    if (font.bold) {
        attrs += " font-weight=\"bold\"";
    }
    attrs += " fill=\"" + to_hex(gc.color) + "\"" + opacity_attr("fill-opacity", gc.color.a);
    if (ha == HAlign::center) {
        attrs += " text-anchor=\"middle\"";
    } else if (ha == HAlign::right) {
        attrs += " text-anchor=\"end\"";
    }
    if (angle_deg != 0.0) {
        // CCW in y-up display coords == negative rotation in the y-down document.
        attrs += " transform=\"rotate(" + fmt_trim(-angle_deg, 2) + " " + fmt_trim(p.x, 2) + " " +
                 fmt_trim(p.y, 2) + ")\"";
    }
    body_ += "<text" + attrs + ">" + xml_escape(text) + "</text>\n";
}

std::string SvgRenderer::finalize() const {
    std::string out;
    out += "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n";
    out += "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
           "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + fmt_trim(width_, 2) +
           "pt\" height=\"" + fmt_trim(height_, 2) + "pt\" viewBox=\"0 0 " + fmt_trim(width_, 2) +
           " " + fmt_trim(height_, 2) + "\" version=\"1.1\">\n";
    if (!defs_.empty()) {
        out += "<defs>\n" + defs_ + "</defs>\n";
    }
    out += body_;
    out += "</svg>\n";
    return out;
}

} // namespace graphlib
