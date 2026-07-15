#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "graphlib/errors.hpp"

#include "text/font_manager.hpp"
#include "text/mathtext.hpp"

using namespace graphlib;
using detail::FontManager;

namespace {
constexpr double kEm = 100.0 / 72.0 * 12.0; // 12pt at 100dpi
}

TEST_CASE("math run splitting and detection", "[mathtext]") {
    CHECK(detail::contains_math("a $x$ b"));
    CHECK_FALSE(detail::contains_math("costs \\$5"));
    CHECK_FALSE(detail::contains_math("plain"));

    const auto runs = detail::split_math_runs("pre $x^2$ mid \\$5 $y$");
    REQUIRE(runs.size() == 4);
    CHECK((runs[0].text == "pre " && !runs[0].math));
    CHECK((runs[1].text == "x^2" && runs[1].math));
    CHECK((runs[2].text == " mid $5 " && !runs[2].math));
    CHECK((runs[3].text == "y" && runs[3].math));

    CHECK_THROWS_AS(detail::split_math_runs("odd $x"), ValueError);
}

TEST_CASE("scripts scale by 0.7 and shift by the mpl constants", "[mathtext]") {
    const auto& fm = FontManager::instance();
    const double x_height = FontManager::x_height(kEm);

    const detail::MathBox plain = detail::layout_math("x", kEm);
    const detail::MathBox sup = detail::layout_math("x^2", kEm);
    const detail::MathBox sub = detail::layout_math("x_2", kEm);

    // Superscript: shifted up by sup1 (0.7) * xHeight; the raised digit's top
    // sets the box height (digit metrics at SHRINK_FACTOR 0.7 em).
    const auto two = fm.glyph_metrics(U'2', 0.7 * kEm, detail::FaceId::regular);
    CHECK(sup.height == Catch::Approx(0.7 * x_height + two.ymax).margin(1e-6));
    CHECK(sup.width > plain.width);
    // Subscript: shifted down by sub1 (0.3) * xHeight; '2' has no descender,
    // so the script's drop is the whole depth.
    CHECK(sub.depth == Catch::Approx(0.3 * x_height).margin(0.5));
    CHECK(sub.depth > plain.depth);
    CHECK(sub.height == Catch::Approx(plain.height).margin(1e-6));
}

TEST_CASE("frac stacks numerator and denominator around the bar", "[mathtext]") {
    const detail::MathBox frac = detail::layout_math("\\frac{1}{2}", kEm);
    const detail::MathBox one = detail::layout_math("1", kEm);
    CHECK(frac.height > one.height);
    CHECK(frac.depth > 0.5); // denominator hangs below the baseline
    CHECK_FALSE(frac.path.empty());
}

TEST_CASE("symbols map through the generated tex2uni table", "[mathtext]") {
    // Codepoints pinned from matplotlib 3.10.8 _mathtext_data.tex2uni.
    CHECK_NOTHROW(detail::layout_math("\\alpha\\pi\\Omega\\times\\infty\\leq", kEm));
    CHECK_NOTHROW(detail::layout_math("\\sum_{i=0}^{n} i", kEm));
    CHECK_NOTHROW(detail::layout_math("\\int_0^1 f", kEm));
    CHECK_NOTHROW(detail::layout_math("\\sqrt[3]{x}", kEm));
    CHECK_NOTHROW(detail::layout_math("\\mathrm{d}x + \\mathbf{v}", kEm));
    CHECK_NOTHROW(detail::layout_math("f'(x)", kEm));
}

TEST_CASE("unsupported mathtext names ValueError with the command", "[mathtext]") {
    CHECK_THROWS_AS(detail::layout_math("\\notacommand", kEm), ValueError);
    CHECK_THROWS_AS(detail::layout_math("\\left( x \\right)", kEm), ValueError);
    CHECK_THROWS_AS(detail::layout_math("x^2^3", kEm), ValueError);
    CHECK_THROWS_AS(detail::layout_math("{unclosed", kEm), ValueError);
    try {
        detail::layout_math("\\left( x \\right)", kEm);
    } catch (const ValueError& e) {
        CHECK(std::string(e.what()).find("\\left") != std::string::npos);
    }
}

TEST_CASE("text extent grows for tall math", "[mathtext]") {
    const auto& fm = FontManager::instance();
    const auto plain = fm.text_extent("x", kEm);
    const auto math = fm.text_extent("$\\frac{a}{b}$", kEm);
    CHECK(math.height > plain.height);
    CHECK(math.descent > plain.descent);
    const auto mixed = fm.text_extent("pre $x$ post", kEm);
    CHECK(mixed.width > fm.text_extent("pre  post", kEm).width);
}
