#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}
} // namespace

TEST_CASE("bar autoscale matches the oracle (sticky zero)", "[bar]") {
    for (const auto& c : fixtures::bar_autoscale) {
        INFO("heights n=" << c.heights.size());
        Figure fig;
        Axes& ax = fig.add_subplot();
        std::vector<double> x(c.heights.size());
        for (size_t i = 0; i < x.size(); ++i) {
            x[i] = static_cast<double>(i);
        }
        ax.bar(x, c.heights);
        const auto [x0, x1] = ax.get_xlim();
        const auto [y0, y1] = ax.get_ylim();
        CHECK(close(x0, c.xlo));
        CHECK(close(x1, c.xhi));
        CHECK(close(y0, c.ylo));
        CHECK(close(y1, c.yhi));
    }
}

TEST_CASE("bar defaults mirror matplotlib", "[bar]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> x{0.0, 1.0};
    const std::vector<double> h{1.0, 2.0};
    auto bars = ax.bar(x, h, {.label = "series"});
    REQUIRE(bars.size() == 2);
    CHECK(bars[0]->width == 0.8);
    CHECK(bars[0]->x == -0.4);                     // align='center'
    CHECK(bars[0]->facecolor == to_color("C0"));   // one cycle color per call
    CHECK(bars[1]->facecolor == bars[0]->facecolor);
    CHECK(bars[0]->edgecolor == Color::none());    // transparent edge (oracle)
    CHECK(bars[0]->label == "series");             // one legend entry per call
    CHECK(bars[1]->label.empty());
    CHECK(bars[0]->zorder == 1.0);

    Legend& lg = ax.legend({.loc = "upper left"});
    REQUIRE(lg.entries.size() == 1);
    CHECK(lg.entries[0].patch_swatch);
}

TEST_CASE("categorical bar sets tick labels", "[bar]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> h{1.0, 3.0, 2.0};
    ax.bar(std::vector<std::string>{"aa", "bb", "cc"}, h);
    const auto ticks = ax.xaxis().compute_ticks(ax);
    REQUIRE(ticks.locs.size() == 3);
    CHECK(ticks.locs == std::vector<double>{0.0, 1.0, 2.0});
    CHECK(ticks.labels == std::vector<std::string>{"aa", "bb", "cc"});
}

TEST_CASE("hist matches np.histogram binning (oracle)", "[hist]") {
    for (const auto& c : fixtures::hist_bins) {
        INFO("n=" << c.data.size() << " bins=" << c.bins);
        Figure fig;
        Axes& ax = fig.add_subplot();
        auto bars = ax.hist(c.data, {.bins = c.bins});
        REQUIRE(bars.size() == c.counts.size());
        for (size_t i = 0; i < bars.size(); ++i) {
            INFO("bin " << i);
            CHECK(close(bars[i]->height, static_cast<double>(c.counts[i])));
            CHECK(close(bars[i]->x, c.edges[i]));
            CHECK(close(bars[i]->x + bars[i]->width, c.edges[i + 1]));
        }
    }
}

TEST_CASE("Wedge path stays near its circle", "[patches]") {
    Wedge w({0.0, 0.0}, 2.0, 0.0, 108.0);
    const Bbox b = w.get_path().get_extents();
    // get_extents() is conservative (includes Bézier control points, which sit
    // at r*sqrt(1+k^2) ~ 1.051r for a 54-degree segment) — DESIGN §5.
    const double hull = 2.0 * 1.06;
    CHECK(b.x1() <= hull);
    CHECK(b.y1() <= hull);
    CHECK(b.x0() >= -hull);
    CHECK(close(b.y0(), 0.0)); // wedge in the upper half plane
    // On-curve points are exact:
    CHECK(close(w.get_path().vertices()[1].x, 2.0)); // arc start at theta1
}
