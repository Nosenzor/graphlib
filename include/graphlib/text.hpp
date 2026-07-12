#pragma once
// Mirrors matplotlib.text.Text — v0.1 subset: position + alignment + rotation.
// Metrics-based layout arrives with the FontManager in v0.2.
#include <string>

#include "graphlib/artist.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/color.hpp"

namespace graphlib {

class Axes;

class Text final : public Artist {
public:
    Text() { zorder = 3.0; }

    std::string text;
    Point position;                  // interpretation depends on `coords`
    enum class Coords { data, pixels } coords = Coords::pixels;
    double fontsize = 10.0;          // rc font.size, points
    Color color{0, 0, 0, 1};        // rc text.color
    HAlign ha = HAlign::left;
    VAlign va = VAlign::baseline;
    double rotation_deg = 0.0;       // CCW
    bool bold = false;

    /// Owning axes; required when coords == data (set by Axes::text()).
    Axes* axes = nullptr;

    void draw(Renderer& renderer) override;
};

} // namespace graphlib
