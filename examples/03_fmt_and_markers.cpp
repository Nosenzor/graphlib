// matplotlib format shorthand: "[marker][line][color]" in any order.
#include <vector>

#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>

namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 9.0, 10);
    auto shifted = [&x](double dy) {
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = 0.5 * x[i] + dy;
        }
        return y;
    };

    plt::plot(x, shifted(0), "r--o");
    plt::plot(x, shifted(2), "g^");   // marker only: no line
    plt::plot(x, shifted(4), "C4s-"); // cycle color 4, squares, solid
    plt::plot(x, shifted(6), "k:D");
    plt::plot(shifted(9), "b*-");     // y-only overload: x = 0..N-1
    plt::title("Format strings: \"r--o\", \"g^\", \"C4s-\", \"k:D\", \"b*-\"");
    plt::savefig("fmt_and_markers.svg");
    return 0;
}
