// Interactive smoke (built only with GRAPHLIB_INTERACTIVE): open a real window
// (xvfb on CI), render frames with pan/zoom between them, exit cleanly.
// Input synthesis isn't possible headlessly — the event/navigation MATH is
// covered by the headless unit tests; this proves the window path end-to-end.
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    const auto x = linspace(0.0, 4.0 * std::numbers::pi, 50000);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = std::sin(x[i]);
    }
    auto& fig = plt::figure();
    plt::plot(x, y, {.linewidth = 0.8});
    plt::grid(true);

    int draws = 0;
    fig.mpl_connect("draw_event", [&](const graphlib::Event&) { ++draws; });

    for (int i = 0; i < 3; ++i) {
        plt::pause(0.1);
        plt::gca().zoom_at(1.3, {320.0, 240.0}, {640.0, 480.0});
        plt::gca().pan(5.0, 2.0, {640.0, 480.0});
    }
    std::printf("interactive smoke: draws=%d\n", draws);
    return draws >= 1 ? 0 : 1;
}
