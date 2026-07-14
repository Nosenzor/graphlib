// Fields II: filled contour bands with iso-line overlay.
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    const auto x = linspace(-3.0, 3.0, 120);
    const auto y = linspace(-2.2, 2.2, 90);
    std::vector<double> z;
    z.reserve(x.size() * y.size());
    for (const double yy : y) {
        for (const double xx : x) {
            z.push_back(std::exp(-((xx + 1.0) * (xx + 1.0) + yy * yy)) +
                        0.8 * std::exp(-((xx - 1.2) * (xx - 1.2) + (yy - 0.4) * (yy - 0.4)) / 0.6));
        }
    }
    plt::contourf(x, y, z);
    plt::contour(x, y, z, {.colors = "k", .linewidths = 0.5});
    plt::title("two Gaussians (contourf + contour)");
    plt::xlabel("x");
    plt::ylabel("y");
    plt::savefig("contour_gaussian.svg");
    plt::savefig("contour_gaussian.png", {.dpi = 130});
    return 0;
}
