#include "graphlib/axes.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/rc.hpp"
#include "text/font_manager.hpp"

namespace graphlib {

Axes::Axes(Figure& figure, Bbox position_fraction) : position(position_fraction), figure_(&figure) {
    // rc is read at creation time, like matplotlib artists.
    title_.fontsize = rc().fontsize("axes.titlesize");
    title_.color = rc().color("text.color");
    title_.ha = HAlign::center;
    title_.va = VAlign::bottom;
    for (Text* label : {&xlabel_, &ylabel_}) {
        label->fontsize = rc().fontsize("axes.labelsize");
        label->color = rc().color("axes.labelcolor");
        label->ha = HAlign::center;
    }
    xlabel_.va = VAlign::top;
    ylabel_.va = VAlign::bottom;
    ylabel_.rotation_deg = 90.0;
    grid_on_ = rc().flag("axes.grid");
    margin_x_ = rc().number("axes.xmargin");
    margin_y_ = rc().number("axes.ymargin");
    cycle_ = rc().color_cycle();
}

Line2D& Axes::plot(std::span<const double> x, std::span<const double> y, std::string_view fmt,
                   const LineOpts& opts) {
    if (x.size() != y.size()) {
        throw ValueError("plot: x and y must have same first dimension, but have shapes (" +
                         std::to_string(x.size()) + ",) and (" + std::to_string(y.size()) + ",)");
    }
    const detail::ParsedFormat parsed = detail::parse_plot_format(fmt);

    // Defaulting rules from _process_plot_format: a fmt with neither linestyle
    // nor marker gets the rc linestyle; one with only a marker gets no line.
    std::string ls_token;
    if (!opts.linestyle.empty()) {
        ls_token = std::string(opts.linestyle); // explicit kwargs win over fmt (mpl)
    } else if (parsed.linestyle) {
        ls_token = *parsed.linestyle;
    } else {
        ls_token = parsed.marker ? "None" : "-"; // rc lines.linestyle
    }
    std::string marker_token;
    if (!opts.marker.empty()) {
        marker_token = std::string(opts.marker);
    } else if (parsed.marker) {
        marker_token = *parsed.marker;
    }

    std::string color_token;
    if (!opts.color.empty()) {
        color_token = std::string(opts.color);
    } else if (parsed.color) {
        color_token = *parsed.color;
    }
    const Color col = color_token.empty() ? next_cycle_color() : to_color(color_token);

    auto line = std::make_unique<Line2D>();
    line->axes = this;
    line->xdata.assign(x.begin(), x.end());
    line->ydata.assign(y.begin(), y.end());
    line->color = col;
    line->linewidth = opts.linewidth.value_or(rc().number("lines.linewidth"));
    line->linestyle = LineStyle::parse(ls_token);
    if (!marker_token.empty() && marker_token != "None") {
        line->marker = &get_marker(marker_token);
    }
    line->markersize = opts.markersize.value_or(rc().number("lines.markersize"));
    line->markerfacecolor =
        opts.markerfacecolor.empty() ? col : to_color(opts.markerfacecolor);
    line->markeredgecolor =
        opts.markeredgecolor.empty() ? col : to_color(opts.markeredgecolor);
    line->markeredgewidth = opts.markeredgewidth.value_or(rc().number("lines.markeredgewidth"));
    line->alpha = opts.alpha;
    line->zorder = opts.zorder.value_or(2.0);
    line->label = std::string(opts.label);

    add_line_datalim(*line);
    autoscale_view();

    Line2D& ref = *line;
    children_.push_back(std::move(line));
    return ref;
}

Line2D& Axes::plot(std::span<const double> y, std::string_view fmt, const LineOpts& opts) {
    std::vector<double> x(y.size());
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = static_cast<double>(i);
    }
    return plot(x, y, fmt, opts);
}

Text& Axes::text(double x, double y, std::string s, const TextOpts& opts) {
    auto t = std::make_unique<Text>();
    t->axes = this;
    t->coords = Text::Coords::data;
    t->position = {x, y};
    t->text = std::move(s);
    t->fontsize = opts.fontsize.value_or(rc().number("font.size"));
    t->color = opts.color.empty() ? rc().color("text.color") : to_color(opts.color);
    t->ha = opts.ha;
    t->va = opts.va;
    t->rotation_deg = opts.rotation;
    Text& ref = *t;
    children_.push_back(std::move(t));
    return ref;
}

