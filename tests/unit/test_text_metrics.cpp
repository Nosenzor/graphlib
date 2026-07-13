#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "text/font_manager.hpp"

using namespace graphlib;
using detail::FontManager;
using Catch::Matchers::WithinAbs;

TEST_CASE("DejaVu metrics have sane magnitudes at 10px em", "[text]") {
    const auto& fm = FontManager::instance();
    // DejaVu Sans hhea: ascent 0.928 em, descent 0.236 em.
    CHECK_THAT(fm.ascent(10.0), WithinAbs(9.28, 0.15));
    CHECK_THAT(fm.descent(10.0), WithinAbs(2.36, 0.15));

    const auto ext = fm.text_extent("x", 10.0);
    CHECK_THAT(ext.height, WithinAbs(11.64, 0.3)); // ascent + descent
    CHECK(ext.width > 3.0);
    CHECK(ext.width < 8.0);
}

TEST_CASE("widths order and scale sensibly", "[text]") {
    const auto& fm = FontManager::instance();
    const double iii = fm.text_extent("iii", 10.0).width;
    const double mmm = fm.text_extent("MMM", 10.0).width;
    CHECK(iii < mmm);
    // linear in em size
    CHECK_THAT(fm.text_extent("MMM", 20.0).width, WithinAbs(2.0 * mmm, 1e-6));
    // bold is wider
    CHECK(fm.text_extent("MMM", 10.0, /*bold=*/true).width > mmm);
}

TEST_CASE("kerning tightens pairs", "[text]") {
    const auto& fm = FontManager::instance();
    const double av = fm.text_extent("AV", 10.0).width;
    const double a_plus_v = fm.text_extent("A", 10.0).width + fm.text_extent("V", 10.0).width;
    CHECK(av < a_plus_v); // DejaVu kerns A/V
}

TEST_CASE("UTF-8: unicode minus measures wider than hyphen digit", "[text]") {
    const auto& fm = FontManager::instance();
    const auto cps = detail::decode_utf8("−1"); // U+2212 then '1'
    REQUIRE(cps.size() == 2);
    CHECK(cps[0] == char32_t{0x2212});
    CHECK(fm.text_extent("−1", 10.0).width > fm.text_extent("1", 10.0).width);
}

TEST_CASE("multi-line extent grows by 1.2 em per line", "[text]") {
    const auto& fm = FontManager::instance();
    const auto one = fm.text_extent("ab", 10.0);
    const auto two = fm.text_extent("ab\ncd", 10.0);
    CHECK_THAT(two.height - one.height, WithinAbs(12.0, 1e-9));
    CHECK_THAT(two.width, WithinAbs(one.width, 1e-9));
}

TEST_CASE("glyph outlines are non-empty closed paths inside the extent box", "[text]") {
    const auto& fm = FontManager::instance();
    const Path p = fm.text_path("Ag", 10.0);
    REQUIRE_FALSE(p.empty());
    REQUIRE(p.has_codes());
    const Bbox box = p.get_extents();
    const auto ext = fm.text_extent("Ag", 10.0);
    CHECK(box.x0() >= -0.5);
    CHECK(box.x1() <= ext.width + 0.5);
    CHECK(box.y1() <= fm.ascent(10.0) + 0.5);
    CHECK(box.y0() >= -fm.descent(10.0) - 0.5); // 'g' descends below the baseline
    CHECK(box.y0() < -0.5);                     // ... strictly below
}
