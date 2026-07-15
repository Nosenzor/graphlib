// Date axes: plot against datenums, format with AutoDateLocator +
// ConciseDateFormatter (mirrors matplotlib's date_concise_formatter example).
#include <chrono>
#include <cmath>
#include <vector>

#include <graphlib/dates.hpp>
#include <graphlib/pyplot.hpp>

namespace plt = graphlib::pyplot;
namespace dates = graphlib::dates;

int main() {
    using namespace std::chrono;
    // Ninety days of a noisy-looking (but deterministic) series.
    const double d0 = dates::date2num(year_month_day{year{2026}, month{4}, day{16}});
    std::vector<double> day_num(90);
    std::vector<double> value(day_num.size());
    for (size_t i = 0; i < day_num.size(); ++i) {
        const double di = static_cast<double>(i);
        day_num[i] = d0 + di;
        value[i] = 0.05 * di + std::sin(di / 6.0) + 0.4 * std::sin(di / 1.7);
    }
    plt::plot(day_num, value, {.linewidth = 1.5, .label = "daily index"});
    plt::gca().xaxis_date();
    plt::legend();
    plt::title("Spring 2026");
    plt::ylabel("index");
    plt::savefig("dates.svg");
    plt::savefig("dates.png", {.dpi = 200});
    return 0;
}
