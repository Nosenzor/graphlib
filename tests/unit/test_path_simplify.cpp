#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "graphlib/path.hpp"
#include "graphlib/rc.hpp"
#include "graphlib/style.hpp"

#include "core/path_simplify.hpp"
#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

TEST_CASE("simplified paths match Path.cleaned(simplify=True)", "[simplify]") {
    for (const auto& c : fixtures::path_simplify) {
        INFO(c.name);
        std::vector<PathCode> codes;
        for (const int code : c.in_codes) {
            codes.push_back(static_cast<PathCode>(code));
        }
        const Path input = codes.empty() ? Path::line(c.in_x, c.in_y)
                                         : Path([&] {
                                               std::vector<Point> v;
                                               for (size_t i = 0; i < c.in_x.size(); ++i) {
                                                   v.push_back({c.in_x[i], c.in_y[i]});
                                               }
                                               return v;
                                           }(),
                                               codes);
        REQUIRE(detail::should_simplify(input));

        const Path got = detail::simplify_path(input, rc().number("path.simplify_threshold"));
        REQUIRE(got.size() * 3 == c.out.size());
        for (size_t i = 0; i < got.size(); ++i) {
            INFO("vertex " << i);
            CHECK_THAT(got.vertices()[i].x, WithinAbs(c.out[3 * i], 1e-9));
            CHECK_THAT(got.vertices()[i].y, WithinAbs(c.out[3 * i + 1], 1e-9));
            CHECK(static_cast<int>(got.code_at(i)) == static_cast<int>(c.out[3 * i + 2]));
        }
    }
}

TEST_CASE("should_simplify follows matplotlib's gate", "[simplify]") {
    std::vector<double> x(200);
    std::vector<double> y(200);
    for (size_t i = 0; i < 200; ++i) {
        x[i] = static_cast<double>(i);
        y[i] = 1.0;
    }
    CHECK(detail::should_simplify(Path::line(x, y)));

    const std::vector<double> small{0.0, 1.0, 2.0};
    CHECK_FALSE(detail::should_simplify(Path::line(small, small))); // < 128 vertices

    Path closed = Path::line(x, y);
    closed.close_subpath(); // CLOSEPOLY disqualifies
    CHECK_FALSE(detail::should_simplify(closed));

    rc().set("path.simplify", false);
    CHECK_FALSE(detail::should_simplify(Path::line(x, y)));
    style::use("default"); // restore rc for the rest of the suite
}
