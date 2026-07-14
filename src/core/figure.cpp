#include "graphlib/figure.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "graphlib/backend/agg.hpp"
#include "graphlib/backend/svg.hpp"
#include "graphlib/errors.hpp"
#include "graphlib/gridspec.hpp"
#include "graphlib/rc.hpp"
#include "text/font_manager.hpp"

namespace graphlib {

namespace {
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

Figure::Figure(const FigureOpts& opts) {
    if (opts.figsize) {
        figsize = *opts.figsize;
    } else {
        const auto& v = std::get<std::vector<double>>(rc().at("figure.figsize"));
        figsize = {v[0], v[1]};
    }
    dpi = opts.dpi.value_or(rc().number("figure.dpi"));
    facecolor = opts.facecolor.empty() ? rc().color("figure.facecolor") : to_color(opts.facecolor);
}

Axes& Figure::add_subplot() { return add_subplot(1, 1, 1); }

Axes& Figure::add_subplot(int nrows, int ncols, int index) {
    if (index < 1 || index > nrows * ncols) {
        throw ValueError("add_subplot: index out of range (1-based, like matplotlib)");
    }
    const GridSpec gs(nrows, ncols);
    const int row = (index - 1) / ncols;
    const int col = (index - 1) % ncols;
    Axes& ax = add_axes(gs.cell(row, col));
    // The full-figure cell bounds tight_layout's redistribution.
    const GridSpec outer(nrows, ncols, Bbox::unit(), 0.0, 0.0);
    ax.outer_cell = outer.cell(row, col);
    return ax;
}

Axes& Figure::add_axes(Bbox position_fraction) {
    axes_.push_back(std::make_unique<Axes>(*this, position_fraction));
    return *axes_.back();
}

std::vector<std::vector<Axes*>> Figure::subplots(int nrows, int ncols,
                                                 const SubplotsOpts& opts) {
    std::vector<std::vector<Axes*>> grid(static_cast<size_t>(nrows));
    auto share_x = opts.sharex ? std::make_shared<Axes::ShareGroup>() : nullptr;
    auto share_y = opts.sharey ? std::make_shared<Axes::ShareGroup>() : nullptr;
    for (int r = 0; r < nrows; ++r) {
        for (int c = 0; c < ncols; ++c) {
            Axes& ax = add_subplot(nrows, ncols, r * ncols + c + 1);
            if (share_x) {
                ax.join_share_x(share_x);
            }
            if (share_y) {
                ax.join_share_y(share_y);
            }
            // sharex: only the bottom row keeps x labels; sharey: first column.
            ax.set_tick_label_visibility(!opts.sharex || r == nrows - 1,
                                         !opts.sharey || c == 0);
            grid[static_cast<size_t>(r)].push_back(&ax);
        }
    }
    return grid;
}

void Figure::suptitle(std::string text) { suptitle_ = std::move(text); }

namespace {
Axes& make_colorbar(Figure& fig, Axes& host, const Colormap& cmap, double vmin, double vmax,
                    const ColorbarOpts& opts) {
    // mpl make_axes-style space stealing (fraction + pad of the host width).
    const Bbox hp = host.position;
    const double w = hp.width();
    host.position = Bbox::from_extents(hp.x0(), hp.y0(), hp.x1() - 0.20 * w, hp.y1());
    const double cax_x0 = hp.x1() - 0.20 * w + 0.04 * w;
    Axes& cax = fig.add_axes(Bbox::from_extents(cax_x0, hp.y0(), cax_x0 + 0.045 * w, hp.y1()));

    std::vector<double> gradient(256);
    for (size_t i = 0; i < gradient.size(); ++i) {
        gradient[i] = vmin + (vmax - vmin) * static_cast<double>(i) / 255.0;
    }
    cax.imshow(gradient, 256, 1,
               {.cmap = cmap.name(),
                .vmin = vmin,
                .vmax = vmax,
                .origin = "lower",
                .extent = {{0.0, 1.0, vmin, vmax}},
                .interpolation = "nearest",
                .aspect = "auto"});
    cax.set_xticks({});     // no x axis on a colorbar
    cax.yaxis_tick_right(); // values read on the right, like mpl
    if (!opts.label.empty()) {
        cax.set_ylabel(opts.label);
    }
    return cax;
}
} // namespace

Axes& Figure::colorbar(const AxesImage& mappable, const ColorbarOpts& opts) {
    return make_colorbar(*this, *mappable.axes, *mappable.cmap, mappable.vmin, mappable.vmax,
                         opts);
}

Axes& Figure::colorbar(const QuadMesh& mappable, const ColorbarOpts& opts) {
    return make_colorbar(*this, *mappable.axes, *mappable.cmap, mappable.vmin, mappable.vmax,
                         opts);
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
    if (!suptitle_.empty()) {
        GraphicsContext gc;
        gc.color = rc().color("text.color");
        const Size canvas = renderer.canvas_size();
        // mpl suptitle: centered at y=0.98, va='top', fontsize 'large'.
        renderer.draw_text(gc, {canvas.width / 2.0, 0.98 * canvas.height}, suptitle_,
                           FontProperties{rc().fontsize("axes.titlesize"), false, false}, 0.0,
                           HAlign::center, VAlign::top);
    }
    renderer.close_group();
}

// Metrics-based tight_layout v1: measure each axes' decorations at 72 dpi and
// inset its position within its grid cell so nothing collides. mpl's version
// redistributes globally; this per-cell approximation covers the common cases.
void Figure::tight_layout(double pad) {
    const auto& fm = detail::FontManager::instance();
    const double W = figsize[0] * 72.0;
    const double H = figsize[1] * 72.0;
    const double pad_px = pad * rc().number("font.size");
    const double tick_out = rc().number("xtick.major.size") + rc().number("xtick.major.pad");
    const double tick_em = rc().fontsize("xtick.labelsize");
    const double labelpad = rc().number("axes.labelpad");

    for (const auto& ax : axes_) {
        if (ax->outer_cell.is_null() || ax->is_twin()) {
            continue; // free-floating add_axes / twins: handled below or skipped
        }
        const auto yticks = ax->yaxis().compute_ticks(*ax);
        double ylabels_w = 0.0;
        for (const auto& l : yticks.labels) {
            ylabels_w = std::max(ylabels_w, fm.text_extent(l, tick_em).width);
        }
        const double label_block = fm.ascent(tick_em) + fm.descent(tick_em);

        double left = pad_px + tick_out + ylabels_w;
        double bottom = pad_px + tick_out + label_block;
        double top = pad_px;
        double right = pad_px;
        // xlabel/ylabel/title blocks (measured with their real font sizes)
        const double axlabel_em = rc().fontsize("axes.labelsize");
        left += labelpad + (fm.ascent(axlabel_em) + fm.descent(axlabel_em));
        bottom += labelpad + (fm.ascent(axlabel_em) + fm.descent(axlabel_em));
        const double title_em = rc().fontsize("axes.titlesize");
        top += rc().number("axes.titlepad") + fm.ascent(title_em) + fm.descent(title_em);

        const Bbox& cell = ax->outer_cell;
        const double x0 = cell.x0() + left / W;
        const double x1 = cell.x1() - right / W;
        const double y0 = cell.y0() + bottom / H;
        const double y1 = cell.y1() - top / H;
        if (x1 - x0 > 0.05 && y1 - y0 > 0.05) { // keep a sane minimum plot area
            ax->position = Bbox::from_extents(x0, y0, x1, y1);
        }
    }
    // Twins overlay their host exactly, whatever the host's new position is.
    for (const auto& ax : axes_) {
        if (ax->is_twin()) {
            if (const Axes* host = ax->share_host()) {
                ax->position = host->position;
            }
        }
    }
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
        // rc savefig.dpi = 'figure': inherit figure.dpi unless overridden.
        const double render_dpi = opts.dpi.value_or(dpi);
        if (render_dpi <= 0) {
            throw ValueError("savefig: dpi must be positive");
        }
        const int w = static_cast<int>(std::lround(figsize[0] * render_dpi));
        const int h = static_cast<int>(std::lround(figsize[1] * render_dpi));
        AggRenderer renderer(w, h, render_dpi);
        transparent_render_ = opts.transparent;
        draw(renderer);
        transparent_render_ = false;
        renderer.write_png(filename);
        return;
    }
    if (ext == "pdf") {
        throw ValueError("savefig: format 'pdf' arrives in v0.6 — supported now: svg, png");
    }
    throw ValueError("savefig: unknown format '" + ext + "' (supported: svg, png)");
}

} // namespace graphlib
