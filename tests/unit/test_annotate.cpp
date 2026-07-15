#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <optional>

#include "graphlib/graphlib.hpp"

#include "core/annotate.hpp"
#include "fixtures/fixtures.inc"

using namespace graphlib;
using Catch::Matchers::WithinAbs;

// mpl clips the connector by bisection to 0.01 px; analytic endpoints agree
// within that.
constexpr double kTol = 0.02;

TEST_CASE("arrow geometry matches the FancyArrowPatch oracle", "[annotate]") {
    for (const auto& c : fixtures::annotate_arrows) {
        INFO(c.style << " lw=" << c.lw << " A(" << c.ax << "," << c.ay << ") B(" << c.bx << ","
                     << c.by << ")");
        std::optional<Bbox> patchA;
        if (!c.patchA.empty()) {
            patchA = Bbox::from_extents(c.patchA[0], c.patchA[1], c.patchA[0] + c.patchA[2],
                                        c.patchA[1] + c.patchA[3]);
        }
        const detail::ArrowStyleSpec style = detail::parse_arrowstyle(c.style);
        const detail::ArrowGeometry g = detail::build_arrow(
            {c.ax, c.ay}, {c.bx, c.by}, patchA, 2.0, 2.0, style, 10.0, c.lw);

        CHECK_THAT(g.tail_begin.x, WithinAbs(c.tail_x0, kTol));
        CHECK_THAT(g.tail_begin.y, WithinAbs(c.tail_y0, kTol));
        CHECK_THAT(g.tail_end.x, WithinAbs(c.tail_x1, kTol));
        CHECK_THAT(g.tail_end.y, WithinAbs(c.tail_y1, kTol));
        REQUIRE(g.head.size() * 2 == c.head.size());
        for (size_t i = 0; i < g.head.size(); ++i) {
            INFO("head vertex " << i);
            CHECK_THAT(g.head[i].x, WithinAbs(c.head[2 * i], kTol));
            CHECK_THAT(g.head[i].y, WithinAbs(c.head[2 * i + 1], kTol));
        }
        CHECK(g.head_filled == c.filled);
    }
}

TEST_CASE("annotate validates arrowstyle and coordinate tokens", "[annotate]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    CHECK_THROWS_AS(ax.annotate("x", {0.5, 0.5}, {.arrowprops = ArrowProps{.arrowstyle = "fancy"}}),
                    ValueError);
    CHECK_THROWS_AS(ax.annotate("x", {0.5, 0.5}, {.xycoords = "figure fraction"}), ValueError);
    CHECK_THROWS_AS(ax.annotate("x", {0.5, 0.5}, {.textcoords = "polar"}), ValueError);
    // "offset points" is text-only, like mpl's most common usage.
    CHECK_THROWS_AS(ax.annotate("x", {0.5, 0.5}, {.xycoords = "offset points"}), ValueError);
}

TEST_CASE("annotate draws arrow and text into the SVG", "[annotate]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_xlim(0.0, 1.0);
    ax.set_ylim(0.0, 1.0);
    ax.annotate("peak", {0.7, 0.3},
                {.xytext = {{0.2, 0.8}},
                 .xycoords = "axes fraction",
                 .arrowprops = ArrowProps{.arrowstyle = "-|>"}});
    ax.annotate("shifted", {0.5, 0.5},
                {.xytext = {{10.0, -20.0}}, .textcoords = "offset points"});

    SvgRenderer renderer(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
    fig.draw(renderer);
    const std::string svg = renderer.finalize();
    CHECK(svg.find("annotation-arrow") != std::string::npos);
    CHECK(svg.find("text") != std::string::npos);
}
