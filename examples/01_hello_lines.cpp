// The graphlib hero example: two lines, labels, a title -> hello.svg
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 2.0 * std::numbers::pi, 200);
    std::vector<double> sin_y(x.size());
    std::vector<double> cos_y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        sin_y[i] = std::sin(x[i]);
        cos_y[i] = std::cos(x[i]);
    }

    plt::plot(x, sin_y, {.linewidth = 2.0, .label = "sin(x)"});
    plt::plot(x, cos_y, "--", {.label = "cos(x)"});
    plt::title("Hello graphlib");
    plt::xlabel("x [rad]");
    plt::ylabel("amplitude");
    plt::grid(true);
    plt::savefig("hello.svg");
    return 0;
}
