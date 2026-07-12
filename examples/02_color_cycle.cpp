// The tab10 property cycle: ten phase-shifted sines, no colors specified.
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 2.0 * std::numbers::pi, 300);
    for (int k = 0; k < 10; ++k) {
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = std::sin(x[i] - 0.3 * k) + 0.4 * k;
        }
        plt::plot(x, y);
    }
    plt::title("The tab10 property cycle");
    plt::savefig("color_cycle.svg");
    return 0;
}
