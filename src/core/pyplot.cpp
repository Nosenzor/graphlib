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
