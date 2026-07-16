#pragma once
// Mirrors matplotlib.text.Text and .Annotation.
#include <optional>
#include <string>
#include <utility>

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

/// Text box behind annotate text (mirrors the `bbox` dict: boxstyle
/// 'round'/'square' with pad in fractions of the font size).
struct TextBboxProps {
    std::string boxstyle = "round";        ///< "round" or "square"
    double pad = 0.3;                      ///< x fontsize (BoxStyle default)
    std::optional<double> rounding_size;   ///< corner radius, x fontsize; default: pad
    std::string facecolor;                 ///< rc patch.facecolor
    std::string edgecolor;                 ///< rc patch.edgecolor
    std::optional<double> linewidth;       ///< rc patch.linewidth
    double alpha = 1.0;
};

/// Arrow options for Axes::annotate (mirrors the `arrowprops` dict with an
/// `arrowstyle` key; the legacy width/headwidth YAArrow form is not supported).
struct ArrowProps {
    std::string arrowstyle = "->";         ///< "-", "->" or "-|>" (ArrowStyle subset)
    double shrinkA = 2.0;                  ///< tail gap, points (FancyArrowPatch default)
    double shrinkB = 2.0;                  ///< head gap, points
    std::optional<double> mutation_scale;  ///< head scale, points; default: text fontsize
    std::string color;                     ///< face *and* edge; default: patch.* rc colors
    std::optional<double> linewidth;       ///< rc patch.linewidth
    std::pair<double, double> relpos{0.5, 0.5}; ///< tail anchor inside the text bbox
};

/// Mirrors matplotlib.text.Annotation: a Text placed relative to an annotated
/// point `xy`, optionally connected to it by a FancyArrowPatch-style arrow.
/// (Composition instead of inheritance: the text artist is the `text` member.)
class Annotation final : public Artist {
public:
    Annotation() { zorder = 3.0; }

    Text text;      ///< annotation text; its position is computed at draw time
    Point xy;       ///< annotated point, in `xycoords`
    Point xytext;   ///< text anchor, in `textcoords`
    enum class CoordSys { data, axes_fraction, offset_points };
    CoordSys xycoords = CoordSys::data;
    CoordSys textcoords = CoordSys::data; ///< offset_points is relative to xy
    std::optional<ArrowProps> arrowprops;
    std::optional<TextBboxProps> bbox_props; ///< mpl's `bbox=` kwarg

    Axes* axes = nullptr;

    void draw(Renderer& renderer) override;
};

} // namespace graphlib
