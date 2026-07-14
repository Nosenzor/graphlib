#pragma once
// Mirrors matplotlib.axes.Axes — the central object; plotting methods live here.
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/axis.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/collections.hpp"
#include "graphlib/legend.hpp"
#include "graphlib/lines.hpp"
#include "graphlib/text.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

class Figure;

/// kwargs of Axes::text.
struct TextOpts {
    std::optional<double> fontsize{}; // rc font.size = 10
    std::string_view color{};         // rc text.color = black
    HAlign ha = HAlign::left;
    VAlign va = VAlign::baseline;
    double rotation = 0.0;
};

class Axes {
public:
    Axes(Figure& figure, Bbox position_fraction);

    // ---- plotting (mirrors matplotlib.axes.Axes.plot) ----
    Line2D& plot(std::span<const double> x, std::span<const double> y, std::string_view fmt = "",
                 const LineOpts& opts = {});
    Line2D& plot(std::span<const double> x, std::span<const double> y, const LineOpts& opts) {
        return plot(x, y, "", opts);
    }
    /// y only: x defaults to 0..N-1 (mpl behavior).
    Line2D& plot(std::span<const double> y, std::string_view fmt = "",
                 const LineOpts& opts = {});

    Text& text(double x, double y, std::string s, const TextOpts& opts = {});

    /// Scatter plot of y vs x with per-point sizes (mirrors Axes.scatter;
    /// c-arrays with colormaps arrive in v0.4).
    PathCollection& scatter(std::span<const double> x, std::span<const double> y,
                            const ScatterOpts& opts = {});

    /// Place a legend from the labeled artists (mirrors Axes.legend).
    Legend& legend(const LegendOpts& opts = {});

    /// Transformed data polylines for legend 'best' placement (internal).
    void collect_legend_avoidance(std::vector<std::vector<Point>>& lines_px, Size canvas) const;

    // ---- titles & labels ----
    void set_title(std::string t) { title_.text = std::move(t); }
    void set_xlabel(std::string t) { xlabel_.text = std::move(t); }
    void set_ylabel(std::string t) { ylabel_.text = std::move(t); }

    // ---- limits (setting disables autoscale for that axis, like mpl) ----
    void set_xlim(double left, double right);
    void set_ylim(double bottom, double top);
    [[nodiscard]] std::pair<double, double> get_xlim() const { return {vx0_, vx1_}; }
    [[nodiscard]] std::pair<double, double> get_ylim() const { return {vy0_, vy1_}; }

    void grid(bool on = true) { grid_on_ = on; }

    // ---- geometry & transforms (docs/DESIGN.md §4) ----
    /// Axes position as figure fraction (default [0.125, 0.11] -> [0.9, 0.88]).
    Bbox position;

    [[nodiscard]] Bbox bbox_pixels(Size canvas) const;
    /// transData = transScale + (transLimits + transAxes); transScale is
    /// identity until log scales (v0.3), so the result is a single affine.
    [[nodiscard]] Affine2D trans_data(Size canvas) const;

    void draw(Renderer& renderer);

    [[nodiscard]] Figure& figure() const { return *figure_; }
    [[nodiscard]] Axis& xaxis() { return xaxis_; }
    [[nodiscard]] Axis& yaxis() { return yaxis_; }
    [[nodiscard]] const Axis& xaxis() const { return xaxis_; }
    [[nodiscard]] const Axis& yaxis() const { return yaxis_; }

    /// Next color of the property cycle (tab10 until rcParams land, v0.3).
    Color next_cycle_color();

private:
    void add_line_datalim(const Line2D& line);
    void autoscale_view();

    Figure* figure_;
    std::vector<std::unique_ptr<Artist>> children_;
    std::unique_ptr<Legend> legend_;
    Axis xaxis_{Axis::Kind::x};
    Axis yaxis_{Axis::Kind::y};
    Text title_, xlabel_, ylabel_;

    Bbox data_lim_ = Bbox::null();
    double vx0_ = 0, vx1_ = 1, vy0_ = 0, vy1_ = 1;
    bool autoscale_x_ = true;
    bool autoscale_y_ = true;
    bool grid_on_ = false;   // seeded from rc axes.grid in the ctor
    double margin_x_ = 0.05; // rc axes.xmargin
    double margin_y_ = 0.05; // rc axes.ymargin
    std::vector<Color> cycle_; // rc axes.prop_cycle, captured at creation (mpl semantics)
    size_t cycle_index_ = 0;
};

} // namespace graphlib
