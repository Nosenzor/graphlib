#pragma once
// Mirrors matplotlib.figure.Figure — owns the artist tree, savefig dispatch.
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "graphlib/axes.hpp"
#include "graphlib/color.hpp"
#include "graphlib/events.hpp"

namespace graphlib {

struct FigureOpts {
    std::optional<std::array<double, 2>> figsize{}; // rc figure.figsize, inches
    std::optional<double> dpi{};                    // rc figure.dpi (SVG renders at 72, like mpl)
    std::string_view facecolor{};                   // rc figure.facecolor
};

struct SaveOpts {
    std::optional<double> dpi; // raster backends only (v0.2); ignored by SVG
    bool transparent = false;  // suppress figure/axes face patches
};

/// kwargs of Figure::subplots.
struct SubplotsOpts {
    bool sharex = false; // shared limits; inner rows lose x tick labels
    bool sharey = false;
};

/// kwargs of Figure::colorbar.
struct ColorbarOpts {
    std::string label{};
};

class Figure {
public:
    Figure() : Figure(FigureOpts{}) {}
    explicit Figure(const FigureOpts& opts);

    /// Single axes at the default rect (== add_subplot(1, 1, 1)).
    Axes& add_subplot();
    /// mpl-style grid placement, 1-based index (add_subplot(2, 2, 3)).
    Axes& add_subplot(int nrows, int ncols, int index);
    Axes& add_axes(Bbox position_fraction);

    /// Grid of axes, row-major, row 0 on top (mirrors plt.subplots).
    std::vector<std::vector<Axes*>> subplots(int nrows, int ncols,
                                             const SubplotsOpts& opts = {});

    /// Centered figure title (mirrors Figure.suptitle).
    void suptitle(std::string text);

    /// Attach a colorbar for the mappable, stealing space from its host axes
    /// (mirrors fig.colorbar geometry). Returns the colorbar's own Axes.
    Axes& colorbar(const AxesImage& mappable, const ColorbarOpts& opts = {});
    Axes& colorbar(const QuadMesh& mappable, const ColorbarOpts& opts = {});

    /// Fit decorations (tick labels, axis labels, titles) inside each axes'
    /// grid cell — a metrics-based v1 of mpl's tight_layout.
    void tight_layout(double pad = 1.08);

    /// Current (last) axes; creates one if none exist — mirrors pyplot.gca.
    Axes& gca();

    // ---- events (mirrors canvas.mpl_connect; docs/DESIGN.md §backend) ----
    /// Register a callback for a canonical event name; returns the connection id.
    int mpl_connect(std::string_view event_name, EventCallback callback) {
        return events_.connect(event_name, std::move(callback));
    }
    void mpl_disconnect(int cid) { events_.disconnect(cid); }
    /// Fill inaxes/xdata/ydata from display coordinates, then dispatch to the
    /// registered callbacks. Backends call this; tests synthesize events with it.
    void process_event(Event event, Size canvas);

    /// Render to file; the format comes from the extension (.svg in v0.1).
    /// Unknown extensions throw ValueError naming the milestone that adds them.
    void savefig(const std::string& filename, const SaveOpts& opts = {}) const;

    void draw(Renderer& renderer) const;

    std::array<double, 2> figsize;
    double dpi;
    Color facecolor;

    [[nodiscard]] const std::vector<std::unique_ptr<Axes>>& axes_list() const { return axes_; }
    /// True while a savefig(transparent=true) render is in flight — the axes
    /// patch checks this to skip its background.
    [[nodiscard]] bool transparent_render() const { return transparent_render_; }

private:
    std::vector<std::unique_ptr<Axes>> axes_;
    std::string suptitle_;
    EventRegistry events_;
    mutable bool transparent_render_ = false; // savefig(transparent=true) scope flag
};

} // namespace graphlib
