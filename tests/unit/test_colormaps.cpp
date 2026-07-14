#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "graphlib/colormaps.hpp"
#include "graphlib/errors.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

TEST_CASE("colormap lookups match the matplotlib oracle (60 samples)", "[cmap]") {
    for (const auto& c : fixtures::colormap_samples) {
        INFO(c.cmap << "(" << c.x << ")");
        const Color got = get_cmap(c.cmap)(c.x);
        // LUTs store 8-bit channels; the oracle reports float rgba — half a
        // quantum tolerance.
        CHECK_THAT(got.r, WithinAbs(c.r, 0.5 / 255.0 + 1e-9));
        CHECK_THAT(got.g, WithinAbs(c.g, 0.5 / 255.0 + 1e-9));
        CHECK_THAT(got.b, WithinAbs(c.b, 0.5 / 255.0 + 1e-9));
        CHECK_THAT(got.a, WithinAbs(c.a, 1e-9));
    }
}

TEST_CASE("norms match the matplotlib oracle", "[cmap]") {
    for (const auto& c : fixtures::norm_cases) {
        INFO(c.kind << " [" << c.vmin << "," << c.vmax << "] v=" << c.v);
        const double got = c.kind == "linear" ? Normalize{c.vmin, c.vmax}(c.v)
                                              : LogNorm{c.vmin, c.vmax}(c.v);
        CHECK_THAT(got, WithinAbs(c.out, 1e-12));
    }
}

TEST_CASE("colormap registry", "[cmap]") {
    CHECK(get_cmap("viridis").size() == 256);
    CHECK(get_cmap("tab10").size() == 10);
    CHECK_THROWS_AS(get_cmap("rainbow_unicorn"), ValueError);
    CHECK(get_cmap("viridis")(std::nan("")) == Color::none()); // bad -> transparent
}
