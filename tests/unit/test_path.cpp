#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "graphlib/errors.hpp"
#include "graphlib/path.hpp"

using namespace graphlib;

TEST_CASE("Path::line without NaN is an implicit polyline", "[path]") {
    const std::vector<double> x{0, 1, 2};
    const std::vector<double> y{5, 6, 7};
    const Path p = Path::line(x, y);
    CHECK(p.size() == 3);
    CHECK_FALSE(p.has_codes());
    CHECK(p.code_at(0) == PathCode::moveto);
    CHECK(p.code_at(1) == PathCode::lineto);
}

TEST_CASE("Path::line with NaN opens a gap", "[path]") {
    const double nan = std::nan("");
    const std::vector<double> x{0, 1, nan, 3, 4};
    const std::vector<double> y{0, 1, 2, 3, 4};
    const Path p = Path::line(x, y);
    CHECK(p.size() == 4); // NaN vertex dropped
    REQUIRE(p.has_codes());
    CHECK(p.codes()[0] == PathCode::moveto);
    CHECK(p.codes()[1] == PathCode::lineto);
    CHECK(p.codes()[2] == PathCode::moveto); // pen restarts after the gap
    CHECK(p.codes()[3] == PathCode::lineto);
    CHECK(p.vertices()[2].x == 3);
}

TEST_CASE("Path::line length mismatch throws", "[path]") {
    const std::vector<double> x{0, 1};
    const std::vector<double> y{0};
    CHECK_THROWS_AS(Path::line(x, y), ValueError);
}

TEST_CASE("Path builder and close()", "[path]") {
    Path p;
    p.move_to(0, 0);
    p.line_to(1, 0);
    p.line_to(1, 1);
    p.close();
    REQUIRE(p.size() == 4);
    CHECK(p.codes()[0] == PathCode::moveto);
    CHECK(p.codes()[3] == PathCode::closepoly);
    CHECK(p.vertices()[3].x == 0); // close appends the first vertex
}

TEST_CASE("Path extents and transform", "[path]") {
    Path p;
    p.move_to(-1, 2);
    p.line_to(3, -4);
    const Bbox e = p.get_extents();
    CHECK(e.x0() == -1);
    CHECK(e.y0() == -4);
    CHECK(e.x1() == 3);
    CHECK(e.y1() == 2);

    const Path q = p.transformed(Affine2D::scaling(2, 2));
    CHECK(q.vertices()[1].x == 6);
    CHECK(q.code_at(0) == PathCode::moveto);
}

TEST_CASE("Path code/vertex size mismatch throws", "[path]") {
    std::vector<Point> v{{0, 0}, {1, 1}};
    std::vector<PathCode> c{PathCode::moveto};
    CHECK_THROWS_AS(Path(v, c), ValueError);
}
