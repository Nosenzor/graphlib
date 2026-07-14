// Live animation: FuncAnimation updates the line data each frame.
// Build with -DGRAPHLIB_INTERACTIVE=ON.
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    const auto x = linspace(0.0, 2.0 * std::numbers::pi, 400);
    std::vector<double> y(x.size(), 0.0);
    auto& fig = plt::figure();
    auto& line = plt::plot(x, y, {.linewidth = 2.0});
    plt::ylim(-1.2, 1.2);
    plt::title("live sine (close the window to stop)");
    plt::grid(true);

    graphlib::FuncAnimation anim(
        fig,
        [&](int frame) {
            const double phase = 0.12 * frame;
            for (size_t i = 0; i < x.size(); ++i) {
                line.ydata[i] = std::sin(x[i] + phase) * (0.6 + 0.4 * std::cos(0.05 * frame));
            }
        },
        {.interval = 33}); // ~30 fps
    anim.run();
    return 0;
}
