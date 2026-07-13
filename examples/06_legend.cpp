// legend(): auto 'best' placement dodging the data, matplotlib-style handles.
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 2.0 * std::numbers::pi, 200);
    std::vector<double> damped(x.size());
    std::vector<double> envelope(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        envelope[i] = std::exp(-0.35 * x[i]);
        damped[i] = envelope[i] * std::cos(4.0 * x[i]);
    }
    plt::plot(x, damped, {.linewidth = 1.5, .label = "e^{-t/3} cos 4t"});
    plt::plot(x, envelope, "--", {.label = "envelope"});

    std::vector<double> px;
    std::vector<double> py;
    for (int i = 0; i < 8; ++i) {
        const double t = 0.25 + 0.75 * i;
        px.push_back(t);
        py.push_back(std::exp(-0.35 * t) * std::cos(4.0 * t));
    }
    plt::scatter(px, py, {.c = "tab:red", .label = "samples"});

    plt::title("legend(loc=\"best\") avoids the data");
    plt::xlabel("t [s]");
    plt::ylabel("signal");
    plt::grid(true);
    plt::legend();
    plt::savefig("legend.svg");
    plt::savefig("legend.png", {.dpi = 150});
    return 0;
}
