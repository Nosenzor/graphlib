#include "graphlib/ticker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "fmt.hpp"

namespace graphlib {

namespace detail {

// Port of matplotlib.transforms.nonsingular (increasing=True).
std::pair<double, double> nonsingular(double vmin, double vmax, double expander, double tiny) {
    if (!std::isfinite(vmin) || !std::isfinite(vmax)) {
        return {-expander, expander};
    }
    if (vmax < vmin) {
        std::swap(vmin, vmax);
    }
    const double maxabsvalue = std::max(std::abs(vmin), std::abs(vmax));
    if (maxabsvalue < (1e6 / tiny) * std::numeric_limits<double>::min()) {
        vmin = -expander;
        vmax = expander;
    } else if (vmax - vmin <= maxabsvalue * tiny) {
        if (vmax == 0.0 && vmin == 0.0) {
            vmin = -expander;
            vmax = expander;
        } else {
            vmin -= expander * std::abs(vmin);
            vmax += expander * std::abs(vmax);
        }
    }
    return {vmin, vmax};
}

namespace {

// Port of matplotlib.ticker.scale_range.
std::pair<double, double> scale_range(double vmin, double vmax, int n, double threshold = 100) {
    const double dv = std::abs(vmax - vmin); // > 0: nonsingular ran before
    const double meanv = (vmax + vmin) / 2.0;
    double offset = 0.0;
    if (std::abs(meanv) / dv >= threshold) {
        offset = std::copysign(std::pow(10.0, std::floor(std::log10(std::abs(meanv)))), meanv);
    }
    const double scale = std::pow(10.0, std::floor(std::log10(dv / n)));
    return {scale, offset};
}

// Port of matplotlib.ticker._Edge_integer: integer multiples of `step` with
// floating-point slop that grows with the magnitude of `offset`.
class EdgeInteger {
public:
    EdgeInteger(double step, double offset) : step_(step), offset_(std::abs(offset)) {}

    [[nodiscard]] double le(double x) const { // largest n with n*step <= x
        const auto [d, m] = divmod(x);
        return closeto(m / step_, 1.0) ? d + 1 : d;
    }
    [[nodiscard]] double ge(double x) const { // smallest n with n*step >= x
        const auto [d, m] = divmod(x);
        return closeto(m / step_, 0.0) ? d : d + 1;
    }

private:
    [[nodiscard]] std::pair<double, double> divmod(double x) const { // python divmod semantics
        const double d = std::floor(x / step_);
        return {d, x - d * step_};
    }
    [[nodiscard]] bool closeto(double ms, double edge) const {
        double tol = 1e-10;
        if (offset_ > 0) {
            const double digits = std::log10(offset_ / step_);
            tol = std::min(0.4999, std::max(1e-10, std::pow(10.0, digits - 12)));
        }
        return std::abs(ms - edge) < tol;
    }

    double step_;
    double offset_;
};

// python round(x, n) rounds half to even; nearbyint honors FE_TONEAREST which is
// round-half-even — this must match numpy for ScalarFormatter's threshold scan.
double round_to_decimals(double x, int n) {
    const double scale = std::pow(10.0, n);
    return std::nearbyint(x * scale) / scale;
}

// rcParams axes.unicode_minus (default true): U+2212 instead of ASCII hyphen.
std::string fix_minus(std::string s) {
    if (!s.empty() && s.front() == '-') {
        s.replace(0, 1, "−");
    }
    return s;
}

} // namespace
} // namespace detail

std::pair<double, double> Locator::nonsingular(double v0, double v1) const {
    return detail::nonsingular(v0, v1, /*expander=*/0.05);
}

MaxNLocator::MaxNLocator(Params p) : params_(std::move(p)) {
    // _staircase: 0.1 * steps[:-1] ++ steps ++ [10 * steps[1]]
    const auto& s = params_.steps;
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        extended_steps_.push_back(0.1 * s[i]);
    }
    extended_steps_.insert(extended_steps_.end(), s.begin(), s.end());
    if (s.size() > 1) {
        extended_steps_.push_back(10.0 * s[1]);
    }
}

