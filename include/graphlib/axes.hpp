#pragma once
// Mirrors matplotlib.axes.Axes — the central object; plotting methods live here.
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/axis.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/collections.hpp"
#include "graphlib/contour.hpp"
#include "graphlib/image.hpp"
#include "graphlib/legend.hpp"
#include "graphlib/lines.hpp"
#include "graphlib/patches.hpp"
#include "graphlib/text.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

class Figure;

/// kwargs of Axes::bar / Axes::barh.
struct BarOpts {
    std::optional<double> width{}; // 0.8 (bar height for barh)
    double bottom = 0.0;           // left for barh
    std::string_view color{};      // "" -> one cycle color for the whole call
    std::string_view edgecolor{};  // mpl default: fully transparent
    std::optional<double> linewidth{};
    std::optional<double> alpha{};
    std::string_view label{};
};

/// kwargs of Axes::hist.
struct HistOpts {
    std::optional<int> bins{}; // rc hist.bins = 10
    std::span<const double> edges{}; // explicit bin edges (overrides bins)
    std::string_view color{};
    std::optional<double> alpha{};
    std::string_view label{};
};

/// Return of Axes::hist — matplotlib's `(n, bins, patches)` triple.
struct HistResult {
    std::vector<double> counts;       ///< mpl `n`
    std::vector<double> edges;        ///< mpl `bins`
    std::vector<Rectangle*> patches;  ///< one bar per bin
};

/// Return of Axes::pie — matplotlib's `(patches, texts)` pair.
struct PieResult {
    std::vector<Wedge*> wedges;
    std::vector<Text*> texts; ///< empty when no labels were given
};

/// Return of Axes::errorbar — matplotlib's ErrorbarContainer.
struct ErrorbarResult {
    Line2D* line = nullptr;         ///< the data line (mpl `data_line`)
    std::vector<Line2D*> caplines;  ///< cap markers, when capsize > 0
    std::vector<Line2D*> barlines;  ///< the error bar segments
};

/// kwargs of Axes::fill_between / span patches.
struct FillOpts {
    std::string_view color{}; // "" -> next cycle color
    std::optional<double> alpha{};
    std::string_view label{};
};

/// kwargs of Axes::errorbar (line styling flows through LineOpts-like fields).
struct ErrorbarOpts {
    std::span<const double> yerr{}; // one value broadcasts, else per point
    std::span<const double> xerr{};
    std::string_view fmt{};             // plot fmt for the data line
    std::string_view color{};           // data line color ("" -> cycle)
    std::string_view ecolor{};          // error bar color ("" -> line color)
    std::optional<double> elinewidth{}; // "" -> lines.linewidth
    std::optional<double> capsize{};    // rc errorbar.capsize = 0 (points)
    std::optional<double> linewidth{};
    std::optional<double> alpha{};
    std::string_view label{};
};

/// kwargs of Axes::pie.
struct PieOpts {
    std::vector<std::string> labels{};
    std::vector<std::string> colors{}; // empty -> the property cycle
    double startangle = 0.0;                    // degrees CCW from +x
    std::optional<double> radius{};             // 1.0
};

/// kwargs of Axes::text.
struct TextOpts {
    std::optional<double> fontsize{}; // rc font.size = 10
    std::string_view color{};         // rc text.color = black
    HAlign ha = HAlign::left;
    VAlign va = VAlign::baseline;
    double rotation = 0.0;
};

/// kwargs of Axes::annotate. Coordinate tokens: "data" (default) and
/// "axes fraction" for both; textcoords also accepts "offset points"
/// (offset from xy). Other matplotlib tokens raise ValueError until needed.
struct AnnotateOpts {
    std::optional<Point> xytext{};                     // default: at xy
    std::string_view xycoords = "data";
    std::string_view textcoords{};                     // default: xycoords
    std::optional<ArrowProps> arrowprops{};
    std::optional<TextBboxProps> bbox{};               // round/square text box
    std::optional<double> fontsize{};                  // rc font.size = 10
    std::string_view color{};                          // rc text.color = black
    HAlign ha = HAlign::left;
    VAlign va = VAlign::baseline;
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

