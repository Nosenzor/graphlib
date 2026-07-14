#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/rc.hpp"
#include "graphlib/style.hpp"

using namespace graphlib;

namespace {
// Every test that touches rc restores the defaults on exit.
struct StyleGuard {
    ~StyleGuard() { style::use("default"); }
};
} // namespace

TEST_CASE("rc defaults mirror matplotlib", "[rc]") {
    StyleGuard guard;
    CHECK(rc().number("lines.linewidth") == 1.5);
    CHECK(rc().flag("axes.grid") == false);
    CHECK(rc().str("legend.loc") == "best");
    CHECK(rc().color("grid.color") == to_color("#b0b0b0"));
    CHECK(rc().color_cycle().front() == to_color("#1f77b4"));
    CHECK(rc().fontsize("axes.titlesize") == 12.0); // 'large' = 1.2 x 10
    CHECK(rc().fontsize("font.size") == 10.0);
}

TEST_CASE("rc set/get, unknown keys, type mismatches", "[rc]") {
    StyleGuard guard;
    rc().set("lines.linewidth", 3.0);
    CHECK(rc().number("lines.linewidth") == 3.0);
    rc().set("axes.titlesize", 20.0); // numeric override of a named size
    CHECK(rc().fontsize("axes.titlesize") == 20.0);
    CHECK_THROWS_AS(rc().set("lines.linewdith", 1.0), KeyError); // typo fails loudly
    CHECK_THROWS_AS(rc().at("nope.nope"), KeyError);
    CHECK_THROWS_AS(rc().set("lines.linewidth", std::string("thick")), ValueError);
    rc().reset();
    CHECK(rc().number("lines.linewidth") == 1.5);
}

TEST_CASE("style::use applies sheets and artists capture rc at creation", "[rc][style]") {
    StyleGuard guard;

    style::use("ggplot");
    CHECK(rc().flag("axes.grid") == true);
    CHECK(rc().color("axes.facecolor") == to_color("#E5E5E5"));
    {
        Figure fig;
        Axes& ax = fig.add_subplot();
        const std::vector<double> v{0.0, 1.0};
        Line2D& l = ax.plot(v, v);
        CHECK(l.color == to_color("#E24A33")); // ggplot cycle
    }

    style::use("dark_background");
    CHECK(rc().color("figure.facecolor") == to_color("black"));
    {
        Figure fig; // captures rc at creation
        CHECK(fig.facecolor == to_color("black"));
        Line2D& l = fig.add_subplot().plot(std::vector<double>{0.0, 1.0},
                                           std::vector<double>{0.0, 1.0});
        CHECK(l.color == to_color("#8dd3c7"));
    }

    style::use("default");
    Figure fig;
    CHECK(fig.facecolor == to_color("white"));

    CHECK_THROWS_AS(style::use("solarized"), KeyError);
}