PathCollection& Axes::scatter(std::span<const double> x, std::span<const double> y,
                              const ScatterOpts& opts) {
    if (x.size() != y.size()) {
        throw ValueError("scatter: x and y must be the same size");
    }
    if (!opts.s.empty() && opts.s.size() != 1 && opts.s.size() != x.size()) {
        throw ValueError("scatter: s must be a scalar or the same size as x");
    }
    auto pc = std::make_unique<PathCollection>();
    pc->axes = this;
    pc->xdata.assign(x.begin(), x.end());
    pc->ydata.assign(y.begin(), y.end());
    if (opts.s.empty()) {
        pc->sizes = {36.0}; // mpl default: lines.markersize^2
    } else {
        pc->sizes.assign(opts.s.begin(), opts.s.end());
    }
    const Color face = opts.c.empty() ? next_cycle_color() : to_color(opts.c);
    pc->facecolor = face;
    const std::string_view edge_spec =
        opts.edgecolors.empty() ? std::string_view(rc().str("scatter.edgecolors"))
                                : opts.edgecolors;
    pc->edgecolor = edge_spec == "face" ? face : to_color(edge_spec);
    pc->marker =
        &get_marker(opts.marker.empty() ? std::string_view(rc().str("scatter.marker"))
                                        : opts.marker);
    pc->linewidth = opts.linewidths.value_or(1.0);
    pc->alpha = opts.alpha;
    pc->zorder = opts.zorder.value_or(1.0);
    pc->label = std::string(opts.label);

    data_lim_.update(pc->data_extents());
    autoscale_view();

    PathCollection& ref = *pc;
    children_.push_back(std::move(pc));
    return ref;
}

Legend& Axes::legend(const LegendOpts& opts) {
    auto lg = std::make_unique<Legend>();
    lg->axes = this;
    lg->loc_code =
        Legend::parse_loc(opts.loc.empty() ? std::string_view(rc().str("legend.loc")) : opts.loc);
    lg->fontsize = opts.fontsize.value_or(rc().fontsize("legend.fontsize"));
    lg->frameon = opts.frameon.value_or(rc().flag("legend.frameon"));
    lg->framealpha = opts.framealpha.value_or(rc().number("legend.framealpha"));

    for (const auto& child : children_) {
        if (child->label.empty()) {
            continue;
        }
        Legend::Entry e;
        e.label = child->label;
        if (const auto* line = dynamic_cast<const Line2D*>(child.get())) {
            e.color = line->color;
            e.linestyle = line->linestyle;
            e.linewidth = line->linewidth;
            e.marker = line->marker;
            e.markerfacecolor = line->markerfacecolor;
            e.markeredgecolor = line->markeredgecolor;
            e.markersize = line->markersize;
            e.markeredgewidth = line->markeredgewidth;
        } else if (const auto* pc = dynamic_cast<const PathCollection*>(child.get())) {
            e.linestyle = LineStyle{LineStyle::Kind::none};
            e.marker = pc->marker;
            e.markerfacecolor = pc->facecolor;
            e.markeredgecolor = pc->edgecolor;
            e.markersize = std::sqrt(pc->sizes.front());
            e.markeredgewidth = pc->linewidth;
        } else if (const auto* patch = dynamic_cast<const Patch*>(child.get())) {
            e.linestyle = LineStyle{LineStyle::Kind::none};
            e.patch_swatch = true;
            e.patch_facecolor = patch->facecolor;
            e.patch_edgecolor = patch->edgecolor;
        } else {
            continue; // Text labels don't get legend entries (mpl behavior)
        }
        lg->entries.push_back(std::move(e));
    }
    legend_ = std::move(lg);
    return *legend_;
}

