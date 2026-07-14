// Fields I: imshow heatmap with a labeled colorbar.
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const int R = 60, C = 90;
    std::vector<double> z(static_cast<size_t>(R) * C);
    for (int r = 0; r < R; ++r) {
        for (int c = 0; c < C; ++c) {
            const double x = (c - C / 2.0) / 12.0;
            const double y = (r - R / 2.0) / 9.0;
            z[static_cast<size_t>(r) * C + c] =
                std::sin(1.4 * x) * std::cos(1.1 * y) * std::exp(-0.08 * (x * x + y * y));
        }
    }
    auto& fig = plt::figure();
    auto& im = plt::imshow(z, R, C, {.cmap = "coolwarm"});
    plt::title("interference pattern");
    fig.colorbar(im, {.label = "amplitude"});
    fig.savefig("heatmap_colorbar.svg");
    fig.savefig("heatmap_colorbar.png", {.dpi = 130});
    return 0;
}
