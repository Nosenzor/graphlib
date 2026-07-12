#pragma once
// Mirrors matplotlib.ticker — pipeline in docs/DESIGN.md §8.
// MaxNLocator/ScalarFormatter are faithful ports of matplotlib 3.10.8 algorithms
// (ticker.py: MaxNLocator._raw_ticks, _Edge_integer, scale_range,
//  ScalarFormatter._set_format); oracle fixtures pin the behavior.
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace graphlib {

/// Tick location provider (mirrors matplotlib.ticker.Locator).
class Locator {
public:
    virtual ~Locator() = default;

    /// Tick locations covering [vmin, vmax] (may extend slightly beyond; the
    /// Axis trims to the view at draw time, like matplotlib).
    [[nodiscard]] virtual std::vector<double> tick_values(double vmin, double vmax) const = 0;

    /// Expand a degenerate range (mirrors Locator.nonsingular: expander = 0.05).
    [[nodiscard]] virtual std::pair<double, double> nonsingular(double v0, double v1) const;

    /// Axis geometry hint used by 'auto' bin counts (mirrors axis.get_tick_space()).
    void set_tick_space(int n) { tick_space_ = n; }

protected:
    std::optional<int> tick_space_;
};

/// Finds up to N+1 "nice" tick locations (mirrors MaxNLocator; AutoLocator ==
/// MaxNLocator(nbins='auto', steps=[1, 2, 2.5, 5, 10])).
class MaxNLocator final : public Locator {
public:
    struct Params {
        int nbins = -1; ///< -1 means 'auto': derived from set_tick_space(), capped at 9
        std::vector<double> steps = {1, 2, 2.5, 5, 10};
        int min_n_ticks = 2;
        // matplotlib's integer/symmetric/prune params arrive when a feature needs them.
    };

    MaxNLocator() : MaxNLocator(Params{}) {}
    explicit MaxNLocator(Params p);

    [[nodiscard]] std::vector<double> tick_values(double vmin, double vmax) const override;

private:
    [[nodiscard]] std::vector<double> raw_ticks(double vmin, double vmax) const;

    Params params_;
    std::vector<double> extended_steps_; // staircase: 0.1*steps[:-1] + steps + 10*steps[1]
};

/// Fixed, user-supplied locations.
class FixedLocator final : public Locator {
public:
    explicit FixedLocator(std::vector<double> locs) : locs_(std::move(locs)) {}
    [[nodiscard]] std::vector<double> tick_values(double, double) const override { return locs_; }

private:
    std::vector<double> locs_;
};

/// No ticks.
class NullLocator final : public Locator {
public:
    [[nodiscard]] std::vector<double> tick_values(double, double) const override { return {}; }
};

/// Tick label provider (mirrors matplotlib.ticker.Formatter).
class Formatter {
public:
    virtual ~Formatter() = default;

    /// Labels for `locs`; the view interval feeds label-precision decisions.
    [[nodiscard]] virtual std::vector<std::string>
    format_ticks(std::span<const double> locs, double view_vmin, double view_vmax) const = 0;
};

/// Default label formatter (mirrors ScalarFormatter).
/// v0.1 implements fixed notation only; the scientific/offset-text machinery is
/// TODO(v0.3) — see docs/PARITY.md.
class ScalarFormatter final : public Formatter {
public:
    [[nodiscard]] std::vector<std::string>
    format_ticks(std::span<const double> locs, double view_vmin, double view_vmax) const override;

private:
    [[nodiscard]] int decimals_for(std::span<const double> locs, double view_vmin,
                                   double view_vmax) const;
};

namespace detail {
/// Modify vmin/vmax to avoid singular ranges (port of mtransforms.nonsingular).
std::pair<double, double> nonsingular(double vmin, double vmax, double expander,
                                      double tiny = 1e-15);
} // namespace detail

} // namespace graphlib