void Axes::collect_legend_avoidance(std::vector<std::vector<Point>>& lines_px,
                                    Size canvas) const {
    const Affine2D tf = trans_data(canvas);
    for (const auto& child : children_) {
        std::vector<Point> pts;
        if (const auto* line = dynamic_cast<const Line2D*>(child.get())) {
            pts.reserve(line->xdata.size());
            for (size_t i = 0; i < line->xdata.size(); ++i) {
                if (std::isfinite(line->xdata[i]) && std::isfinite(line->ydata[i])) {
                    pts.push_back(tf.apply({line->xdata[i], line->ydata[i]}));
                }
            }
        } else if (const auto* pc = dynamic_cast<const PathCollection*>(child.get())) {
            pts.reserve(pc->xdata.size());
            for (size_t i = 0; i < pc->xdata.size(); ++i) {
                pts.push_back(tf.apply({pc->xdata[i], pc->ydata[i]}));
            }
        }
        if (!pts.empty()) {
            lines_px.push_back(std::move(pts));
        }
    }
}

Patch& Axes::adopt_patch(std::unique_ptr<Patch> patch) {
    patch->axes = this;
    data_lim_.update(patch->data_extents());
    sticky_x_.insert(sticky_x_.end(), patch->sticky_x.begin(), patch->sticky_x.end());
    sticky_y_.insert(sticky_y_.end(), patch->sticky_y.begin(), patch->sticky_y.end());
    Patch& ref = *patch;
    children_.push_back(std::move(patch));
    autoscale_view();
    return ref;
}

Rectangle& Axes::add_patch(std::unique_ptr<Rectangle> patch) {
    return static_cast<Rectangle&>(adopt_patch(std::move(patch)));
}
Polygon& Axes::add_patch(std::unique_ptr<Polygon> patch) {
    return static_cast<Polygon&>(adopt_patch(std::move(patch)));
}
Wedge& Axes::add_patch(std::unique_ptr<Wedge> patch) {
    return static_cast<Wedge&>(adopt_patch(std::move(patch)));
}

std::vector<Rectangle*> Axes::bar(std::span<const double> x, std::span<const double> height,
                                  const BarOpts& opts) {
    if (x.size() != height.size()) {
        throw ValueError("bar: x and height must be the same size");
    }
    const double width = opts.width.value_or(0.8); // mpl default
    const Color face = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    // mpl bar edge default: fully transparent (oracle-checked)
    const Color edge = opts.edgecolor.empty() ? Color::none() : to_color(opts.edgecolor);

    std::vector<Rectangle*> bars;
    bars.reserve(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        auto r = std::make_unique<Rectangle>(x[i] - width / 2.0, opts.bottom, width,
                                             height[i]); // align='center'
        r->facecolor = face;
        r->edgecolor = edge;
        r->linewidth = opts.linewidth.value_or(1.0);
        r->alpha = opts.alpha;
        r->sticky_y = {opts.bottom}; // margins must not cross the baseline
        if (i == 0) {
            r->label = std::string(opts.label); // one legend entry per call
        }
        bars.push_back(&add_patch(std::move(r)));
    }
    return bars;
}

std::vector<Rectangle*> Axes::bar(const std::vector<std::string>& labels,
                                  std::span<const double> height, const BarOpts& opts) {
    std::vector<double> positions(labels.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] = static_cast<double>(i);
    }
    auto bars = bar(positions, height, opts);
    set_xticks(positions, labels); // categorical x, like mpl
    return bars;
}

std::vector<Rectangle*> Axes::barh(std::span<const double> y, std::span<const double> width,
                                   const BarOpts& opts) {
    if (y.size() != width.size()) {
        throw ValueError("barh: y and width must be the same size");
    }
    const double bar_height = opts.width.value_or(0.8);
    const Color face = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    const Color edge = opts.edgecolor.empty() ? Color::none() : to_color(opts.edgecolor);

    std::vector<Rectangle*> bars;
    bars.reserve(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        auto r = std::make_unique<Rectangle>(opts.bottom, y[i] - bar_height / 2.0, width[i],
                                             bar_height);
        r->facecolor = face;
        r->edgecolor = edge;
        r->linewidth = opts.linewidth.value_or(1.0);
        r->alpha = opts.alpha;
        r->sticky_x = {opts.bottom};
        if (i == 0) {
            r->label = std::string(opts.label);
        }
        bars.push_back(&add_patch(std::move(r)));
    }
    return bars;
}

