#pragma once
// Internal user-facing warnings (mirrors matplotlib's UserWarning to stderr).
// Deliberately minimal: no registry, no filtering — rc-configurable post-1.0
// if anyone asks.
#include <cstdio>
#include <string>

namespace graphlib::detail {

inline void warn(const std::string& message) {
    std::fprintf(stderr, "graphlib warning: %s\n", message.c_str());
}

} // namespace graphlib::detail
