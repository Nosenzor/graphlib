#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinRel;

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}
} // namespace

TEST_CASE("autoscale (nonsingular + margins) matches the matplotlib oracle", "[axes]") {
    for (const auto& c : fixtures::autoscale) {
        INFO("data [" << c.dlo << ", " << c.dhi << "]");
        Figure fig;
        Axes& ax = fig.add_subplot();
        const std::vector<double> v{c.dlo, c.dhi};
        ax.plot(v, v);
        const auto [x0, x1] = ax.get_xlim();
        const auto [y0, y1] = ax.get_ylim();
        CHECK(close(x0, c.xlo));
        CHECK(close(x1, c.xhi));
        CHECK(close(y0, c.ylo));
        CHECK(close(y1, c.yhi));
    }
}

TEST_CASE("end-to-end ticks on a default axes match the oracle", "[axes]") {
    for (const auto& c : fixtures::axis_ticks) {
        INFO("lims [" << c.vmin << ", " << c.vmax << "]");
        Figure fig;
        Axes& ax = fig.add_subplot();
        ax.set_xlim(c.vmin, c.vmax);
        ax.set_ylim(c.vmin, c.vmax);
        const auto xt = ax.xaxis().compute_ticks(ax);
        // The fixture stores the untrimmed locator output; compare the subset
        // inside the view (compute_ticks trims at draw time, like mpl).
        std::vector<double> expected_locs;
        std::vector<std::string> expected_labels;
        for (size_t i = 0; i < c.xticks.size(); ++i) {
            if (c.xticks[i] >= c.vmin - 1e-12 && c.xticks[i] <= c.vmax + 1e-12) {
                expected_locs.push_back(c.xticks[i]);
                expected_labels.push_back(c.xlabels[i]);
            }
        }
        REQUIRE(xt.locs.size() == expected_locs.size());
        for (size_t i = 0; i < xt.locs.size(); ++i) {
            CHECK(close(xt.locs[i], expected_locs[i]));
            CHECK(xt.labels[i] == expected_labels[i]);
        }
    }
}

TEST_CASE("property cycle assigns tab10 in order", "[axes]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0, 1};
    Line2D& l0 = ax.plot(v, v);
    Line2D& l1 = ax.plot(v, v);
    Line2D& l2 = ax.plot(v, v, "r"); // explicit color must not consume the cycle
    Line2D& l3 = ax.plot(v, v);
    CHECK(l0.color == colors::tab10[0]);
    CHECK(l1.color == colors::tab10[1]);
    CHECK(l2.color == to_color("r"));
    CHECK(l3.color == colors::tab10[2]);
}

TEST_CASE("plot format shorthand drives style like matplotlib", "[axes]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0, 1};

    Line2D& both = ax.plot(v, v, "r--o");
    CHECK(both.color == to_color("r"));
    CHECK(both.linestyle.kind == LineStyle::Kind::dashed);
    REQUIRE(both.marker != nullptr);
    CHECK(both.marker->name == "o");

    Line2D& marker_only = ax.plot(v, v, "o"); // marker without line => no line
    CHECK_FALSE(marker_only.linestyle.drawn());
    REQUIRE(marker_only.marker != nullptr);

    Line2D& color_only = ax.plot(v, v, "g"); // color alone keeps the rc line
    CHECK(color_only.linestyle.kind == LineStyle::Kind::solid);
    CHECK(color_only.marker == nullptr);

    CHECK_THROWS_AS(ax.plot(v, v, "rg"), ValueError);  // two colors
    CHECK_THROWS_AS(ax.plot(v, v, "--:"), ValueError); // two linestyles
    CHECK_THROWS_AS(ax.plot(v, v, "q"), ValueError);   // unknown character
}

TEST_CASE("set_xlim disables autoscale; y-only plot indexes x", "[axes]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_xlim(-3, 3);
    const std::vector<double> v{10, 20, 30};
    ax.plot(v); // y-only: x = 0, 1, 2
    const auto [x0, x1] = ax.get_xlim();
    CHECK(x0 == -3);
    CHECK(x1 == 3); // unchanged by the plot
    const auto [y0, y1] = ax.get_ylim();
    CHECK_THAT(y0, WithinRel(9.0, 1e-12)); // 10 - 0.05 * 20
    CHECK_THAT(y1, WithinRel(31.0, 1e-12));
}

TEST_CASE("mismatched x/y sizes throw like matplotlib", "[axes]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> x{0, 1, 2};
    const std::vector<double> y{0, 1};
    CHECK_THROWS_AS(ax.plot(x, y), ValueError);
}

TEST_CASE("savefig rejects unknown formats with milestone hints", "[figure]") {
    Figure fig;
    fig.add_subplot();
    CHECK_THROWS_AS(fig.savefig("out.bmp"), ValueError);
    CHECK_THROWS_AS(fig.savefig("noextension"), ValueError);
    CHECK_THROWS_AS(fig.savefig("out.png", {.dpi = -100}), ValueError);
}
