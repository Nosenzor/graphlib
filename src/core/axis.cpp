#include "graphlib/axis.hpp"

#include <cmath>
#include <limits>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/rc.hpp"

namespace graphlib {

namespace {
// Direction 'out' only in v0.3 (in/inout arrive with tick param API).
std::string rc_key(Axis::Kind kind, const char* suffix) {
    return std::string(kind == Axis::Kind::x ? "xtick." : "ytick.") + suffix;
}
} // namespace

Axis::Axis(Kind kind)
    : kind_(kind), locator_(std::make_unique<MaxNLocator>()),
      formatter_(std::make_unique<ScalarFormatter>()) {}

std::vector<double> Axis::compute_minor_ticks(const Axes& axes) const {
    if (minor_locator_ == nullptr) {
        return {};
    }
    const auto [v0, v1] = kind_ == Kind::x ? axes.get_xlim() : axes.get_ylim();
    minor_locator_->set_tick_space(9);
    if (auto* log_loc = dynamic_cast<LogLocator*>(minor_locator_.get())) {
        log_loc->set_minpos(kind_ == Kind::x ? axes.minpos_x() : axes.minpos_y());
    }
    std::vector<double> out;
    const double eps = 1e-10 * std::abs(v1 - v0);
    for (const double loc : minor_locator_->tick_values(v0, v1)) {
        if (loc >= std::min(v0, v1) - eps && loc <= std::max(v0, v1) + eps) {
            out.push_back(loc);
        }
    }
    return out;
}

void Axis::draw_minor_ticks(Renderer& renderer, const Axes& axes,
                            const std::vector<double>& locs, bool far_side) const {
    if (locs.empty()) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    const Bbox bbox = axes.bbox_pixels(canvas);
    const Affine2D tf = axes.trans_data(canvas);
    const double size_px = renderer.points_to_pixels(rc().number(rc_key(kind_, "minor.size")));

    GraphicsContext gc;
    gc.color = rc().color(rc_key(kind_, "color"));
    gc.linewidth = rc().number(rc_key(kind_, "minor.width"));
    gc.capstyle = CapStyle::butt;

    renderer.open_group(kind_ == Kind::x ? "xtick_minor" : "ytick_minor");
    for (const double loc : locs) {
        Path mark;
        if (kind_ == Kind::x) {
            const double px = tf.apply(axes.scale_point({loc, 1.0})).x;
            const double edge = far_side ? bbox.y1() : bbox.y0();
            const double sign = far_side ? 1.0 : -1.0;
            mark.move_to(px, edge);
            mark.line_to(px, edge + sign * size_px);
        } else {
            const double py = tf.apply(axes.scale_point({1.0, loc})).y;
            const double edge = far_side ? bbox.x1() : bbox.x0();
            const double sign = far_side ? 1.0 : -1.0;
            mark.move_to(edge, py);
            mark.line_to(edge + sign * size_px, py);
        }
        renderer.draw_path(gc, mark, Affine2D::identity());
    }
    renderer.close_group();
}

int Axis::tick_space(Kind kind, double length_pt, double label_fontsize_pt) {
    const double size =
        kind == Kind::x ? label_fontsize_pt * 3 : label_fontsize_pt * 2;
    if (size <= 0) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(std::floor(length_pt / size));
}

Axis::TickData Axis::compute_ticks(const Axes& axes) const {
    const auto [v0, v1] = kind_ == Kind::x ? axes.get_xlim() : axes.get_ylim();
    // Tick density depends on physical length only (inches x 72), never on dpi.
    const auto figsize = axes.figure().figsize;
    const double length_in = kind_ == Kind::x ? axes.position.width() * figsize[0]
                                              : axes.position.height() * figsize[1];
    locator_->set_tick_space(
        tick_space(kind_, length_in * 72.0, rc().fontsize(rc_key(kind_, "labelsize"))));
    if (auto* log_loc = dynamic_cast<LogLocator*>(locator_.get())) {
        log_loc->set_minpos(kind_ == Kind::x ? axes.minpos_x() : axes.minpos_y());
    }
    const std::vector<double> locs = locator_->tick_values(v0, v1);
    const std::vector<std::string> labels = formatter_->format_ticks(locs, v0, v1);

    // Trim to the view interval with a small relative slop (mpl trims at draw);
    // min/max so inverted axes (imshow origin 'upper') keep their ticks.
    TickData out;
    out.offset_text = formatter_->offset_text(locs, v0, v1);
    const double lo = std::min(v0, v1);
    const double hi = std::max(v0, v1);
    const double eps = 1e-10 * (hi - lo);
    for (size_t i = 0; i < locs.size(); ++i) {
        if (locs[i] >= lo - eps && locs[i] <= hi + eps) {
            out.locs.push_back(locs[i]);
            out.labels.push_back(labels[i]);
        }
    }
    return out;
}

void Axis::draw_ticks(Renderer& renderer, const Axes& axes, const TickData& ticks, bool far_side,
                      bool with_labels) const {
    const Size canvas = renderer.canvas_size();
    const Bbox bbox = axes.bbox_pixels(canvas);
    const Affine2D tf = axes.trans_data(canvas);
    const double size_px = renderer.points_to_pixels(rc().number(rc_key(kind_, "major.size")));
    const double pad_px = renderer.points_to_pixels(rc().number(rc_key(kind_, "major.pad")));

    GraphicsContext gc;
    gc.color = rc().color(rc_key(kind_, "color"));
    gc.linewidth = rc().number(rc_key(kind_, "major.width"));
    gc.capstyle = CapStyle::butt;

    GraphicsContext label_gc;
    label_gc.color = gc.color; // rc tick labelcolor follows tick color ('inherit')
    const FontProperties font{rc().fontsize(rc_key(kind_, "labelsize")), false, false};

    renderer.open_group(kind_ == Kind::x ? "xtick" : "ytick");
    for (size_t i = 0; i < ticks.locs.size(); ++i) {
        const double loc = ticks.locs[i];
        if (kind_ == Kind::x) {
            const double px = tf.apply(axes.scale_point({loc, 1.0})).x;
            // direction 'out': away from the axes box on the chosen side
            const double edge = far_side ? bbox.y1() : bbox.y0();
            const double sign = far_side ? 1.0 : -1.0;
            Path mark;
            mark.move_to(px, edge);
            mark.line_to(px, edge + sign * size_px);
            renderer.draw_path(gc, mark, Affine2D::identity());
            if (with_labels) {
                renderer.draw_text(label_gc, {px, edge + sign * (size_px + pad_px)},
                                   ticks.labels[i], font, 0.0, HAlign::center,
                                   far_side ? VAlign::bottom : VAlign::top);
            }
        } else {
            const double py = tf.apply(axes.scale_point({1.0, loc})).y;
            const double edge = far_side ? bbox.x1() : bbox.x0();
            const double sign = far_side ? 1.0 : -1.0;
            Path mark;
            mark.move_to(edge, py);
            mark.line_to(edge + sign * size_px, py);
            renderer.draw_path(gc, mark, Affine2D::identity());
            if (with_labels) {
                renderer.draw_text(label_gc, {edge + sign * (size_px + pad_px), py},
                                   ticks.labels[i], font, 0.0,
                                   far_side ? HAlign::left : HAlign::right, VAlign::center);
            }
        }
    }
    renderer.close_group();
}

} // namespace graphlib
