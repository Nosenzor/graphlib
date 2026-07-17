#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "graphlib/figure.hpp"

using namespace graphlib;

TEST_CASE("fill_between builds the forward+backward polygon", "[fill]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> x{0.0, 1.0, 2.0};
    const std::vector<double> y1{1.0, 2.0, 1.5};
    Polygon& poly = ax.fill_between(x, y1, {.alpha = 0.4, .label = "band"});
    REQUIRE(poly.points.size() == 6);
    CHECK(poly.points[0].y == 1.0);  // forward along y1
    CHECK(poly.points[3].y == 0.0);  // back along the baseline
    CHECK(poly.edgecolor == Color::none());
    const auto [y0, yy1] = ax.get_ylim();
    CHECK(y0 < 0.0); // baseline joins the data limits
    CHECK(yy1 > 2.0);
}

TEST_CASE("step uses the steps-pre staircase", "[step]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> x{0.0, 1.0, 2.0};
    const std::vector<double> y{0.0, 1.0, 0.5};
    Line2D& line = ax.step(x, y);
    CHECK(line.drawstyle == DrawStyle::steps_pre);
}

TEST_CASE("axhline spans axes fraction and only y joins data limits", "[spans]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{10.0, 20.0};
    ax.plot(v, v);
    Line2D& hl = ax.axhline(25.0);
    const auto [x0, x1] = ax.get_xlim();
    CHECK(x0 > 9.0); // the 0..1 fraction span must NOT widen the x limits
    const auto [y0, y1] = ax.get_ylim();
    CHECK(y1 > 25.0); // ... but y=25 does join the y limits
    (void)y0;
    (void)x1;
}

TEST_CASE("hlines packs NaN-gapped segments into one artist", "[spans]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> ys{1.0, 2.0, 3.0};
    Line2D& lines = ax.hlines(ys, 0.0, 10.0);
    CHECK(lines.xdata.size() == 9); // (x0, x1, nan) x 3
    CHECK(std::isnan(lines.xdata[2]));
}

TEST_CASE("axvspan is a fraction-height rectangle", "[spans]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 10.0};
    ax.plot(v, v);
    Rectangle& span = ax.axvspan(2.0, 4.0, {.alpha = 0.3});
    CHECK(span.x == 2.0);
    CHECK(span.width == 2.0);
}
