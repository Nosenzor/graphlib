#include "graphlib/markers.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "graphlib/errors.hpp"

namespace graphlib {

namespace {

struct MarkerDef {
    std::string_view name;
    std::vector<Point> vertices;
    std::vector<std::uint8_t> codes;
    bool filled;
};

#include "marker_paths.inc"

// All matplotlib string marker characters, shipped or not (fmt parser needs the full set).
constexpr std::string_view kAllMarkerChars = ".,ov^<>12348spP*hH+xXDd|_";

Marker make_marker(const MarkerDef& def) {
    std::vector<PathCode> codes;
    codes.reserve(def.codes.size());
    for (const auto c : def.codes) {
        codes.push_back(static_cast<PathCode>(c));
    }
    return Marker{def.name, Path(def.vertices, std::move(codes)), def.filled};
}

} // namespace

const Marker& get_marker(std::string_view token) {
    static const std::vector<Marker> markers = [] {
        std::vector<Marker> out;
        for (const auto& def : kMarkerDefs) {
            out.push_back(make_marker(def));
        }
        return out;
    }();
    for (const auto& m : markers) {
        if (m.name == token) {
            return m;
        }
    }
    if (token.size() == 1 && is_marker_char(token[0])) {
        throw ValueError("marker '" + std::string(token) +
                         "' is valid in matplotlib but not shipped yet (full set: v0.2)");
    }
    throw ValueError("Unrecognized marker style '" + std::string(token) + "'");
}

bool is_marker_char(char c) { return kAllMarkerChars.find(c) != std::string_view::npos; }

} // namespace graphlib
