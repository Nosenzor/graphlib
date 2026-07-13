// scatter(): per-point sizes in pt^2, cycle colors, marker choice.
#include <cmath>
#include <vector>

#include <graphlib/pyplot.hpp>

namespace plt = graphlib::pyplot;

int main() {
    // A deterministic swirl (no RNG in examples: reproducible gallery).
    std::vector<double> x1;
    std::vector<double> y1;
    std::vector<double> sizes;
    for (int i = 0; i < 60; ++i) {
        const double t = 0.18 * i;
        x1.push_back(0.05 * t * std::cos(2.2 * t));
        y1.push_back(0.05 * t * std::sin(2.2 * t));
        sizes.push_back(9.0 + i * 2.5); // pt^2, grows along the swirl
    }
    plt::scatter(x1, y1, {.s = sizes, .label = "swirl"});

    std::vector<double> x2;
    std::vector<double> y2;
    for (int i = 0; i < 40; ++i) {
        const double t = 0.16 * i;
        x2.push_back(-0.35 + 0.04 * t * std::cos(2.9 * t + 1.5));
        y2.push_back(0.25 + 0.04 * t * std::sin(2.9 * t + 1.5));
    }
    plt::scatter(x2, y2, {.marker = "^", .alpha = 0.7, .label = "second cluster"});

    plt::title("scatter: sizes in points^2");
    plt::grid(true);
    plt::legend({.loc = "lower right"});
    plt::savefig("scatter.svg");
    plt::savefig("scatter.png", {.dpi = 150});
    return 0;
}
