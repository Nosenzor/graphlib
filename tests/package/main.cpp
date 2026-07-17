// Exercises the installed package end to end: pyplot facade, mathtext,
// legend, and both vector backends.
#include <graphlib/pyplot.hpp>
#include <graphlib/version.hpp>

#include <vector>

namespace plt = graphlib::pyplot;

int main() {
    const std::vector<double> x{0, 1, 2, 3, 4};
    const std::vector<double> y{0, 1, 4, 9, 16};
    plt::plot(x, y, "o-", {.label = "$x^2$"});
    plt::legend();
    plt::title("installed graphlib");
    plt::savefig("package_smoke.svg");
    plt::savefig("package_smoke.pdf");
    return 0;
}
