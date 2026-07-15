#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

#include "graphlib/dates.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

TEST_CASE("date2num/num2date round-trip on the 1970 epoch", "[dates]") {
    using namespace std::chrono;
    CHECK(dates::date2num(year_month_day{year{1970}, month{1}, day{1}}) == 0.0);
    CHECK(dates::date2num(year_month_day{year{2026}, month{7}, day{15}}) == 20649.0);
    const auto c = dates::num2date(20649.75); // 18:00
    CHECK(c.year == 2026);
    CHECK(c.month == 7);
    CHECK(c.day == 15);
    CHECK(c.hour == 18);
    CHECK(c.minute == 0);
}

TEST_CASE("date ticks/labels/offset match the matplotlib oracle", "[dates]") {
    const AutoDateLocator locator;
    const ConciseDateFormatter formatter;
    for (const auto& c : fixtures::date_ticks) {
        INFO("range [" << c.vmin << ", " << c.vmax << "]");
        const auto ticks = locator.tick_values(c.vmin, c.vmax);
        REQUIRE(ticks.size() == c.ticks.size());
        for (size_t i = 0; i < ticks.size(); ++i) {
            INFO("tick " << i);
            CHECK_THAT(ticks[i], WithinAbs(c.ticks[i], 1e-6));
        }
        const auto labels = formatter.format_ticks(ticks, c.vmin, c.vmax);
        REQUIRE(labels.size() == c.labels.size());
        for (size_t i = 0; i < labels.size(); ++i) {
            INFO("label " << i << " tick " << ticks[i]);
            CHECK(labels[i] == c.labels[i]);
        }
        CHECK(formatter.offset_text(ticks, c.vmin, c.vmax) == c.offset);
    }
}
