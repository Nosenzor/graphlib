#pragma once
// The scripting layer — mirrors matplotlib.pyplot's stateful API.
// Canonical usage:  namespace plt = graphlib::pyplot;
// State (figure registry, current figure/axes) is process-global and
// thread-confined, exactly like matplotlib.
#include <span>
#include <string>

#include "graphlib/figure.hpp"

namespace graphlib::pyplot {

/// Create a new figure and make it current (plt.figure()).
Figure& figure(const FigureOpts& opts = {});
/// Create or activate figure `num` (plt.figure(num)).
Figure& figure(int num, const FigureOpts& opts = {});
/// Current figure (created on demand).
Figure& gcf();
/// Current axes of the current figure (created on demand).
Axes& gca();

Line2D& plot(std::span<const double> x, std::span<const double> y, std::string_view fmt = "",
             const LineOpts& opts = {});
Line2D& plot(std::span<const double> x, std::span<const double> y, const LineOpts& opts);
Line2D& plot(std::span<const double> y, std::string_view fmt = "", const LineOpts& opts = {});
Text& text(double x, double y, std::string s, const TextOpts& opts = {});
PathCollection& scatter(std::span<const double> x, std::span<const double> y,
                        const ScatterOpts& opts = {});
Legend& legend(const LegendOpts& opts = {});
std::vector<Rectangle*> bar(std::span<const double> x, std::span<const double> height,
                            const BarOpts& opts = {});
std::vector<Rectangle*> bar(const std::vector<std::string>& labels,
                            std::span<const double> height, const BarOpts& opts = {});
std::vector<Rectangle*> barh(std::span<const double> y, std::span<const double> width,
                             const BarOpts& opts = {});
std::vector<Rectangle*> hist(std::span<const double> data, const HistOpts& opts = {});
Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                      std::span<const double> y2, const FillOpts& opts = {});
Polygon& fill_between(std::span<const double> x, std::span<const double> y1,
                      const FillOpts& opts = {});
Line2D& step(std::span<const double> x, std::span<const double> y, std::string_view fmt = "",
             const LineOpts& opts = {});
Line2D& axhline(double y, double xmin = 0.0, double xmax = 1.0, const LineOpts& opts = {});
Line2D& axvline(double x, double ymin = 0.0, double ymax = 1.0, const LineOpts& opts = {});
Rectangle& axhspan(double ymin, double ymax, const FillOpts& opts = {});
Rectangle& axvspan(double xmin, double xmax, const FillOpts& opts = {});
Line2D& hlines(std::span<const double> y, double xmin, double xmax, const LineOpts& opts = {});
Line2D& vlines(std::span<const double> x, double ymin, double ymax, const LineOpts& opts = {});
Line2D& errorbar(std::span<const double> x, std::span<const double> y,
                 const ErrorbarOpts& opts = {});
std::vector<Wedge*> pie(std::span<const double> sizes, const PieOpts& opts = {});
AxesImage& imshow(std::span<const double> data, int rows, int cols,
                  const ImshowOpts& opts = {});
QuadMesh& pcolormesh(std::span<const double> x_edges, std::span<const double> y_edges,
                     std::span<const double> values, const PcolorOpts& opts = {});
ContourSet& contour(std::span<const double> x, std::span<const double> y,
                    std::span<const double> z, const ContourOpts& opts = {});
ContourSet& contourf(std::span<const double> x, std::span<const double> y,
                     std::span<const double> z, const ContourOpts& opts = {});

void title(std::string t);
void xlabel(std::string t);
void ylabel(std::string t);
void xlim(double left, double right);
void ylim(double bottom, double top);
void grid(bool on = true);

void savefig(const std::string& filename, const SaveOpts& opts = {});

void close(int num);
void close_all();

} // namespace graphlib::pyplot
