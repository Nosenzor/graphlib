#include "graphlib/lines.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "graphlib/axes.hpp"
#include "graphlib/backend/renderer.hpp"
#include "graphlib/errors.hpp"

namespace graphlib {

namespace detail {

ParsedFormat parse_plot_format(std::string_view fmt) {
    ParsedFormat out;
    if (fmt.empty()) {
        return out;
    }
    // Whole string as a colorspec first ("red", "C2", "#abc"), excluding "0"/"1"
    // which mean the tri_down/tri_up markers in a format string (mpl rule).
    if (fmt != "0" && fmt != "1" && is_color_like(fmt)) {
        out.color = std::string(fmt);
        return out;
    }
    size_t i = 0;
    auto fail = [&](const std::string& why) {
        throw ValueError("'" + std::string(fmt) + "' is not a valid format string (" + why + ")");
    };
    while (i < fmt.size()) {
        const char c = fmt[i];
        const std::string_view two = fmt.substr(i, 2);
        if (two == "--" || two == "-.") {
            if (out.linestyle) {
                fail("two linestyle symbols");
            }
            out.linestyle = std::string(two);
            i += 2;
        } else if (c == '-' || c == ':' || c == ' ') {
            if (out.linestyle) {
                fail("two linestyle symbols");
            }
            out.linestyle = std::string(1, c);
            i += 1;
        } else if (is_marker_char(c)) {
            if (out.marker) {
                fail("two marker symbols");
            }
            out.marker = std::string(1, c);
            i += 1;
        } else if (std::string_view("bgrcmykw").find(c) != std::string_view::npos) {
            if (out.color) {
                fail("two color symbols");
            }
            out.color = std::string(1, c);
            i += 1;
        } else if (c == 'C') {
            size_t j = i + 1;
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j])) != 0) {
                ++j;
            }
            if (j == i + 1) {
                fail("'C' must be followed by a number");
            }
            if (out.color) {
                fail("two color symbols");
            }
            out.color = std::string(fmt.substr(i, j - i));
            i = j;
        } else {
            fail("unrecognized character '" + std::string(1, c) + "'");
        }
    }
    return out;
}

} // namespace detail

Bbox Line2D::data_extents() const {
    Bbox box = Bbox::null();
    for (size_t i = 0; i < xdata.size(); ++i) {
        box.update({xdata[i], ydata[i]}); // NaN points are ignored by update()
    }
    return box;
}

void Line2D::draw(Renderer& renderer) {
    if (!visible || xdata.empty() || axes == nullptr) {
        return;
    }
    const Size canvas = renderer.canvas_size();
    const Affine2D tf = axes->trans_data(canvas);

    GraphicsContext gc;
    gc.color = color;
    if (alpha) {
        gc.color.a = *alpha;
    }
    gc.linewidth = linewidth;
    gc.dashes = linestyle.dash_pattern(linewidth);
    gc.capstyle = linestyle.default_capstyle();
    gc.joinstyle = JoinStyle::round; // rc lines.solid_joinstyle / dash_joinstyle
    gc.clip_rect = axes->bbox_pixels(canvas);

    const Path path = Path::line(xdata, ydata);

    renderer.open_group("line2d");
    if (linestyle.drawn()) {
        renderer.draw_path(gc, path, tf);
    }
    if (marker != nullptr) {
        GraphicsContext mgc = gc;
        mgc.dashes = {};
        mgc.capstyle = CapStyle::butt;
        mgc.linewidth = markeredgewidth;
        mgc.color = markeredgecolor;
        if (alpha) {
            mgc.color.a = *alpha;
        }
        std::optional<Color> face;
        if (marker->filled) {
            face = markerfacecolor;
            if (alpha) {
                face->a = *alpha;
            }
        }
        const double s = renderer.points_to_pixels(markersize);
        renderer.draw_markers(mgc, marker->path, Affine2D::scaling(s, s), path, tf, face);
    }
    renderer.close_group();
}

} // namespace graphlib
