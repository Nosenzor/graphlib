#include "graphlib/axes.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/rc.hpp"
#include "core/warn.hpp"
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
    if (!opts.c_array.empty()) { // mpl's c= value array through cmap/norm
        if (opts.c_array.size() != x.size()) {
            throw ValueError("scatter: c must be the same size as x");
        }
        const Colormap& cmap = get_cmap(opts.cmap.empty() ? "viridis" : opts.cmap);
        auto [mn, mx] = std::minmax_element(opts.c_array.begin(), opts.c_array.end());
        const Normalize norm{opts.vmin.value_or(*mn), opts.vmax.value_or(*mx)};
        pc->facecolors.reserve(opts.c_array.size());
        for (const double v : opts.c_array) {
            pc->facecolors.push_back(cmap(std::isnan(v) ? v : norm(v)));
        }
        pc->facecolor = pc->facecolors.front(); // legend swatch color
    }
    const std::string_view edge_spec =
        opts.edgecolors.empty() ? std::string_view(rc().str("scatter.edgecolors"))
                                : opts.edgecolors;
    pc->edge_follows_face = edge_spec == "face";
    pc->edgecolor = pc->edge_follows_face ? face : to_color(edge_spec);
    pc->marker =
        &get_marker(opts.marker.empty() ? std::string_view(rc().str("scatter.marker"))
                                        : opts.marker);
    pc->linewidth = opts.linewidths.value_or(1.0);
    pc->alpha = opts.alpha;
    pc->zorder = opts.zorder.value_or(1.0);
    pc->label = std::string(opts.label);

    data_lim_.update(pc->data_extents());
    for (size_t i = 0; i < pc->xdata.size(); ++i) {
        track_minpos({{Point{pc->xdata[i], pc->ydata[i]}}});
    }
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
                    pts.push_back(tf.apply(scale_point({line->xdata[i], line->ydata[i]})));
                }
            }
        } else if (const auto* pc = dynamic_cast<const PathCollection*>(child.get())) {
            pts.reserve(pc->xdata.size());
            for (size_t i = 0; i < pc->xdata.size(); ++i) {
                pts.push_back(tf.apply(scale_point({pc->xdata[i], pc->ydata[i]})));
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
    track_minpos(patch->get_path().vertices());
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
        } else if (const auto* im = dynamic_cast<const AxesImage*>(child.get())) {
            data_lim_.update(im->data_extents());
        } else if (const auto* mesh = dynamic_cast<const QuadMesh*>(child.get())) {
            data_lim_.update(mesh->data_extents());
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

Line2D& Axes::errorbar(std::span<const double> x, std::span<const double> y,
                       const ErrorbarOpts& opts) {
    if (x.size() != y.size()) {
        throw ValueError("errorbar: x and y must be the same size");
    }
    auto err_at = [](std::span<const double> err, size_t i) {
        return err.size() == 1 ? err[0] : err[i];
    };
    for (auto [err, name] : {std::pair{opts.yerr, "yerr"}, std::pair{opts.xerr, "xerr"}}) {
        if (!err.empty() && err.size() != 1 && err.size() != x.size()) {
            throw ValueError(std::string("errorbar: ") + name +
                             " must be a scalar or match the data size");
        }
    }

    // Data line first: it consumes the cycle color the bars will inherit.
    Line2D& line = plot(x, y, opts.fmt,
                        LineOpts{.color = opts.color,
                                 .linewidth = opts.linewidth,
                                 .alpha = opts.alpha,
                                 .label = opts.label});
    const Color ecolor = opts.ecolor.empty() ? line.color : to_color(opts.ecolor);
    const double elw = opts.elinewidth.value_or(line.linewidth);
    const double capsize = opts.capsize.value_or(rc().number("errorbar.capsize"));
    const double nan = std::numeric_limits<double>::quiet_NaN();

    auto add_bars = [&](bool vertical, std::span<const double> err) {
        if (err.empty()) {
            return;
        }
        std::vector<double> sx;
        std::vector<double> sy;
        std::vector<double> cap_x0;
        std::vector<double> cap_y0;
        std::vector<double> cap_x1;
        std::vector<double> cap_y1;
        for (size_t i = 0; i < x.size(); ++i) {
            const double e = err_at(err, i);
            const double lo = (vertical ? y[i] : x[i]) - e;
            const double hi = (vertical ? y[i] : x[i]) + e;
            if (vertical) {
                sx.insert(sx.end(), {x[i], x[i], nan});
                sy.insert(sy.end(), {lo, hi, nan});
                cap_x0.push_back(x[i]);
                cap_y0.push_back(lo);
                cap_x1.push_back(x[i]);
                cap_y1.push_back(hi);
            } else {
                sx.insert(sx.end(), {lo, hi, nan});
                sy.insert(sy.end(), {y[i], y[i], nan});
                cap_x0.push_back(lo);
                cap_y0.push_back(y[i]);
                cap_x1.push_back(hi);
                cap_y1.push_back(y[i]);
            }
        }
        auto& bars = plot(sx, sy, "",
                          LineOpts{.linewidth = elw, .alpha = opts.alpha});
        bars.color = ecolor;
        if (capsize > 0) { // mpl caps are the '_' / '|' markers, 2*capsize points wide
            for (auto [cx, cy] : {std::pair{&cap_x0, &cap_y0}, std::pair{&cap_x1, &cap_y1}}) {
                auto& caps = plot(*cx, *cy, vertical ? "_" : "|",
                                  LineOpts{.markersize = 2.0 * capsize,
                                           .markeredgewidth = elw,
                                           .alpha = opts.alpha});
                caps.markeredgecolor = ecolor;
                caps.color = ecolor;
            }
        }
    };
    add_bars(true, opts.yerr);
    add_bars(false, opts.xerr);
    return line;
}

std::vector<Wedge*> Axes::pie(std::span<const double> sizes, const PieOpts& opts) {
    if (sizes.empty()) {
        throw ValueError("pie: sizes must be non-empty");
    }
    double total = 0.0;
    for (const double s : sizes) {
        if (s < 0) {
            throw ValueError("pie: sizes must be non-negative");
        }
        total += s;
    }
    if (total <= 0) {
        throw ValueError("pie: sizes sum to zero");
    }
    if (!opts.labels.empty() && opts.labels.size() != sizes.size()) {
        throw ValueError("pie: labels must match sizes");
    }
    const double r = opts.radius.value_or(1.0);
    std::vector<Wedge*> wedges;
    double theta = opts.startangle;
    for (size_t i = 0; i < sizes.size(); ++i) {
        const double sweep = 360.0 * sizes[i] / total;
        auto w = std::make_unique<Wedge>(Point{0.0, 0.0}, r, theta, theta + sweep);
        w->facecolor = opts.colors.empty() ? next_cycle_color()
                                           : to_color(opts.colors[i % opts.colors.size()]);
        w->edgecolor = Color::none();
        Wedge& ref = add_patch(std::move(w));
        wedges.push_back(&ref);
        if (!opts.labels.empty()) {
            const double mid = (theta + sweep / 2.0) * std::numbers::pi / 180.0;
            const double lx = 1.1 * r * std::cos(mid); // mpl labeldistance = 1.1
            const double ly = 1.1 * r * std::sin(mid);
            Text& t = text(lx, ly, opts.labels[i]);
            t.ha = lx > 0 ? HAlign::left : HAlign::right;
            t.va = VAlign::center;
        }
        theta += sweep;
    }
    // mpl pie: frameless equal-aspect axes with fixed limits.
    set_axis_off();
    set_aspect_equal();
    set_xlim(-1.25 * r, 1.25 * r);
    set_ylim(-1.25 * r, 1.25 * r);
    return wedges;
}

namespace {
Axes::Scale parse_scale(std::string_view scale) {
    if (scale == "linear") {
        return Axes::Scale::linear;
    }
    if (scale == "log") {
        return Axes::Scale::log;
    }
    throw ValueError("scale must be 'linear' or 'log' (symlog/logit: icebox)");
}
} // namespace

void Axes::pan(double dx_px, double dy_px, Size canvas) {
    const Bbox box = bbox_pixels(canvas);
    if (box.width() <= 0 || box.height() <= 0) {
        return;
    }
    // Convert the pixel delta into scale-space spans, shift, and map back —
    // log axes therefore pan multiplicatively, like mpl.
    const Point lo_t = scale_point({vx0_, vy0_});
    const Point hi_t = scale_point({vx1_, vy1_});
    const double dx_t = -dx_px * (hi_t.x - lo_t.x) / box.width(); // drag right moves data right
    const double dy_t = -dy_px * (hi_t.y - lo_t.y) / box.height();
    const Point new_lo = inverse_scale_point({lo_t.x + dx_t, lo_t.y + dy_t});
    const Point new_hi = inverse_scale_point({hi_t.x + dx_t, hi_t.y + dy_t});
    set_xlim(new_lo.x, new_hi.x); // share groups follow
    set_ylim(new_lo.y, new_hi.y);
}

void Axes::zoom_at(double factor, Point center_px, Size canvas) {
    if (factor <= 0) {
        return;
    }
    const Affine2D inv = trans_data(canvas).inverted();
    const Point center_t = inv.apply(center_px); // scale-space anchor
    const Point lo_t = scale_point({vx0_, vy0_});
    const Point hi_t = scale_point({vx1_, vy1_});
    const double s = 1.0 / factor; // factor > 1 shrinks the span (zoom in)
    const Point new_lo = inverse_scale_point(
        {center_t.x + (lo_t.x - center_t.x) * s, center_t.y + (lo_t.y - center_t.y) * s});
    const Point new_hi = inverse_scale_point(
        {center_t.x + (hi_t.x - center_t.x) * s, center_t.y + (hi_t.y - center_t.y) * s});
    set_xlim(new_lo.x, new_hi.x);
    set_ylim(new_lo.y, new_hi.y);
}

void Axes::save_home() {
    home_view_ = {vx0_, vx1_, vy0_, vy1_};
    home_autoscale_x_ = autoscale_x_;
    home_autoscale_y_ = autoscale_y_;
}

void Axes::restore_home() {
    if (!home_view_) {
        return;
    }
    for (Axes* ax : share_x_ ? share_x_->members : std::vector<Axes*>{this}) {
        ax->vx0_ = (*home_view_)[0];
        ax->vx1_ = (*home_view_)[1];
    }
    for (Axes* ax : share_y_ ? share_y_->members : std::vector<Axes*>{this}) {
        ax->vy0_ = (*home_view_)[2];
        ax->vy1_ = (*home_view_)[3];
    }
    autoscale_x_ = home_autoscale_x_;
    autoscale_y_ = home_autoscale_y_;
}

void Axes::set_xscale(std::string_view scale) {
    xscale_ = parse_scale(scale);
    if (xscale_ == Scale::log) {
        xaxis_.set_major_locator(std::make_unique<LogLocator>());
        xaxis_.set_major_formatter(std::make_unique<LogFormatter>());
        xaxis_.set_minor_locator(std::make_unique<LogLocator>(/*minor_subs=*/true));
    } else {
        xaxis_.set_major_locator(std::make_unique<MaxNLocator>());
        xaxis_.set_major_formatter(std::make_unique<ScalarFormatter>());
        xaxis_.set_minor_locator(nullptr);
    }
    autoscale_view();
}

void Axes::set_yscale(std::string_view scale) {
    yscale_ = parse_scale(scale);
    if (yscale_ == Scale::log) {
        yaxis_.set_major_locator(std::make_unique<LogLocator>());
        yaxis_.set_major_formatter(std::make_unique<LogFormatter>());
        yaxis_.set_minor_locator(std::make_unique<LogLocator>(/*minor_subs=*/true));
    } else {
        yaxis_.set_major_locator(std::make_unique<MaxNLocator>());
        yaxis_.set_major_formatter(std::make_unique<ScalarFormatter>());
        yaxis_.set_minor_locator(nullptr);
    }
    autoscale_view();
}

void Axes::minorticks_on() {
    if (xscale_ == Scale::linear) {
        xaxis_.set_minor_locator(std::make_unique<AutoMinorLocator>(&xaxis_.major_locator()));
    }
    if (yscale_ == Scale::linear) {
        yaxis_.set_minor_locator(std::make_unique<AutoMinorLocator>(&yaxis_.major_locator()));
    }
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
        detail::warn("Attempting to set identical low and high xlims makes transformation "
                     "singular; automatically expanding.");
        std::tie(left, right) = detail::nonsingular(left, right, 0.05);
    }
    for (Axes* ax : share_x_ ? share_x_->members : std::vector<Axes*>{this}) {
        ax->vx0_ = left;
        ax->vx1_ = right;
        ax->autoscale_x_ = false;
    }
}

void Axes::set_ylim(double bottom, double top) {
    if (bottom == top) {
        detail::warn("Attempting to set identical low and high ylims makes transformation "
                     "singular; automatically expanding.");
        std::tie(bottom, top) = detail::nonsingular(bottom, top, 0.05);
    }
    for (Axes* ax : share_y_ ? share_y_->members : std::vector<Axes*>{this}) {
        ax->vy0_ = bottom;
        ax->vy1_ = top;
        ax->autoscale_y_ = false;
    }
}

void Axes::join_share_x(const std::shared_ptr<ShareGroup>& group) {
    share_x_ = group;
    if (std::find(group->members.begin(), group->members.end(), this) ==
        group->members.end()) {
        group->members.push_back(this);
    }
}

void Axes::join_share_y(const std::shared_ptr<ShareGroup>& group) {
    share_y_ = group;
    if (std::find(group->members.begin(), group->members.end(), this) ==
        group->members.end()) {
        group->members.push_back(this);
    }
}

Axes* Axes::share_host() const {
    for (const auto* group : {share_x_.get(), share_y_.get()}) {
        if (group == nullptr) {
            continue;
        }
        for (Axes* ax : group->members) {
            if (ax != this && !ax->is_twin()) {
                return ax;
            }
        }
    }
    return nullptr;
}

Axes& Axes::twinx() {
    Axes& twin = figure_->add_axes(position);
    twin.outer_cell = outer_cell;
    twin.patch_off_ = true;   // the host's background stays visible
    twin.show_x_axis_ = false; // the host draws the shared x axis
    twin.yaxis_right_ = true;
    twin.grid_on_ = false;
    auto group = share_x_ ? share_x_ : std::make_shared<ShareGroup>();
    join_share_x(group);
    twin.join_share_x(group);
    twin.vx0_ = vx0_;
    twin.vx1_ = vx1_;
    twin.autoscale_x_ = autoscale_x_;
    return twin;
}

Axes& Axes::twiny() {
    Axes& twin = figure_->add_axes(position);
    twin.outer_cell = outer_cell;
    twin.patch_off_ = true;
    twin.show_y_axis_ = false;
    twin.xaxis_top_ = true;
    twin.grid_on_ = false;
    auto group = share_y_ ? share_y_ : std::make_shared<ShareGroup>();
    join_share_y(group);
    twin.join_share_y(group);
    twin.vy0_ = vy0_;
    twin.vy1_ = vy1_;
    twin.autoscale_y_ = autoscale_y_;
    return twin;
}

Color Axes::next_cycle_color() { return cycle_[cycle_index_++ % cycle_.size()]; }

void Axes::track_minpos(std::span<const Point> pts) {
    for (const Point& p : pts) {
        if (p.x > 0 && p.x < minpos_x_) {
            minpos_x_ = p.x;
        }
        if (p.y > 0 && p.y < minpos_y_) {
            minpos_y_ = p.y;
        }
    }
}

void Axes::add_line_datalim(const Line2D& line) {
    data_lim_.update(line.data_extents());
    for (size_t i = 0; i < line.xdata.size(); ++i) {
        track_minpos({{Point{line.xdata[i], line.ydata[i]}}});
    }
}

// Port of autoscale_view's per-axis handling (autolimit_mode='data'):
// locator.nonsingular, then symmetric margins clamped at sticky edges
// (handle_single_axis in mpl — margins must not cross a sticky value).
void Axes::autoscale_view() {
    // Shared axes autoscale on the union of the group's data limits — per
    // dimension only (a twinx shares x but must keep its own y span).
    double dx0 = data_lim_.x0();
    double dx1 = data_lim_.x1();
    double dy0 = data_lim_.y0();
    double dy1 = data_lim_.y1();
    if (share_x_) {
        for (const Axes* ax : share_x_->members) { // null bboxes are ±inf: min/max no-ops
            dx0 = std::min(dx0, ax->data_lim_.x0());
            dx1 = std::max(dx1, ax->data_lim_.x1());
        }
    }
    if (share_y_) {
        for (const Axes* ax : share_y_->members) {
            dy0 = std::min(dy0, ax->data_lim_.y0());
            dy1 = std::max(dy1, ax->data_lim_.y1());
        }
    }
    const auto apply = [](double lo, double hi, double margin, std::vector<double> stickies,
                          const Locator& locator, Scale scale, double minpos) {
        if (scale == Scale::log) {
            // Port of the log branch: clip to positive data, expand in
            // transformed (log) space.
            if (lo <= 0) {
                lo = std::isfinite(minpos) ? minpos : 1.0;
            }
            if (hi <= 0) {
                hi = 10.0 * lo;
            }
        }
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
        double out_lo = lo;
        double out_hi = hi;
        if (scale == Scale::log) { // margins in transformed space (mpl)
            const double lt = std::log10(lo);
            const double ht = std::log10(hi);
            const double delta = (ht - lt) * margin;
            out_lo = std::pow(10.0, lt - delta);
            out_hi = std::pow(10.0, ht + delta);
        } else {
            const double delta = (hi - lo) * margin;
            out_lo = lo - delta;
            out_hi = hi + delta;
        }
        // Margins must not cross a sticky value (either scale).
        if (lo_bound) {
            out_lo = std::max(out_lo, *lo_bound);
        }
        if (hi_bound) {
            out_hi = std::min(out_hi, *hi_bound);
        }
        return std::pair{out_lo, out_hi};
    };
    if (autoscale_x_ && dx0 <= dx1) {
        std::tie(vx0_, vx1_) = apply(dx0, dx1, margin_x_, sticky_x_, xaxis_.major_locator(),
                                     xscale_, minpos_x_);
        if (share_x_) { // members stay in lockstep without recomputing
            for (Axes* ax : share_x_->members) {
                ax->vx0_ = vx0_;
                ax->vx1_ = vx1_;
            }
        }
    }
    if (autoscale_y_ && dy0 <= dy1) {
        std::tie(vy0_, vy1_) = apply(dy0, dy1, margin_y_, sticky_y_, yaxis_.major_locator(),
                                     yscale_, minpos_y_);
        if (share_y_) {
            for (Axes* ax : share_y_->members) {
                ax->vy0_ = vy0_;
                ax->vy1_ = vy1_;
            }
        }
    }
}

Bbox Axes::bbox_pixels(Size canvas) const {
    Bbox box = Bbox::from_extents(position.x0() * canvas.width, position.y0() * canvas.height,
                                  position.x1() * canvas.width, position.y1() * canvas.height);
    if (aspect_) { // adjustable='box': shrink one dimension, centered (mpl)
        const double x_span = std::abs(vx1_ - vx0_);
        const double y_span = std::abs(vy1_ - vy0_);
        if (x_span > 0 && y_span > 0) {
            const double needed_hw = *aspect_ * y_span / x_span; // required h/w in px
            const double cx = (box.x0() + box.x1()) / 2.0;
            const double cy = (box.y0() + box.y1()) / 2.0;
            double w = box.width();
            double h = box.height();
            if (h / w > needed_hw) {
                h = w * needed_hw;
            } else {
                w = h / needed_hw;
            }
            box = Bbox::from_extents(cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0);
        }
    }
    return box;
}

QuadMesh& Axes::pcolormesh(std::span<const double> x_edges, std::span<const double> y_edges,
                           std::span<const double> values, const PcolorOpts& opts) {
    if (x_edges.size() < 2 || y_edges.size() < 2 ||
        values.size() != (x_edges.size() - 1) * (y_edges.size() - 1)) {
        throw ValueError("pcolormesh: values must be (len(y)-1) x (len(x)-1)");
    }
    auto mesh = std::make_unique<QuadMesh>();
    mesh->axes = this;
    mesh->x_edges.assign(x_edges.begin(), x_edges.end());
    mesh->y_edges.assign(y_edges.begin(), y_edges.end());
    mesh->values.assign(values.begin(), values.end());
    mesh->cmap = &get_cmap(opts.cmap.empty() ? "viridis" : opts.cmap); // rc image.cmap
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const double v : values) {
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    mesh->vmin = opts.vmin.value_or(std::isfinite(lo) ? lo : 0.0);
    mesh->vmax = opts.vmax.value_or(std::isfinite(hi) ? hi : 1.0);
    if (mesh->vmax == mesh->vmin) {
        mesh->vmax = mesh->vmin + 1.0;
    }
    mesh->alpha = opts.alpha;

    // Sticky edges on all four sides: pcolormesh sits tight, like mpl.
    data_lim_.update(mesh->data_extents());
    track_minpos({{Point{mesh->x_edges.front(), mesh->y_edges.front()}}});
    track_minpos({{Point{mesh->x_edges.back(), mesh->y_edges.back()}}});
    sticky_x_.insert(sticky_x_.end(), {mesh->x_edges.front(), mesh->x_edges.back()});
    sticky_y_.insert(sticky_y_.end(), {mesh->y_edges.front(), mesh->y_edges.back()});
    QuadMesh& ref = *mesh;
    children_.push_back(std::move(mesh));
    autoscale_view();
    return ref;
}

namespace {
ContourSet& make_contour(Axes& axes, std::span<const double> x, std::span<const double> y,
                         std::span<const double> z, const ContourOpts& opts, bool filled,
                         std::vector<std::unique_ptr<Artist>>& children) {
    if (x.size() < 2 || y.size() < 2 || z.size() != x.size() * y.size()) {
        throw ValueError("contour: z must be len(y) x len(x)");
    }
    auto cs = std::make_unique<ContourSet>();
    cs->axes = &axes;
    cs->x.assign(x.begin(), x.end());
    cs->y.assign(y.begin(), y.end());
    cs->z.assign(z.begin(), z.end());
    cs->filled = filled;
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const double v : z) {
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    if (!std::isfinite(lo)) {
        throw ValueError("contour: z has no finite values");
    }
    if (!opts.levels.empty()) {
        cs->levels.assign(opts.levels.begin(), opts.levels.end());
    } else {
        // mpl _autolev: MaxNLocator over the data range; unfilled contours drop
        // the boundary levels, filled ones keep them so bands cover the data.
        const MaxNLocator locator;
        for (const double lev : locator.tick_values(lo, hi)) {
            if (filled || (lev > lo && lev < hi)) {
                cs->levels.push_back(lev);
            }
        }
    }
    if (!opts.colors.empty()) {
        cs->single_color = to_color(opts.colors);
    } else {
        cs->cmap = &get_cmap(opts.cmap.empty() ? "viridis" : opts.cmap);
    }
    cs->linewidth = opts.linewidths.value_or(rc().number("lines.linewidth"));
    cs->alpha = opts.alpha;

    ContourSet& ref = *cs;
    children.push_back(std::move(cs));
    return ref;
}
} // namespace

ContourSet& Axes::contour(std::span<const double> x, std::span<const double> y,
                          std::span<const double> z, const ContourOpts& opts) {
    ContourSet& cs = make_contour(*this, x, y, z, opts, /*filled=*/false, children_);
    data_lim_.update(cs.data_extents());
    autoscale_view();
    return cs;
}

ContourSet& Axes::contourf(std::span<const double> x, std::span<const double> y,
                           std::span<const double> z, const ContourOpts& opts) {
    ContourSet& cs = make_contour(*this, x, y, z, opts, /*filled=*/true, children_);
    cs.zorder = 1.0; // filled bands sit with patches, below lines (mpl)
    data_lim_.update(cs.data_extents());
    // contourf sits tight against its grid, like pcolormesh.
    sticky_x_.insert(sticky_x_.end(), {x.front(), x.back()});
    sticky_y_.insert(sticky_y_.end(), {y.front(), y.back()});
    autoscale_view();
    return cs;
}

void Axes::set_aspect(double ratio) {
    if (ratio <= 0 || !std::isfinite(ratio)) {
        throw ValueError("set_aspect: ratio must be positive and finite");
    }
    aspect_ = ratio;
}

void Axes::set_aspect(std::string_view mode) {
    if (mode == "auto") {
        aspect_.reset();
    } else if (mode == "equal") {
        aspect_ = 1.0;
    } else {
        throw ValueError("set_aspect: expected 'auto', 'equal' or a positive number");
    }
}

AxesImage& Axes::imshow(std::span<const double> data, int rows, int cols,
                        const ImshowOpts& opts) {
    if (rows <= 0 || cols <= 0 ||
        data.size() != static_cast<size_t>(rows) * static_cast<size_t>(cols)) {
        throw ValueError("imshow: data must be rows*cols values");
    }
    auto im = std::make_unique<AxesImage>();
    im->axes = this;
    im->data.assign(data.begin(), data.end());
    im->rows = rows;
    im->cols = cols;
    im->cmap = &get_cmap(opts.cmap.empty() ? "viridis" : opts.cmap); // rc image.cmap
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const double v : data) {
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    im->vmin = opts.vmin.value_or(std::isfinite(lo) ? lo : 0.0);
    im->vmax = opts.vmax.value_or(std::isfinite(hi) ? hi : 1.0);
    if (im->vmax == im->vmin) {
        im->vmax = im->vmin + 1.0; // degenerate range, like mpl's autoscaled images
    }
    const std::string_view origin = opts.origin.empty() ? "upper" : opts.origin; // rc image.origin
    if (origin != "upper" && origin != "lower") {
        throw ValueError("imshow: origin must be 'upper' or 'lower'");
    }
    im->origin_upper = origin == "upper";
    if (opts.extent) {
        im->extent = *opts.extent;
    } else if (im->origin_upper) { // mpl default extents (cell-centered indices)
        im->extent = {-0.5, cols - 0.5, rows - 0.5, -0.5};
    } else {
        im->extent = {-0.5, cols - 0.5, -0.5, rows - 0.5};
    }
    const std::string_view interp = opts.interpolation.empty() ? "nearest" : opts.interpolation;
    if (interp == "nearest" || interp == "auto") { // 'auto' -> nearest (D12)
        im->interpolation = Interp::nearest;
    } else if (interp == "bilinear") {
        im->interpolation = Interp::bilinear;
    } else {
        throw ValueError("imshow: interpolation must be 'nearest', 'bilinear' or 'auto'");
    }
    im->alpha = opts.alpha;

    // rc image.aspect = 'equal': imshow forces square pixels unless told not to.
    const std::string_view aspect = opts.aspect.empty() ? "equal" : opts.aspect;
    if (aspect != "equal" && aspect != "auto") {
        throw ValueError("imshow: aspect must be 'equal' or 'auto'");
    }
    set_aspect(aspect);

    // mpl sets the view straight from the extent — origin 'upper' therefore
    // INVERTS the y axis (0 at the top); no margins around images.
    data_lim_.update(im->data_extents());
    set_xlim(im->extent[0], im->extent[1]);
    set_ylim(im->extent[2], im->extent[3]);
    AxesImage& ref = *im;
    children_.push_back(std::move(im));
    return ref;
}



Affine2D Axes::trans_data(Size canvas) const {
    // Nonlinear scales: the affine maps scale-space -> px; artists pre-map
    // their coordinates through scale_point (DESIGN §4).
    const Point lo = scale_point({vx0_, vy0_});
    const Point hi = scale_point({vx1_, vy1_});
    const Bbox view = Bbox::from_extents(lo.x, lo.y, hi.x, hi.y);
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

    // 1. axes patch (edge drawn by the spines; twins keep the host's visible)
    if (!figure_->transparent_render() && !axis_off_ && !patch_off_) {
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
    if (grid_on_ && !axis_off_) {
        GraphicsContext grid_gc;
        grid_gc.color = rc().color("grid.color");
        grid_gc.color.a *= rc().number("grid.alpha");
        grid_gc.linewidth = rc().number("grid.linewidth");
        renderer.open_group("grid");
        for (const double loc : xticks.locs) {
            const double px = tf.apply(scale_point({loc, 1.0})).x;
            Path p;
            p.move_to(px, bpx.y0());
            p.line_to(px, bpx.y1());
            renderer.draw_path(grid_gc, p, Affine2D::identity());
        }
        for (const double loc : yticks.locs) {
            const double py = tf.apply(scale_point({1.0, loc})).y;
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

    // 4. spines + 5. ticks (suppressed by axis('off'), e.g. pie)
    if (!axis_off_) {
        GraphicsContext spine_gc;
        spine_gc.color = rc().color("axes.edgecolor");
        spine_gc.linewidth = rc().number("axes.linewidth");
        spine_gc.capstyle = CapStyle::projecting;
        renderer.open_group("spines");
        if (!patch_off_) { // twins: the host already drew the box
            renderer.draw_path(spine_gc, rect_path, Affine2D::identity());
        }
        renderer.close_group();
        if (show_x_axis_) {
            xaxis_.draw_ticks(renderer, *this, xticks, xaxis_top_, show_x_ticklabels_);
            xaxis_.draw_minor_ticks(renderer, *this, xaxis_.compute_minor_ticks(*this),
                                    xaxis_top_);
        }
        if (show_y_axis_) {
            yaxis_.draw_ticks(renderer, *this, yticks, yaxis_right_, show_y_ticklabels_);
            yaxis_.draw_minor_ticks(renderer, *this, yaxis_.compute_minor_ticks(*this),
                                    yaxis_right_);
        }
    }
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
    if (!xlabel_.text.empty() && show_x_axis_) {
        // beyond the x tick label block (ascent+descent)
        const double tick_label_h =
            fm.ascent(tick_label_em) + fm.descent(tick_label_em);
        const double off = tick_out + tick_label_h + labelpad;
        xlabel_.position = {cx, xaxis_top_ ? bpx.y1() + off : bpx.y0() - off};
        xlabel_.va = xaxis_top_ ? VAlign::bottom : VAlign::top;
        xlabel_.draw(renderer);
    }
    if (!ylabel_.text.empty() && show_y_axis_) {
        double labels_width = 0.0;
        for (const auto& l : yticks.labels) {
            labels_width = std::max(labels_width, fm.text_extent(l, tick_label_em).width);
        }
        const double off = tick_out + labels_width + labelpad;
        ylabel_.position = {yaxis_right_ ? bpx.x1() + off : bpx.x0() - off, cy};
        ylabel_.va = yaxis_right_ ? VAlign::top : VAlign::bottom;
        ylabel_.draw(renderer);
    }
    if (!title_.text.empty()) {
        title_.position = {cx, bpx.y1() + rc().number("axes.titlepad") * ppt};
        title_.draw(renderer);
    }

    // Offset/scale texts at the axis ends (mpl: x right-below, y top-left).
    if (!axis_off_) {
        GraphicsContext offset_gc;
        offset_gc.color = rc().color("xtick.color");
        const FontProperties offset_font{rc().fontsize("xtick.labelsize"), false, false};
        if (!xticks.offset_text.empty() && show_x_axis_) {
            const double label_h = fm.ascent(tick_label_em) + fm.descent(tick_label_em);
            renderer.draw_text(offset_gc, {bpx.x1(), bpx.y0() - tick_out - label_h},
                               xticks.offset_text, offset_font, 0.0, HAlign::right, VAlign::top);
        }
        if (!yticks.offset_text.empty() && show_y_axis_) {
            renderer.draw_text(offset_gc,
                               {yaxis_right_ ? bpx.x1() : bpx.x0(), bpx.y1() + 2.0 * ppt},
                               yticks.offset_text, offset_font, 0.0, HAlign::left,
                               VAlign::bottom);
        }
    }
    renderer.close_group();
}

} // namespace graphlib
