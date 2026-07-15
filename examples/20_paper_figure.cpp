// The v0.6 "Publication Grade" showcase: a two-panel paper figure combining
// mathtext, date axes, annotations and vector PDF output — everything a
// camera-ready submission needs, from one C++ file.
#include <chrono>
#include <cmath>
#include <vector>

#include <graphlib/dates.hpp>
#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;
namespace dates = graphlib::dates;

int main() {
    using namespace std::chrono;
    graphlib::Figure& fig = plt::figure({.figsize = {{8.0, 3.6}}});
    auto axes = fig.subplots(1, 2);
    graphlib::Axes& left = *axes[0][0];
    graphlib::Axes& right = *axes[0][1];

    // Left: model vs theory, annotated.
    const auto x = graphlib::linspace(0.0, 4.0, 300);
    std::vector<double> model(x.size());
    std::vector<double> theory(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        theory[i] = std::exp(-x[i]) * std::sin(6.0 * x[i]) / (1.0 + x[i]);
        model[i] = theory[i] + 0.02 * std::sin(40.0 * x[i]);
    }
    left.plot(x, model, {.linewidth = 1.0, .label = "measured"});
    left.plot(x, theory, "--", {.linewidth = 1.5, .label = "$\\frac{e^{-t}\\sin 6t}{1+t}$"});
    left.set_title("$\\Gamma(t)$ response");
    left.set_xlabel("$t$ (s)");
    left.set_ylabel("$\\Gamma$");
    left.legend();
    left.annotate("ringing", {0.26, theory[20]},
                  {.xytext = {{0.42, 0.18}},
                   .textcoords = "axes fraction",
                   .arrowprops = graphlib::ArrowProps{.arrowstyle = "->"}});

    // Right: a month of daily measurements on a date axis.
    const double d0 = dates::date2num(year_month_day{year{2026}, month{6}, day{15}});
    std::vector<double> day_num(30);
    std::vector<double> level(day_num.size());
    for (size_t i = 0; i < day_num.size(); ++i) {
        const double di = static_cast<double>(i);
        day_num[i] = d0 + di;
        level[i] = 5.0 + 0.08 * di + 0.6 * std::sin(di / 3.1);
    }
    right.plot(day_num, level, "o-", {.markersize = 3.0, .label = "$\\mu_{daily}$"});
    right.xaxis_date();
    right.set_title("field data, $\\sigma < 0.5$");
    right.set_ylabel("level (m)");
    right.legend();

    fig.tight_layout();
    plt::savefig("paper_figure.pdf"); // selectable text, byte-deterministic
    plt::savefig("paper_figure.svg");
    plt::savefig("paper_figure.png", {.dpi = 300});
    return 0;
}
