#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "graphlib/ticker.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}
} // namespace

TEST_CASE("MaxNLocator matches the matplotlib oracle (48 ranges)", "[ticker]") {
    for (const auto& c : fixtures::ticker_raw) {
        INFO("range [" << c.vmin << ", " << c.vmax << "] nbins=" << c.nbins);
        MaxNLocator::Params p;
        p.nbins = c.nbins; // -1 encodes 'auto' (no axis attached -> 9)
        const MaxNLocator loc{p};
        const auto got = loc.tick_values(c.vmin, c.vmax);
        REQUIRE(got.size() == c.ticks.size());
        for (size_t i = 0; i < got.size(); ++i) {
            INFO("tick " << i << ": got " << got[i] << " expected " << c.ticks[i]);
            CHECK(close(got[i], c.ticks[i]));
        }
    }
}

TEST_CASE("ScalarFormatter labels match the matplotlib oracle", "[ticker]") {
    const ScalarFormatter fmt;
    for (const auto& c : fixtures::axis_ticks) {
        INFO("view [" << c.vmin << ", " << c.vmax << "]");
        const auto labels = fmt.format_ticks(c.xticks, c.vmin, c.vmax);
        REQUIRE(labels.size() == c.xlabels.size());
        for (size_t i = 0; i < labels.size(); ++i) {
            INFO("loc " << c.xticks[i]);
            CHECK(labels[i] == c.xlabels[i]);
        }
    }
}

TEST_CASE("Locator::nonsingular expands degenerate ranges", "[ticker]") {
    const MaxNLocator loc;
    auto [a, b] = loc.nonsingular(2.0, 2.0);
    CHECK(a == 1.9);
    CHECK(b == 2.1);
    std::tie(a, b) = loc.nonsingular(0.0, 0.0);
    CHECK(a == -0.05);
    CHECK(b == 0.05);
    // non-finite input collapses to the expander interval
    std::tie(a, b) = loc.nonsingular(std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity());
    CHECK(a == -0.05);
    CHECK(b == 0.05);
}

TEST_CASE("FixedLocator and NullLocator", "[ticker]") {
    const FixedLocator fixed({1.0, 2.0, 3.0});
    CHECK(fixed.tick_values(0, 10) == std::vector<double>{1.0, 2.0, 3.0});
    const NullLocator null;
    CHECK(null.tick_values(0, 10).empty());
}
