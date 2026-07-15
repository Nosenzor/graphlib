// Mathtext: TeX-like math in any text element (mirrors matplotlib's
// mathtext demo — subset: scripts, \frac, \sqrt, greek, operators).
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 2.0, 200);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] * x[i] * std::exp(-2.0 * x[i]);
    }
    plt::plot(x, y, {.linewidth = 2.0, .label = "$x^2 e^{-2x}$"});
    plt::title("$f(x) = x^2 e^{-2x}$");
    plt::xlabel("$x$");
    plt::ylabel("$f(x)$");
    plt::legend();
    plt::annotate("$\\max f = e^{-2}$", {1.0, std::exp(-2.0)},
                  {.xytext = {{1.35, 0.125}},
                   .arrowprops = graphlib::ArrowProps{.arrowstyle = "->"}});
    plt::text(0.98, 0.04, "$\\sigma = \\sqrt{\\frac{1}{n}\\sum_{i=1}^{n}(x_i-\\mu)^2}$",
              {.fontsize = 14.0});
    plt::savefig("mathtext.svg");
    plt::savefig("mathtext.png", {.dpi = 200});
    return 0;
}