std::vector<Rectangle*> Axes::hist(std::span<const double> data, const HistOpts& opts) {
    if (data.empty()) {
        return {};
    }
    // np.histogram semantics: `bins` equal-width bins over [min, max];
    // right-open intervals except the last, which is closed.
    std::vector<double> edges;
    if (!opts.edges.empty()) {
        edges.assign(opts.edges.begin(), opts.edges.end());
        if (edges.size() < 2) {
            throw ValueError("hist: need at least two bin edges");
        }
    } else {
        const int bins = opts.bins.value_or(static_cast<int>(rc().number("hist.bins")));
        if (bins < 1) {
            throw ValueError("hist: bins must be positive");
        }
        auto [mn_it, mx_it] = std::minmax_element(data.begin(), data.end());
        double lo = *mn_it;
        double hi = *mx_it;
        if (lo == hi) { // np.histogram expands a degenerate range by 0.5 each side
            lo -= 0.5;
            hi += 0.5;
        }
        edges.resize(static_cast<size_t>(bins) + 1);
        for (size_t i = 0; i < edges.size(); ++i) {
            edges[i] = lo + (hi - lo) * static_cast<double>(i) / bins;
        }
        edges.back() = hi; // exact, no accumulation error
    }
    std::vector<double> counts(edges.size() - 1, 0.0);
    for (const double v : data) {
        if (std::isnan(v) || v < edges.front() || v > edges.back()) {
            continue;
        }
        const auto it = std::upper_bound(edges.begin(), edges.end(), v);
        const size_t idx =
            std::min(counts.size() - 1,
                     static_cast<size_t>(std::max<std::ptrdiff_t>(
                         0, std::distance(edges.begin(), it) - 1)));
        counts[idx] += 1.0;
    }

    const Color face = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    std::vector<Rectangle*> bars;
    bars.reserve(counts.size());
    for (size_t i = 0; i < counts.size(); ++i) {
        auto r = std::make_unique<Rectangle>(edges[i], 0.0, edges[i + 1] - edges[i], counts[i]);
        r->facecolor = face;
        r->edgecolor = Color::none();
        r->alpha = opts.alpha;
        r->sticky_y = {0.0};
        if (i == 0) {
            r->label = std::string(opts.label);
        }
        bars.push_back(&add_patch(std::move(r)));
    }
    return bars;
}

Polygon& Axes::fill_between(std::span<const double> x, std::span<const double> y1,
                            std::span<const double> y2, const FillOpts& opts) {
    if (x.size() != y1.size() || (!y2.empty() && y2.size() != x.size())) {
        throw ValueError("fill_between: x, y1 and y2 must be the same size");
    }
    std::vector<Point> pts;
    pts.reserve(2 * x.size());
    for (size_t i = 0; i < x.size(); ++i) { // forward along y1
        pts.push_back({x[i], y1[i]});
    }
    for (size_t i = x.size(); i-- > 0;) { // back along y2 (or the baseline)
        pts.push_back({x[i], y2.empty() ? 0.0 : y2[i]});
    }
    auto poly = std::make_unique<Polygon>(std::move(pts));
    poly->facecolor = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    poly->edgecolor = Color::none(); // mpl fill_between draws no edge by default
    poly->alpha = opts.alpha;
    poly->label = std::string(opts.label);
    return add_patch(std::move(poly));
}

Polygon& Axes::fill_between(std::span<const double> x, std::span<const double> y1,
                            const FillOpts& opts) {
    return fill_between(x, y1, {}, opts);
}

Line2D& Axes::step(std::span<const double> x, std::span<const double> y, std::string_view fmt,
                   const LineOpts& opts) {
    Line2D& line = plot(x, y, fmt, opts);
    line.drawstyle = DrawStyle::steps_pre; // mpl step(where='pre')
    return line;
}

// Fraction-coordinate artists must not contribute their fraction axis to the
// data limits — rebuild dataLim from the pure-data artists.
void Axes::recompute_data_lim() {
    data_lim_ = Bbox::null();
    for (const auto& child : children_) {
        if (const auto* l = dynamic_cast<const Line2D*>(child.get())) {
            if (!l->x_axes_fraction && !l->y_axes_fraction) {
                data_lim_.update(l->data_extents());
            }
        } else if (const auto* p = dynamic_cast<const Patch*>(child.get())) {
            if (!p->x_axes_fraction && !p->y_axes_fraction) {
                data_lim_.update(p->data_extents());
            }
        } else if (const auto* pc = dynamic_cast<const PathCollection*>(child.get())) {
            data_lim_.update(pc->data_extents());
        }
    }
}

