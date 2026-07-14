#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "graphlib/figure.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}
} // namespace

TEST_CASE("ScalarFormatter offset/scientific matches the matplotlib oracle", "[offset]") {
    for (const auto& c : fixtures::offset_labels) {
        INFO("view [" << c.vmin << ", " << c.vmax << "]");
        Figure fig;
        Axes& ax = fig.add_subplot();
        ax.set_xlim(c.vmin, c.vmax);
        const auto ticks = ax.xaxis().compute_ticks(ax);
        CHECK(ticks.offset_text == c.offset_text);

        // Compare labels on the in-view subset (fixture stores untrimmed locs).
        std::vector<double> expected_locs;
        std::vector<std::string> expected_labels;
        for (size_t i = 0; i < c.ticks.size(); ++i) {
            if (c.ticks[i] >= c.vmin - 1e-12 * std::abs(c.vmax) &&
                c.ticks[i] <= c.vmax + 1e-12 * std::abs(c.vmax)) {
                expected_locs.push_back(c.ticks[i]);
                expected_labels.push_back(c.labels[i]);
            }
        }
        REQUIRE(ticks.locs.size() == expected_locs.size());
        for (size_t i = 0; i < ticks.locs.size(); ++i) {
            INFO("loc " << ticks.locs[i] << " expected label '" << expected_labels[i] << "'");
            CHECK(close(ticks.locs[i], expected_locs[i]));
            CHECK(ticks.labels[i] == expected_labels[i]);
        }
    }
}
