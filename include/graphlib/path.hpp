#pragma once
// Mirrors matplotlib.path.Path — codes and semantics in docs/DESIGN.md §5.
#include <cstdint>
#include <span>
#include <vector>

#include "graphlib/transforms.hpp"

namespace graphlib {

enum class PathCode : std::uint8_t {
    stop = 0,
    moveto = 1,
    lineto = 2,
    curve3 = 3,    // quadratic Bézier: 1 control + endpoint (2 vertices)
    curve4 = 4,    // cubic Bézier: 2 controls + endpoint (3 vertices)
    closepoly = 79 // vertex value is ignored
};

/// Vertex list + optional per-vertex codes. No codes => implicit polyline
/// (MOVETO then LINETOs). The single geometry currency of the library.
class Path {
public:
    Path() = default;
    explicit Path(std::vector<Point> vertices) : vertices_(std::move(vertices)) {}
    Path(std::vector<Point> vertices, std::vector<PathCode> codes); // throws ValueError on size mismatch

    /// Polyline from x/y spans; NaN in either coordinate opens a gap (mpl behavior).
    /// Throws ValueError if x and y differ in length.
    static Path line(std::span<const double> x, std::span<const double> y);

    void move_to(double x, double y);
    void line_to(double x, double y);
    void curve3_to(Point c, Point p); // quadratic (TrueType glyph contours)
    void curve4_to(Point c1, Point c2, Point p);
    /// Close the current subpath (back to its MOVETO, not the first vertex).
    void close_subpath();

    [[nodiscard]] bool empty() const { return vertices_.empty(); }
    [[nodiscard]] size_t size() const { return vertices_.size(); }
    [[nodiscard]] std::span<const Point> vertices() const { return vertices_; }
    [[nodiscard]] bool has_codes() const { return !codes_.empty(); }
    [[nodiscard]] std::span<const PathCode> codes() const { return codes_; }
    [[nodiscard]] PathCode code_at(size_t i) const {
        return codes_.empty() ? (i == 0 ? PathCode::moveto : PathCode::lineto) : codes_[i];
    }

    /// Bounds over vertices (control points included — conservative for Béziers;
    /// exact-extrema bounds can come with v0.4 if an artist needs them).
    [[nodiscard]] Bbox get_extents() const;

    [[nodiscard]] Path transformed(const Affine2D& t) const;

    /// Vertex-wise remap (nonlinear scale pre-transform — DESIGN §4).
    template <class Fn> [[nodiscard]] Path mapped(Fn&& fn) const {
        Path out;
        out.vertices_.reserve(vertices_.size());
        for (const Point& p : vertices_) {
            out.vertices_.push_back(fn(p));
        }
        out.codes_ = codes_;
        out.subpath_start_ = subpath_start_;
        return out;
    }

private:
    void materialize_codes();

    std::vector<Point> vertices_;
    std::vector<PathCode> codes_;
    size_t subpath_start_ = 0; // index of the current subpath's MOVETO vertex
};

} // namespace graphlib
