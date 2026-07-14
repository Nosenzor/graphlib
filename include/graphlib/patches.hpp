#pragma once
// Mirrors matplotlib.patches — filled shapes with face + edge. The foundation
// of bar/hist/spans/pie/fill_between.
#include <vector>

#include "graphlib/artist.hpp"
#include "graphlib/color.hpp"
#include "graphlib/graphics_context.hpp"
#include "graphlib/path.hpp"

namespace graphlib {

class Axes;

/// Base of filled shapes (mirrors matplotlib.patches.Patch); zorder 1.
class Patch : public Artist {
public:
    Patch(); // captures rc patch.* at creation

    Color facecolor;
    Color edgecolor;
    double linewidth = 1.0; // rc patch.linewidth
    LineStyle linestyle{};

    Axes* axes = nullptr;

    /// Geometry in data coordinates.
    [[nodiscard]] virtual Path get_path() const = 0;
    [[nodiscard]] Bbox data_extents() const { return get_path().get_extents(); }

    /// Values the autoscale margins must not cross (mpl sticky_edges):
    /// bar/hist bottoms pin to 0, spans to their extents.
    std::vector<double> sticky_x;
    std::vector<double> sticky_y;

    void draw(Renderer& renderer) override;
};

/// Axis-aligned rectangle anchored at (x, y) with signed width/height.
class Rectangle final : public Patch {
public:
    Rectangle(double x, double y, double width, double height)
        : x(x), y(y), width(width), height(height) {}
    double x;
    double y;
    double width;
    double height;
    [[nodiscard]] Path get_path() const override;
};

/// Closed (or open) polygon through `points`.
class Polygon final : public Patch {
public:
    explicit Polygon(std::vector<Point> pts, bool closed = true)
        : points(std::move(pts)), closed(closed) {}
    std::vector<Point> points;
    bool closed;
    [[nodiscard]] Path get_path() const override;
};

/// Pie-slice wedge: center, radius, theta1..theta2 in degrees CCW.
class Wedge final : public Patch {
public:
    Wedge(Point center, double r, double theta1, double theta2)
        : center(center), r(r), theta1(theta1), theta2(theta2) {}
    Point center;
    double r;
    double theta1;
    double theta2;
    [[nodiscard]] Path get_path() const override; // arc as cubic Béziers
};

} // namespace graphlib
