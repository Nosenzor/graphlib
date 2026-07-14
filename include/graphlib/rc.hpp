#pragma once
// Mirrors matplotlib's rcParams: a typed key->value store seeded with the
// matplotlib defaults (docs/DESIGN.md §12). Artists read rc at creation time,
// exactly like matplotlib; style sheets are overlays (graphlib/style.hpp).
// Process-global and thread-confined, like the pyplot registry.
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "graphlib/color.hpp"

namespace graphlib {

using RcValue =
    std::variant<bool, double, std::string, std::vector<double>, std::vector<std::string>>;

class RcParams {
public:
    static RcParams& instance();

    /// Raw access; throws graphlib::KeyError for unknown keys.
    [[nodiscard]] const RcValue& at(std::string_view key) const;
    /// Set a known key; graphlib::KeyError if unknown, ValueError on type mismatch.
    void set(std::string_view key, RcValue value);
    /// Restore every key to the matplotlib default (style 'default').
    void reset();

    // Typed conveniences (ValueError if the key holds another type).
    [[nodiscard]] double number(std::string_view key) const;
    [[nodiscard]] bool flag(std::string_view key) const;
    [[nodiscard]] const std::string& str(std::string_view key) const;
    /// Parse the stored string as a color spec.
    [[nodiscard]] Color color(std::string_view key) const;
    /// axes.prop_cycle, parsed.
    [[nodiscard]] std::vector<Color> color_cycle() const;
    /// Font size keys hold a number or a named size ('large' = 1.2 x font.size, ...).
    [[nodiscard]] double fontsize(std::string_view key) const;

private:
    RcParams();
    struct Entry {
        std::string key;
        RcValue value;
    };
    std::vector<Entry> entries_; // sorted by key (binary search; deterministic iteration)
    Entry* find(std::string_view key);
    [[nodiscard]] const Entry* find(std::string_view key) const;
};

/// The global store (mirrors `matplotlib.rcParams`).
inline RcParams& rc() { return RcParams::instance(); }

} // namespace graphlib
