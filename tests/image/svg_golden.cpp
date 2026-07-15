// Golden SVG tests (image-tests skill): byte-equal against committed baselines.
// Regenerate deliberately with:  cmake --build --preset dev --target update-baselines
// then REVIEW each diff before committing.
//
// Determinism note: test data avoids libm (no sin/cos) so coordinates are pure
// arithmetic — SVG output must be byte-identical on every platform.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "graphlib/backend/svg.hpp"
#include "graphlib/dates.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/util.hpp"

#include "conformance_scene.hpp"

using namespace graphlib;
namespace fs = std::filesystem;

namespace {

void check_golden(const std::string& name, const Figure& fig) {
    SvgRenderer renderer(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
    fig.draw(renderer);
    const std::string got = renderer.finalize();

    const fs::path baseline = fs::path(GRAPHLIB_TESTS_DIR) / "image" / "baselines" / "svg" /
                              (name + ".svg");
    if (std::getenv("GRAPHLIB_UPDATE_BASELINES") != nullptr) {
        fs::create_directories(baseline.parent_path());
        std::ofstream(baseline, std::ios::binary) << got;
        WARN("baseline updated: " << baseline.string());
        return;
    }
    std::ifstream in(baseline, std::ios::binary);
    if (!in) {
        FAIL("missing baseline " << baseline.string()
                                 << " — run: cmake --build --preset dev --target update-baselines");
    }
    std::stringstream expected;
    expected << in.rdbuf();
    if (got != expected.str()) {
        const fs::path dir = fs::path("image_failures") / name;
        fs::create_directories(dir);
        std::ofstream(dir / "got.svg", std::ios::binary) << got;
        std::ofstream(dir / "expected.svg", std::ios::binary) << expected.str();
        FAIL("SVG mismatch for '" << name << "' — inspect " << fs::absolute(dir).string());
    }
    SUCCEED();
}

} // namespace

TEST_CASE("golden: parabola with labels and title", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const auto x = linspace(-2.0, 2.0, 41);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] * x[i] - 1.0;
    }
    ax.plot(x, y, {.label = "x^2 - 1"});
    ax.set_title("graphlib first light");
    ax.set_xlabel("x");
    ax.set_ylabel("f(x)");
    check_golden("parabola_labels", fig);
}

TEST_CASE("golden: linestyles, colors and the cycle", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const auto x = linspace(0.0, 10.0, 11);
    for (int k = 0; k < 4; ++k) {
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = x[i] * 0.5 + k * 2.0;
        }
        const char* styles[] = {"-", "--", "-.", ":"};
        ax.plot(x, y, styles[k]); // colors come from the cycle
    }
    check_golden("linestyles_cycle", fig);
}

TEST_CASE("golden: markers via format strings", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const auto x = linspace(0.0, 8.0, 9);
    const char* fmts[] = {"o-", "s--", "^:", "D-", "x-", "*-"};
    for (int k = 0; k < 6; ++k) {
        std::vector<double> y(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            y[i] = 0.25 * x[i] + k;
        }
        ax.plot(x, y, fmts[k]);
    }
    check_golden("markers_fmt", fig);
}

TEST_CASE("golden: manual limits, grid, clipping, NaN gap, text", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    std::vector<double> x = linspace(-5.0, 5.0, 21);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = 0.125 * x[i] * x[i] * x[i]; // exits the view: exercises clipping
    }
    y[10] = std::numeric_limits<double>::quiet_NaN(); // gap at x=0
    ax.plot(x, y, "C3");
    ax.set_xlim(-4, 4);
    ax.set_ylim(-2, 2);
    ax.grid(true);
    ax.text(-3.5, 1.5, "clipped & gapped");
    check_golden("limits_grid_clip", fig);
}

TEST_CASE("golden: annotate arrow styles and coord systems", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    std::vector<double> x = linspace(0.0, 4.0, 41);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = (x[i] - 1.0) * (x[i] - 3.0); // parabola: min at (2, -1), no libm
    }
    ax.plot(x, y);
    ax.set_ylim(-2.0, 4.0);
    ax.annotate("minimum", {2.0, -1.0},
                {.xytext = {{2.6, 1.8}}, .arrowprops = ArrowProps{.arrowstyle = "->"}});
    ax.annotate("filled", {0.0, 3.0},
                {.xytext = {{0.55, 0.85}},
                 .xycoords = "data",
                 .textcoords = "axes fraction",
                 .arrowprops = ArrowProps{.arrowstyle = "-|>"}});
    ax.annotate("plain", {1.0, 0.0},
                {.xytext = {{0.4, 3.2}}, .arrowprops = ArrowProps{.arrowstyle = "-"}});
    ax.annotate("offset", {3.0, 0.0}, {.xytext = {{8.0, 12.0}}, .textcoords = "offset points"});
    check_golden("annotate_svg", fig);
}

TEST_CASE("golden: date axis with concise labels", "[golden]") {
    using namespace std::chrono;
    Figure fig;
    Axes& ax = fig.add_subplot();
    const double d0 = dates::date2num(year_month_day{year{2026}, month{7}, day{1}});
    std::vector<double> x(11);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = d0 + static_cast<double>(i);
        y[i] = 0.25 * static_cast<double>(i % 4) + 0.1 * static_cast<double>(i);
    }
    ax.plot(x, y, "o-");
    ax.xaxis_date();
    ax.set_title("daily samples");
    check_golden("dates_svg", fig);
}

TEST_CASE("golden: backend conformance scene (SVG)", "[golden]") {
    // The same scene every backend must draw correctly (new-backend skill).
    Figure fig;
    build_conformance_scene(fig);
    check_golden("conformance_svg", fig);
}

TEST_CASE("golden: imshow embeds a base64 PNG (SVG draw_image)", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    std::vector<double> z;
    for (int i = 0; i < 12; ++i) {
        z.push_back(static_cast<double>(i));
    }
    ax.imshow(z, 3, 4, {.cmap = "viridis"});
    ax.set_title("imshow svg");
    check_golden("imshow_svg", fig);
}

TEST_CASE("golden: transparent savefig drops patches", "[golden]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 1.0};
    ax.plot(v, v);
    // Render via the same path savefig(transparent=true) uses.
    const fs::path tmp = fs::path("image_failures") / "transparent_probe.svg";
    fs::create_directories(tmp.parent_path());
    fig.savefig(tmp.string(), {.transparent = true});
    std::stringstream buf;
    buf << std::ifstream(tmp).rdbuf();
    CHECK(buf.str().find("fill=\"#ffffff\"") == std::string::npos);
}
