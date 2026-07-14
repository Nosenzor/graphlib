#include "graphlib/image.hpp"

#include <algorithm>
#include <cmath>

#include "graphlib/axes.hpp"

namespace graphlib {

Bbox AxesImage::data_extents() const {
    return Bbox::from_extents(std::min(extent[0], extent[1]), std::min(extent[2], extent[3]),
                              std::max(extent[0], extent[1]), std::max(extent[2], extent[3]));
}

void AxesImage::draw(Renderer& renderer) {
    if (!visible || data.empty() || axes == nullptr || cmap == nullptr) {
        return;
    }
    // Data -> RGBA through norm + colormap (artist alpha baked in).
    ImageBuffer buf;
    buf.width = cols;
    buf.height = rows;
    buf.rgba.resize(static_cast<size_t>(cols) * static_cast<size_t>(rows) * 4);
    const Normalize norm{vmin, vmax};
    const double a_mult = alpha.value_or(1.0);
    for (int r = 0; r < rows; ++r) {
        // ImageBuffer row 0 is the top; origin 'upper' puts data row 0 there.
        const int src_row = origin_upper ? r : rows - 1 - r;
        for (int c = 0; c < cols; ++c) {
            const double v =
                data[static_cast<size_t>(src_row) * static_cast<size_t>(cols) +
                     static_cast<size_t>(c)];
            const Color col = (*cmap)(std::isnan(v) ? v : norm(v));
            const size_t o = (static_cast<size_t>(r) * static_cast<size_t>(cols) +
                              static_cast<size_t>(c)) *
                             4;
            buf.rgba[o + 0] = static_cast<unsigned char>(std::lround(col.r * 255.0));
            buf.rgba[o + 1] = static_cast<unsigned char>(std::lround(col.g * 255.0));
            buf.rgba[o + 2] = static_cast<unsigned char>(std::lround(col.b * 255.0));
            buf.rgba[o + 3] = static_cast<unsigned char>(std::lround(col.a * a_mult * 255.0));
        }
    }

    const Size canvas = renderer.canvas_size();
    const Affine2D tf = axes->trans_data(canvas);
    const Point p0 = tf.apply(axes->scale_point({extent[0], extent[2]}));
    const Point p1 = tf.apply(axes->scale_point({extent[1], extent[3]}));
    Bbox dest = Bbox::null();
    dest.update(p0);
    dest.update(p1);

    GraphicsContext gc;
    gc.clip_rect = axes->bbox_pixels(canvas);
    renderer.open_group("image");
    renderer.draw_image(gc, dest, buf, interpolation);
    renderer.close_group();
}

} // namespace graphlib
