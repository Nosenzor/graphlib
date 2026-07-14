// Style sheets: the same plot under default / ggplot / dark_background.
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

namespace {
void make_plot(const std::string& style_name) {
    graphlib::style::use(style_name);
    plt::figure({.figsize = {{6.4, 4.0}}});
    const auto x = linspace(0.0, 2.0 * std::numbers::pi, 200);
    for (int k = 0; k < 4; ++k) {
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = std::sin(x[i] + 0.6 * k) * (1.0 - 0.15 * k);
        }
        plt::plot(x, y, {.linewidth = 2.0, .label = "wave " + std::to_string(k)});
    }
    plt::title("style: " + style_name);
    plt::legend({.loc = "lower left"});
    plt::savefig("style_" + style_name + ".svg");
    plt::savefig("style_" + style_name + ".png", {.dpi = 120});
    plt::close_all();
}
} // namespace

int main() {
    for (const auto* style : {"default", "ggplot", "dark_background"}) {
        make_plot(style);
    }
    graphlib::style::use("default"); // leave the process clean
    return 0;
}
