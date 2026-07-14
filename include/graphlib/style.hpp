#pragma once
// Mirrors matplotlib.style: style sheets are rcParams overlays.
#include <string_view>
#include <vector>

namespace graphlib::style {

/// Apply a style sheet: 'default', 'ggplot', 'dark_background'.
/// Resets to defaults first, then applies the sheet (mirrors plt.style.use
/// semantics for a single style). Throws graphlib::KeyError on unknown names.
void use(std::string_view name);

/// Names accepted by use().
std::vector<std::string_view> available();

} // namespace graphlib::style
