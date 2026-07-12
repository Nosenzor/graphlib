#include "graphlib/backend/renderer.hpp"

namespace graphlib {

void Renderer::draw_markers(const GraphicsContext& gc, const Path& marker,
                            const Affine2D& marker_transform, const Path& positions,
                            const Affine2D& transform, const std::optional<Color>& face) {
    for (const Point& v : positions.vertices()) {
        const Point px = transform.apply(v);
        const Affine2D place = marker_transform.then(Affine2D::translation(px.x, px.y));
        draw_path(gc, marker, place, face);
    }
}

} // namespace graphlib
