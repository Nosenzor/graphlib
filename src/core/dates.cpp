#include "graphlib/dates.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include "fmt.hpp"

namespace graphlib {

namespace dates {

double date2num(std::chrono::sys_days day) {
    return static_cast<double>(day.time_since_epoch().count());
}

double date2num(std::chrono::year_month_day ymd) {
    return date2num(std::chrono::sys_days(ymd));
}

double date2num(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    const auto since_epoch = duration_cast<duration<double>>(tp.time_since_epoch());
    return since_epoch.count() / 86400.0;
}

CivilTime num2date(double datenum) {
    using namespace std::chrono;
    double day_floor = std::floor(datenum);
    // mpl _from_ordinalf rounds the fractional day to the nearest microsecond,
    // so exact-second ticks never land on 29.999999... or -1e-8.
    long long us = std::llround((datenum - day_floor) * 86400.0 * 1e6);
    if (us >= 86400000000LL) { // rounded up to a full day
        day_floor += 1.0;
        us -= 86400000000LL;
    }
    const sys_days day{days{static_cast<long long>(day_floor)}};
    const year_month_day ymd{day};

    CivilTime out;
    out.year = static_cast<int>(ymd.year());
    out.month = static_cast<unsigned>(ymd.month());
    out.day = static_cast<unsigned>(ymd.day());
    out.hour = static_cast<int>(us / 3600000000LL);
    out.minute = static_cast<int>((us % 3600000000LL) / 60000000LL);
    out.second = static_cast<double>(us % 60000000LL) / 1e6;
    return out;
}

} // namespace dates

namespace {

using dates::CivilTime;
using dates::date2num;
using dates::num2date;

constexpr std::array<std::string_view, 12> kMonthAbbrev = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string two_digits(long v) {
    std::string s = std::to_string(v);
    return v < 10 ? "0" + s : s;
}

/// The strftime subset the Concise tables use: %Y %b %d %H %M %S (+ literals).
std::string strf(const CivilTime& t, std::string_view fmt) {
    std::string out;
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 >= fmt.size()) {
            out += fmt[i];
            continue;
        }
        ++i;
        switch (fmt[i]) {
        case 'Y':
            out += std::to_string(t.year);
            break;
        case 'b':
            out += kMonthAbbrev[t.month - 1];
            break;
        case 'm':
            out += two_digits(t.month);
            break;
        case 'd':
            out += two_digits(t.day);
            break;
        case 'H':
            out += two_digits(t.hour);
            break;
        case 'M':
            out += two_digits(t.minute);
            break;
        case 'S':
            out += two_digits(static_cast<long>(std::floor(t.second)));
            break;
        default:
            out += fmt[i];
        }
    }
    return out;
}

// AutoDateLocator tables (extracted from mpl 3.10.8): allowed intervals per
// unit and the tick-count caps. Units: years, months, days, hours, minutes,
// seconds (mpl's weekly/microsecond levels are not needed for the fixtures'
// ranges and stay unimplemented — a finer range falls through to seconds).
constexpr int kMinTicks = 5;
const std::vector<std::vector<long>> kIntervald = {
    {1, 2, 4, 5, 10, 20, 40, 50, 100, 200, 400, 500, 1000, 2000, 4000, 5000, 10000}, // years
    {1, 2, 3, 4, 6},                                                                  // months
    {1, 2, 4, 7, 14},                                                                 // days
    {1, 2, 3, 4, 6, 12},                                                              // hours
    {1, 5, 10, 15, 30},                                                               // minutes
    {1, 5, 10, 15, 30},                                                               // seconds
};
constexpr std::array<int, 6> kMaxTicks = {11, 12, 11, 12, 11, 11};

} // namespace

std::pair<double, double> AutoDateLocator::nonsingular(double v0, double v1) const {
    if (v1 < v0) {
        std::swap(v0, v1);
    }
    if (v0 == v1) { // mpl DateLocator: pad degenerate ranges by two days
        v0 -= 2.0;
        v1 += 2.0;
    }
    return {v0, v1};
}

