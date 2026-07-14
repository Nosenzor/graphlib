// Layout: sharex column, twinx, fill_between band, spans, steps.
#include <cmath>
#include <numbers>
#include <vector>

#include <graphlib/graphlib.hpp>

namespace plt = graphlib::pyplot;
using graphlib::linspace;

int main() {
    auto& fig = plt::figure({.figsize = {{9.0, 6.5}}});
    auto grid = fig.subplots(2, 1, {.sharex = true}); // inner row loses x labels

    const auto t = linspace(0.0, 4.0 * std::numbers::pi, 400);
    std::vector<double> sig(t.size()), env_hi(t.size()), env_lo(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        const double env = std::exp(-0.18 * t[i]);
        sig[i] = env * std::sin(3.0 * t[i]);
        env_hi[i] = env;
        env_lo[i] = -env;
    }
    grid[0][0]->fill_between(t, env_hi, env_lo, {.color = "tab:blue", .alpha = 0.2});
    grid[0][0]->plot(t, sig, {.label = "signal"});
    grid[0][0]->axhline(0.0, 0, 1, {.color = "0.4", .linewidth = 0.8});
    grid[0][0]->set_title("damped oscillator");
    grid[0][0]->set_ylabel("amplitude");
    auto& rate = grid[0][0]->twinx();
    std::vector<double> energy(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        energy[i] = std::exp(-0.36 * t[i]);
    }
    rate.plot(t, energy, "C3--");
    rate.set_ylabel("energy");

    std::vector<double> steps(t.size());
    for (size_t i = 0; i < t.size(); ++i) {
        steps[i] = std::floor(2.0 + 1.8 * std::sin(0.9 * t[i]));
    }
    grid[1][0]->step(t, steps, "", {.color = "tab:green", .label = "quantized"});
    grid[1][0]->axvspan(4.0, 6.0, {.color = "tab:orange", .alpha = 0.25});
    grid[1][0]->set_xlabel("t [s]");
    grid[1][0]->set_ylabel("level");
    grid[1][0]->grid(true);

    fig.suptitle("layout: sharex + twinx + spans");
    fig.tight_layout();
    fig.savefig("subplots_dashboard.svg");
    fig.savefig("subplots_dashboard.png", {.dpi = 130});
    return 0;
}
