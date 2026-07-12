#pragma once
// Internal float formatting — locale-free via std::to_chars (std::format is banned
// in core, see AGENTS.md §Coding standards). Determinism rules: docs/DESIGN.md §13.
#include <array>
#include <charconv>
#include <string>

namespace graphlib::detail {

/// Fixed-point with exactly `decimals` digits (mirrors printf "%1.<n>f", used by
/// ScalarFormatter — trailing zeros are kept, matplotlib keeps them too).
inline std::string fmt_fixed(double v, int decimals) {
    std::array<char, 64> buf{};
    auto [ptr, ec] =
        std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::fixed, decimals);
    if (ec != std::errc{}) {
        return "nan";
    }
    return {buf.data(), ptr};
}

/// Fixed-point capped at `max_decimals`, trailing zeros (and a bare '.') trimmed,
/// "-0" normalized to "0". Used for coordinate emission in vector backends —
/// rounding absorbs cross-platform libm ULP noise (docs/DESIGN.md §13).
inline std::string fmt_trim(double v, int max_decimals = 2) {
    std::string s = fmt_fixed(v, max_decimals);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

} // namespace graphlib::detail
