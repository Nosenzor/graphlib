#pragma once
// Mirrors matplotlib.cm / matplotlib.colors norms — LUTs generated from mpl.
#include <cstdint>
#include <span>
#include <string_view>

#include "graphlib/color.hpp"

namespace graphlib {

/// A lookup-table colormap (mirrors matplotlib.colors.Colormap semantics):
/// input in [0, 1]; below 0 -> under, above 1 -> over, NaN -> bad.
class Colormap {
public:
    // The qualitative flag is carried by the generated table for documentation;
    // lookup is the same floor(x*N) for both kinds (mpl semantics).
    Colormap(std::string_view name, std::span<const std::uint32_t> lut, bool /*qualitative*/);

    [[nodiscard]] Color operator()(double x) const;
    [[nodiscard]] std::string_view name() const { return name_; }
    [[nodiscard]] size_t size() const { return lut_.size(); }

    Color under; // defaults: first/last LUT entry, transparent bad (like mpl)
    Color over;
    Color bad = Color::none();

private:
    std::string_view name_;
    std::span<const std::uint32_t> lut_;
};

/// Registry lookup: viridis, plasma, inferno, magma, cividis, coolwarm, RdBu,
/// gray, jet, tab10, tab20. Throws graphlib::ValueError for unknown names.
const Colormap& get_cmap(std::string_view name);

/// Linear data->[0,1] normalization (no clipping, like mpl's default).
struct Normalize {
    double vmin = 0.0;
    double vmax = 1.0;
    [[nodiscard]] double operator()(double v) const { return (v - vmin) / (vmax - vmin); }
};

/// Logarithmic normalization (mirrors matplotlib.colors.LogNorm).
struct LogNorm {
    double vmin = 1.0;
    double vmax = 10.0;
    [[nodiscard]] double operator()(double v) const;
};

} // namespace graphlib