    /// Annotate the point `xy` with text (mirrors Axes.annotate; arrowstyle
    /// subset "-", "->", "-|>").
    Annotation& annotate(std::string s, Point xy, const AnnotateOpts& opts = {});

    /// Scatter plot of y vs x with per-point sizes (mirrors Axes.scatter;
    /// c-arrays with colormaps arrive in v0.4).
    PathCollection& scatter(std::span<const double> x, std::span<const double> y,
                            const ScatterOpts& opts = {});

    /// Place a legend from the labeled artists (mirrors Axes.legend).
    Legend& legend(const LegendOpts& opts = {});

    /// Vertical bars at numeric positions (mirrors Axes.bar; returns one
    /// Rectangle per bar, first one carries the legend label).
    std::vector<Rectangle*> bar(std::span<const double> x, std::span<const double> height,
                                const BarOpts& opts = {});
    /// Categorical bars: labels become the x tick labels at positions 0..N-1.
    std::vector<Rectangle*> bar(const std::vector<std::string>& labels,
                                std::span<const double> height, const BarOpts& opts = {});
    /// Horizontal bars (mirrors Axes.barh).
    std::vector<Rectangle*> barh(std::span<const double> y, std::span<const double> width,
                                 const BarOpts& opts = {});

    /// Histogram of `data` (np.histogram-compatible binning); returns
    /// counts, bin edges and the bar patches like matplotlib's (n, bins, patches).
    HistResult hist(std::span<const double> data, const HistOpts& opts = {});

