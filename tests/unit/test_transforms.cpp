#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "graphlib/errors.hpp"
#include "graphlib/transforms.hpp"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

namespace {
void check_point(Point got, double x, double y, double tol = 1e-12) {
    CHECK_THAT(got.x, WithinAbs(x, tol));
    CHECK_THAT(got.y, WithinAbs(y, tol));
}
} // namespace

TEST_CASE("Affine2D basics", "[transforms]") {
    check_point(Affine2D::identity().apply({3, 4}), 3, 4);
    check_point(Affine2D::translation(1, -2).apply({3, 4}), 4, 2);
    check_point(Affine2D::scaling(2, 3).apply({3, 4}), 6, 12);
    check_point(Affine2D::rotation_deg(90).apply({1, 0}), 0, 1, 1e-15);
    check_point(Affine2D::rotation_deg(180).apply({1, 2}), -1, -2, 1e-15);
}

TEST_CASE("Affine2D composition applies left-to-right (mpl A + B)", "[transforms]") {
    // translate then scale: (x + 1) * 2
    const Affine2D t = Affine2D::translation(1, 0).then(Affine2D::scaling(2, 2));
    check_point(t.apply({3, 0}), 8, 0);
    // scale then translate: x * 2 + 1
    const Affine2D u = Affine2D::scaling(2, 2).then(Affine2D::translation(1, 0));
    check_point(u.apply({3, 0}), 7, 0);
}

TEST_CASE("Affine2D inversion round-trips", "[transforms]") {
    const Affine2D t =
        Affine2D::rotation_deg(37).then(Affine2D::scaling(2.5, 0.5)).then(
            Affine2D::translation(-4, 9));
    const Point p{1.25, -3.5};
    const Point q = t.inverted().apply(t.apply(p));
    check_point(q, p.x, p.y, 1e-12);

    CHECK_THROWS_AS(Affine2D::scaling(0, 1).inverted(), ValueError);
}

TEST_CASE("Bbox null accumulates points and ignores NaN", "[transforms]") {
    Bbox b = Bbox::null();
    CHECK(b.is_null());
    b.update({1, 2});
    b.update({-1, 5});
    b.update({std::numeric_limits<double>::quiet_NaN(), 0});
    CHECK_FALSE(b.is_null());
    CHECK(b.x0() == -1);
    CHECK(b.y0() == 2);
    CHECK(b.x1() == 1);
    CHECK(b.y1() == 5);

    Bbox other = Bbox::null();
    b.update(other); // union with null is a no-op
    CHECK(b.x1() == 1);
}

TEST_CASE("bbox transforms map unit square", "[transforms]") {
    const Bbox box = Bbox::from_extents(10, 20, 110, 220);
    check_point(bbox_transform_to(box).apply({0, 0}), 10, 20);
    check_point(bbox_transform_to(box).apply({1, 1}), 110, 220);
    check_point(bbox_transform_from(box).apply({10, 20}), 0, 0);
    check_point(bbox_transform_from(box).apply({110, 220}), 1, 1);
    CHECK_THROWS_AS(bbox_transform_from(Bbox::from_extents(0, 0, 0, 1)), ValueError);
}

TEST_CASE("transData composition: viewLim -> axes pixels", "[transforms]") {
    // transData = transScale + (transLimits + transAxes); transScale is identity in v0.1.
    // Default axes of a 640x480 canvas: rect [0.125, 0.11, 0.775, 0.77].
    const Bbox axes_px = Bbox::from_extents(0.125 * 640, 0.11 * 480, 0.9 * 640, 0.88 * 480);
    const Bbox view = Bbox::from_extents(2, 10, 4, 20);
    const Affine2D trans_data =
        bbox_transform_from(view).then(bbox_transform_to(axes_px));

    check_point(trans_data.apply({2, 10}), 80, 52.8, 1e-9);
    check_point(trans_data.apply({4, 20}), 576, 422.4, 1e-9);
    check_point(trans_data.apply({3, 15}), (80 + 576) / 2.0, (52.8 + 422.4) / 2.0, 1e-9);
}
