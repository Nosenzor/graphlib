#pragma once
// Mirrors matplotlib.colors — the color spec grammar is docs/DESIGN.md §9.
#include <array>
#include <string>
#include <string_view>

namespace graphlib {

/// RGBA, components in [0, 1].
struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;

    static constexpr Color none() { return {0, 0, 0, 0}; }
    [[nodiscard]] constexpr Color with_alpha(double alpha) const { return {r, g, b, alpha}; }
    friend constexpr bool operator==(const Color&, const Color&) = default;
};

namespace colors {
/// The default property cycle (rcParams axes.prop_cycle) — tab10.
extern const std::array<Color, 10> tab10;
} // namespace colors

/// Parse any matplotlib color spec: 'r', 'red', 'tab:blue', '#1f77b4', '#abc',
/// '#rrggbbaa', '0.8' (gray), 'C0'..'Cn' (property cycle), 'none'.
/// Throws graphlib::ValueError on anything else.
/// NOTE(v0.3): 'CN' resolves against the default tab10 cycle until rcParams land.
Color to_color(std::string_view spec);

/// True if `spec` parses as a color (mirrors matplotlib.colors.is_color_like).
bool is_color_like(std::string_view spec);

/// "#rrggbb" (or "#rrggbbaa" when keep_alpha and a < 1), lowercase.
std::string to_hex(const Color& c, bool keep_alpha = false);

} // namespace graphlib
