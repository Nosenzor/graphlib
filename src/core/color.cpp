#include "graphlib/color.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>

#include "graphlib/errors.hpp"

#include "fmt.hpp"

namespace graphlib {

namespace {

struct NamedColor {
    std::string_view name;
    std::uint32_t rgb;
};

#include "css4_colors.inc"
#include "tab_base_colors.inc"

constexpr Color from_rgb24(std::uint32_t v) {
    return {static_cast<double>((v >> 16) & 0xFF) / 255.0,
            static_cast<double>((v >> 8) & 0xFF) / 255.0, static_cast<double>(v & 0xFF) / 255.0,
            1.0};
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::optional<Color> parse_hex(std::string_view s) {
    if (s.empty() || s[0] != '#') {
        return std::nullopt;
    }
    s.remove_prefix(1);
    const size_t n = s.size();
    if (n != 3 && n != 4 && n != 6 && n != 8) {
        return std::nullopt;
    }
    std::array<int, 8> d{};
    for (size_t i = 0; i < n; ++i) {
        d[i] = hex_digit(s[i]);
        if (d[i] < 0) {
            return std::nullopt;
        }
    }
    auto byte2 = [&](size_t i) { return (d[i] * 16 + d[i + 1]) / 255.0; };
    auto byte1 = [&](size_t i) { return (d[i] * 16 + d[i]) / 255.0; }; // '#abc' => 'aabbcc'
    if (n == 6 || n == 8) {
        return Color{byte2(0), byte2(2), byte2(4), n == 8 ? byte2(6) : 1.0};
    }
    return Color{byte1(0), byte1(1), byte1(2), n == 4 ? byte1(3) : 1.0};
}

std::optional<Color> lookup_named(std::string_view s) {
    for (const auto& bc : kBaseColors) {
        if (s.size() == 1 && s[0] == bc.c) {
            return Color{bc.r, bc.g, bc.b, 1.0};
        }
    }
    for (const auto& tc : kTabColors) {
        if (s == tc.name) {
            return from_rgb24(tc.rgb);
        }
    }
    // kCss4Colors is sorted by name (generated) — binary search.
    const auto* it = std::lower_bound(
        std::begin(kCss4Colors), std::end(kCss4Colors), s,
        [](const NamedColor& nc, std::string_view key) { return nc.name < key; });
    if (it != std::end(kCss4Colors) && it->name == s) {
        return from_rgb24(it->rgb);
    }
    return std::nullopt;
}

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool is_cycle_ref(std::string_view s) {
    if (s.size() < 2 || s[0] != 'C') {
        return false;
    }
    return std::all_of(s.begin() + 1, s.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::optional<Color> try_parse(std::string_view spec) {
    if (spec.empty()) {
        return std::nullopt;
    }
    // Order mirrors matplotlib.colors.to_rgba / _to_rgba_no_colorcycle:
    // 1. 'C' + digits -> property cycle (default tab10 until rcParams land, v0.3).
    if (is_cycle_ref(spec)) {
        unsigned long idx = 0;
        std::from_chars(spec.data() + 1, spec.data() + spec.size(), idx);
        return colors::tab10[idx % colors::tab10.size()];
    }
    // 2. 'none' (case-insensitive) -> transparent.
    const std::string lower = to_lower(spec);
    if (lower == "none") {
        return Color::none();
    }
    // 3. Named color: exact match first, lowercase fallback for multi-char specs
    //    (so "RED" works but a single "R" does not — matches mpl).
    if (auto c = lookup_named(spec)) {
        return c;
    }
    if (spec.size() > 1) {
        if (auto c = lookup_named(lower)) {
            return c;
        }
    }
    // 4. Hex.
    if (auto c = parse_hex(spec)) {
        return c;
    }
    // 5. Grayscale string: a float in [0, 1].
    if (const auto gray = detail::parse_double(spec)) {
        if (*gray < 0.0 || *gray > 1.0 || std::isnan(*gray)) {
            throw ValueError("Invalid string grayscale value '" + std::string(spec) +
                             "'. Value must be within 0-1 range");
        }
        return Color{*gray, *gray, *gray, 1.0};
    }
    return std::nullopt;
}

} // namespace

namespace colors {
const std::array<Color, 10> tab10 = {
    from_rgb24(0x1F77B4), from_rgb24(0xFF7F0E), from_rgb24(0x2CA02C), from_rgb24(0xD62728),
    from_rgb24(0x9467BD), from_rgb24(0x8C564B), from_rgb24(0xE377C2), from_rgb24(0x7F7F7F),
    from_rgb24(0xBCBD22), from_rgb24(0x17BECF),
};
} // namespace colors

Color to_color(std::string_view spec) {
    if (auto c = try_parse(spec)) {
        return *c;
    }
    throw ValueError("Invalid RGBA argument: '" + std::string(spec) + "'");
}

bool is_color_like(std::string_view spec) {
    try {
        return try_parse(spec).has_value();
    } catch (const ValueError&) {
        return true; // out-of-range gray IS color-like syntax; mirrors mpl's behavior closely enough
    }
}

std::string to_hex(const Color& c, bool keep_alpha) {
    auto channel = [](double v) {
        const int i = static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
        constexpr char digits[] = "0123456789abcdef";
        return std::string{digits[i / 16], digits[i % 16]};
    };
    std::string out = "#" + channel(c.r) + channel(c.g) + channel(c.b);
    if (keep_alpha && c.a < 1.0) {
        out += channel(c.a);
    }
    return out;
}

} // namespace graphlib
