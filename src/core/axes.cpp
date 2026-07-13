#include "graphlib/axes.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"
#include "text/font_manager.hpp"

namespace graphlib {

Axes::Axes(Figure& figure, Bbox position_fraction) : position(position_fraction), figure_(&figure) {
    title_.fontsize = 12.0; // rc axes.titlesize 'large' = 1.2 x font.size
    title_.ha = HAlign::center;
    title_.va = VAlign::bottom;
    xlabel_.ha = HAlign::center;
    xlabel_.va = VAlign::top;
    ylabel_.ha = HAlign::center;
    ylabel_.va = VAlign::bottom;
    ylabel_.rotation_deg = 90.0;
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
    line->linewidth = opts.linewidth.value_or(1.5);           // rc lines.linewidth
    line->linestyle = LineStyle::parse(ls_token);
    if (!marker_token.empty() && marker_token != "None") {
        line->marker = &get_marker(marker_token);
    }
    line->markersize = opts.markersize.value_or(6.0);         // rc lines.markersize
    line->markerfacecolor =
        opts.markerfacecolor.empty() ? col : to_color(opts.markerfacecolor);
    line->markeredgecolor =
        opts.markeredgecolor.empty() ? col : to_color(opts.markeredgecolor);
    line->markeredgewidth = opts.markeredgewidth.value_or(1.0); // rc lines.markeredgewidth
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
    t->fontsize = opts.fontsize.value_or(10.0); // rc font.size
    t->color = opts.color.empty() ? Color{0, 0, 0, 1} : to_color(opts.color);
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
    // rc scatter.edgecolors = 'face': the edge follows the face color
    pc->edgecolor =
        (opts.edgecolors.empty() || opts.edgecolors == "face") ? face : to_color(opts.edgecolors);
    pc->marker = &get_marker(opts.marker.empty() ? "o" : opts.marker); // rc scatter.marker
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
    lg->loc_code = Legend::parse_loc(opts.loc.empty() ? "best" : opts.loc); // rc legend.loc
    lg->fontsize = opts.fontsize.value_or(10.0);   // rc legend.fontsize 'medium'
    lg->frameon = opts.frameon.value_or(true);     // rc legend.frameon
    lg->framealpha = opts.framealpha.value_or(0.8); // rc legend.framealpha

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

void Axes::set_xlim(double left, double right) {
    if (left == right) {
        // mpl warns and lets the transform degenerate; we expand like the
        // locator would. TODO(v0.2): warning infrastructure.
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

Color Axes::next_cycle_color() {
    return colors::tab10[cycle_index_++ % colors::tab10.size()];
}

void Axes::add_line_datalim(const Line2D& line) { data_lim_.update(line.data_extents()); }

// Port of autoscale_view's per-axis handling (autolimit_mode='data'):
// locator.nonsingular first, then symmetric margins. Sticky edges: v0.3.
void Axes::autoscale_view() {
    if (data_lim_.is_null()) {
        return; // no data: keep the (0, 1) defaults, like mpl
    }
    if (autoscale_x_) {
        auto [x0, x1] = xaxis_.major_locator().nonsingular(data_lim_.x0(), data_lim_.x1());
        const double delta = (x1 - x0) * margin_x_;
        vx0_ = x0 - delta;
        vx1_ = x1 + delta;
    }
    if (autoscale_y_) {
        auto [y0, y1] = yaxis_.major_locator().nonsingular(data_lim_.y0(), data_lim_.y1());
        const double delta = (y1 - y0) * margin_y_;
        vy0_ = y0 - delta;
        vy1_ = y1 + delta;
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

    // 1. axes patch (rc axes.facecolor white, edge drawn by the spines)
    if (!figure_->transparent_render()) {
        GraphicsContext patch_gc;
        patch_gc.color.a = 0; // no stroke
        renderer.draw_path(patch_gc, rect_path, Affine2D::identity(),
                           Color{1, 1, 1, 1});
    }

    // Ticks are computed at draw time from the current view (DESIGN §3).
    const Axis::TickData xticks = xaxis_.compute_ticks(*this);
    const Axis::TickData yticks = yaxis_.compute_ticks(*this);
    const Affine2D tf = trans_data(canvas);

    // 2. grid, below the data (rc axes.axisbelow 'line')
    if (grid_on_) {
        GraphicsContext grid_gc;
        grid_gc.color = to_color("#b0b0b0"); // rc grid.color
        grid_gc.linewidth = 0.8;             // rc grid.linewidth
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

    // 4. spines (rc axes.edgecolor black, axes.linewidth 0.8)
    {
        GraphicsContext spine_gc;
        spine_gc.color = {0, 0, 0, 1};
        spine_gc.linewidth = 0.8;
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
    const double tick_out = (3.5 + 3.5) * ppt;      // tick size + label pad
    const double tick_label_em = 10.0 * ppt;        // rc xtick.labelsize 'medium'
    const double labelpad = 4.0 * ppt;              // rc axes.labelpad
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
        title_.position = {cx, bpx.y1() + 6.0 * ppt}; // rc axes.titlepad
        title_.draw(renderer);
    }
    renderer.close_group();
}

} // namespace graphlib
