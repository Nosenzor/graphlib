#include "graphlib/gridspec.hpp"

#include "graphlib/errors.hpp"
#include "graphlib/rc.hpp"

namespace graphlib {

GridSpec::GridSpec(int nrows, int ncols)
    : GridSpec(nrows, ncols,
               GridSpecOpts{.left = rc().number("figure.subplot.left"),
                            .right = rc().number("figure.subplot.right"),
                            .bottom = rc().number("figure.subplot.bottom"),
                            .top = rc().number("figure.subplot.top"),
                            .wspace = rc().number("figure.subplot.wspace"),
                            .hspace = rc().number("figure.subplot.hspace")}) {
    if (nrows < 1 || ncols < 1) {
        throw ValueError("GridSpec: nrows and ncols must be positive");
    }
}

// Port of GridSpecBase.get_grid_positions (equal ratios): the wspace/hspace
// separators are fractions of the average cell size.
Bbox GridSpec::cell(int row, int col) const {
    if (row < 0 || row >= nrows_ || col < 0 || col >= ncols_) {
        throw ValueError("GridSpec: cell index out of range");
    }
    const double cell_h = region_.height() / (nrows_ + hspace_ * (nrows_ - 1));
    const double sep_h = hspace_ * cell_h;
    const double cell_w = region_.width() / (ncols_ + wspace_ * (ncols_ - 1));
    const double sep_w = wspace_ * cell_w;
    const double x0 = region_.x0() + col * (cell_w + sep_w);
    const double y1 = region_.y1() - row * (cell_h + sep_h); // row 0 on top
    return Bbox::from_extents(x0, y1 - cell_h, x0 + cell_w, y1);
}

} // namespace graphlib