std::vector<double> MaxNLocator::tick_values(double vmin, double vmax) const {
    std::tie(vmin, vmax) = detail::nonsingular(vmin, vmax, /*expander=*/1e-13, /*tiny=*/1e-14);
    return raw_ticks(vmin, vmax);
}

// Port of MaxNLocator._raw_ticks (autolimit_mode='data' branch; the
// 'round_numbers' large_steps refinement is TODO(v0.3) with the rc system).
std::vector<double> MaxNLocator::raw_ticks(double vmin, double vmax) const {
    int nbins = 0;
    if (params_.nbins == -1) { // 'auto'
        nbins = tick_space_ ? std::clamp(*tick_space_, std::max(1, params_.min_n_ticks - 1), 9)
                            : 9;
    } else {
        nbins = params_.nbins;
    }

    const auto [scale, offset] = detail::scale_range(vmin, vmax, nbins);
    const double svmin = vmin - offset;
    const double svmax = vmax - offset;

    std::vector<double> steps;
    steps.reserve(extended_steps_.size());
    for (const double s : extended_steps_) {
        steps.push_back(s * scale);
    }

    const double raw_step = (svmax - svmin) / nbins;
    size_t istep = steps.size() - 1; // fall back to the largest step
    for (size_t i = 0; i < steps.size(); ++i) {
        if (steps[i] >= raw_step) {
            istep = i;
            break;
        }
    }

    // Walk from the chosen step down until one yields at least min_n_ticks
    // ticks inside [vmin, vmax].
    std::vector<double> ticks;
    for (size_t k = istep + 1; k-- > 0;) {
        const double step = steps[k];
        const double best_vmin = std::floor(svmin / step) * step;
        const detail::EdgeInteger edge(step, offset);
        const double low = edge.le(svmin - best_vmin);
        const double high = edge.ge(svmax - best_vmin);
        ticks.clear();
        for (double n = low; n <= high; n += 1.0) {
            ticks.push_back(n * step + best_vmin);
        }
        const auto inside = std::count_if(ticks.begin(), ticks.end(), [&](double t) {
            return t >= svmin && t <= svmax;
        });
        if (inside >= params_.min_n_ticks) {
            break;
        }
    }
    for (double& t : ticks) {
        t += offset;
    }
    return ticks;
}

// Port of ScalarFormatter._set_format (fixed notation; offset=0, orderOfMagnitude=0).
int ScalarFormatter::decimals_for(std::span<const double> locs, double view_vmin,
                                  double view_vmax) const {
    std::vector<double> ext(locs.begin(), locs.end());
    const bool augmented = ext.size() < 2;
    if (augmented) { // use the axis end points for the range estimate only
        ext.push_back(view_vmin);
        ext.push_back(view_vmax);
    }
    const auto [mn, mx] = std::minmax_element(ext.begin(), ext.end());
    double loc_range = *mx - *mn;
    if (loc_range == 0.0) {
        loc_range = std::max(std::abs(*mn), std::abs(*mx));
    }
    if (loc_range == 0.0) {
        loc_range = 1.0;
    }
    if (augmented) {
        ext.resize(ext.size() - 2);
    }
    const int loc_range_oom = static_cast<int>(std::floor(std::log10(loc_range)));
    int sigfigs = std::max(0, 3 - loc_range_oom);
    const double thresh = 1e-3 * std::pow(10.0, loc_range_oom);
    while (sigfigs >= 0) {
        double worst = 0.0;
        for (const double v : ext) {
            worst = std::max(worst, std::abs(v - detail::round_to_decimals(v, sigfigs)));
        }
        if (worst < thresh) {
            --sigfigs;
        } else {
            break;
        }
    }
    return sigfigs + 1;
}

std::vector<std::string> ScalarFormatter::format_ticks(std::span<const double> locs,
                                                       double view_vmin, double view_vmax) const {
    std::vector<std::string> out;
    if (locs.empty()) {
        return out;
    }
    const int decimals = decimals_for(locs, view_vmin, view_vmax);
    out.reserve(locs.size());
    for (double v : locs) {
        if (std::abs(v) < 1e-8) { // ScalarFormatter.__call__ zero snap
            v = 0.0;
        }
        out.push_back(detail::fix_minus(detail::fmt_fixed(v, decimals)));
    }
    return out;
}

} // namespace graphlib
