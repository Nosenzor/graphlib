// The v0.7 "Fast Path" showcase: a million-point line and a hundred-thousand
// point scatter in one figure, rendered in tens of milliseconds thanks to
// matplotlib's path simplification + marker stamping (see benchmarks/).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const size_t n_line = 1'000'000;
    const auto x = graphlib::linspace(0.0, 100.0, n_line);
    std::vector<double> y(n_line);
    for (size_t i = 0; i < n_line; ++i) {
        const double t = x[i];
        y[i] = std::sin(t) * std::exp(-t / 60.0) + 0.05 * std::sin(37.0 * t);
    }
    plt::plot(x, y, {.linewidth = 0.8, .label = "1M-point line"});

    const size_t n_pts = 100'000;
    std::vector<double> sx(n_pts);
    std::vector<double> sy(n_pts);
    for (size_t i = 0; i < n_pts; ++i) { // deterministic golden-ratio spread
        const double di = static_cast<double>(i);
        sx[i] = 100.0 * std::fmod(di * 0.61803398875, 1.0);
        sy[i] = 2.4 * std::fmod(di * 0.41421356237, 1.0) - 1.2; // sqrt(2)-1
    }
    plt::scatter(sx, sy, {.s = std::vector<double>{4.0}, .alpha = 0.05,
                          .label = "100k-point scatter"});

    plt::title("1.1 million points");
    plt::legend();
    const auto t0 = std::chrono::steady_clock::now();
    plt::savefig("perf.png", {.dpi = 100});
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    std::printf("savefig(perf.png): %lld ms for 1.1M points\n",
                static_cast<long long>(ms));
    return 0;
}
