// Manual limits (clipping!), grid, NaN gaps, and data-coordinate text.
#include <cmath>
#include <limits>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(-6.0, 6.0, 400);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = std::tan(x[i]); // wild values: exercises clipping hard
        if (std::abs(y[i]) > 40.0) {
            y[i] = std::numeric_limits<double>::quiet_NaN(); // break at the poles
        }
    }
    plt::plot(x, y, {.linewidth = 1.5, .label = "tan(x)"});
    plt::xlim(-5, 5);
    plt::ylim(-4, 4);
    plt::grid(true);
    plt::text(-4.6, 3.2, "tan(x), poles as gaps");
    plt::title("Manual limits + grid");
    plt::xlabel("x");
    plt::ylabel("tan(x)");
    plt::savefig("limits_grid.svg");
    return 0;
}
