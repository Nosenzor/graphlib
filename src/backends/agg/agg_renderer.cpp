#include "graphlib/backend/agg.hpp"

#include "core/path_simplify.hpp"
#include "graphlib/rc.hpp"

#include <algorithm>
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
#include <agg_scanline_storage_aa.h>
#include <agg_scanline_u.h>
#include <agg_trans_affine.h>
#include <stb_image_write.h>

#include "graphlib/errors.hpp"

namespace graphlib {

namespace {
bool has_curves(const Path& p) {
    if (!p.has_codes()) {
        return false;
    }
    for (const PathCode c : p.codes()) {
        if (c == PathCode::curve3 || c == PathCode::curve4) {
            return true;
        }
    }
    return false;
}
} // namespace


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

void AggRenderer::draw_markers(const GraphicsContext& gc, const Path& marker,
                                const Affine2D& marker_transform, const Path& positions,
                                const Affine2D& transform, const std::optional<Color>& face) {
    if (marker.empty() || positions.empty()) {
        return;
    }
    Impl& I = *impl_;

    // Rasterize the marker once at the origin, serialize the coverage spans,
    // then replay them per position snapped to whole pixels — mpl's
    // _backend_agg::draw_markers (positions are x=(int)x, y=(int)y there too).
    agg::path_storage mp = to_agg(marker);
    agg::trans_affine mtx(marker_transform.a(), marker_transform.b(), marker_transform.c(),
                          marker_transform.d(), marker_transform.e(), marker_transform.f());
    agg::conv_transform<agg::path_storage> tp(mp, mtx);
    agg::conv_curve<agg::conv_transform<agg::path_storage>> curve(tp);

    std::vector<agg::int8u> fill_buf;
    if (face && face->a > 0.0) {
        agg::scanline_storage_aa8 storage;
        I.ras.reset();
        I.ras.filling_rule(agg::fill_non_zero);
        I.ras.clip_box(-1e9, -1e9, 1e9, 1e9); // capture unclipped; clip at blend
        I.ras.add_path(curve);
        agg::render_scanlines(I.ras, I.sl, storage);
        fill_buf.resize(storage.byte_size());
        storage.serialize(fill_buf.data());
    }
    std::vector<agg::int8u> stroke_buf;
    if (gc.color.a > 0.0 && gc.linewidth > 0.0) {
        agg::conv_stroke<decltype(curve)> stroke(curve);
        stroke.width(points_to_pixels(gc.linewidth));
        stroke.line_cap(to_agg(gc.capstyle));
        stroke.line_join(to_agg(gc.joinstyle));
        agg::scanline_storage_aa8 storage;
        I.ras.reset();
        I.ras.filling_rule(agg::fill_non_zero);
        I.ras.clip_box(-1e9, -1e9, 1e9, 1e9);
        I.ras.add_path(stroke);
        agg::render_scanlines(I.ras, I.sl, storage);
        stroke_buf.resize(storage.byte_size());
        storage.serialize(stroke_buf.data());
    }
    if (fill_buf.empty() && stroke_buf.empty()) {
        return;
    }

    // Blend-time clipping (the serialized replay bypasses the rasterizer).
    if (gc.clip_rect) {
        I.rb.clip_box(static_cast<int>(gc.clip_rect->x0()), static_cast<int>(gc.clip_rect->y0()),
                      static_cast<int>(gc.clip_rect->x1()), static_cast<int>(gc.clip_rect->y1()));
    }
    agg::renderer_scanline_aa_solid<Impl::RendererBase> ren(I.rb);
    agg::scanline_u8 sl;
    for (const Point& v : positions.vertices()) {
        const Point px = transform.apply(v);
        if (!std::isfinite(px.x) || !std::isfinite(px.y)) {
            continue;
        }
        // Quantize to 2 decimals before snapping: FMA-contraction differences
        // between compilers are ~1 ulp, which a bare (int) cast can amplify
        // into a whole-pixel jump. The same quantization keeps the SVG
        // goldens byte-identical across platforms.
        const double sx = std::floor(std::round(px.x * 100.0) / 100.0);
        const double sy = std::floor(std::round(px.y * 100.0) / 100.0);
        if (!fill_buf.empty()) {
            agg::serialized_scanlines_adaptor_aa8 sa(fill_buf.data(),
                                                     static_cast<unsigned>(fill_buf.size()),
                                                     sx, sy);
            ren.color(to_rgba8(*face));
            agg::render_scanlines(sa, sl, ren);
        }
        if (!stroke_buf.empty()) {
            agg::serialized_scanlines_adaptor_aa8 sa(stroke_buf.data(),
                                                     static_cast<unsigned>(stroke_buf.size()),
                                                     sx, sy);
            ren.color(to_rgba8(gc.color));
            agg::render_scanlines(sa, sl, ren);
        }
    }
    I.rb.reset_clipping(true); // restore: draw_path relies on rasterizer clip only
}

void AggRenderer::draw_path(const GraphicsContext& gc, const Path& path,
                            const Affine2D& transform, const std::optional<Color>& face) {
    if (path.empty()) {
        return;
    }
    Impl& I = *impl_;
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

    // agg.path.chunksize: bound rasterizer memory on huge polylines by
    // stroking vertex ranges (first vertex of each chunk repeats the last of
    // the previous one, like mpl's _backend_agg chunking). Default 0 = off.
    const double chunk_rc = rc().number("agg.path.chunksize");
    const size_t chunksize = chunk_rc > 0.0 ? static_cast<size_t>(chunk_rc) : 0;
    if (chunksize >= 2 && (!face || face->a <= 0.0) && src->size() > chunksize &&
        !has_curves(*src)) {
        const auto verts = src->vertices();
        for (size_t start = 0; start < verts.size(); start += chunksize - 1) {
            const size_t end = std::min(verts.size(), start + chunksize);
            Path chunk;
            for (size_t i = start; i < end; ++i) {
                if (i == start || src->code_at(i) == PathCode::moveto) {
                    if (i == start && src->code_at(i) != PathCode::moveto) {
                        chunk.move_to(verts[i].x, verts[i].y);
                        continue;
                    }
                    chunk.move_to(verts[i].x, verts[i].y);
                } else {
                    chunk.line_to(verts[i].x, verts[i].y);
                }
            }
            draw_path(gc, chunk, tf, face);
            if (end == verts.size()) {
                break;
            }
        }
        return;
    }

    agg::path_storage ps = to_agg(*src);
    // agg::trans_affine(sx, shy, shx, sy, tx, ty) == our [a b c d e f]
    agg::trans_affine mtx(tf.a(), tf.b(), tf.c(), tf.d(), tf.e(), tf.f());
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

void AggRenderer::draw_image(const GraphicsContext& gc, const Bbox& dest,
                             const ImageBuffer& image, Interp interpolation) {
    if (image.width <= 0 || image.height <= 0 || dest.width() <= 0 || dest.height() <= 0) {
        return;
    }
    Impl& I = *impl_;
    // Device iteration bounds: dest ∩ clip ∩ canvas, in y-up pixel indices.
    double cx0 = 0;
    double cy0 = 0;
    double cx1 = I.width;
    double cy1 = I.height;
    if (gc.clip_rect) {
        cx0 = std::max(cx0, gc.clip_rect->x0());
        cy0 = std::max(cy0, gc.clip_rect->y0());
        cx1 = std::min(cx1, gc.clip_rect->x1());
        cy1 = std::min(cy1, gc.clip_rect->y1());
    }
    const int px0 = static_cast<int>(std::floor(std::max(dest.x0(), cx0)));
    const int py0 = static_cast<int>(std::floor(std::max(dest.y0(), cy0)));
    const int px1 = static_cast<int>(std::ceil(std::min(dest.x1(), cx1)));
    const int py1 = static_cast<int>(std::ceil(std::min(dest.y1(), cy1)));

    auto texel = [&](int ix, int iy) { // iy in image rows (0 = top)
        ix = std::clamp(ix, 0, image.width - 1);
        iy = std::clamp(iy, 0, image.height - 1);
        return &image.rgba[(static_cast<size_t>(iy) * static_cast<size_t>(image.width) +
                            static_cast<size_t>(ix)) *
                           4];
    };

    for (int py = py0; py < py1; ++py) {
        // Pixel center in dest-normalized coords; image row 0 at the rect TOP.
        const double v = (dest.y1() - (py + 0.5)) / dest.height() * image.height;
        for (int px = px0; px < px1; ++px) {
            const double u = ((px + 0.5) - dest.x0()) / dest.width() * image.width;
            double rgba[4];
            if (interpolation == Interp::nearest) {
                const unsigned char* t =
                    texel(static_cast<int>(std::floor(u)), static_cast<int>(std::floor(v)));
                for (int c = 0; c < 4; ++c) {
                    rgba[c] = t[c];
                }
            } else { // bilinear on straight alpha (edge behavior noted in PARITY)
                const double fu = u - 0.5;
                const double fv = v - 0.5;
                const int iu = static_cast<int>(std::floor(fu));
                const int iv = static_cast<int>(std::floor(fv));
                const double du = fu - iu;
                const double dv = fv - iv;
                const unsigned char* t00 = texel(iu, iv);
                const unsigned char* t10 = texel(iu + 1, iv);
                const unsigned char* t01 = texel(iu, iv + 1);
                const unsigned char* t11 = texel(iu + 1, iv + 1);
                for (int c = 0; c < 4; ++c) {
                    rgba[c] = (1 - du) * (1 - dv) * t00[c] + du * (1 - dv) * t10[c] +
                              (1 - du) * dv * t01[c] + du * dv * t11[c];
                }
            }
            const double a = rgba[3]; // artist alpha is baked into the buffer
            if (a <= 0) {
                continue;
            }
            const agg::rgba8 c(static_cast<agg::int8u>(std::lround(rgba[0])),
                               static_cast<agg::int8u>(std::lround(rgba[1])),
                               static_cast<agg::int8u>(std::lround(rgba[2])),
                               static_cast<agg::int8u>(std::lround(a)));
            I.rb.blend_pixel(px, py, c, 255);
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
