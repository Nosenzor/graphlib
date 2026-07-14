#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

TEST_CASE("imshow origin 'upper' inverts y and pins the extent", "[imshow]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> z{0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    AxesImage& im = ax.imshow(z, 2, 3);
    CHECK(im.vmin == 0.0);
    CHECK(im.vmax == 5.0); // autoscaled to the data
    CHECK(im.zorder == 0.0);
    const auto [x0, x1] = ax.get_xlim();
    const auto [y0, y1] = ax.get_ylim();
    CHECK(x0 == -0.5);
    CHECK(x1 == 2.5);
    CHECK(y0 == 1.5);  // inverted: bottom of the view is the LAST row
    CHECK(y1 == -0.5); // row 0 at the top, like matplotlib
}

TEST_CASE("imshow origin 'lower' keeps normal orientation", "[imshow]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> z{0.0, 1.0, 2.0, 3.0};
    ax.imshow(z, 2, 2, {.origin = "lower"});
    const auto [y0, y1] = ax.get_ylim();
    CHECK(y0 == -0.5);
    CHECK(y1 == 1.5);
}

TEST_CASE("imshow equal aspect shrinks the box to square pixels", "[imshow]") {
    Figure fig; // 640x480 canvas at dpi 100
    Axes& ax = fig.add_subplot();
    std::vector<double> z(40 * 60, 0.5);
    ax.imshow(z, 40, 60); // y span 40, x span 60 -> box h/w must be 2/3
    const Bbox box = ax.bbox_pixels({640.0, 480.0});
    CHECK_THAT(box.height() / box.width(), WithinAbs(40.0 / 60.0, 1e-9));
}

TEST_CASE("imshow validates input", "[imshow]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> z{1.0, 2.0, 3.0};
    CHECK_THROWS_AS(ax.imshow(z, 2, 2), ValueError);          // wrong size
    const std::vector<double> ok{1.0, 2.0, 3.0, 4.0};
    CHECK_THROWS_AS(ax.imshow(ok, 2, 2, {.origin = "middle"}), ValueError);
    CHECK_THROWS_AS(ax.imshow(ok, 2, 2, {.interpolation = "lanczos"}), ValueError);
    CHECK_THROWS_AS(ax.set_aspect(-1.0), ValueError);
}
