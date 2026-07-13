#include "graphlib/backend/renderer.hpp"

#include "text/font_manager.hpp"

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

void Renderer::draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                         const FontProperties& font, double angle_deg, HAlign ha, VAlign va) {
    if (text.empty()) {
        return;
    }
    const auto& fm = detail::FontManager::instance();
    const double em = points_to_pixels(font.size_pt);
    const detail::TextExtent ext = fm.text_extent(text, em, font.bold);
    const double asc = fm.ascent(em, font.bold);
    const double desc = fm.descent(em, font.bold);

    // Anchor offsets in the (unrotated) text frame; the text box spans
    // [baseline - descent, baseline + ascent] vertically (y-up).
    double dx = 0.0;
    if (ha == HAlign::center) {
        dx = -ext.width / 2.0;
    } else if (ha == HAlign::right) {
        dx = -ext.width;
    }
    double dy = 0.0; // shift of the first baseline relative to the anchor
    switch (va) {
    case VAlign::top:
        dy = -asc;
        break;
    case VAlign::center:
        dy = -(asc - desc) / 2.0;
        break;
    case VAlign::bottom:
        dy = ext.height - asc; // box bottom (last-line descent) sits on the anchor
        break;
    case VAlign::baseline:
        dy = 0.0;
        break;
    }

    const Affine2D place = Affine2D::translation(dx, dy)
                               .then(Affine2D::rotation_deg(angle_deg))
                               .then(Affine2D::translation(pos.x, pos.y));
    GraphicsContext fill_gc = gc;
    fill_gc.color.a = 0.0; // glyphs are filled, never stroked
    draw_path(fill_gc, fm.text_path(text, em, font.bold), place, gc.color);
}

} // namespace graphlib
