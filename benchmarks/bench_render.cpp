// Render benchmarks (ROADMAP v0.7 "Fast Path"). Deterministic data only —
// results are comparable across runs and machines of the same class.
// Targets: 10M-point line savefig < 1 s; 1M-point scatter redraw at 60 fps
// (16.7 ms) on an M-series Mac.
#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "graphlib/backend/agg.hpp"
#include "graphlib/backend/svg.hpp"
#include "graphlib/figure.hpp"
#include "graphlib/util.hpp"

using namespace graphlib;

namespace {

std::vector<double> wave(size_t n) {
    std::vector<double> y(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * 0.001;
        y[i] = std::sin(t) + 0.3 * std::sin(t * 7.7);
    }
    return y;
}

void BM_line_draw(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::vector<double> x = linspace(0.0, 100.0, n);
    const std::vector<double> y = wave(n);
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.plot(x, y);
    for (auto _ : state) {
        AggRenderer renderer(640, 480, 100.0);
        fig.draw(renderer);
        benchmark::DoNotOptimize(renderer);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(n));
}
BENCHMARK(BM_line_draw)->Arg(1000)->Arg(100000)->Arg(1000000)->Unit(benchmark::kMillisecond);

void BM_line_savefig_png(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::vector<double> x = linspace(0.0, 100.0, n);
    const std::vector<double> y = wave(n);
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.plot(x, y);
    const std::string out =
        (std::filesystem::temp_directory_path() / "graphlib_bench.png").string();
    for (auto _ : state) {
        fig.savefig(out);
    }
    std::remove(out.c_str());
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(n));
}
BENCHMARK(BM_line_savefig_png)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kMillisecond)
    ->MinWarmUpTime(0.5);

void BM_scatter_draw(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<double> x(n);
    std::vector<double> y(n);
    for (size_t i = 0; i < n; ++i) { // deterministic blue-noise-ish spread
        x[i] = std::fmod(static_cast<double>(i) * 0.61803398875, 1.0);
        y[i] = std::fmod(static_cast<double>(i) * 0.38196601125, 1.0);
    }
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.scatter(x, y);
    for (auto _ : state) {
        AggRenderer renderer(640, 480, 100.0);
        fig.draw(renderer);
        benchmark::DoNotOptimize(renderer);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(n));
}
BENCHMARK(BM_scatter_draw)->Arg(10000)->Arg(100000)->Arg(1000000)->Unit(benchmark::kMillisecond);

void BM_line_svg(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    const std::vector<double> x = linspace(0.0, 100.0, n);
    const std::vector<double> y = wave(n);
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.plot(x, y);
    for (auto _ : state) {
        SvgRenderer renderer(fig.figsize[0] * 72.0, fig.figsize[1] * 72.0);
        fig.draw(renderer);
        benchmark::DoNotOptimize(renderer.finalize());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(n));
}
BENCHMARK(BM_line_svg)->Arg(100000)->Unit(benchmark::kMillisecond);

void BM_pcolormesh_draw(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0)); // n x n cells
    std::vector<double> edges(n + 1);
    for (size_t i = 0; i <= n; ++i) {
        edges[i] = static_cast<double>(i);
    }
    std::vector<double> z(n * n);
    for (size_t i = 0; i < z.size(); ++i) {
        z[i] = std::sin(static_cast<double>(i) * 0.01);
    }
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.pcolormesh(edges, edges, z, {});
    for (auto _ : state) {
        AggRenderer renderer(640, 480, 100.0);
        fig.draw(renderer);
        benchmark::DoNotOptimize(renderer);
    }
}
BENCHMARK(BM_pcolormesh_draw)->Arg(100)->Arg(300)->Unit(benchmark::kMillisecond);

void BM_text_mathtext(benchmark::State& state) {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_title("$\\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}$ and plain text");
    ax.set_xlabel("elapsed $t$ (s)");
    ax.set_ylabel("$\\sigma = \\sqrt{\\sum (x_i - \\mu)^2 / n}$");
    for (int k = 0; k < 8; ++k) {
        ax.text(0.1, 0.1 + 0.1 * k, "label $x_{" + std::to_string(k) + "}$");
    }
    for (auto _ : state) {
        AggRenderer renderer(640, 480, 100.0);
        fig.draw(renderer);
        benchmark::DoNotOptimize(renderer);
    }
}
BENCHMARK(BM_text_mathtext)->Unit(benchmark::kMillisecond);

void BM_figure_lifecycle(benchmark::State& state) {
    const std::vector<double> v{0.0, 1.0, 0.5, 2.0};
    for (auto _ : state) {
        Figure fig;
        Axes& ax = fig.add_subplot();
        ax.plot(v, v);
        benchmark::DoNotOptimize(fig);
    }
}
BENCHMARK(BM_figure_lifecycle)->Unit(benchmark::kMicrosecond);

} // namespace

BENCHMARK_MAIN();
