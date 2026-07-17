#pragma once
// Mirrors matplotlib.axis — the tick pipeline (docs/DESIGN.md §8). Owned by Axes.
#include <memory>
#include <string>
#include <vector>

#include "graphlib/ticker.hpp"

namespace graphlib {

class Axes;
class Renderer;

class Axis {
public:
    enum class Kind { x, y };

    explicit Axis(Kind kind);

    void set_major_locator(std::unique_ptr<Locator> loc) { locator_ = std::move(loc); }
    void set_major_formatter(std::unique_ptr<Formatter> fmt) { formatter_ = std::move(fmt); }
    void set_minor_locator(std::unique_ptr<Locator> loc) { minor_locator_ = std::move(loc); }
    [[nodiscard]] Locator& major_locator() { return *locator_; }
    [[nodiscard]] Locator* minor_locator() { return minor_locator_.get(); }
    [[nodiscard]] Kind kind() const { return kind_; }

    struct TickData {
        std::vector<double> locs;        // trimmed to the view interval
        std::vector<std::string> labels; // parallel to locs
        std::string offset_text;         // axis-end offset/scale ("1e6", "+1e8")
    };

    /// Run locator + formatter for the current view (ticks are computed at draw
    /// time — DESIGN §3). Public so tests can pin the pipeline without a backend.
    [[nodiscard]] TickData compute_ticks(const Axes& axes) const;

    /// Tick marks + labels (grid lines are drawn earlier by Axes, below data).
    /// Move ticks/labels to the far side: right for y, top for x
    /// (mirrors ax.yaxis.tick_right() / ax.xaxis.tick_top()).
    void tick_right() { ticks_far_side_ = true; }
    void tick_top() { ticks_far_side_ = true; }
    [[nodiscard]] bool ticks_far_side() const { return ticks_far_side_; }

    /// far_side: right for y / top for x (twin axes). with_labels: sharex/sharey
    /// hide inner labels but keep the marks.
    void draw_ticks(Renderer& renderer, const Axes& axes, const TickData& ticks,
                    bool far_side = false, bool with_labels = true) const;

    /// Minor tick locations for the current view (empty without a minor locator).
    [[nodiscard]] std::vector<double> compute_minor_ticks(const Axes& axes) const;
    /// Minor tick marks only (rc {x,y}tick.minor.*; never labeled in v0.3).
    void draw_minor_ticks(Renderer& renderer, const Axes& axes,
                          const std::vector<double>& locs, bool far_side = false) const;

    /// Port of X/YAxis.get_tick_space: how many labels fit along `length_pt`
    /// (heuristic: label aspect 3:1 on x, spacing 2 on y).
    static int tick_space(Kind kind, double length_pt, double label_fontsize_pt);

private:
    bool ticks_far_side_ = false;
    Kind kind_;
    std::unique_ptr<Locator> locator_;
    std::unique_ptr<Formatter> formatter_;
    std::unique_ptr<Locator> minor_locator_;
};

} // namespace graphlib
