#include "graphlib/legend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/errors.hpp"
#include "graphlib/rc.hpp"
#include "text/font_manager.hpp"

namespace graphlib {

namespace {

// rc legend.* paddings, in font-size units (multiply by fontsize px).
constexpr double kBorderPad = 0.4;
constexpr double kLabelSpacing = 0.5;
constexpr double kHandleLength = 2.0;
constexpr double kHandleHeight = 0.7;
constexpr double kHandleTextPad = 0.8;
constexpr double kBorderAxesPad = 0.5;

// Anchor letters per loc code (offsetbox._get_anchored_bbox):
// [_, NE, NW, SW, SE, E, W, E, S, N, C] — 'right'(5) and 'center right'(7)
// both anchor East, an mpl quirk kept as-is.
enum class Anchor { NE, NW, SW, SE, E, W, S, N, C };
constexpr std::array<Anchor, 11> kAnchors = {Anchor::C /*unused slot 0*/, Anchor::NE, Anchor::NW,
                                             Anchor::SW, Anchor::SE,      Anchor::E,  Anchor::W,
                                             Anchor::E,  Anchor::S,       Anchor::N,  Anchor::C};

Point anchored_origin(Anchor a, double w, double h, const Bbox& container) {
    double x = 0;
    double y = 0;
    switch (a) { // x
    case Anchor::NE:
    case Anchor::SE:
    case Anchor::E:
        x = container.x1() - w;
        break;
    case Anchor::NW:
    case Anchor::SW:
    case Anchor::W:
        x = container.x0();
        break;
    case Anchor::N:
    case Anchor::S:
    case Anchor::C:
        x = (container.x0() + container.x1()) / 2.0 - w / 2.0;
        break;
    }
    switch (a) { // y (display coords, y-up)
    case Anchor::NE:
    case Anchor::NW:
    case Anchor::N:
        y = container.y1() - h;
        break;
    case Anchor::SE:
    case Anchor::SW:
    case Anchor::S:
        y = container.y0();
        break;
    case Anchor::E:
    case Anchor::W:
    case Anchor::C:
        y = (container.y0() + container.y1()) / 2.0 - h / 2.0;
        break;
    }
    return {x, y};
}

bool segment_intersects_bbox(Point p, Point q, const Bbox& box) {
    // Conservative port of Path.intersects_bbox(filled=False): endpoint inside
    // or the segment's bbox overlaps the box (cheap, adequate for placement).
    if (box.contains(p) || box.contains(q)) {
        return true;
    }
    const double lo_x = std::min(p.x, q.x);
    const double hi_x = std::max(p.x, q.x);
    const double lo_y = std::min(p.y, q.y);
    const double hi_y = std::max(p.y, q.y);
    return lo_x <= box.x1() && hi_x >= box.x0() && lo_y <= box.y1() && hi_y >= box.y0();
}

} // namespace

LegendLoc Legend::parse_loc(std::string_view loc) {
    static constexpr std::array<std::string_view, 11> names = {
        "best",        "upper right",  "upper left", "lower left",   "lower right", "right",
        "center left", "center right", "lower center", "upper center", "center"};
    for (size_t i = 0; i < names.size(); ++i) {
        if (loc == names[i]) {
            return static_cast<LegendLoc>(i);
        }
    }
    throw ValueError("legend: unrecognized loc '" + std::string(loc) + "'");
}

void Legend::draw(Renderer& renderer) {
    if (!visible || entries.empty() || axes == nullptr) {
        return;
    }
    const auto& fm = detail::FontManager::instance();
    const Size canvas = renderer.canvas_size();
    const double fs = renderer.points_to_pixels(fontsize);
    const double asc = fm.ascent(fs);
    const double desc = fm.descent(fs);

    // ---- layout ----
    const double border = kBorderPad * fs;
    const double handle_len = kHandleLength * fs;
    const double handle_pad = kHandleTextPad * fs;
    const double row_gap = kLabelSpacing * fs;
    const double row_h = std::max(asc + desc, kHandleHeight * fs);

    double max_label_w = 0.0;
    for (const Entry& e : entries) {
        max_label_w = std::max(max_label_w, fm.text_extent(e.label, fs).width);
    }
    const double width = 2 * border + handle_len + handle_pad + max_label_w;
    const double height = 2 * border + static_cast<double>(entries.size()) * row_h +
                          static_cast<double>(entries.size() - 1) * row_gap;

    // ---- placement (port of _find_best_position / _get_anchored_bbox) ----
    const Bbox axes_box = axes->bbox_pixels(canvas);
    const double axes_pad = kBorderAxesPad * fs;
    const Bbox container = Bbox::from_extents(axes_box.x0() + axes_pad, axes_box.y0() + axes_pad,
                                              axes_box.x1() - axes_pad, axes_box.y1() - axes_pad);
    Point origin;
    int loc_code = static_cast<int>(loc);
    if (loc_code == 0) { // 'best': scan codes 1..10, count data overlap
        std::vector<std::vector<Point>> lines_px;
        axes->collect_legend_avoidance(lines_px, canvas);
        double best_badness = std::numeric_limits<double>::infinity();
        for (int code = 1; code <= 10; ++code) {
            const Point o =
                anchored_origin(kAnchors[static_cast<size_t>(code)], width, height, container);
            const Bbox box = Bbox::from_extents(o.x, o.y, o.x + width, o.y + height);
            double badness = 0;
            for (const auto& line : lines_px) {
                for (const Point& v : line) {
                    badness += box.contains(v) ? 1 : 0;
                }
                for (size_t i = 0; i + 1 < line.size(); ++i) {
                    badness += segment_intersects_bbox(line[i], line[i + 1], box) ? 1 : 0;
                }
            }
            if (badness < best_badness) { // strict '<' favors lower codes on ties
                best_badness = badness;
                origin = o;
            }
            if (badness == 0) {
                break;
            }
        }
    } else {
        origin =
            anchored_origin(kAnchors[static_cast<size_t>(loc_code)], width, height, container);
    }

    // ---- draw ----
    renderer.open_group("legend");
    Path frame;
    frame.move_to(origin.x, origin.y);
    frame.line_to(origin.x + width, origin.y);
    frame.line_to(origin.x + width, origin.y + height);
    frame.line_to(origin.x, origin.y + height);
    frame.close_subpath();
    if (frameon) {
        GraphicsContext frame_gc;
        frame_gc.color = rc().color("legend.edgecolor");
        frame_gc.linewidth = rc().number("patch.linewidth");
        Color face = rc().color("axes.facecolor"); // legend face follows the axes background
        face.a = framealpha;
        renderer.draw_path(frame_gc, frame, Affine2D::identity(), face);
    }

    double row_center = origin.y + height - border - row_h / 2.0;
    for (const Entry& e : entries) {
        const double hx0 = origin.x + border;
        if (e.patch_swatch) { // bar/hist/fill handle: filled rectangle
            GraphicsContext gc;
            gc.color = e.patch_edgecolor;
            gc.linewidth = 1.0;
            const double half_h = kHandleHeight * fs / 2.0;
            Path swatch;
            swatch.move_to(hx0, row_center - half_h);
            swatch.line_to(hx0 + handle_len, row_center - half_h);
            swatch.line_to(hx0 + handle_len, row_center + half_h);
            swatch.line_to(hx0, row_center + half_h);
            swatch.close_subpath();
            renderer.draw_path(gc, swatch, Affine2D::identity(), e.patch_facecolor);
        }
        // handle: line sample and/or marker at its center
        if (e.linestyle.drawn()) {
            GraphicsContext gc;
            gc.color = e.color;
            if (alpha) {
                gc.color.a = *alpha;
            }
            gc.linewidth = e.linewidth;
            gc.dashes = e.linestyle.dash_pattern(e.linewidth);
            gc.capstyle = e.linestyle.default_capstyle();
            Path sample;
            sample.move_to(hx0, row_center);
            sample.line_to(hx0 + handle_len, row_center);
            renderer.draw_path(gc, sample, Affine2D::identity());
        }
        if (e.marker != nullptr) {
            GraphicsContext mgc;
            mgc.color = e.markeredgecolor;
            mgc.linewidth = e.markeredgewidth;
            const double s =
                renderer.points_to_pixels(e.markersize) * e.marker_scale; // rc legend.markerscale
            Path pos;
            pos.move_to(hx0 + handle_len / 2.0, row_center);
            renderer.draw_markers(mgc, e.marker->path, Affine2D::scaling(s, s), pos,
                                  Affine2D::identity(),
                                  e.marker->filled ? std::optional(e.markerfacecolor)
                                                   : std::nullopt);
        }
        GraphicsContext text_gc;
        text_gc.color = rc().color("text.color"); // rc legend.labelcolor 'None' -> text.color
        renderer.draw_text(text_gc, {hx0 + handle_len + handle_pad, row_center}, e.label,
                           FontProperties{fontsize, false, false}, 0.0, HAlign::left,
                           VAlign::center);
        row_center -= row_h + row_gap;
    }
    renderer.close_group();
}

} // namespace graphlib
