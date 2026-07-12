#include "graphlib/transforms.hpp"

#include <cmath>
#include <limits>
#include <numbers>

#include "graphlib/errors.hpp"

namespace graphlib {

Affine2D Affine2D::rotation_deg(double degrees) {
    const double t = degrees * std::numbers::pi / 180.0;
    const double c = std::cos(t);
    const double s = std::sin(t);
    return {c, s, -s, c, 0, 0};
}

Affine2D Affine2D::inverted() const {
    const double det = m_[0] * m_[3] - m_[1] * m_[2];
    if (std::abs(det) < std::numeric_limits<double>::min()) {
        throw ValueError("Affine2D::inverted: singular transform");
    }
    const double ia = m_[3] / det;
    const double ib = -m_[1] / det;
    const double ic = -m_[2] / det;
    const double id = m_[0] / det;
    return {ia, ib, ic, id, (m_[2] * m_[5] - m_[3] * m_[4]) / det,
            (m_[1] * m_[4] - m_[0] * m_[5]) / det};
}

Bbox::Bbox()
    : x0_(std::numeric_limits<double>::infinity()), y0_(std::numeric_limits<double>::infinity()),
      x1_(-std::numeric_limits<double>::infinity()), y1_(-std::numeric_limits<double>::infinity()) {}

bool Bbox::is_null() const { return x0_ > x1_ || y0_ > y1_; }

void Bbox::update(Point p) {
    if (std::isnan(p.x) || std::isnan(p.y)) {
        return;
    }
    x0_ = std::min(x0_, p.x);
    y0_ = std::min(y0_, p.y);
    x1_ = std::max(x1_, p.x);
    y1_ = std::max(y1_, p.y);
}

void Bbox::update(const Bbox& other) {
    if (other.is_null()) {
        return;
    }
    update(Point{other.x0_, other.y0_});
    update(Point{other.x1_, other.y1_});
}

Bbox Bbox::transformed(const Affine2D& t) const {
    Bbox out = Bbox::null();
    out.update(t.apply({x0_, y0_}));
    out.update(t.apply({x1_, y0_}));
    out.update(t.apply({x0_, y1_}));
    out.update(t.apply({x1_, y1_}));
    return out;
}

Affine2D bbox_transform_to(const Bbox& box) {
    return {box.width(), 0, 0, box.height(), box.x0(), box.y0()};
}

Affine2D bbox_transform_from(const Bbox& box) {
    if (box.width() == 0.0 || box.height() == 0.0) {
        throw ValueError("bbox_transform_from: degenerate bbox");
    }
    return bbox_transform_to(box).inverted();
}

} // namespace graphlib
