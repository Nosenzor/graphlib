// Batch port of matplotlib's streaming PathSimplifier (path_converters.h,
// mpl 3.10.8). The state machine and emission order are kept 1:1 — the only
// change is emitting into an output Path instead of a 9-slot pop queue, and
// an inline NaN pre-pass standing in for PathNanRemover (gaps -> moveto).
#include "core/path_simplify.hpp"

#include <cmath>

#include "graphlib/rc.hpp"

namespace graphlib::detail {

bool should_simplify(const Path& path) {
    if (path.size() < 128) {
        return false;
    }
    if (path.has_codes()) {
        for (const PathCode c : path.codes()) {
            if (c != PathCode::moveto && c != PathCode::lineto) {
                return false; // curves / closepoly: mpl skips entirely
            }
        }
    }
    return rc().flag("path.simplify") && rc().number("path.simplify_threshold") > 0.0;
}

namespace {

enum class Cmd { moveto, lineto, stop };

/// Iterates (cmd, x, y) like mpl's nan-removed vertex source: a non-finite
/// vertex ends the current subpath and the next finite one arrives as moveto.
class VertexSource {
public:
    explicit VertexSource(const Path& path) : path_(path) {}

    Cmd next(double* x, double* y) {
        while (i_ < path_.size()) {
            const Point v = path_.vertices()[i_];
            const PathCode code = path_.code_at(i_);
            ++i_;
            if (!std::isfinite(v.x) || !std::isfinite(v.y)) {
                gap_ = true;
                continue;
            }
            *x = v.x;
            *y = v.y;
            if (gap_ || code == PathCode::moveto) {
                gap_ = false;
                return Cmd::moveto;
            }
            return Cmd::lineto;
        }
        return Cmd::stop;
    }

private:
    const Path& path_;
    size_t i_ = 0;
    bool gap_ = false;
};

} // namespace

Path simplify_path(const Path& path, double threshold_px) {
    Path out;
    const auto emit = [&out](Cmd cmd, double x, double y) {
        if (cmd == Cmd::moveto) {
            out.move_to(x, y);
        } else {
            out.line_to(x, y);
        }
    };

    VertexSource source(path);
    const double simplify_threshold = threshold_px * threshold_px;

    // State (names kept from mpl for line-by-line comparability).
    bool moveto = true;
    bool after_moveto = false;
    bool clipped = false;
    double lastx = 0.0;
    double lasty = 0.0;
    double origdx = 0.0;
    double origdy = 0.0;
    double origdNorm2 = 0.0;
    double dnorm2ForwardMax = 0.0;
    double dnorm2BackwardMax = 0.0;
    bool lastForwardMax = false;
    bool lastBackwardMax = false;
    double nextX = 0.0;
    double nextY = 0.0;
    double nextBackwardX = 0.0;
    double nextBackwardY = 0.0;
    double currVecStartX = 0.0;
    double currVecStartY = 0.0;
    double lastWrittenX = 0.0; // m_queue[m_queue_write - 1] equivalent
    double lastWrittenY = 0.0;

    const auto push = [&](double x, double y) {
        const bool need_backward = dnorm2BackwardMax > 0.0;
        if (need_backward) {
            if (lastForwardMax) {
                emit(Cmd::lineto, nextBackwardX, nextBackwardY);
                emit(Cmd::lineto, nextX, nextY);
                lastWrittenX = nextX;
                lastWrittenY = nextY;
            } else {
                emit(Cmd::lineto, nextX, nextY);
                emit(Cmd::lineto, nextBackwardX, nextBackwardY);
                lastWrittenX = nextBackwardX;
                lastWrittenY = nextBackwardY;
            }
        } else {
            emit(Cmd::lineto, nextX, nextY);
            lastWrittenX = nextX;
            lastWrittenY = nextY;
        }
        if (clipped) {
            emit(Cmd::moveto, lastx, lasty);
            lastWrittenX = lastx;
            lastWrittenY = lasty;
        } else if (!lastForwardMax && !lastBackwardMax) {
            // Would be a moveto if not for stroke-join artifacts (mpl note).
            emit(Cmd::lineto, lastx, lasty);
            lastWrittenX = lastx;
            lastWrittenY = lasty;
        }
        // Reset for the next accumulated line.
        origdx = x - lastx;
        origdy = y - lasty;
        origdNorm2 = origdx * origdx + origdy * origdy;
        dnorm2ForwardMax = origdNorm2;
        lastForwardMax = true;
        currVecStartX = lastWrittenX;
        currVecStartY = lastWrittenY;
        lastx = nextX = x;
        lasty = nextY = y;
        dnorm2BackwardMax = 0.0;
        lastBackwardMax = false;
        clipped = false;
    };

    double x = 0.0;
    double y = 0.0;
    Cmd cmd;
    while ((cmd = source.next(&x, &y)) != Cmd::stop) {
        if (moveto || cmd == Cmd::moveto) {
            if (origdNorm2 != 0.0 && !after_moveto) {
                push(x, y);
            }
            after_moveto = true;
            lastx = x;
            lasty = y;
            moveto = false;
            origdNorm2 = 0.0;
            dnorm2BackwardMax = 0.0;
            clipped = true;
            continue;
        }
        after_moveto = false;

        if (origdNorm2 == 0.0) { // no reference vector yet: start one
            if (clipped) {
                emit(Cmd::moveto, lastx, lasty);
                lastWrittenX = lastx;
                lastWrittenY = lasty;
                clipped = false;
            }
            origdx = x - lastx;
            origdy = y - lasty;
            origdNorm2 = origdx * origdx + origdy * origdy;
            dnorm2ForwardMax = origdNorm2;
            dnorm2BackwardMax = 0.0;
            lastForwardMax = true;
            lastBackwardMax = false;
            currVecStartX = lastx;
            currVecStartY = lasty;
            nextX = lastx = x;
            nextY = lasty = y;
            continue;
        }

        // Perpendicular distance of v (last written point -> current) from
        // the reference vector o:  p = v - (o.v)o/(o.o).
        const double totdx = x - currVecStartX;
        const double totdy = y - currVecStartY;
        const double totdot = origdx * totdx + origdy * totdy;
        const double paradx = totdot * origdx / origdNorm2;
        const double parady = totdot * origdy / origdNorm2;
        const double perpdx = totdx - paradx;
        const double perpdy = totdy - parady;
        const double perpdNorm2 = perpdx * perpdx + perpdy * perpdy;

        if (perpdNorm2 < simplify_threshold) {
            const double paradNorm2 = paradx * paradx + parady * parady;
            lastForwardMax = false;
            lastBackwardMax = false;
            if (totdot > 0.0) {
                if (paradNorm2 > dnorm2ForwardMax) {
                    lastForwardMax = true;
                    dnorm2ForwardMax = paradNorm2;
                    nextX = x;
                    nextY = y;
                }
            } else {
                if (paradNorm2 > dnorm2BackwardMax) {
                    lastBackwardMax = true;
                    dnorm2BackwardMax = paradNorm2;
                    nextBackwardX = x;
                    nextBackwardY = y;
                }
            }
            lastx = x;
            lasty = y;
            continue;
        }

        push(x, y); // too far off the line: flush and restart from here
    }

    // Tail flush (mpl's path_cmd_stop block).
    if (origdNorm2 != 0.0) {
        emit((moveto || after_moveto) ? Cmd::moveto : Cmd::lineto, nextX, nextY);
        if (dnorm2BackwardMax > 0.0) {
            emit((moveto || after_moveto) ? Cmd::moveto : Cmd::lineto, nextBackwardX,
                 nextBackwardY);
        }
        moveto = false;
        after_moveto = false;
    }
    if (!(moveto && out.empty())) { // mpl always emits the final point
        emit((moveto || after_moveto) ? Cmd::moveto : Cmd::lineto, lastx, lasty);
    }
    return out;
}

} // namespace graphlib::detail