    /// Fill the region between y1 and y2 (default 0) along x.
    Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                          std::span<const double> y2, const FillOpts& opts = {});
    Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                          const FillOpts& opts = {});

    /// Step plot (plot with drawstyle steps-pre, mpl's step default).
    Line2D& step(std::span<const double> x, std::span<const double> y,
                 std::string_view fmt = "", const LineOpts& opts = {});

    /// Horizontal/vertical reference lines spanning the axes (fraction range).
    Line2D& axhline(double y, double xmin = 0.0, double xmax = 1.0, const LineOpts& opts = {});
    Line2D& axvline(double x, double ymin = 0.0, double ymax = 1.0, const LineOpts& opts = {});
    /// Shaded horizontal/vertical band across the axes.
    Rectangle& axhspan(double ymin, double ymax, const FillOpts& opts = {});
    Rectangle& axvspan(double xmin, double xmax, const FillOpts& opts = {});
    /// Segments at each y (or x) between two data coordinates, one artist.
    Line2D& hlines(std::span<const double> y, double xmin, double xmax,
                   const LineOpts& opts = {});
    Line2D& vlines(std::span<const double> x, double ymin, double ymax,
                   const LineOpts& opts = {});

    /// Data line + error bars (mirrors Axes.errorbar and its container return).
    ErrorbarResult errorbar(std::span<const double> x, std::span<const double> y,
                            const ErrorbarOpts& opts = {});

    /// Pie chart: wedges CCW from startangle, r=1, labels at r=1.1; the axes
    /// goes frameless with an equal-aspect box, like matplotlib.
    PieResult pie(std::span<const double> sizes, const PieOpts& opts = {});

    /// Display a 2D array as an image (mirrors Axes.imshow; data is row-major
    /// rows x cols). Sets equal aspect by default, like matplotlib.
    AxesImage& imshow(std::span<const double> data, int rows, int cols,
                      const ImshowOpts& opts = {});

    /// Flat-shaded quadrilateral mesh (mirrors Axes.pcolormesh with 1D edge
    /// arrays: x has cols+1 entries, y rows+1, values rows*cols row-major).
    QuadMesh& pcolormesh(std::span<const double> x_edges, std::span<const double> y_edges,
                         std::span<const double> values, const PcolorOpts& opts = {});

    /// Iso-lines / filled bands over a grid (mirrors Axes.contour/contourf;
    /// x has cols entries, y rows, z rows*cols row-major). Default levels come
    /// from MaxNLocator over the data range, like matplotlib.
    ContourSet& contour(std::span<const double> x, std::span<const double> y,
                        std::span<const double> z, const ContourOpts& opts = {});
    ContourSet& contourf(std::span<const double> x, std::span<const double> y,
                         std::span<const double> z, const ContourOpts& opts = {});

    /// Frameless axes: no patch, grid, spines or ticks (mirrors ax.axis('off')).
    void set_axis_off() { axis_off_ = true; }
    /// Aspect control (mirrors set_aspect with adjustable='box'): the axes box
    /// shrinks so one y data-unit renders `ratio` times as tall as one x
    /// data-unit is wide. "auto" removes the constraint.
    void set_aspect(double ratio);
    void set_aspect(std::string_view mode); // "auto" | "equal"

    /// Twin axes sharing this x-axis, with an independent y-axis on the right.
    Axes& twinx();
    /// Twin axes sharing this y-axis, with an independent x-axis on the top.
    Axes& twiny();

    /// Adopt a patch built by a plotting method (updates dataLim + stickies).
    Rectangle& add_patch(std::unique_ptr<Rectangle> patch);
    Polygon& add_patch(std::unique_ptr<Polygon> patch);
    Wedge& add_patch(std::unique_ptr<Wedge> patch);

    /// Manual major ticks (mirrors set_xticks/set_yticks; empty labels keep the
    /// default formatter).
    void set_xticks(std::vector<double> locs, std::vector<std::string> labels = {});
    void set_yticks(std::vector<double> locs, std::vector<std::string> labels = {});

    /// Axis scales (mirrors set_xscale/set_yscale; 'linear' or 'log').
    enum class Scale { linear, log };
    void set_xscale(std::string_view scale);
    void set_yscale(std::string_view scale);
    [[nodiscard]] Scale xscale() const { return xscale_; }
    [[nodiscard]] Scale yscale() const { return yscale_; }
    /// True when any axis has a nonlinear scale: artists must pre-map their
    /// coordinates through scale_point before the affine (DESIGN §4).
    [[nodiscard]] bool nonlinear_scale() const {
        return xscale_ != Scale::linear || yscale_ != Scale::linear;
    }
    [[nodiscard]] Point scale_point(Point p) const {
        return {xscale_ == Scale::log ? std::log10(p.x) : p.x,
                yscale_ == Scale::log ? std::log10(p.y) : p.y};
    }
    [[nodiscard]] Point inverse_scale_point(Point p) const {
        return {xscale_ == Scale::log ? std::pow(10.0, p.x) : p.x,
                yscale_ == Scale::log ? std::pow(10.0, p.y) : p.y};
    }
    /// Smallest positive data values (log autoscale clips to these, like mpl).
    [[nodiscard]] double minpos_x() const { return minpos_x_; }
    [[nodiscard]] double minpos_y() const { return minpos_y_; }

    /// Linear-axis minor ticks (mirrors ax.minorticks_on with AutoMinorLocator).
    void minorticks_on();

    /// Treat x values as datenums (mirrors ax.xaxis_date: AutoDateLocator +
    /// ConciseDateFormatter; datenums via graphlib::dates::date2num).
    void xaxis_date();

    // ---- interactive navigation (drag-pan / scroll-zoom; log-correct) ----
    /// Shift the view by a pixel delta (drag). Works in scale space, so log
    /// axes pan multiplicatively; propagates through share groups.
    void pan(double dx_px, double dy_px, Size canvas);
    /// Zoom by `factor` (>1 zooms in) keeping the data under `center_px` fixed.
    void zoom_at(double factor, Point center_px, Size canvas);
    /// Snapshot / restore the view for the 'h' (home) key.
    void save_home();
    void restore_home();

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
    /// Blended transform: per-axis choice of data coords or axes fraction
    /// (axhline/axhspan — mpl's blended_transform_factory for the affine case).
    [[nodiscard]] Affine2D blended_transform(bool x_fraction, bool y_fraction,
                                             Size canvas) const;

    void draw(Renderer& renderer);

    [[nodiscard]] Figure& figure() const { return *figure_; }
    [[nodiscard]] Axis& xaxis() { return xaxis_; }
    [[nodiscard]] Axis& yaxis() { return yaxis_; }
    [[nodiscard]] const Axis& xaxis() const { return xaxis_; }
    [[nodiscard]] const Axis& yaxis() const { return yaxis_; }

    /// Next color of the property cycle (tab10 until rcParams land, v0.3).
    Color next_cycle_color();

    /// Twins overlay their host and must follow its position in layout passes.
    [[nodiscard]] bool is_twin() const { return is_twin_; }

