#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "graphlib/figure.hpp"
#include "graphlib/ticker.hpp"

#include "fixtures/fixtures.inc"

using namespace graphlib;

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}
} // namespace

TEST_CASE("log-scale ticks match the matplotlib oracle", "[log]") {
    for (const auto& c : fixtures::log_ticks) {
        INFO("range [" << c.vmin << ", " << c.vmax << "]");
        Figure fig;
        Axes& ax = fig.add_subplot();
        ax.set_xscale("log");
        ax.set_xlim(c.vmin, c.vmax);
        // Compare untrimmed locator output (the fixture records get_xticks()).
        LogLocator loc;
        loc.set_tick_space(9); // default axes width -> 9, like the oracle run
        const auto got = loc.tick_values(c.vmin, c.vmax);
        REQUIRE(got.size() == c.major.size());
        for (size_t i = 0; i < got.size(); ++i) {
            INFO("major " << i);
            CHECK(close(got[i], c.major[i]));
        }
        // Minor subs: the fixture records the untrimmed locator output too.
        LogLocator minor_loc{/*minor_subs=*/true};
        minor_loc.set_tick_space(9);
        const auto minor = minor_loc.tick_values(c.vmin, c.vmax);
        REQUIRE(minor.size() == c.minor.size());
        for (size_t i = 0; i < minor.size(); ++i) {
            INFO("minor " << i << " got " << minor[i] << " want " << c.minor[i]);
            CHECK(close(minor[i], c.minor[i]));
        }
    }
}

TEST_CASE("log labels use unicode superscript decades (D10)", "[log]") {
    const LogFormatter fmt;
    const std::vector<double> locs{0.01, 1.0, 100.0, 5.0};
    const auto labels = fmt.format_ticks(locs, 0.01, 100.0);
    CHECK(labels[0] == "10⁻²");
    CHECK(labels[1] == "10⁰");
    CHECK(labels[2] == "10²");
    CHECK(labels[3].empty()); // non-decade: unlabeled
}

TEST_CASE("AutoMinorLocator matches the matplotlib oracle", "[minor]") {
    for (const auto& c : fixtures::minor_ticks) {
        INFO("range [" << c.vmin << ", " << c.vmax << "]");
        Figure fig;
        Axes& ax = fig.add_subplot();
        ax.set_xlim(c.vmin, c.vmax);
        ax.minorticks_on();
        const auto minor = ax.xaxis().compute_minor_ticks(ax);
        REQUIRE(minor.size() == c.minor.size());
        for (size_t i = 0; i < minor.size(); ++i) {
            INFO("minor " << i << " got " << minor[i] << " want " << c.minor[i]);
            CHECK(close(minor[i], c.minor[i]));
        }
    }
}

TEST_CASE("log autoscale clips to positive data with log-space margins", "[log]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> x{1.0, 10.0, 100.0, 1000.0};
    ax.plot(x, x);
    ax.set_xscale("log");
    const auto [x0, x1] = ax.get_xlim();
    // mpl: margins in log space -> factor 10^(3*0.05) ≈ 1.4125 on each side
    CHECK(close(x0, std::pow(10.0, 0.0 - 0.15)));
    CHECK(close(x1, std::pow(10.0, 3.0 + 0.15)));

    Axes& ax2 = fig.add_axes(Bbox::from_extents(0.1, 0.1, 0.9, 0.9));
    const std::vector<double> mixed_x{-5.0, 0.5, 8.0}; // nonpositive data present
    ax2.plot(mixed_x, mixed_x);
    ax2.set_yscale("log");
    const auto [y0, y1] = ax2.get_ylim();
    CHECK(y0 > 0.0); // clipped to minpos
    CHECK(y0 < 0.5);
    CHECK(y1 > 8.0);
}
