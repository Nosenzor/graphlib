// Everyday plots: categorical bars, histogram, errorbar, pie — one figure.
#include <cmath>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;

int main() {
    auto& fig = plt::figure({.figsize = {{10.0, 7.5}}});
    auto grid = fig.subplots(2, 2);

    grid[0][0]->bar(std::vector<std::string>{"mon", "tue", "wed", "thu", "fri"},
                    std::vector<double>{4.2, 7.1, 6.4, 8.3, 5.9}, {.label = "sessions"});
    grid[0][0]->set_title("bar");
    grid[0][0]->set_ylabel("count");

    std::vector<double> data;
    for (int i = 0; i < 300; ++i) {
        data.push_back(2.0 * std::sin(0.7 * i) + std::sin(0.13 * i + 1.0));
    }
    grid[0][1]->hist(data, {.bins = 15, .alpha = 0.85});
    grid[0][1]->set_title("hist");

    const std::vector<double> x{1, 2, 3, 4, 5, 6};
    const std::vector<double> y{2.0, 3.5, 3.1, 4.6, 4.2, 5.1};
    const std::vector<double> err{0.4, 0.3, 0.5, 0.25, 0.45, 0.3};
    grid[1][0]->errorbar(x, y, {.yerr = err, .fmt = "s-", .capsize = 3.0});
    grid[1][0]->set_title("errorbar");
    grid[1][0]->grid(true);

    grid[1][1]->pie(std::vector<double>{35, 25, 20, 12, 8},
                    {.labels = {"core", "text", "backends", "tests", "docs"},
                     .startangle = 90});
    grid[1][1]->set_title("pie");

    fig.suptitle("graphlib everyday plots");
    fig.tight_layout();
    fig.savefig("bar_hist.svg");
    fig.savefig("bar_hist.png", {.dpi = 130});
    return 0;
}
