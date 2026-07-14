#include "graphlib/contour.hpp"

#include <cmath>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"

namespace graphlib {

namespace {
// Linear interpolation of the level crossing between two grid values.
double crossing(double a, double b, double t) {
    const double d = b - a;
    return d == 0.0 ? 0.5 : (t - a) / d;
}
} // namespace

Bbox ContourSet::data_extents() const {
    Bbox box = Bbox::null();
    if (!x.empty() && !y.empty()) {
        box.update({x.front(), y.front()});
        box.update({x.back(), y.back()});
    }
    return box;
}

Path ContourSet::level_lines(double level) const {
    Path out;
    for (size_t r = 0; r + 1 < rows(); ++r) {
        for (size_t c = 0; c + 1 < cols(); ++c) {
            const double z00 = at(r, c);
            const double z10 = at(r, c + 1);
            const double z11 = at(r + 1, c + 1);
            const double z01 = at(r + 1, c);
            if (std::isnan(z00) || std::isnan(z10) || std::isnan(z11) || std::isnan(z01)) {
                continue;
            }
            // Edge crossing points (parametrized along each cell edge).
            const Point bottom{x[c] + crossing(z00, z10, level) * (x[c + 1] - x[c]), y[r]};
            const Point right{x[c + 1], y[r] + crossing(z10, z11, level) * (y[r + 1] - y[r])};
            const Point top{x[c] + crossing(z01, z11, level) * (x[c + 1] - x[c]), y[r + 1]};
            const Point left{x[c], y[r] + crossing(z00, z01, level) * (y[r + 1] - y[r])};

            const int idx = (z00 >= level ? 1 : 0) | (z10 >= level ? 2 : 0) |
                            (z11 >= level ? 4 : 0) | (z01 >= level ? 8 : 0);
            auto seg = [&out](Point a, Point b) {
                out.move_to(a.x, a.y);
                out.line_to(b.x, b.y);
            };
            switch (idx) { // the 16 marching-squares cases
            case 1:
            case 14:
                seg(left, bottom);
                break;
            case 2:
            case 13:
                seg(bottom, right);
                break;
            case 3:
            case 12:
                seg(left, right);
                break;
            case 4:
            case 11:
                seg(top, right);
                break;
            case 6:
            case 9:
                seg(bottom, top);
                break;
            case 7:
            case 8:
                seg(left, top);
                break;
            case 5:
            case 10: { // saddle: resolve with the cell-center average
                const bool center_high = (z00 + z10 + z11 + z01) / 4.0 >= level;
                if ((idx == 5) == center_high) {
                    seg(left, top);
                    seg(bottom, right);
                } else {
                    seg(left, bottom);
                    seg(top, right);
                }
                break;
            }
            default:
                break; // 0 and 15: no crossing
            }
        }
    }
    return out;
}

Path ContourSet::threshold_fill(double level) const {
    Path out;
    for (size_t r = 0; r + 1 < rows(); ++r) {
        for (size_t c = 0; c + 1 < cols(); ++c) {
            // Walk the cell boundary; keep corners above the level and insert
            // interpolated crossings on sign changes (clip-polygon style).
            const Point corners[4] = {{x[c], y[r]},
                                      {x[c + 1], y[r]},
                                      {x[c + 1], y[r + 1]},
                                      {x[c], y[r + 1]}};
            const double vals[4] = {at(r, c), at(r, c + 1), at(r + 1, c + 1), at(r + 1, c)};
            if (std::isnan(vals[0]) || std::isnan(vals[1]) || std::isnan(vals[2]) ||
                std::isnan(vals[3])) {
                continue;
            }
            Point poly[8];
            int n = 0;
            for (int i = 0; i < 4; ++i) {
                const int j = (i + 1) % 4;
                if (vals[i] >= level) {
                    poly[n++] = corners[i];
                }
                if ((vals[i] >= level) != (vals[j] >= level)) {
                    const double f = crossing(vals[i], vals[j], level);
                    poly[n++] = {corners[i].x + f * (corners[j].x - corners[i].x),
                                 corners[i].y + f * (corners[j].y - corners[i].y)};
                }
            }
            if (n < 3) {
                continue;
            }
            out.move_to(poly[0].x, poly[0].y);
            for (int i = 1; i < n; ++i) {
                out.line_to(poly[i].x, poly[i].y);
            }
            out.close_subpath();
        }
    }
    return out;
}

void ContourSet::draw(Renderer& renderer) {
    if (!visible || levels.empty() || axes == nullptr || z.empty()) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    const Affine2D tf = axes->trans_data(canvas);
    const Normalize norm{levels.front(),
                         levels.back() == levels.front() ? levels.front() + 1.0 : levels.back()};
    const double a_mult = alpha.value_or(1.0);

    auto level_color = [&](double representative) {
        Color col = single_color ? *single_color
                                 : (cmap != nullptr ? (*cmap)(norm(representative))
                                                    : Color{0, 0, 0, 1});
        col.a *= a_mult;
        return col;
    };

    renderer.open_group(filled ? "contourf" : "contour");
    GraphicsContext gc;
    gc.clip_rect = axes->bbox_pixels(canvas);
    if (filled) {
        gc.color.a = 0; // fills only
        // Ascending stacked fills: each band paints over the previous.
        for (size_t i = 0; i + 1 < levels.size(); ++i) {
            const double mid = (levels[i] + levels[i + 1]) / 2.0; // mpl layer midpoint
            Path region = threshold_fill(levels[i]);
            if (region.empty()) {
                continue;
            }
            if (axes->nonlinear_scale()) {
                region = region.mapped([this](Point p) { return axes->scale_point(p); });
            }
            renderer.draw_path(gc, region, tf, level_color(mid));
        }
    } else {
        gc.linewidth = linewidth;
        for (const double level : levels) {
            Path lines = level_lines(level);
            if (lines.empty()) {
                continue;
            }
            if (axes->nonlinear_scale()) {
                lines = lines.mapped([this](Point p) { return axes->scale_point(p); });
            }
            gc.color = level_color(level);
            renderer.draw_path(gc, lines, tf);
        }
    }
    renderer.close_group();
}

} // namespace graphlib