Line2D& Axes::axhline(double y, double xmin, double xmax, const LineOpts& opts) {
    Line2D& line = plot(std::vector<double>{xmin, xmax}, std::vector<double>{y, y}, "", opts);
    line.x_axes_fraction = true;
    recompute_data_lim();
    data_lim_.update({data_lim_.is_null() ? 0.0 : data_lim_.x0(), y}); // y joins the limits
    autoscale_view();
    return line;
}

Line2D& Axes::axvline(double x, double ymin, double ymax, const LineOpts& opts) {
    Line2D& line = plot(std::vector<double>{x, x}, std::vector<double>{ymin, ymax}, "", opts);
    line.y_axes_fraction = true;
    recompute_data_lim();
    data_lim_.update({x, data_lim_.is_null() ? 0.0 : data_lim_.y0()});
    autoscale_view();
    return line;
}

Rectangle& Axes::axhspan(double ymin, double ymax, const FillOpts& opts) {
    auto r = std::make_unique<Rectangle>(0.0, ymin, 1.0, ymax - ymin);
    r->x_axes_fraction = true;
    r->facecolor = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    r->edgecolor = Color::none();
    r->alpha = opts.alpha;
    r->label = std::string(opts.label);
    r->axes = this;
    // Only the y extent joins the data limits.
    Rectangle& ref = *r;
    children_.push_back(std::move(r));
    if (!data_lim_.is_null()) {
        data_lim_.update({data_lim_.x0(), ymin});
        data_lim_.update({data_lim_.x0(), ymax});
        autoscale_view();
    }
    return ref;
}

Rectangle& Axes::axvspan(double xmin, double xmax, const FillOpts& opts) {
    auto r = std::make_unique<Rectangle>(xmin, 0.0, xmax - xmin, 1.0);
    r->y_axes_fraction = true;
    r->facecolor = opts.color.empty() ? next_cycle_color() : to_color(opts.color);
    r->edgecolor = Color::none();
    r->alpha = opts.alpha;
    r->label = std::string(opts.label);
    r->axes = this;
    Rectangle& ref = *r;
    children_.push_back(std::move(r));
    if (!data_lim_.is_null()) {
        data_lim_.update({xmin, data_lim_.y0()});
        data_lim_.update({xmax, data_lim_.y0()});
        autoscale_view();
    }
    return ref;
}

Line2D& Axes::hlines(std::span<const double> y, double xmin, double xmax,
                     const LineOpts& opts) {
    // One artist: NaN-gapped segments (mpl uses a LineCollection).
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> xs;
    std::vector<double> ys;
    for (const double v : y) {
        xs.insert(xs.end(), {xmin, xmax, nan});
        ys.insert(ys.end(), {v, v, nan});
    }
    return plot(xs, ys, "", opts);
}

Line2D& Axes::vlines(std::span<const double> x, double ymin, double ymax,
                     const LineOpts& opts) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> xs;
    std::vector<double> ys;
    for (const double v : x) {
        xs.insert(xs.end(), {v, v, nan});
        ys.insert(ys.end(), {ymin, ymax, nan});
    }
    return plot(xs, ys, "", opts);
}

void Axes::set_xticks(std::vector<double> locs, std::vector<std::string> labels) {
    if (!labels.empty() && labels.size() != locs.size()) {
        throw ValueError("set_xticks: labels must match the number of locations");
    }
    xaxis_.set_major_locator(std::make_unique<FixedLocator>(std::move(locs)));
    if (!labels.empty()) {
        xaxis_.set_major_formatter(std::make_unique<FixedFormatter>(std::move(labels)));
    }
}

void Axes::set_yticks(std::vector<double> locs, std::vector<std::string> labels) {
    if (!labels.empty() && labels.size() != locs.size()) {
        throw ValueError("set_yticks: labels must match the number of locations");
    }
    yaxis_.set_major_locator(std::make_unique<FixedLocator>(std::move(locs)));
    if (!labels.empty()) {
        yaxis_.set_major_formatter(std::make_unique<FixedFormatter>(std::move(labels)));
    }
}

