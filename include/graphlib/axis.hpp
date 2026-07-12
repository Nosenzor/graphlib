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
    [[nodiscard]] Locator& major_locator() { return *locator_; }

    struct TickData {
        std::vector<double> locs;        // trimmed to the view interval
        std::vector<std::string> labels; // parallel to locs
    };

    /// Run locator + formatter for the current view (ticks are computed at draw
    /// time — DESIGN §3). Public so tests can pin the pipeline without a backend.
    [[nodiscard]] TickData compute_ticks(const Axes& axes) const;

    /// Tick marks + labels (grid lines are drawn earlier by Axes, below data).
    void draw_ticks(Renderer& renderer, const Axes& axes, const TickData& ticks) const;

    /// Port of X/YAxis.get_tick_space: how many labels fit along `length_pt`
    /// (heuristic: label aspect 3:1 on x, spacing 2 on y).
    static int tick_space(Kind kind, double length_pt, double label_fontsize_pt);

private:
    Kind kind_;
    std::unique_ptr<Locator> locator_;
    std::unique_ptr<Formatter> formatter_;
};

} // namespace graphlib
