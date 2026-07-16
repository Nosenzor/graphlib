#pragma once
// Internal: mpl's path simplification (src/path_converters.h PathSimplifier,
// the Haldane/Droettboom/Rose perpendicular-distance merger). Collapses runs
// of nearly-collinear segments while keeping forward/backward extrema, so
// dense polylines rasterize in a fraction of the vertices with no visible
// difference at <= threshold px. Fixture-verified against Path.cleaned().
#include "graphlib/path.hpp"

namespace graphlib::detail {

/// mpl Path.should_simplify: >= 128 vertices, only MOVETO/LINETO codes,
/// rc path.simplify on and rc path.simplify_threshold > 0. Backends
/// additionally only simplify unfilled (stroke-only) draws.
[[nodiscard]] bool should_simplify(const Path& path);

/// Simplify a polyline already in display space; threshold in pixels.
/// Non-finite vertices act as gaps (mpl's PathNanRemover runs upstream).
[[nodiscard]] Path simplify_path(const Path& path, double threshold_px);

} // namespace graphlib::detail
