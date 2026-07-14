// Fields IV: scatter with value-mapped colors (c= array through a colormap).
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;

int main() {
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> speed;
    std::vector<double> size;
    for (int i = 0; i < 220; ++i) {
        const double t = 0.055 * i;
        x.push_back(t * std::cos(3.1 * t));
        y.push_back(0.8 * t * std::sin(3.1 * t));
        speed.push_back(std::abs(std::sin(2.0 * t)) * t);
        size.push_back(12.0 + 3.0 * t);
    }
    plt::scatter(x, y, {.s = size, .c_array = speed, .cmap = "plasma", .alpha = 0.85});
    plt::title("scatter(c=speed, cmap=\"plasma\")");
    plt::grid(true);
    plt::savefig("scatter_cmap.svg");
    plt::savefig("scatter_cmap.png", {.dpi = 130});
    return 0;
}
