#include "graphlib/ticker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "fmt.hpp"
#include "graphlib/errors.hpp"

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

// Port of LogLocator.tick_values (base 10; numticks 'auto' -> tick space).
std::vector<double> LogLocator::tick_values(double vmin, double vmax) const {
    int numticks = tick_space_ ? std::clamp(*tick_space_, 2, 9) : 9;
    if (vmin <= 0.0) {
        vmin = minpos_;
        if (vmin <= 0.0 || !std::isfinite(vmin)) {
            throw ValueError("Data has no positive values, and therefore cannot be log-scaled.");
        }
    }
    if (vmax < vmin) {
        std::swap(vmin, vmax);
    }
    // Faithful port detail: mpl computes log(v)/log(b), NOT log10(v) — the two
    // round differently at exact decade boundaries, changing floor/ceil below.
    const double log_vmin = std::log(vmin) / std::log(10.0);
    const double log_vmax = std::log(vmax) / std::log(10.0);
    const auto numdec =
        static_cast<long>(std::floor(log_vmax)) - static_cast<long>(std::ceil(log_vmin));

    std::vector<double> subs{1.0};
    if (minor_subs_) {
        if (numdec > 10) {
            return {}; // mpl: no minor ticks over huge ranges
        }
        subs.clear();
        for (double s = 2.0; s < 10.0; s += 1.0) {
            subs.push_back(s);
        }
    }
    long stride = numdec / numticks + 1;
    if (stride >= numdec) {
        stride = std::max<long>(1, numdec - 1);
    }
    std::vector<double> ticks;
    const auto d0 = static_cast<long>(std::floor(log_vmin)) - stride;
    const auto d1 = static_cast<long>(std::ceil(log_vmax)) + 2 * stride;
    for (long d = d0; d < d1; d += stride) {
        for (const double s : subs) {
            ticks.push_back(s * std::pow(10.0, static_cast<double>(d)));
        }
    }
    return ticks;
}

std::pair<double, double> LogLocator::nonsingular(double v0, double v1) const {
    // Port of LogLocator.nonsingular: fall back to a decade around the data.
    if (!std::isfinite(v0) || !std::isfinite(v1)) {
        return {1.0, 10.0};
    }
    if (v1 < v0) {
        std::swap(v0, v1);
    }
    if (v1 <= 0.0) {
        return {1.0, 10.0};
    }
    if (v0 <= 0.0) {
        v0 = minpos_ > 0.0 && std::isfinite(minpos_) ? minpos_ : v1 / 10.0;
    }
    if (v0 == v1) {
        return {v0 / 10.0, v1 * 10.0};
    }
    return {v0, v1};
}

namespace {
std::string superscript(long exponent) {
    static constexpr const char* digits[10] = {"⁰", "¹", "²", "³", "⁴",
                                               "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string out;
    if (exponent < 0) {
        out += "⁻";
        exponent = -exponent;
    }
    const std::string dec = std::to_string(exponent);
    for (const char c : dec) {
        out += digits[c - '0'];
    }
    return out;
}
} // namespace

std::vector<std::string> LogFormatter::format_ticks(std::span<const double> locs, double,
                                                    double) const {
    std::vector<std::string> out;
    out.reserve(locs.size());
    for (const double v : locs) {
        if (v <= 0) {
            out.emplace_back();
            continue;
        }
        const double lg = std::log10(v);
        const double rounded = std::nearbyint(lg);
        if (std::abs(lg - rounded) < 1e-9) { // exact decade: 10^k
            out.push_back("10" + superscript(static_cast<long>(rounded)));
        } else {
            out.emplace_back(); // minor/sub ticks are unlabeled by default (mpl)
        }
    }
    return out;
}

// Port of AutoMinorLocator.__call__ (ndivs 'auto').
std::vector<double> AutoMinorLocator::tick_values(double vmin, double vmax) const {
    const std::vector<double> majors = major_->tick_values(vmin, vmax);
    if (majors.size() < 2) {
        return {};
    }
    const double majorstep = majors[1] - majors[0];
    const double mantissa =
        std::pow(10.0, std::log10(majorstep) - std::floor(std::log10(majorstep)));
    const bool nice = std::abs(mantissa - 1.0) < 1e-9 || std::abs(mantissa - 2.5) < 1e-9 ||
                      std::abs(mantissa - 5.0) < 1e-9 || std::abs(mantissa - 10.0) < 1e-9;
    const int ndivs = nice ? 5 : 4;
    const double step = majorstep / ndivs;

    if (vmax < vmin) {
        std::swap(vmin, vmax);
    }
    const double t0 = majors[0];
    std::vector<double> out;
    // Same construction as mpl: integer multiples of `step` from the first
    // major, keeping only those inside the view and not on a major.
    const auto i0 = static_cast<long>(std::ceil((vmin - t0) / step));
    const auto i1 = static_cast<long>(std::floor((vmax - t0) / step));
    for (long i = i0; i <= i1; ++i) {
        if ((i % ndivs) == 0) {
            continue; // that's a major tick
        }
        out.push_back(t0 + static_cast<double>(i) * step);
    }
    return out;
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
