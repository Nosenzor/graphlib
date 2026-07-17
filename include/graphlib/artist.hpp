#pragma once
// Mirrors matplotlib.artist.Artist (docs/DESIGN.md §2). Everything visible is
// an Artist; containers (Figure, Axes) draw their children sorted by zorder.
#include <optional>
#include <string>

namespace graphlib {

class Renderer;

/// Abstract base class for objects that render into a figure canvas.
class Artist {
public:
    virtual ~Artist() = default;

    /// Render through the backend contract only (ground rule #2).
    virtual void draw(Renderer& renderer) = 0;

    // zorder defaults: images 0, patches/collections 1, lines 2, text 3, legend 5.
    double zorder = 0.0;
    bool visible = true;
    std::optional<double> alpha; // multiplies the artist's color alphas when set
    std::string label;           // picked up by legend() (v0.2)
};

} // namespace graphlib
