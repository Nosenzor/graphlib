#pragma once
// Mirrors matplotlib.transforms — see docs/DESIGN.md §4.
// v0.1 is affine-only; the nonaffine scale slot (log axes) arrives in v0.3 and is
// applied at the artist/axes layer, so the renderer contract stays affine.
#include <array>

namespace graphlib {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

/// 2D affine transform, SVG matrix layout: [a c e; b d f] — apply: x' = a·x + c·y + e.
class Affine2D {
public:
    constexpr Affine2D() = default;
    constexpr Affine2D(double a, double b, double c, double d, double e, double f)
        : m_{a, b, c, d, e, f} {}

    static constexpr Affine2D identity() { return {}; }
    static constexpr Affine2D translation(double tx, double ty) {
        return {1, 0, 0, 1, tx, ty};
    }
    static constexpr Affine2D scaling(double sx, double sy) { return {sx, 0, 0, sy, 0, 0}; }
    static Affine2D rotation_deg(double degrees); // counter-clockwise about the origin

    [[nodiscard]] constexpr Point apply(Point p) const {
        return {m_[0] * p.x + m_[2] * p.y + m_[4], m_[1] * p.x + m_[3] * p.y + m_[5]};
    }

    /// Composition, matplotlib's `A + B` semantics: apply *this* first, then `next`.
    [[nodiscard]] constexpr Affine2D then(const Affine2D& n) const {
        return {n.m_[0] * m_[0] + n.m_[2] * m_[1], n.m_[1] * m_[0] + n.m_[3] * m_[1],
                n.m_[0] * m_[2] + n.m_[2] * m_[3], n.m_[1] * m_[2] + n.m_[3] * m_[3],
                n.m_[0] * m_[4] + n.m_[2] * m_[5] + n.m_[4],
                n.m_[1] * m_[4] + n.m_[3] * m_[5] + n.m_[5]};
    }

    [[nodiscard]] Affine2D inverted() const; // throws ValueError if singular

    [[nodiscard]] constexpr double a() const { return m_[0]; }
    [[nodiscard]] constexpr double b() const { return m_[1]; }
    [[nodiscard]] constexpr double c() const { return m_[2]; }
    [[nodiscard]] constexpr double d() const { return m_[3]; }
    [[nodiscard]] constexpr double e() const { return m_[4]; }
    [[nodiscard]] constexpr double f() const { return m_[5]; }

    friend constexpr bool operator==(const Affine2D&, const Affine2D&) = default;

private:
    std::array<double, 6> m_{1, 0, 0, 1, 0, 0};
};

/// Axis-aligned bounding box (x0,y0)-(x1,y1). A default ("null") Bbox is the
/// accumulation identity: x0=y0=+inf, x1=y1=-inf (mirrors Bbox.null()).
class Bbox {
public:
    Bbox(); // null bbox
    constexpr Bbox(double x0, double y0, double x1, double y1) : x0_(x0), y0_(y0), x1_(x1), y1_(y1) {}

    static Bbox null() { return {}; }
    static constexpr Bbox unit() { return {0, 0, 1, 1}; }
    static constexpr Bbox from_extents(double x0, double y0, double x1, double y1) {
        return {x0, y0, x1, y1};
    }

    [[nodiscard]] bool is_null() const;

    [[nodiscard]] constexpr double x0() const { return x0_; }
    [[nodiscard]] constexpr double y0() const { return y0_; }
    [[nodiscard]] constexpr double x1() const { return x1_; }
    [[nodiscard]] constexpr double y1() const { return y1_; }
    [[nodiscard]] constexpr double width() const { return x1_ - x0_; }
    [[nodiscard]] constexpr double height() const { return y1_ - y0_; }

    void update(Point p); // grow to include p (NaN points are ignored)
    void update(const Bbox& other);

    [[nodiscard]] constexpr bool contains(Point p) const {
        return p.x >= x0_ && p.x <= x1_ && p.y >= y0_ && p.y <= y1_;
    }

    [[nodiscard]] Bbox transformed(const Affine2D& t) const;

    friend constexpr bool operator==(const Bbox&, const Bbox&) = default;

private:
    double x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

/// unit square -> bbox (mirrors BboxTransformTo).
Affine2D bbox_transform_to(const Bbox& box);
/// bbox -> unit square (mirrors BboxTransformFrom); throws ValueError on a degenerate box.
Affine2D bbox_transform_from(const Bbox& box);

} // namespace graphlib
