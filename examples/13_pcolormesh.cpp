// Fields III: pcolormesh on a non-uniform grid + log-scaled y.
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    // Non-uniform y edges: logarithmically spaced.
    const auto x_edges = linspace(0.0, 10.0, 41);
    std::vector<double> y_edges;
    for (int i = 0; i <= 24; ++i) {
        y_edges.push_back(std::pow(10.0, -1.0 + 3.0 * i / 24.0));
    }
    std::vector<double> c;
    for (size_t r = 0; r + 1 < y_edges.size(); ++r) {
        for (size_t k = 0; k + 1 < x_edges.size(); ++k) {
            const double xm = (x_edges[k] + x_edges[k + 1]) / 2.0;
            const double ym = std::sqrt(y_edges[r] * y_edges[r + 1]);
            c.push_back(std::sin(xm) * std::log10(ym));
        }
    }
    auto& fig = plt::figure();
    auto& mesh = plt::pcolormesh(x_edges, y_edges, c, {.cmap = "viridis"});
    plt::gca().set_yscale("log");
    plt::title("pcolormesh, log y");
    fig.colorbar(mesh);
    fig.savefig("pcolormesh.svg");
    fig.savefig("pcolormesh.png", {.dpi = 130});
    return 0;
}
