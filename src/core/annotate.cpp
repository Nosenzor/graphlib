// Annotation artist + the FancyArrowPatch geometry pipeline it draws with.
// Port of matplotlib.text.Annotation.update_positions and
// patches.ArrowStyle._Curve.transmute, specialized to the default straight
// "arc3,rad=0" connector (docs/PARITY.md).
#include "core/annotate.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "graphlib/axes.hpp"
#include "graphlib/errors.hpp"
#include "graphlib/path.hpp"
#include "graphlib/rc.hpp"
#include "graphlib/text.hpp"
#include "text/font_manager.hpp"

namespace graphlib {

namespace detail {

ArrowStyleSpec parse_arrowstyle(std::string_view style) {
    if (style == "-") {
        return {false, false};
    }
    if (style == "->") {
        return {true, false};
    }
    if (style == "-|>") {
        return {true, true};
    }
    throw ValueError("annotate: unsupported arrowstyle '" + std::string(style) +
                     "' (graphlib supports '-', '->', '-|>')");
}

ArrowGeometry build_arrow(Point posA, Point posB, const std::optional<Bbox>& patchA,
                          double shrinkA_px, double shrinkB_px, const ArrowStyleSpec& style,
                          double mutation_px, double linewidth_px) {
    Point a = posA;
    Point b = posB;

    // Clip the tail out of patchA — only when the path actually starts inside
    // and leaves it, like mpl's split_path_inout (ValueError -> clip skipped).
    if (patchA) {
        const auto inside = [&](const Point& p) {
            return p.x >= patchA->x0() && p.x <= patchA->x1() && p.y >= patchA->y0() &&
                   p.y <= patchA->y1();
        };
        if (inside(a) && !inside(b)) {
            const double dx = b.x - a.x;
            const double dy = b.y - a.y;
            double t = std::numeric_limits<double>::infinity();
            if (dx > 0.0) {
                t = std::min(t, (patchA->x1() - a.x) / dx);
            } else if (dx < 0.0) {
                t = std::min(t, (patchA->x0() - a.x) / dx);
            }
            if (dy > 0.0) {
                t = std::min(t, (patchA->y1() - a.y) / dy);
            } else if (dy < 0.0) {
                t = std::min(t, (patchA->y0() - a.y) / dy);
            }
            if (std::isfinite(t) && t > 0.0 && t < 1.0) {
                a = {a.x + t * dx, a.y + t * dy};
            }
        }
    }

    // Shrink circles centered on the (clipped) endpoints; skipped when the
    // whole remaining path lies inside the circle (mpl behavior).
    const auto pull = [](Point& p, const Point& toward, double r) {
        const double d = std::sqrt((toward.x - p.x) * (toward.x - p.x) +
                                   (toward.y - p.y) * (toward.y - p.y));
        if (r > 0.0 && d > r) {
            p = {p.x + (toward.x - p.x) * r / d, p.y + (toward.y - p.y) * r / d};
        }
    };
    pull(a, b, shrinkA_px);
    pull(b, a, shrinkB_px);

    ArrowGeometry out;
    out.tail_begin = a;
    out.tail_end = b;
    if (!style.head || (a.x == b.x && a.y == b.y)) {
        return out;
    }

    // ArrowStyle._Curve.transmute / _get_arrow_wedge: an open (or filled)
    // wedge at the tip, plus a pad that pulls the tail back so its projected
    // cap does not overshoot the tip.
    const double head_length = 0.4 * mutation_px; // _Curve defaults
    const double head_width = 0.2 * mutation_px;
    const double head_dist =
        std::sqrt(head_length * head_length + head_width * head_width);
    const double cos_t = head_length / head_dist;
    const double sin_t = head_width / head_dist;

    double dx = a.x - b.x; // from the tip back along the tail
    double dy = a.y - b.y;
    double cp = std::sqrt(dx * dx + dy * dy);
    if (cp == 0.0) {
        cp = 1.0;
    }
    const double pad = 0.5 * linewidth_px / sin_t;
    const double ddx = pad * dx / cp;
    const double ddy = pad * dy / cp;
    dx = dx / cp * head_dist;
    dy = dy / cp * head_dist;
    const Point barb1{cos_t * dx + sin_t * dy, -sin_t * dx + cos_t * dy};
    const Point barb2{cos_t * dx - sin_t * dy, sin_t * dx + cos_t * dy};

    const Point tip{b.x + ddx, b.y + ddy};
    out.head = {{tip.x + barb1.x, tip.y + barb1.y}, tip, {tip.x + barb2.x, tip.y + barb2.y}};
    out.head_filled = style.filled;
    out.tail_end = tip;
    return out;
}

} // namespace detail

void Annotation::draw(Renderer& renderer) {
    if (!visible) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    const Affine2D to_px = axes->trans_data(canvas);
    const auto to_display = [&](Point p, CoordSys cs, Point ref) -> Point {
        switch (cs) {
        case CoordSys::data:
            return to_px.apply(axes->scale_point(p));
        case CoordSys::axes_fraction:
            return {(axes->position.x0() + p.x * (axes->position.x1() - axes->position.x0())) *
                        canvas.width,
                    (axes->position.y0() + p.y * (axes->position.y1() - axes->position.y0())) *
                        canvas.height};
        case CoordSys::offset_points:
        default:
            return {ref.x + renderer.points_to_pixels(p.x),
                    ref.y + renderer.points_to_pixels(p.y)};
        }
    };
    const Point xy_px = to_display(xy, xycoords, {});
    const Point text_px = to_display(xytext, textcoords, xy_px);
    text.axes = axes;
    text.coords = CoordSys::pixels;
    text.position = text_px;
    text.zorder = zorder;
    text.alpha = alpha;

    Bbox bbox = Bbox::from_extents(text_px.x, text_px.y, text_px.x, text_px.y);
    std::optional<Bbox> box_outer; // the drawn text box's rect (arrow clip target)
    std::optional<Path> box_path;  // painted after the arrow (mpl draw order)
    if (arrowprops || bbox_props) {
        // Text window extent — same anchor math as Renderer::draw_text.
        const auto& fm = detail::FontManager::instance();
        const double em = renderer.points_to_pixels(text.fontsize);
        const detail::TextExtent ext = fm.text_extent(text.text, em, text.bold);
        const double asc = fm.ascent(em, text.bold);
        const double desc = fm.descent(em, text.bold);
        double dx = 0.0;
        if (text.ha == HAlign::center) {
            dx = -ext.width / 2.0;
        } else if (text.ha == HAlign::right) {
            dx = -ext.width;
        }
        double dy = 0.0;
        switch (text.va) {
        case VAlign::top:
            dy = -asc;
            break;
        case VAlign::center:
            dy = -(asc - desc) / 2.0;
            break;
        case VAlign::bottom:
            dy = ext.height - asc;
            break;
        case VAlign::baseline:
            dy = 0.0;
            break;
        }
        const double x0 = text_px.x + dx;
        const double y1 = text_px.y + dy + asc;
        bbox = Bbox::from_extents(x0, y1 - ext.height, x0 + ext.width, y1);
    }

    if (bbox_props && !text.text.empty()) {
        const TextBboxProps& bp = *bbox_props;
        // BoxStyle.Round/.Square: pad (and corner radius) x fontsize.
        const double mutation = renderer.points_to_pixels(text.fontsize);
        const double pad = mutation * bp.pad;
        const double x0 = bbox.x0() - pad;
        const double y0 = bbox.y0() - pad;
        const double x1 = bbox.x1() + pad;
        const double y1 = bbox.y1() + pad;
        Path box;
        if (bp.boxstyle == "round") {
            const double dr = mutation * bp.rounding_size.value_or(bp.pad);
            // Quadratic-Bezier corners, exactly BoxStyle.Round's vertex list.
            box.move_to(x0 + dr, y0);
            box.line_to(x1 - dr, y0);
            box.curve3_to({x1, y0}, {x1, y0 + dr});
            box.line_to(x1, y1 - dr);
            box.curve3_to({x1, y1}, {x1 - dr, y1});
            box.line_to(x0 + dr, y1);
            box.curve3_to({x0, y1}, {x0, y1 - dr});
            box.line_to(x0, y0 + dr);
            box.curve3_to({x0, y0}, {x0 + dr, y0});
            box.close_subpath();
        } else if (bp.boxstyle == "square") {
            box.move_to(x0, y0);
            box.line_to(x1, y0);
            box.line_to(x1, y1);
            box.line_to(x0, y1);
            box.close_subpath();
        } else {
            throw ValueError("annotate: unsupported boxstyle '" + bp.boxstyle +
                             "' (graphlib supports 'round', 'square')");
        }
        box_path = std::move(box);
        box_outer = Bbox::from_extents(x0, y0, x1, y1);
    }

    if (arrowprops) {
        const ArrowProps& props = *arrowprops;
        const detail::ArrowStyleSpec style = detail::parse_arrowstyle(props.arrowstyle);

        // Tail anchor on the text box (arrowprops relpos, default center),
        // clipped out of the drawn box (or the 4pt-padded extent, like
        // update_positions when no bbox patch is set).
        const Point begin{bbox.x0() + (bbox.x1() - bbox.x0()) * props.relpos.first,
                          bbox.y0() + (bbox.y1() - bbox.y0()) * props.relpos.second};
        std::optional<Bbox> patchA;
        if (box_outer) {
            patchA = box_outer;
        } else if (!text.text.empty()) {
            const double pad = renderer.points_to_pixels(4.0) / 2.0;
            patchA = Bbox::from_extents(bbox.x0() - pad, bbox.y0() - pad, bbox.x1() + pad,
                                        bbox.y1() + pad);
        }
        const double ms_px =
            renderer.points_to_pixels(props.mutation_scale.value_or(text.fontsize));
        const double lw_pt = props.linewidth.value_or(rc().number("patch.linewidth"));
        const detail::ArrowGeometry geom = detail::build_arrow(
            begin, xy_px, patchA, renderer.points_to_pixels(props.shrinkA),
            renderer.points_to_pixels(props.shrinkB), style, ms_px,
            renderer.points_to_pixels(lw_pt));

        GraphicsContext gc;
        gc.color = props.color.empty() ? rc().color("patch.edgecolor") : to_color(props.color);
        if (alpha) {
            gc.color.a = *alpha;
        }
        gc.linewidth = lw_pt;
        gc.capstyle = CapStyle::projecting; // _Curve strokes with projected caps
        gc.joinstyle = JoinStyle::miter;

        renderer.open_group("annotation-arrow");
        Path tail;
        tail.move_to(geom.tail_begin.x, geom.tail_begin.y);
        tail.line_to(geom.tail_end.x, geom.tail_end.y);
        renderer.draw_path(gc, tail, Affine2D{}, std::nullopt);
        if (!geom.head.empty()) {
            Path head;
            head.move_to(geom.head[0].x, geom.head[0].y);
            head.line_to(geom.head[1].x, geom.head[1].y);
            head.line_to(geom.head[2].x, geom.head[2].y);
            std::optional<Color> face;
            if (geom.head_filled) {
                head.close_subpath();
                face = props.color.empty() ? rc().color("patch.facecolor")
                                           : to_color(props.color);
                if (alpha) {
                    face->a = *alpha;
                }
            }
            renderer.draw_path(gc, head, Affine2D{}, face);
        }
        renderer.close_group();
    }

    if (box_path) {
        const TextBboxProps& bp = *bbox_props;
        GraphicsContext box_gc;
        box_gc.color = bp.edgecolor.empty() ? rc().color("patch.edgecolor")
                                            : to_color(bp.edgecolor);
        box_gc.linewidth = bp.linewidth.value_or(rc().number("patch.linewidth"));
        Color face = bp.facecolor.empty() ? rc().color("patch.facecolor")
                                          : to_color(bp.facecolor);
        box_gc.color.a *= bp.alpha * alpha.value_or(1.0);
        face.a *= bp.alpha * alpha.value_or(1.0);
        renderer.open_group("annotation-bbox");
        renderer.draw_path(box_gc, *box_path, Affine2D{}, face);
        renderer.close_group();
    }
    text.draw(renderer);
}

Annotation& Axes::annotate(std::string s, Point xy, const AnnotateOpts& opts) {
    const auto parse_coords = [](std::string_view token, bool for_text) {
        if (token == "data") {
            return CoordSys::data;
        }
        if (token == "axes fraction") {
            return CoordSys::axes_fraction;
        }
        if (for_text && token == "offset points") {
            return CoordSys::offset_points;
        }
        throw ValueError("annotate: unsupported coordinate system '" + std::string(token) +
                         "' (graphlib supports 'data', 'axes fraction'" +
                         (for_text ? ", 'offset points'" : "") + ")");
    };

    auto ann = std::make_unique<Annotation>();
    ann->axes = this;
    ann->xy = xy;
    ann->xycoords = parse_coords(opts.xycoords, false);
    const std::string_view tc = opts.textcoords.empty() ? opts.xycoords : opts.textcoords;
    ann->textcoords = parse_coords(tc, true);
    ann->xytext = opts.xytext.value_or(xy);
    ann->arrowprops = opts.arrowprops;
    ann->bbox_props = opts.bbox;
    if (ann->arrowprops) {
        detail::parse_arrowstyle(ann->arrowprops->arrowstyle); // validate at call site
    }

    ann->text.text = std::move(s);
    ann->text.fontsize = opts.fontsize.value_or(rc().number("font.size"));
    ann->text.color = opts.color.empty() ? rc().color("text.color") : to_color(opts.color);
    ann->text.ha = opts.ha;
    ann->text.va = opts.va;
    ann->text.axes = this;

    Annotation& ref = *ann;
    children_.push_back(std::move(ann));
    return ref;
}

} // namespace graphlib
