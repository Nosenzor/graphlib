#include "graphlib/figure.hpp"

#include <algorithm>
#include <fstream>

#include "graphlib/backend/svg.hpp"
#include "graphlib/errors.hpp"

namespace graphlib {

namespace {
// rc figure.subplot.{left,bottom,right,top}
constexpr Bbox kDefaultSubplotRect{0.125, 0.11, 0.9, 0.88};

std::string lower_ext(const std::string& filename) {
    const size_t dot = filename.rfind('.');
    if (dot == std::string::npos || dot + 1 == filename.size()) {
        return {};
    }
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}
} // namespace

Figure::Figure(const FigureOpts& opts)
    : figsize(opts.figsize), dpi(opts.dpi), facecolor(to_color(opts.facecolor)) {}

Axes& Figure::add_subplot() { return add_axes(kDefaultSubplotRect); }

Axes& Figure::add_axes(Bbox position_fraction) {
    axes_.push_back(std::make_unique<Axes>(*this, position_fraction));
    return *axes_.back();
}

Axes& Figure::gca() {
    if (axes_.empty()) {
        return add_subplot();
    }
    return *axes_.back();
}

void Figure::draw(Renderer& renderer) const {
    renderer.open_group("figure");
    if (!transparent_render_ && facecolor.a > 0) {
        const Size canvas = renderer.canvas_size();
        Path patch;
        patch.move_to(0, 0);
        patch.line_to(canvas.width, 0);
        patch.line_to(canvas.width, canvas.height);
        patch.line_to(0, canvas.height);
        patch.close_subpath();
        GraphicsContext gc;
        gc.color.a = 0; // rc figure.edgecolor is effectively invisible by default
        renderer.draw_path(gc, patch, Affine2D::identity(), facecolor);
    }
    for (const auto& ax : axes_) {
        ax->draw(renderer);
    }
    renderer.close_group();
}

void Figure::savefig(const std::string& filename, const SaveOpts& opts) const {
    const std::string ext = lower_ext(filename);
    if (ext.empty()) {
        throw ValueError("savefig: cannot infer format from '" + filename + "'");
    }
    if (ext == "svg") {
        // SVG renders at 72 dpi with pt-sized canvas, exactly like matplotlib's
        // SVG backend — figure.dpi applies to raster outputs (v0.2).
        SvgRenderer renderer(figsize[0] * 72.0, figsize[1] * 72.0);
        transparent_render_ = opts.transparent;
        draw(renderer);
        transparent_render_ = false;
        std::ofstream out(filename, std::ios::binary);
        out << renderer.finalize();
        if (!out) {
            throw Error("savefig: cannot write '" + filename + "'");
        }
        return;
    }
    if (ext == "png") {
        throw ValueError("savefig: format 'png' arrives in v0.2 — supported now: svg");
    }
    if (ext == "pdf") {
        throw ValueError("savefig: format 'pdf' arrives in v0.6 — supported now: svg");
    }
    throw ValueError("savefig: unknown format '" + ext + "' (supported: svg)");
}

} // namespace graphlib
