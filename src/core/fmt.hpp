#pragma once
// Internal float formatting/parsing — locale-free via std::to_chars (std::format
// is banned in core, see AGENTS.md §Coding standards). Determinism: DESIGN.md §13.
#include <array>
#include <cctype>
#include <charconv>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace graphlib::detail {

/// Locale-independent full-string -> double ("0.8", "-1e-3", "+.5"; leading
/// whitespace or trailing junk rejected). Apple's libc++ ships no floating-point
/// std::from_chars (no __cpp_lib_to_chars — cross-platform skill), so the
/// fallback parses via a classic-locale stream. Define GRAPHLIB_NO_FP_FROM_CHARS
/// to force the fallback on toolchains that do have from_chars (fallback tests).
inline std::optional<double> parse_double(std::string_view s) {
    if (!s.empty() && s.front() == '+') { // python float() accepts '+'; from_chars doesn't
        s.remove_prefix(1);
    }
    if (s.empty() ||
        !(std::isdigit(static_cast<unsigned char>(s.front())) != 0 || s.front() == '.' ||
          s.front() == '-' || s.front() == 'n' || s.front() == 'i')) {
        return std::nullopt; // also keeps stream semantics aligned with from_chars
    }
#if defined(__cpp_lib_to_chars) && !defined(GRAPHLIB_NO_FP_FROM_CHARS)
    double v = 0.0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        return v;
    }
    return std::nullopt;
#else
    std::istringstream stream{std::string(s)};
    stream.imbue(std::locale::classic());
    double v = 0.0;
    stream >> v;
    if (!stream.fail() && stream.eof()) {
        return v;
    }
    return std::nullopt;
#endif
}

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