void Axes::set_xlim(double left, double right) {
    if (left == right) {
        // mpl warns and lets the transform degenerate; we expand like the
        // locator would. TODO(v0.3): warning infrastructure (with rc system).
        std::tie(left, right) = detail::nonsingular(left, right, 0.05);
    }
    vx0_ = left;
    vx1_ = right;
    autoscale_x_ = false;
}

void Axes::set_ylim(double bottom, double top) {
    if (bottom == top) {
        std::tie(bottom, top) = detail::nonsingular(bottom, top, 0.05);
    }
    vy0_ = bottom;
    vy1_ = top;
    autoscale_y_ = false;
}

Color Axes::next_cycle_color() { return cycle_[cycle_index_++ % cycle_.size()]; }

void Axes::add_line_datalim(const Line2D& line) { data_lim_.update(line.data_extents()); }

// Port of autoscale_view's per-axis handling (autolimit_mode='data'):
// locator.nonsingular, then symmetric margins clamped at sticky edges
// (handle_single_axis in mpl — margins must not cross a sticky value).
void Axes::autoscale_view() {
    if (data_lim_.is_null()) {
        return; // no data: keep the (0, 1) defaults, like mpl
    }
    const auto apply = [](double lo, double hi, double margin, std::vector<double> stickies,
                          const Locator& locator) {
        std::tie(lo, hi) = locator.nonsingular(lo, hi);
        std::sort(stickies.begin(), stickies.end());
        const double tol = 1e-5 * std::abs(hi - lo);
        std::optional<double> lo_bound;
        std::optional<double> hi_bound;
        for (const double s : stickies) {
            if (s <= lo + tol) {
                lo_bound = s; // largest sticky not above lo
            }
            if (!hi_bound && s >= hi - tol) {
                hi_bound = s; // smallest sticky not below hi
            }
        }
        const double delta = (hi - lo) * margin;
        double out_lo = lo - delta;
        double out_hi = hi + delta;
        if (lo_bound) {
            out_lo = std::max(out_lo, *lo_bound);
        }
        if (hi_bound) {
            out_hi = std::min(out_hi, *hi_bound);
        }
        return std::pair{out_lo, out_hi};
    };
    if (autoscale_x_) {
        std::tie(vx0_, vx1_) =
            apply(data_lim_.x0(), data_lim_.x1(), margin_x_, sticky_x_, xaxis_.major_locator());
    }
    if (autoscale_y_) {
        std::tie(vy0_, vy1_) =
            apply(data_lim_.y0(), data_lim_.y1(), margin_y_, sticky_y_, yaxis_.major_locator());
    }
}

Bbox Axes::bbox_pixels(Size canvas) const {
    return Bbox::from_extents(position.x0() * canvas.width, position.y0() * canvas.height,
                              position.x1() * canvas.width, position.y1() * canvas.height);
}

Affine2D Axes::trans_data(Size canvas) const {
    const Bbox view = Bbox::from_extents(vx0_, vy0_, vx1_, vy1_);
    return bbox_transform_from(view).then(bbox_transform_to(bbox_pixels(canvas)));
}

Affine2D Axes::blended_transform(bool x_fraction, bool y_fraction, Size canvas) const {
    const Affine2D data = trans_data(canvas);
    const Affine2D axes = bbox_transform_to(bbox_pixels(canvas)); // unit -> px
    // Both transforms are axis-aligned (no shear): blend the diagonals.
    const Affine2D& fx = x_fraction ? axes : data;
    const Affine2D& fy = y_fraction ? axes : data;
    return {fx.a(), 0.0, 0.0, fy.d(), fx.e(), fy.f()};
}

