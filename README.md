# graphlib

**A modern C++20 plotting library that mimics [matplotlib](https://matplotlib.org/stable/) —
same names, same defaults, same look.**

If you know `import matplotlib.pyplot as plt`, you already know graphlib:

```cpp
#include <graphlib/pyplot.hpp>
#include <graphlib/util.hpp>
namespace plt = graphlib::pyplot;

int main() {
    const auto x = graphlib::linspace(0.0, 2.0 * std::numbers::pi, 200);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = std::sin(x[i]);

    plt::plot(x, y, {.linewidth = 2.0, .label = "sin(x)"});
    plt::title("Hello graphlib");
    plt::xlabel("x [rad]");
    plt::grid(true);
    plt::savefig("hello.svg");
}
```

![hello](docs/gallery/hello.svg)

Same tab10 colors, same tick algorithm (a faithful port of `MaxNLocator`), same 5% margins,
same dash patterns, same label formatting — pinned by oracle fixtures generated from
matplotlib itself.

| | | |
|---|---|---|
| ![cycle](docs/gallery/color_cycle.svg) | ![markers](docs/gallery/fmt_and_markers.svg) | ![limits](docs/gallery/limits_grid.svg) |

| | |
|---|---|
| ![scatter](docs/gallery/scatter.svg) | ![legend](docs/gallery/legend.svg) |

| | |
|---|---|
| ![bars](docs/gallery/bar_hist.svg) | ![log](docs/gallery/log_scales.svg) |
| ![heatmap](docs/gallery/heatmap_colorbar.svg) | ![contour](docs/gallery/contour_gaussian.svg) |

## Status: v0.4 "Images & Fields"

**SVG and PNG output** (any dpi, via matplotlib's own AGG rasterizer), real DejaVu
typography, and the everyday workhorses: `plot`/`scatter`/`bar`/`hist`/`fill_between`/
`errorbar`/`step`/`pie`, spans and reference lines, `subplots` + `GridSpec` with
`sharex/sharey` and `twinx/twiny`, `tight_layout`, **log scales** with minor ticks,
`legend(loc="best")`, an rcParams + style-sheet system (`ggplot`, `dark_background`),
and the 2D field tools: **`imshow`, `pcolormesh`, `contour`/`contourf`, `colorbar`** with
oracle-exact colormaps (`viridis` & friends) and `Normalize`/`LogNorm`.
Tick and autoscale algorithms are faithful ports pinned by fixtures generated from
matplotlib itself. See [ROADMAP.md](ROADMAP.md) for the ladder to 1.0 and
[docs/PARITY.md](docs/PARITY.md) for coverage.

Zero mandatory dependencies. Platforms: macOS (arm64), Linux (x86_64, arm64),
Windows (x86_64, arm64).

## Build & play

```bash
git clone <this repo> && cd graphlib
cmake --preset release && cmake --build --preset release
./build/release/examples/01_hello_lines   # -> hello.svg, open in any browser
```

Or consume from your own CMake project:

```cmake
include(FetchContent)
FetchContent_Declare(graphlib GIT_REPOSITORY <this repo> GIT_TAG main)
FetchContent_MakeAvailable(graphlib)
target_link_libraries(your_app PRIVATE graphlib::graphlib)
```

Tests (`ctest --preset dev`) compare against committed matplotlib-oracle fixtures and golden
SVG baselines. Contributor guide: [AGENTS.md](AGENTS.md). Architecture:
[docs/DESIGN.md](docs/DESIGN.md).

## License

[MIT](LICENSE)
