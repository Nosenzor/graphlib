#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"

using namespace graphlib;

TEST_CASE("scatter defaults mirror matplotlib", "[scatter]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{1.0, 2.0, 3.0};
    PathCollection& pc = ax.scatter(v, v);
    CHECK(pc.zorder == 1.0);                    // Collection zorder (oracle-checked)
    CHECK(pc.sizes == std::vector<double>{36.0}); // lines.markersize^2
    CHECK(pc.linewidth == 1.0);
    CHECK(pc.facecolor == colors::tab10[0]);    // consumes the property cycle
    CHECK(pc.edgecolor == pc.facecolor);        // rc scatter.edgecolors = 'face'
    CHECK(pc.marker->name == "o");              // rc scatter.marker

    PathCollection& pc2 = ax.scatter(v, v, {.c = "tab:green", .edgecolors = "k"});
    CHECK(pc2.facecolor == to_color("tab:green"));
    CHECK(pc2.edgecolor == to_color("k"));

    // scatter contributes to autoscale
    const auto [x0, x1] = ax.get_xlim();
    CHECK(x0 < 1.0);
    CHECK(x1 > 3.0);
}

TEST_CASE("scatter validates sizes", "[scatter]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{1.0, 2.0, 3.0};
    const std::vector<double> bad_sizes{10.0, 20.0};
    CHECK_THROWS_AS(ax.scatter(v, v, {.s = bad_sizes}), ValueError);
    const std::vector<double> ok{10.0, 20.0, 30.0};
    CHECK(ax.scatter(v, v, {.s = ok}).sizes.size() == 3);
}

TEST_CASE("legend collects labeled artists in order", "[legend]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 1.0};
    ax.plot(v, v, {.label = "first"});
    ax.plot(v, v); // unlabeled: skipped
    ax.plot(v, v, "o", {.label = "markers"});
    ax.scatter(v, v, {.label = "points"});

    Legend& lg = ax.legend({.loc = "upper left"});
    REQUIRE(lg.entries.size() == 3);
    CHECK(lg.entries[0].label == "first");
    CHECK(lg.entries[1].label == "markers");
    CHECK_FALSE(lg.entries[1].linestyle.drawn()); // marker-only handle
    CHECK(lg.entries[1].marker != nullptr);
    CHECK(lg.entries[2].label == "points");
    CHECK(lg.entries[2].marker->name == "o");
    CHECK(lg.loc_code == 2); // matplotlib Legend.codes['upper left']
}

TEST_CASE("legend loc parsing mirrors matplotlib codes", "[legend]") {
    CHECK(Legend::parse_loc("best") == 0);
    CHECK(Legend::parse_loc("upper right") == 1);
    CHECK(Legend::parse_loc("center") == 10);
    CHECK_THROWS_AS(Legend::parse_loc("top left"), ValueError);
}
