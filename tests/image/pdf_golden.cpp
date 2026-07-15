// Golden PDF tests: the writer is deterministic (no timestamps, no /ID,
// fixed object order and float formatting), so whole files byte-compare
// like SVGs. Regenerate deliberately with the update-baselines target and
// REVIEW diffs (a PDF diff means the writer or a layout changed).
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "graphlib/backend/pdf.hpp"
#include "graphlib/dates.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/util.hpp"

#include "conformance_scene.hpp"

using namespace graphlib;
namespace fs = std::filesystem;

namespace {

void check_pdf_golden(const std::string& name, const Figure& fig) {
    PdfRenderer renderer(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
    fig.draw(renderer);
    const std::string got = renderer.finalize();

    const fs::path baseline = fs::path(GRAPHLIB_TESTS_DIR) / "image" / "baselines" / "pdf" /
                              (name + ".pdf");
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
        std::ofstream(dir / "got.pdf", std::ios::binary) << got;
        std::ofstream(dir / "expected.pdf", std::ios::binary) << expected.str();
        FAIL("PDF mismatch for '" << name << "' — inspect " << fs::absolute(dir).string());
    }
    SUCCEED();
}

} // namespace

TEST_CASE("golden: backend conformance scene (PDF)", "[golden]") {
    Figure fig;
    build_conformance_scene(fig);
    check_pdf_golden("conformance_pdf", fig);
}

TEST_CASE("golden: PDF text, dates, annotate, mathtext", "[golden]") {
    using namespace std::chrono;
    Figure fig;
    Axes& ax = fig.add_subplot();
    const double d0 = dates::date2num(year_month_day{year{2026}, month{7}, day{1}});
    std::vector<double> x(9);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = d0 + static_cast<double>(i);
        y[i] = 0.5 + 0.35 * static_cast<double>(i % 3) - 0.05 * static_cast<double>(i);
    }
    ax.plot(x, y, "o-", {.label = "$\\sigma_i$"});
    ax.xaxis_date();
    ax.set_title("weekly report $\\mu = \\frac{1}{n}\\sum x_i$");
    ax.set_ylabel("level");
    ax.legend();
    ax.annotate("dip", {x[5], y[5]},
                {.xytext = {{0.7, 0.85}},
                 .textcoords = "axes fraction",
                 .arrowprops = ArrowProps{.arrowstyle = "->"}});
    check_pdf_golden("text_dates_pdf", fig);
}

TEST_CASE("PDF structure: header, xref offsets, embedded fonts", "[pdf]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 1.0, 0.5};
    ax.plot(v, v, "o-");
    ax.set_title("structure probe");
    PdfRenderer renderer(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
    fig.draw(renderer);
    const std::string pdf = renderer.finalize();

    REQUIRE(pdf.rfind("%PDF-1.4\n", 0) == 0);
    REQUIRE(pdf.substr(pdf.size() - 6) == "%%EOF\n");
    CHECK(pdf.find("/Subtype /CIDFontType2") != std::string::npos);
    CHECK(pdf.find("/FontFile2") != std::string::npos);
    CHECK(pdf.find("/ToUnicode") != std::string::npos);
    CHECK(pdf.find("/Encoding /Identity-H") != std::string::npos);

    // Every xref offset must point at "N 0 obj".
    const size_t xref = pdf.rfind("\nxref\n"); // not the trailer's "startxref"
    REQUIRE(xref != std::string::npos);
    std::istringstream tail(pdf.substr(xref + 6));
    std::string line;
    std::getline(tail, line); // "0 N"
    std::getline(tail, line); // free entry
    int obj_num = 1;
    while (std::getline(tail, line) && line.size() >= 18 && line[10] == ' ' &&
           line.substr(11, 5) == "00000") {
        const size_t off = std::stoul(line.substr(0, 10));
        const std::string expect = std::to_string(obj_num) + " 0 obj";
        REQUIRE(pdf.substr(off, expect.size()) == expect);
        ++obj_num;
    }
    CHECK(obj_num > 8); // catalog/pages/page/contents + a full font set
}

TEST_CASE("PDF renders identically across invocations", "[pdf]") {
    auto render = [] {
        Figure fig;
        Axes& ax = fig.add_subplot();
        const std::vector<double> v{0.0, 2.0, 1.0};
        ax.plot(v, v, {.alpha = 0.5});
        ax.set_xlabel("determinism");
        PdfRenderer r(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
        fig.draw(r);
        return r.finalize();
    };
    REQUIRE(render() == render());
}
