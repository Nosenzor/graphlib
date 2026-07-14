#include "graphlib/patches.hpp"

#include <cmath>
#include <numbers>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/rc.hpp"

namespace graphlib {

Patch::Patch() {
    zorder = 1.0;
    facecolor = rc().color("patch.facecolor"); // 'C0' resolves via the default cycle
    edgecolor = rc().color("patch.edgecolor");
    linewidth = rc().number("patch.linewidth");
}

void Patch::draw(Renderer& renderer) {
    if (!visible || axes == nullptr) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    GraphicsContext gc;
    gc.color = edgecolor;
    gc.linewidth = linewidth;
    gc.dashes = linestyle.drawn() ? linestyle.dash_pattern(linewidth) : DashPattern{};
    gc.joinstyle = JoinStyle::miter; // mpl patch joinstyle default
    gc.clip_rect = axes->bbox_pixels(canvas);
    std::optional<Color> face = facecolor;
    if (alpha) {
        gc.color.a = edgecolor.a * *alpha;
        face->a = facecolor.a * *alpha;
    }
    const Affine2D tf = (x_axes_fraction || y_axes_fraction)
                            ? axes->blended_transform(x_axes_fraction, y_axes_fraction, canvas)
                            : axes->trans_data(canvas);
    renderer.open_group("patch");
    renderer.draw_path(gc, get_path(), tf, face);
    renderer.close_group();
}

Path Rectangle::get_path() const {
    Path p;
    p.move_to(x, y);
    p.line_to(x + width, y);
    p.line_to(x + width, y + height);
    p.line_to(x, y + height);
    p.close_subpath();
    return p;
}

Path Polygon::get_path() const {
    Path p;
    if (points.empty()) {
        return p;
    }
    p.move_to(points[0].x, points[0].y);
    for (size_t i = 1; i < points.size(); ++i) {
        p.line_to(points[i].x, points[i].y);
    }
    if (closed) {
        p.close_subpath();
    }
    return p;
}

Path Wedge::get_path() const {
    // Center -> arc start -> circular arc (cubic Béziers per <=90° segment,
    // k = 4/3 tan(sweep/4)) -> back to center.
    Path p;
    const double t1 = theta1 * std::numbers::pi / 180.0;
    const double t2 = theta2 * std::numbers::pi / 180.0;
    p.move_to(center.x, center.y);
    p.line_to(center.x + r * std::cos(t1), center.y + r * std::sin(t1));
    const int segments = std::max(1, static_cast<int>(std::ceil((t2 - t1) / (std::numbers::pi / 2))));
    double a0 = t1;
    for (int s = 0; s < segments; ++s) {
        const double a1 = t1 + (t2 - t1) * (s + 1) / segments;
        const double k = 4.0 / 3.0 * std::tan((a1 - a0) / 4.0);
        const Point p0{center.x + r * std::cos(a0), center.y + r * std::sin(a0)};
        const Point p3{center.x + r * std::cos(a1), center.y + r * std::sin(a1)};
        const Point c1{p0.x - k * r * std::sin(a0), p0.y + k * r * std::cos(a0)};
        const Point c2{p3.x + k * r * std::sin(a1), p3.y - k * r * std::cos(a1)};
        p.curve4_to(c1, c2, p3);
        a0 = a1;
    }
    p.close_subpath();
    return p;
}

} // namespace graphlib
