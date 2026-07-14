#include "graphlib/pyplot.hpp"

#include <map>
#include <memory>

namespace graphlib::pyplot {

namespace {

// The pyplot state machine: figure registry + current figure number.
// Process-global and thread-confined, like matplotlib's Gcf.
struct Registry {
    std::map<int, std::unique_ptr<Figure>> figures;
    int current = -1;
};

Registry& registry() {
    static Registry r;
    return r;
}

} // namespace

Figure& figure(const FigureOpts& opts) {
    Registry& r = registry();
    const int num = r.figures.empty() ? 1 : r.figures.rbegin()->first + 1;
    return figure(num, opts);
}

Figure& figure(int num, const FigureOpts& opts) {
    Registry& r = registry();
    auto it = r.figures.find(num);
    if (it == r.figures.end()) {
        it = r.figures.emplace(num, std::make_unique<Figure>(opts)).first;
    }
    r.current = num;
    return *it->second;
}

Figure& gcf() {
    Registry& r = registry();
    if (r.current == -1 || r.figures.find(r.current) == r.figures.end()) {
        return figure();
    }
    return *r.figures.at(r.current);
}

Axes& gca() { return gcf().gca(); }

Line2D& plot(std::span<const double> x, std::span<const double> y, std::string_view fmt,
             const LineOpts& opts) {
    return gca().plot(x, y, fmt, opts);
}

Line2D& plot(std::span<const double> x, std::span<const double> y, const LineOpts& opts) {
    return gca().plot(x, y, opts);
}

Line2D& plot(std::span<const double> y, std::string_view fmt, const LineOpts& opts) {
    return gca().plot(y, fmt, opts);
}

Text& text(double x, double y, std::string s, const TextOpts& opts) {
    return gca().text(x, y, std::move(s), opts);
}

PathCollection& scatter(std::span<const double> x, std::span<const double> y,
                        const ScatterOpts& opts) {
    return gca().scatter(x, y, opts);
}

Legend& legend(const LegendOpts& opts) { return gca().legend(opts); }

std::vector<Rectangle*> bar(std::span<const double> x, std::span<const double> height,
                            const BarOpts& opts) {
    return gca().bar(x, height, opts);
}
std::vector<Rectangle*> bar(const std::vector<std::string>& labels,
                            std::span<const double> height, const BarOpts& opts) {
    return gca().bar(labels, height, opts);
}
std::vector<Rectangle*> barh(std::span<const double> y, std::span<const double> width,
                             const BarOpts& opts) {
    return gca().barh(y, width, opts);
}
std::vector<Rectangle*> hist(std::span<const double> data, const HistOpts& opts) {
    return gca().hist(data, opts);
}
Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                      std::span<const double> y2, const FillOpts& opts) {
    return gca().fill_between(x, y1, y2, opts);
}
Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                      const FillOpts& opts) {
    return gca().fill_between(x, y1, opts);
}
Line2D& step(std::span<const double> x, std::span<const double> y, std::string_view fmt,
             const LineOpts& opts) {
    return gca().step(x, y, fmt, opts);
}
Line2D& axhline(double y, double xmin, double xmax, const LineOpts& opts) {
    return gca().axhline(y, xmin, xmax, opts);
}
Line2D& axvline(double x, double ymin, double ymax, const LineOpts& opts) {
    return gca().axvline(x, ymin, ymax, opts);
}
Rectangle& axhspan(double ymin, double ymax, const FillOpts& opts) {
    return gca().axhspan(ymin, ymax, opts);
}
Rectangle& axvspan(double xmin, double xmax, const FillOpts& opts) {
    return gca().axvspan(xmin, xmax, opts);
}
Line2D& hlines(std::span<const double> y, double xmin, double xmax, const LineOpts& opts) {
    return gca().hlines(y, xmin, xmax, opts);
}
Line2D& vlines(std::span<const double> x, double ymin, double ymax, const LineOpts& opts) {
    return gca().vlines(x, ymin, ymax, opts);
}
Line2D& errorbar(std::span<const double> x, std::span<const double> y,
                 const ErrorbarOpts& opts) {
    return gca().errorbar(x, y, opts);
}
std::vector<Wedge*> pie(std::span<const double> sizes, const PieOpts& opts) {
    return gca().pie(sizes, opts);
}
AxesImage& imshow(std::span<const double> data, int rows, int cols, const ImshowOpts& opts) {
    return gca().imshow(data, rows, cols, opts);
}
QuadMesh& pcolormesh(std::span<const double> x_edges, std::span<const double> y_edges,
                     std::span<const double> values, const PcolorOpts& opts) {
    return gca().pcolormesh(x_edges, y_edges, values, opts);
}
ContourSet& contour(std::span<const double> x, std::span<const double> y,
                    std::span<const double> z, const ContourOpts& opts) {
    return gca().contour(x, y, z, opts);
}
ContourSet& contourf(std::span<const double> x, std::span<const double> y,
                     std::span<const double> z, const ContourOpts& opts) {
    return gca().contourf(x, y, z, opts);
}

void title(std::string t) { gca().set_title(std::move(t)); }
void xlabel(std::string t) { gca().set_xlabel(std::move(t)); }
void ylabel(std::string t) { gca().set_ylabel(std::move(t)); }
void xlim(double left, double right) { gca().set_xlim(left, right); }
void ylim(double bottom, double top) { gca().set_ylim(bottom, top); }
void grid(bool on) { gca().grid(on); }

void savefig(const std::string& filename, const SaveOpts& opts) { gcf().savefig(filename, opts); }

void close(int num) {
    Registry& r = registry();
    r.figures.erase(num);
    if (r.current == num) {
        r.current = r.figures.empty() ? -1 : r.figures.rbegin()->first;
    }
}

void close_all() {
    Registry& r = registry();
    r.figures.clear();
    r.current = -1;
}

} // namespace graphlib::pyplot
