// A real window: drag to pan, scroll to zoom at the cursor, 'h' home,
// 's' saves a PNG, 'q' quits. Build with -DGRAPHLIB_INTERACTIVE=ON.
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    const auto t = linspace(0.0, 20.0 * std::numbers::pi, 100000);
    std::vector<double> y(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        y[i] = std::sin(t[i]) * std::exp(-0.02 * t[i]) + 0.1 * std::sin(41.0 * t[i]);
    }
    auto& fig = plt::figure({.figsize = {{9.0, 5.0}}});
    plt::plot(t, y, {.linewidth = 0.7, .label = "100k-point trace"});
    plt::title("drag = pan · scroll = zoom · h = home · s = save · q = quit");
    plt::grid(true);
    plt::legend({.loc = "upper right"});

    // Events use matplotlib's names and payloads:
    fig.mpl_connect("button_press_event", [](const graphlib::Event& e) {
        if (e.inaxes != nullptr && e.xdata && e.ydata) {
            std::printf("click at data (%.3f, %.3f)\n", *e.xdata, *e.ydata);
        }
    });

    plt::show();
    return 0;
}
