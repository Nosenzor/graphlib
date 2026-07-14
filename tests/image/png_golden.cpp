// PNG image-comparison tests (image-tests skill): render via AggRenderer,
// compare against committed baselines with RMS <= 1.0 (0-255 scale) unless a
// commented per-test override says why. Regenerate via update-baselines.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <stb_image.h>
#include <stb_image_write.h>

#include "graphlib/backend/agg.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/util.hpp"

#include "conformance_scene.hpp"

using namespace graphlib;
namespace fs = std::filesystem;

namespace {

struct Image {
    int w = 0;
    int h = 0;
    std::vector<unsigned char> rgba;
};

Image render(const Figure& fig, double dpi) {
    const int w = static_cast<int>(std::lround(fig.figsize[0] * dpi));
    const int h = static_cast<int>(std::lround(fig.figsize[1] * dpi));
    AggRenderer renderer(w, h, dpi);
    fig.draw(renderer);
    return {w, h, {renderer.buffer().begin(), renderer.buffer().end()}};
}

double rms(const Image& a, const Image& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.rgba.size(); ++i) {
        const double d = static_cast<double>(a.rgba[i]) - static_cast<double>(b.rgba[i]);
        sum += d * d;
    }
    return std::sqrt(sum / static_cast<double>(a.rgba.size()));
}

void write_png(const fs::path& p, const Image& im) {
    fs::create_directories(p.parent_path());
    stbi_write_png(p.string().c_str(), im.w, im.h, 4, im.rgba.data(), im.w * 4);
}

void check_png(const std::string& name, const Figure& fig, double tol = 1.0,
               double dpi = 100.0) {
    const Image got = render(fig, dpi);
    const fs::path baseline =
        fs::path(GRAPHLIB_TESTS_DIR) / "image" / "baselines" / "png" / (name + ".png");

    if (std::getenv("GRAPHLIB_UPDATE_BASELINES") != nullptr) {
        write_png(baseline, got);
        WARN("baseline updated: " << baseline.string());
        return;
    }
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* data = stbi_load(baseline.string().c_str(), &w, &h, &comp, 4);
    if (data == nullptr) {
        FAIL("missing baseline " << baseline.string()
                                 << " — run: cmake --build --preset dev --target update-baselines");
    }
    Image expected{w, h, {data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 4}};
    stbi_image_free(data);

    REQUIRE(got.w == expected.w);
    REQUIRE(got.h == expected.h);
    const double err = rms(got, expected);
    if (err > tol) {
        const fs::path dir = fs::path("image_failures") / name;
        write_png(dir / "got.png", got);
        write_png(dir / "expected.png", expected);
        Image diff = got;
        for (size_t i = 0; i < diff.rgba.size(); ++i) {
            const int d = std::abs(static_cast<int>(got.rgba[i]) -
                                   static_cast<int>(expected.rgba[i]));
            diff.rgba[i] = static_cast<unsigned char>(std::min(255, d * 8)); // amplified
        }
        for (size_t i = 3; i < diff.rgba.size(); i += 4) {
            diff.rgba[i] = 255; // opaque alpha so the diff is viewable
        }
        write_png(dir / "diff.png", diff);
        FAIL("PNG RMS " << err << " > " << tol << " for '" << name << "' — inspect "
                        << fs::absolute(dir).string());
    }
    SUCCEED();
}

} // namespace

TEST_CASE("png golden: backend conformance scene", "[golden][png]") {
    Figure fig;
    build_conformance_scene(fig);
    check_png("conformance_agg", fig);
}

TEST_CASE("png golden: parabola at 150 dpi (dpi scaling)", "[golden][png]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const auto x = linspace(-2.0, 2.0, 41);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] * x[i] - 1.0;
    }
    ax.plot(x, y);
    ax.set_title("dpi = 150");
    ax.set_xlabel("x");
    ax.set_ylabel("f(x)");
    check_png("parabola_150dpi", fig, 1.0, 150.0);
}

TEST_CASE("png golden: scatter sizes + legend 'best'", "[golden][png]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    // Data crowds the upper right; 'best' must dodge into another corner.
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> s;
    for (int i = 0; i < 12; ++i) {
        x.push_back(1.0 + 0.18 * i);
        y.push_back(1.0 + 0.16 * i);
        s.push_back(20.0 + 12.0 * i);
    }
    ax.scatter(x, y, {.s = s, .label = "growing"});
    const std::vector<double> lx{1.0, 3.0};
    const std::vector<double> ly{2.9, 1.1};
    ax.plot(lx, ly, "--", {.label = "guide"});
    ax.legend();
    ax.grid(true);
    check_png("scatter_legend_best", fig);
}

TEST_CASE("png golden: everyday plots scene (v0.3)", "[golden][png]") {
    Figure fig;
    // Panel via subplots to also pin GridSpec geometry.
    auto grid = fig.subplots(1, 2);
    Axes& left = *grid[0][0];
    const std::vector<double> h{2.0, 5.0, 3.5};
    left.bar(std::vector<std::string>{"a", "b", "c"}, h, {.label = "bars"});
    left.axhline(4.0, 0, 1, {.color = "0.3", .linewidth = 0.8, .linestyle = "--"});
    left.set_title("bars + axhline");

    Axes& right = *grid[0][1];
    const auto x = linspace(1.0, 100.0, 60);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] * x[i] * 0.5;
    }
    right.plot(x, y);
    right.set_xscale("log");
    right.set_yscale("log");
    right.grid(true);
    right.set_title("log-log");
    check_png("everyday_scene", fig);
}

TEST_CASE("png golden: imshow nearest + bilinear (draw_image contract)", "[golden][png]") {
    Figure fig;
    auto grid = fig.subplots(1, 2);
    std::vector<double> z;
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 8; ++c) {
            z.push_back(((r + c) % 2) * 2.0 + r * 0.3);
        }
    }
    grid[0][0]->imshow(z, 6, 8, {.cmap = "coolwarm"});
    grid[0][0]->set_title("nearest");
    grid[0][1]->imshow(z, 6, 8, {.cmap = "coolwarm", .interpolation = "bilinear"});
    grid[0][1]->set_title("bilinear");
    check_png("imshow_interp", fig);
}

TEST_CASE("png: transparent render has transparent corners", "[png]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 1.0};
    ax.plot(v, v);
    const fs::path tmp = fs::path("image_failures") / "transparent_probe.png";
    fs::create_directories(tmp.parent_path());
    fig.savefig(tmp.string(), {.transparent = true});
    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* data = stbi_load(tmp.string().c_str(), &w, &h, &comp, 4);
    REQUIRE(data != nullptr);
    CHECK(data[3] == 0);                                  // top-left alpha
    CHECK(data[(static_cast<size_t>(w) * h - 1) * 4 + 3] == 0); // bottom-right alpha
    stbi_image_free(data);
}

