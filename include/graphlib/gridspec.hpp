#pragma once
// Mirrors matplotlib.gridspec.GridSpec — grid geometry for subplots.
#include "graphlib/transforms.hpp"

namespace graphlib {

/// A grid of subplot cells inside the figure's subplot region
/// (rc figure.subplot.{left,right,bottom,top,wspace,hspace}).
class GridSpec {
public:
    GridSpec(int nrows, int ncols);
    GridSpec(int nrows, int ncols, Bbox region, double wspace, double hspace)
        : nrows_(nrows), ncols_(ncols), region_(region), wspace_(wspace), hspace_(hspace) {}

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