std::vector<double> AutoDateLocator::tick_values(double vmin, double vmax) const {
    if (vmax < vmin) {
        std::swap(vmin, vmax);
    }
    const CivilTime c0 = num2date(vmin);
    const CivilTime c1 = num2date(vmax);

    // Complete-unit spans (relativedelta semantics for years/months; total
    // spans for the sub-day units, like mpl).
    long num_months = (c1.year - c0.year) * 12L + (static_cast<long>(c1.month) - c0.month);
    if (c1.day < c0.day) {
        --num_months;
    }
    const long num_years = num_months / 12;
    const double span_days = vmax - vmin;
    const std::array<long, 6> nums = {num_years,
                                      num_months,
                                      static_cast<long>(span_days),
                                      static_cast<long>(span_days * 24.0),
                                      static_cast<long>(span_days * 24.0 * 60.0),
                                      static_cast<long>(span_days * 86400.0)};

    int level = 5;
    long interval = 1;
    for (size_t i = 0; i < 6; ++i) {
        if (nums[i] < kMinTicks) {
            continue; // not enough of this unit: go finer
        }
        level = static_cast<int>(i);
        interval = kIntervald[i].back();
        for (const long candidate : kIntervald[i]) {
            if (nums[i] <= candidate * (kMaxTicks[i] - 1)) {
                interval = candidate;
                break;
            }
        }
        break;
    }

    using namespace std::chrono;
    // Ticks outside [vmin, vmax] are legal for the years level only:
    // YearLocator._create_rrule spans Jan 1 of the floored/ceiled interval
    // multiples, while every other level clips via rrule.between(vmin, vmax).
    constexpr double kEps = 1e-9; // FP guard for the inclusive endpoints
    std::vector<double> ticks;
    if (level == 0) {
        const long ymin =
            std::max(c0.year - (((c0.year % interval) + interval) % interval), 1L);
        const long ymax =
            std::min(c1.year + ((interval - (((c1.year % interval) + interval) % interval)) %
                                interval),
                     9999L);
        for (long y = ymin; y <= ymax; y += interval) {
            ticks.push_back(date2num(year_month_day{year{static_cast<int>(y)}, month{1}, day{1}}));
        }
    } else if (level == 1) { // months where (m-1) % interval == 0, at day 1
        for (long y = c0.year; y <= c1.year; ++y) {
            for (unsigned m = 1; m <= 12; ++m) {
                if ((m - 1) % static_cast<unsigned>(interval) != 0) {
                    continue;
                }
                const double t = date2num(
                    year_month_day{year{static_cast<int>(y)}, month{m}, day{1}});
                if (t >= vmin - kEps && t <= vmax + kEps) {
                    ticks.push_back(t);
                }
            }
        }
    } else if (level == 2) { // days of month in {1, 1+i, 1+2i, ...} (mpl bymonthday)
        for (double d = std::floor(vmin); d <= std::floor(vmax); d += 1.0) {
            const CivilTime c = num2date(d);
            if ((c.day - 1) % static_cast<unsigned>(interval) == 0 && d >= vmin - kEps &&
                d <= vmax + kEps) {
                ticks.push_back(d);
            }
        }
    } else { // sub-day units: whole multiples of the interval within each day
        const double unit_days =
            level == 3 ? 1.0 / 24.0 : level == 4 ? 1.0 / (24.0 * 60.0) : 1.0 / 86400.0;
        const double step = unit_days * static_cast<double>(interval);
        for (double k = std::ceil((vmin - kEps) / step);; k += 1.0) {
            const double t = k * step;
            if (t > vmax + kEps) {
                break;
            }
            ticks.push_back(t);
        }
    }
    return ticks;
}

namespace {
// ConciseDateFormatter tables (mpl defaults).
constexpr std::array<std::string_view, 6> kFormats = {"%Y", "%b", "%d", "%H:%M", "%H:%M", "%S"};
constexpr std::array<std::string_view, 6> kZeroFormats = {"",      "%Y",    "%b",
                                                          "%b-%d", "%H:%M", "%H:%M"};
constexpr std::array<std::string_view, 6> kOffsetFormats = {
    "", "%Y", "%Y-%b", "%Y-%b-%d", "%Y-%b-%d", "%Y-%b-%d %H:%M"};

struct ConciseAnalysis {
    int level = 0;
    bool show_offset = true;
};

// Port of the level scan in ConciseDateFormatter.format_ticks.
ConciseAnalysis analyze(std::span<const double> locs) {
    std::vector<CivilTime> civil;
    civil.reserve(locs.size());
    for (const double v : locs) {
        civil.push_back(num2date(v));
    }
    auto component = [](const CivilTime& t, int lvl) -> double {
        switch (lvl) {
        case 0:
            return t.year;
        case 1:
            return t.month;
        case 2:
            return t.day;
        case 3:
            return t.hour;
        case 4:
            return t.minute;
        default:
            return std::floor(t.second);
        }
    };
    ConciseAnalysis out;
    int level = 5;
    for (; level >= 0; --level) {
        bool varies = false;
        bool has_one = false;
        for (const CivilTime& t : civil) {
            varies = varies || component(t, level) != component(civil.front(), level);
            has_one = has_one || component(t, level) == 1.0;
        }
        if (varies) {
            // Year visible in the tick labels themselves: no offset needed.
            if (level < 2 && has_one) {
                out.show_offset = false;
            }
            break;
        }
        if (level == 0) {
            level = 5; // everything equal: label at the finest level
            break;
        }
    }
    out.level = std::max(0, level);
    return out;
}
} // namespace

std::vector<std::string> ConciseDateFormatter::format_ticks(std::span<const double> locs, double,
                                                            double) const {
    std::vector<std::string> labels;
    if (locs.empty()) {
        return labels;
    }
    const ConciseAnalysis a = analyze(locs);
    constexpr std::array<double, 6> zerovals = {0, 1, 1, 0, 0, 0};
    labels.reserve(locs.size());
    for (const double v : locs) {
        const CivilTime t = num2date(v);
        const double comp = a.level == 0   ? t.year
                            : a.level == 1 ? t.month
                            : a.level == 2 ? t.day
                            : a.level == 3 ? t.hour
                            : a.level == 4 ? t.minute
                                           : std::floor(t.second);
        const bool zero = a.level == 5 ? (std::floor(t.second) == 0.0)
                                       : comp == zerovals[static_cast<size_t>(a.level)];
        const std::string_view fmt =
            zero ? kZeroFormats[static_cast<size_t>(a.level)]
                 : kFormats[static_cast<size_t>(a.level)];
        labels.push_back(strf(t, fmt));
    }
    return labels;
}

std::string ConciseDateFormatter::offset_text(std::span<const double> locs, double, double) const {
    if (locs.empty()) {
        return {};
    }
    const ConciseAnalysis a = analyze(locs);
    if (!a.show_offset) {
        return {};
    }
    // mpl: the offset is the LAST tick rendered at the offset level's format.
    return strf(num2date(locs.back()), kOffsetFormats[static_cast<size_t>(a.level)]);
}

} // namespace graphlib
