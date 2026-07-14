#include "graphlib/collections.hpp"

#include <algorithm>
#include <cmath>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"

namespace graphlib {

Bbox PathCollection::data_extents() const {
    Bbox box = Bbox::null();
    for (size_t i = 0; i < xdata.size(); ++i) {
        box.update({xdata[i], ydata[i]});
    }
    return box;
}

void PathCollection::draw(Renderer& renderer) {
    if (!visible || xdata.empty() || axes == nullptr || marker == nullptr) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    const Affine2D tf = axes->trans_data(canvas);

    GraphicsContext gc;
    gc.color = edgecolor;
    gc.linewidth = linewidth;
    gc.capstyle = CapStyle::butt;
    gc.clip_rect = axes->bbox_pixels(canvas);
    std::optional<Color> face;
    if (marker->filled) {
        face = facecolor;
    }
    if (alpha) {
        gc.color.a = *alpha;
        if (face) {
            face->a = *alpha;
        }
    }

    // Marker paths are in marker units (1 unit == markersize pt); scatter sizes
    // are areas in pt^2, so the linear scale is sqrt(s) (mpl semantics: s=36
    // draws like markersize=6).
    renderer.open_group("pathcollection");
    const bool uniform =
        facecolors.empty() &&
        (sizes.size() == 1 ||
         std::all_of(sizes.begin(), sizes.end(), [&](double v) { return v == sizes.front(); }));
    Path positions = Path::line(xdata, ydata);
    if (axes->nonlinear_scale()) {
        positions = positions.mapped([this](Point p) { return axes->scale_point(p); });
    }
    if (uniform) {
        const double scale = renderer.points_to_pixels(std::sqrt(sizes.front()));
        renderer.draw_markers(gc, marker->path, Affine2D::scaling(scale, scale), positions, tf,
                              face);
    } else {
        for (size_t i = 0; i < xdata.size(); ++i) {
            const double s = sizes[std::min(i, sizes.size() - 1)];
            const double scale = renderer.points_to_pixels(std::sqrt(s));
            const Point px = tf.apply(axes->scale_point({xdata[i], ydata[i]}));
            std::optional<Color> point_face = face;
            GraphicsContext point_gc = gc;
            if (!facecolors.empty()) {
                Color fc = facecolors[std::min(i, facecolors.size() - 1)];
                if (alpha) {
                    fc.a *= *alpha;
                }
                if (marker->filled) {
                    point_face = fc;
                }
                if (edge_follows_face) {
                    point_gc.color = fc;
                }
            }
            renderer.draw_path(point_gc, marker->path,
                               Affine2D::scaling(scale, scale)
                                   .then(Affine2D::translation(px.x, px.y)),
                               point_face);
        }
    }
    renderer.close_group();
}

Bbox QuadMesh::data_extents() const {
    Bbox box = Bbox::null();
    for (const double x : x_edges) {
        box.update({x, y_edges.empty() ? 0.0 : y_edges.front()});
    }
    for (const double y : y_edges) {
        box.update({x_edges.empty() ? 0.0 : x_edges.front(), y});
    }
    return box;
}

void QuadMesh::draw(Renderer& renderer) {
    if (!visible || values.empty() || axes == nullptr || cmap == nullptr) {
        return;
    }
    const size_t cols = x_edges.size() - 1;
    const size_t rows = y_edges.size() - 1;
    const Size canvas = renderer.canvas_size();
    const Affine2D tf = axes->trans_data(canvas);

    GraphicsContext gc;
    gc.color.a = 0; // flat shading, no edges (mpl default)
    gc.clip_rect = axes->bbox_pixels(canvas);
    const Normalize norm{vmin, vmax};
    const double a_mult = alpha.value_or(1.0);

    renderer.open_group("quadmesh");
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            const double v = values[r * cols + c];
            Color face = (*cmap)(std::isnan(v) ? v : norm(v));
            face.a *= a_mult;
            if (face.a <= 0) {
                continue;
            }
            Path quad;
            const auto sp = [this](double x, double y) { return axes->scale_point({x, y}); };
            const Point p0 = sp(x_edges[c], y_edges[r]);
            const Point p1 = sp(x_edges[c + 1], y_edges[r]);
            const Point p2 = sp(x_edges[c + 1], y_edges[r + 1]);
            const Point p3 = sp(x_edges[c], y_edges[r + 1]);
            quad.move_to(p0.x, p0.y);
            quad.line_to(p1.x, p1.y);
            quad.line_to(p2.x, p2.y);
            quad.line_to(p3.x, p3.y);
            quad.close_subpath();
            renderer.draw_path(gc, quad, tf, face);
        }
    }
    renderer.close_group();
}

} // namespace graphlib
