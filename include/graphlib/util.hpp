#pragma once
// Small data helpers for examples and quick scripts (numpy stand-ins).
#include <cstddef>
#include <vector>

namespace graphlib {

/// n evenly spaced values from a to b inclusive (numpy.linspace semantics; n==1 -> {a}).
[[nodiscard]] inline std::vector<double> linspace(double a, double b, std::size_t n) {
    std::vector<double> out;
    out.reserve(n);
    if (n == 1) {
        out.push_back(a);
        return out;
    }
    const double step = (b - a) / static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(a + step * static_cast<double>(i));
    }
    return out;
}

} // namespace graphlib
