#pragma once
// Mirrors matplotlib.dates — datenums are DAYS since 1970-01-01 (mpl's default
// epoch), UTC-naive. Uses only C++20 chrono *calendar* types (year_month_day,
// sys_days — fine on all tier-1 toolchains); no tzdb, no chrono formatting.
#include <chrono>
#include <string>
#include <vector>

#include "graphlib/ticker.hpp"

namespace graphlib {

namespace dates {

/// Days since 1970-01-01 (float; fractional part is time of day).
double date2num(std::chrono::sys_days day);
double date2num(std::chrono::year_month_day ymd);
double date2num(std::chrono::system_clock::time_point tp);

/// Civil breakdown of a datenum (UTC-naive).
struct CivilTime {
    int year = 1970;
    unsigned month = 1; // 1..12
    unsigned day = 1;   // 1..31
    int hour = 0;
    int minute = 0;
    double second = 0.0;
};
CivilTime num2date(double datenum);

} // namespace dates

/// Port of matplotlib AutoDateLocator: picks the calendar unit and interval
/// (years/months/days/hours/minutes/seconds) whose tick count fits.
class AutoDateLocator final : public Locator {
public:
    [[nodiscard]] std::vector<double> tick_values(double vmin, double vmax) const override;
    [[nodiscard]] std::pair<double, double> nonsingular(double v0, double v1) const override;
};

/// Port of matplotlib ConciseDateFormatter: labels carry only the changing
/// part of the date; the rest goes to the axis-end offset text.
class ConciseDateFormatter final : public Formatter {
public:
    [[nodiscard]] std::vector<std::string>
    format_ticks(std::span<const double> locs, double view_vmin, double view_vmax) const override;
    [[nodiscard]] std::string offset_text(std::span<const double> locs, double view_vmin,
                                          double view_vmax) const override;
};

} // namespace graphlib
