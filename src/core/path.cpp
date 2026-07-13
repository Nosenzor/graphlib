#include "graphlib/path.hpp"

#include <cmath>

#include "graphlib/errors.hpp"

namespace graphlib {

Path::Path(std::vector<Point> vertices, std::vector<PathCode> codes)
    : vertices_(std::move(vertices)), codes_(std::move(codes)) {
    if (!codes_.empty() && codes_.size() != vertices_.size()) {
        throw ValueError("Path: codes and vertices must have the same length");
    }
}

Path Path::line(std::span<const double> x, std::span<const double> y) {
    if (x.size() != y.size()) {
        throw ValueError("Path::line: x and y must have the same length");
    }
    Path p;
    p.vertices_.reserve(x.size());
    bool has_nan = false;
    for (size_t i = 0; i < x.size(); ++i) {
        if (std::isnan(x[i]) || std::isnan(y[i])) {
            has_nan = true;
            break;
        }
    }
    if (!has_nan) {
        for (size_t i = 0; i < x.size(); ++i) {
            p.vertices_.push_back({x[i], y[i]});
        }
        return p; // implicit polyline, no codes
    }
    // NaN present: build explicit codes, restarting a subpath after each gap.
    p.codes_.reserve(x.size());
    bool pen_up = true;
    for (size_t i = 0; i < x.size(); ++i) {
        if (std::isnan(x[i]) || std::isnan(y[i])) {
            pen_up = true;
            continue;
        }
        p.vertices_.push_back({x[i], y[i]});
        p.codes_.push_back(pen_up ? PathCode::moveto : PathCode::lineto);
        pen_up = false;
    }
    return p;
}

void Path::materialize_codes() {
    if (codes_.empty() && !vertices_.empty()) {
        codes_.assign(vertices_.size(), PathCode::lineto);
        codes_.front() = PathCode::moveto;
    }
}

void Path::move_to(double x, double y) {
    materialize_codes();
    subpath_start_ = vertices_.size();
    vertices_.push_back({x, y});
    codes_.push_back(PathCode::moveto);
}

void Path::line_to(double x, double y) {
    vertices_.push_back({x, y});
    if (!codes_.empty()) {
        codes_.push_back(PathCode::lineto);
    } else if (vertices_.size() == 1) {
        codes_.push_back(PathCode::moveto);
    }
}

void Path::curve3_to(Point c, Point p) {
    materialize_codes();
    vertices_.push_back(c);
    vertices_.push_back(p);
    codes_.push_back(PathCode::curve3);
    codes_.push_back(PathCode::curve3);
}

void Path::curve4_to(Point c1, Point c2, Point p) {
    materialize_codes();
    vertices_.push_back(c1);
    vertices_.push_back(c2);
    vertices_.push_back(p);
    codes_.push_back(PathCode::curve4);
    codes_.push_back(PathCode::curve4);
    codes_.push_back(PathCode::curve4);
}

void Path::close_subpath() {
    if (vertices_.empty()) {
        return;
    }
    materialize_codes();
    vertices_.push_back(vertices_[subpath_start_]);
    codes_.push_back(PathCode::closepoly);
}

Bbox Path::get_extents() const {
    Bbox box = Bbox::null();
    for (size_t i = 0; i < vertices_.size(); ++i) {
        if (code_at(i) == PathCode::closepoly) {
            continue; // vertex value is ignored by convention
        }
        box.update(vertices_[i]);
    }
    return box;
}

Path Path::transformed(const Affine2D& t) const {
    Path out;
    out.vertices_.reserve(vertices_.size());
    for (const Point& p : vertices_) {
        out.vertices_.push_back(t.apply(p));
    }
    out.codes_ = codes_;
    return out;
}

} // namespace graphlib