private:
    // Sharing / layout / legend plumbing — Figure (subplots, tight_layout)
    // and Legend ('best' placement) are the only intended callers.
    friend class Figure;
    friend class Legend;

    struct ShareGroup {
        std::vector<Axes*> members;
    };
    void join_share_x(const std::shared_ptr<ShareGroup>& group);
    void join_share_y(const std::shared_ptr<ShareGroup>& group);
    void set_tick_label_visibility(bool x_labels, bool y_labels) {
        show_x_ticklabels_ = x_labels;
        show_y_ticklabels_ = y_labels;
    }
    [[nodiscard]] bool xaxis_top() const { return xaxis_top_; }
    [[nodiscard]] Axes* share_host() const; // first non-twin share-group member
    [[nodiscard]] bool x_tick_labels_shown() const { return show_x_ticklabels_; }
    [[nodiscard]] bool y_tick_labels_shown() const { return show_y_ticklabels_; }
    Bbox outer_cell = Bbox::null(); // GridSpec cell (tight_layout works within it)
    bool is_twin_ = false;

    /// Transformed data polylines for legend 'best' placement.
    void collect_legend_avoidance(std::vector<std::vector<Point>>& lines_px, Size canvas) const;


    void add_line_datalim(const Line2D& line);
    void autoscale_view();
    void recompute_data_lim();
    void track_minpos(std::span<const Point> pts);
    Patch& adopt_patch(std::unique_ptr<Patch> patch);

    Figure* figure_;
    std::vector<std::unique_ptr<Artist>> children_;
    std::unique_ptr<Legend> legend_;
    Axis xaxis_{Axis::Kind::x};
    Axis yaxis_{Axis::Kind::y};
    Text title_, xlabel_, ylabel_;

    Bbox data_lim_ = Bbox::null();
    double minpos_x_ = std::numeric_limits<double>::infinity();
    double minpos_y_ = std::numeric_limits<double>::infinity();
    Scale xscale_ = Scale::linear;
    Scale yscale_ = Scale::linear;
    double vx0_ = 0, vx1_ = 1, vy0_ = 0, vy1_ = 1;
    bool autoscale_x_ = true;
    bool autoscale_y_ = true;
    bool grid_on_ = false;   // seeded from rc axes.grid in the ctor
    bool axis_off_ = false;
    std::optional<double> aspect_; // nullopt == 'auto'
    bool patch_off_ = false;      // twins draw no background over their host
    bool xaxis_top_ = false;      // twiny: x ticks/label on the top
    bool show_x_axis_ = true;     // twins hide the shared axis entirely
    bool show_y_axis_ = true;
    bool show_x_ticklabels_ = true; // sharex hides labels on inner rows
    bool show_y_ticklabels_ = true;
    std::shared_ptr<ShareGroup> share_x_;
    std::shared_ptr<ShareGroup> share_y_;
    std::optional<std::array<double, 4>> home_view_; // vx0, vx1, vy0, vy1
    bool home_autoscale_x_ = true;
    bool home_autoscale_y_ = true;
    double margin_x_ = 0.05; // rc axes.xmargin
    double margin_y_ = 0.05; // rc axes.ymargin
    std::vector<Color> cycle_; // rc axes.prop_cycle, captured at creation (mpl semantics)
    size_t cycle_index_ = 0;
    std::vector<double> sticky_x_; // accumulated artist sticky edges (sorted on use)
    std::vector<double> sticky_y_;
};

} // namespace graphlib
