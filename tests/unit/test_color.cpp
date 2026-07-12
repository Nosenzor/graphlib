#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "graphlib/color.hpp"
#include "graphlib/errors.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

TEST_CASE("to_color matches the matplotlib oracle on every grammar form", "[color]") {
    for (const auto& c : fixtures::colors) {
        INFO("spec: " << c.spec);
        const Color got = to_color(c.spec);
        CHECK_THAT(got.r, WithinAbs(c.r, 1e-12));
        CHECK_THAT(got.g, WithinAbs(c.g, 1e-12));
        CHECK_THAT(got.b, WithinAbs(c.b, 1e-12));
        CHECK_THAT(got.a, WithinAbs(c.a, 1e-12));
    }
}

TEST_CASE("to_color rejects invalid specs like matplotlib", "[color]") {
    CHECK_THROWS_AS(to_color("bogus"), ValueError);
    CHECK_THROWS_AS(to_color(""), ValueError);
    CHECK_THROWS_AS(to_color("#12345"), ValueError);   // 5 hex digits
    CHECK_THROWS_AS(to_color("#1f77bg"), ValueError);  // non-hex digit
    CHECK_THROWS_AS(to_color("2.5"), ValueError);      // gray out of [0,1]
    CHECK_THROWS_AS(to_color("C"), ValueError);        // 'C' needs digits
    CHECK_THROWS_AS(to_color("R"), ValueError);        // single letters are case-sensitive
}

TEST_CASE("is_color_like", "[color]") {
    CHECK(is_color_like("tab:blue"));
    CHECK(is_color_like("#abc"));
    CHECK_FALSE(is_color_like("not-a-color"));
}

TEST_CASE("to_hex", "[color]") {
    CHECK(to_hex(to_color("#1f77b4")) == "#1f77b4");
    CHECK(to_hex(Color{1, 0, 0, 0.5}) == "#ff0000");
    CHECK(to_hex(Color{1, 0, 0, 0.5}, /*keep_alpha=*/true) == "#ff000080");
}
