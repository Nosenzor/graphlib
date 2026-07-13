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
        sizes.size() == 1 ||
        std::all_of(sizes.begin(), sizes.end(), [&](double v) { return v == sizes.front(); });
    if (uniform) {
        const double scale = renderer.points_to_pixels(std::sqrt(sizes.front()));
        renderer.draw_markers(gc, marker->path, Affine2D::scaling(scale, scale),
                              Path::line(xdata, ydata), tf, face);
    } else {
        for (size_t i = 0; i < xdata.size(); ++i) {
            const double s = sizes[std::min(i, sizes.size() - 1)];
            const double scale = renderer.points_to_pixels(std::sqrt(s));
            const Point px = tf.apply({xdata[i], ydata[i]});
            renderer.draw_path(gc, marker->path,
                               Affine2D::scaling(scale, scale)
                                   .then(Affine2D::translation(px.x, px.y)),
                               face);
        }
    }
    renderer.close_group();
}

} // namespace graphlib
