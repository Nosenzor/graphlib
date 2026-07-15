// Annotations: point at features with arrows (mirrors the matplotlib
// annotation gallery — arrowstyle subset '-', '->', '-|>').
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto t = graphlib::linspace(0.0, 4.0 * std::numbers::pi, 400);
    std::vector<double> s(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        s[i] = std::exp(-0.15 * t[i]) * std::cos(2.0 * t[i]);
    }
    plt::plot(t, s, {.linewidth = 1.5});
    plt::ylim(-1.2, 1.4);

    const double t_peak = std::numbers::pi; // near the second crest
    plt::annotate("local max", {t_peak, std::exp(-0.15 * t_peak)},
                  {.xytext = {{t_peak + 1.5, 1.1}},
                   .arrowprops = graphlib::ArrowProps{.arrowstyle = "->"}});
    plt::annotate("decay envelope", {7.0, std::exp(-0.15 * 7.0)},
                  {.xytext = {{0.62, 0.82}},
                   .textcoords = "axes fraction",
                   .arrowprops = graphlib::ArrowProps{.arrowstyle = "-|>"}});
    plt::annotate("sampled here", {2.0 * std::numbers::pi, std::exp(-0.15 * 2.0 * std::numbers::pi)},
                  {.xytext = {{-60.0, -35.0}},
                   .textcoords = "offset points",
                   .arrowprops = graphlib::ArrowProps{.arrowstyle = "-"}});

    plt::title("Damped oscillation");
    plt::xlabel("t");
    plt::ylabel("amplitude");
    plt::savefig("annotations.svg");
    plt::savefig("annotations.png", {.dpi = 200});
    return 0;
}