void Axes::draw(Renderer& renderer) {
    renderer.open_group("axes");
    const Size canvas = renderer.canvas_size();
    const Bbox bpx = bbox_pixels(canvas);
    const double ppt = renderer.points_to_pixels(1.0);

    auto rect_path = [&bpx] {
        Path p;
        p.move_to(bpx.x0(), bpx.y0());
        p.line_to(bpx.x1(), bpx.y0());
        p.line_to(bpx.x1(), bpx.y1());
        p.line_to(bpx.x0(), bpx.y1());
        p.close_subpath();
        return p;
    }();

    // 1. axes patch (edge drawn by the spines)
    if (!figure_->transparent_render()) {
        GraphicsContext patch_gc;
        patch_gc.color.a = 0; // no stroke
        renderer.draw_path(patch_gc, rect_path, Affine2D::identity(),
                           rc().color("axes.facecolor"));
    }

    // Ticks are computed at draw time from the current view (DESIGN §3).
    const Axis::TickData xticks = xaxis_.compute_ticks(*this);
    const Axis::TickData yticks = yaxis_.compute_ticks(*this);
    const Affine2D tf = trans_data(canvas);

    // 2. grid, below the data (rc axes.axisbelow 'line')
    if (grid_on_) {
        GraphicsContext grid_gc;
        grid_gc.color = rc().color("grid.color");
        grid_gc.color.a *= rc().number("grid.alpha");
        grid_gc.linewidth = rc().number("grid.linewidth");
        renderer.open_group("grid");
        for (const double loc : xticks.locs) {
            const double px = tf.apply({loc, 0}).x;
            Path p;
            p.move_to(px, bpx.y0());
            p.line_to(px, bpx.y1());
            renderer.draw_path(grid_gc, p, Affine2D::identity());
        }
        for (const double loc : yticks.locs) {
            const double py = tf.apply({0, loc}).y;
            Path p;
            p.move_to(bpx.x0(), py);
            p.line_to(bpx.x1(), py);
            renderer.draw_path(grid_gc, p, Affine2D::identity());
        }
        renderer.close_group();
    }

    // 3. children, stable-sorted by zorder
    std::vector<Artist*> order;
    order.reserve(children_.size());
    for (const auto& c : children_) {
        order.push_back(c.get());
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const Artist* a, const Artist* b) { return a->zorder < b->zorder; });
    for (Artist* artist : order) {
        artist->draw(renderer);
    }

    // 4. spines
    {
        GraphicsContext spine_gc;
        spine_gc.color = rc().color("axes.edgecolor");
        spine_gc.linewidth = rc().number("axes.linewidth");
        spine_gc.capstyle = CapStyle::projecting;
        renderer.open_group("spines");
        renderer.draw_path(spine_gc, rect_path, Affine2D::identity());
        renderer.close_group();
    }

    // 5. ticks + tick labels, then the legend on top (zorder 5)
    xaxis_.draw_ticks(renderer, *this, xticks);
    yaxis_.draw_ticks(renderer, *this, yticks);
    if (legend_) {
        legend_->draw(renderer);
    }

    // 6. axis labels + title, laid out with real FontManager metrics
    const auto& fm = detail::FontManager::instance();
    const double cx = (bpx.x0() + bpx.x1()) / 2.0;
    const double cy = (bpx.y0() + bpx.y1()) / 2.0;
    const double tick_out =
        (rc().number("xtick.major.size") + rc().number("xtick.major.pad")) * ppt;
    const double tick_label_em = rc().fontsize("xtick.labelsize") * ppt;
    const double labelpad = rc().number("axes.labelpad") * ppt;
    if (!xlabel_.text.empty()) {
        // below the x tick label block (ascent+descent), va=top
        const double tick_label_h =
            fm.ascent(tick_label_em) + fm.descent(tick_label_em);
        xlabel_.position = {cx, bpx.y0() - tick_out - tick_label_h - labelpad};
        xlabel_.draw(renderer);
    }
    if (!ylabel_.text.empty()) {
        double labels_width = 0.0;
        for (const auto& l : yticks.labels) {
            labels_width = std::max(labels_width, fm.text_extent(l, tick_label_em).width);
        }
        ylabel_.position = {bpx.x0() - tick_out - labels_width - labelpad, cy};
        ylabel_.draw(renderer);
    }
    if (!title_.text.empty()) {
        title_.position = {cx, bpx.y1() + rc().number("axes.titlepad") * ppt};
        title_.draw(renderer);
    }
    renderer.close_group();
}

} // namespace graphlib
