// Log scales: decades, minor subs, and the ScalarFormatter offset text.
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    auto& fig = plt::figure({.figsize = {{10.0, 4.2}}});
    auto grid = fig.subplots(1, 2);

    const auto x = linspace(0.1, 100.0, 300);
    std::vector<double> power(x.size()), root(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        power[i] = 3.0 * x[i] * x[i];
        root[i] = 20.0 * std::sqrt(x[i]);
    }
    grid[0][0]->plot(x, power, {.label = "3x²"});
    grid[0][0]->plot(x, root, "--", {.label = "20√x"});
    grid[0][0]->set_xscale("log");
    grid[0][0]->set_yscale("log");
    grid[0][0]->grid(true);
    grid[0][0]->set_title("log-log");
    grid[0][0]->legend({.loc = "upper left"});

    // Large magnitudes: the axis-end offset text takes over.
    const auto t = linspace(0.0, 10.0, 100);
    std::vector<double> drift(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        drift[i] = 1.0e8 + 12.0 * t[i] + 3.0 * std::sin(2.0 * t[i]);
    }
    grid[0][1]->plot(t, drift, "C2");
    grid[0][1]->set_title("offset text (+1e8)");
    grid[0][1]->grid(true);

    fig.tight_layout();
    fig.savefig("log_scales.svg");
    fig.savefig("log_scales.png", {.dpi = 130});
    return 0;
}
