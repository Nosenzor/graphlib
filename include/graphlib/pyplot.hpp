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
