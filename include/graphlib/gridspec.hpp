#pragma once
// Mirrors matplotlib.gridspec.GridSpec — grid geometry for subplots.
#include "graphlib/transforms.hpp"

namespace graphlib {

/// A grid of subplot cells inside the figure's subplot region
/// (rc figure.subplot.{left,right,bottom,top,wspace,hspace}).
/// kwargs of GridSpec (mpl names; fractions of figure width/height).
struct GridSpecOpts {
    double left = 0.125;   // rc figure.subplot.left
    double right = 0.9;    // rc figure.subplot.right
    double bottom = 0.11;  // rc figure.subplot.bottom
    double top = 0.88;     // rc figure.subplot.top
    double wspace = 0.2;   // rc figure.subplot.wspace
    double hspace = 0.2;   // rc figure.subplot.hspace
};

/// Grid layout for subplots (mirrors matplotlib.gridspec.GridSpec).
class GridSpec {
public:
    GridSpec(int nrows, int ncols);
    GridSpec(int nrows, int ncols, const GridSpecOpts& opts)
        : nrows_(nrows), ncols_(ncols),
          region_(Bbox::from_extents(opts.left, opts.bottom, opts.right, opts.top)),
          wspace_(opts.wspace), hspace_(opts.hspace) {}

    /// Cell rect in figure fraction; row 0 is the TOP row (mpl convention).
    [[nodiscard]] Bbox cell(int row, int col) const;

    [[nodiscard]] int nrows() const { return nrows_; }
    [[nodiscard]] int ncols() const { return ncols_; }

private:
    int nrows_;
    int ncols_;
    Bbox region_;
    double wspace_;
    double hspace_;
};

} // namespace graphlib
