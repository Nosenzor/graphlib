#include "graphlib/backend/agg.hpp"

#include <cmath>
#include <vector>

#include <agg_conv_curve.h>
#include <agg_conv_dash.h>
#include <agg_conv_stroke.h>
#include <agg_conv_transform.h>
#include <agg_path_storage.h>
#include <agg_pixfmt_rgba.h>
#include <agg_rasterizer_scanline_aa.h>
#include <agg_renderer_base.h>
#include <agg_renderer_scanline.h>
#include <agg_scanline_u.h>
#include <agg_trans_affine.h>
#include <stb_image_write.h>

#include "graphlib/errors.hpp"

namespace graphlib {

namespace {

agg::rgba8 to_rgba8(const Color& c) {
    auto ch = [](double v) {
        return static_cast<agg::int8u>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
    };
    return {ch(c.r), ch(c.g), ch(c.b), ch(c.a)};
}

agg::path_storage to_agg(const Path& path) {
    agg::path_storage ps;
    const auto verts = path.vertices();
    size_t i = 0;
    while (i < verts.size()) {
        switch (path.code_at(i)) {
        case PathCode::moveto:
            ps.move_to(verts[i].x, verts[i].y);
            i += 1;
            break;
        case PathCode::lineto:
            ps.line_to(verts[i].x, verts[i].y);
            i += 1;
            break;
        case PathCode::curve3:
            ps.curve3(verts[i].x, verts[i].y, verts[i + 1].x, verts[i + 1].y);
            i += 2;
            break;
        case PathCode::curve4:
            ps.curve4(verts[i].x, verts[i].y, verts[i + 1].x, verts[i + 1].y, verts[i + 2].x,
                      verts[i + 2].y);
            i += 3;
            break;
        case PathCode::closepoly:
            ps.close_polygon();
            i += 1;
            break;
        case PathCode::stop:
            i = verts.size();
            break;
        }
    }
    return ps;
}

agg::line_cap_e to_agg(CapStyle cap) {
    switch (cap) {
    case CapStyle::round:
        return agg::round_cap;
    case CapStyle::projecting:
        return agg::square_cap;
    case CapStyle::butt:
        break;
    }
    return agg::butt_cap;
}

agg::line_join_e to_agg(JoinStyle join) {
    switch (join) {
    case JoinStyle::round:
        return agg::round_join;
    case JoinStyle::bevel:
        return agg::bevel_join;
    case JoinStyle::miter:
        break;
    }
    return agg::miter_join;
}

} // namespace

struct AggRenderer::Impl {
    using PixFmt = agg::pixfmt_rgba32; // straight alpha, like mpl's Agg backend
    using RendererBase = agg::renderer_base<PixFmt>;

    int width;
    int height;
    std::vector<unsigned char> buf; // top-down rows
    agg::rendering_buffer rbuf;
    PixFmt pixf;
    RendererBase rb;
    agg::rasterizer_scanline_aa<> ras;
    agg::scanline_u8 sl;

    Impl(int w, int h)
        : width(w), height(h), buf(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0),
          // Negative-stride attach: agg row 0 = bottom of the image, so the whole
          // pipeline runs in y-up display coordinates — the single flip site of
          // this backend; the memory stays top-down for the PNG encoder.
          // (row_accessor takes the block START and offsets to the last row itself.)
          rbuf(buf.data(), static_cast<unsigned>(w), static_cast<unsigned>(h), -w * 4),
          pixf(rbuf), rb(pixf) {}

    template <class VertexSource>
    void fill(VertexSource& vs, const Color& color) {
        ras.reset();
        ras.filling_rule(agg::fill_non_zero);
        ras.add_path(vs);
        agg::render_scanlines_aa_solid(ras, sl, rb, to_rgba8(color));
    }

    void set_clip(const GraphicsContext& gc) {
        if (gc.clip_rect) {
            ras.clip_box(gc.clip_rect->x0(), gc.clip_rect->y0(), gc.clip_rect->x1(),
                         gc.clip_rect->y1());
        } else {
            ras.clip_box(0, 0, width, height);
        }
    }
};

AggRenderer::AggRenderer(int width_px, int height_px, double dpi)
    : Renderer(dpi), impl_(std::make_unique<Impl>(width_px, height_px)) {}

AggRenderer::~AggRenderer() = default;

Size AggRenderer::canvas_size() const {
    return {static_cast<double>(impl_->width), static_cast<double>(impl_->height)};
}

std::span<const unsigned char> AggRenderer::buffer() const { return impl_->buf; }

void AggRenderer::draw_path(const GraphicsContext& gc, const Path& path,
                            const Affine2D& transform, const std::optional<Color>& face) {
    if (path.empty()) {
        return;
    }
    Impl& I = *impl_;
    agg::path_storage ps = to_agg(path);
    // agg::trans_affine(sx, shy, shx, sy, tx, ty) == our [a b c d e f]
    agg::trans_affine mtx(transform.a(), transform.b(), transform.c(), transform.d(),
                          transform.e(), transform.f());
    agg::conv_transform<agg::path_storage> tp(ps, mtx);
    agg::conv_curve<agg::conv_transform<agg::path_storage>> curve(tp);
    using CurveT = decltype(curve);

    I.set_clip(gc);
    if (face && face->a > 0.0) {
        I.fill(curve, *face);
    }
    if (gc.color.a > 0.0 && gc.linewidth > 0.0) {
        const double width_px = points_to_pixels(gc.linewidth);
        if (gc.dashes.is_solid()) {
            agg::conv_stroke<CurveT> stroke(curve);
            stroke.width(width_px);
            stroke.line_cap(to_agg(gc.capstyle));
            stroke.line_join(to_agg(gc.joinstyle));
            I.fill(stroke, gc.color);
        } else {
            agg::conv_dash<CurveT> dash(curve);
            const auto& pattern = gc.dashes.on_off; // points, already x linewidth
            for (size_t i = 0; i + 1 < pattern.size(); i += 2) {
                dash.add_dash(points_to_pixels(pattern[i]), points_to_pixels(pattern[i + 1]));
            }
            if (pattern.size() % 2 == 1) { // odd pattern repeats, like SVG
                dash.add_dash(points_to_pixels(pattern.back()),
                              points_to_pixels(pattern.front()));
            }
            dash.dash_start(points_to_pixels(gc.dashes.offset));
            agg::conv_stroke<agg::conv_dash<CurveT>> stroke(dash);
            stroke.width(width_px);
            stroke.line_cap(to_agg(gc.capstyle));
            stroke.line_join(to_agg(gc.joinstyle));
            I.fill(stroke, gc.color);
        }
    }
}

void AggRenderer::write_png(const std::string& filename) const {
    const Impl& I = *impl_;
    if (stbi_write_png(filename.c_str(), I.width, I.height, 4, I.buf.data(), I.width * 4) == 0) {
        throw Error("savefig: cannot write '" + filename + "'");
    }
}

} // namespace graphlib
