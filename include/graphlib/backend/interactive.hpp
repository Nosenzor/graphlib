#pragma once
// The interactive entry points. Implemented by the GLFW backend when built
// with GRAPHLIB_INTERACTIVE=ON; otherwise a stub throws a clear ValueError
// (deviation D5 until enabled). The pyplot facade calls through here.
#include <vector>

namespace graphlib {

class Figure;

namespace backend {

/// Open a window per registered-and-open figure and block until all close
/// (mirrors plt.show()). Keys: 'h' home, 's' save PNG, 'q' close. Drag pans,
/// scroll zooms at the cursor.
void show(const std::vector<Figure*>& figures);

/// Pump window events for ~`seconds` (mirrors plt.pause) — the animation
/// heartbeat. Returns false when every window has been closed.
bool pause(const std::vector<Figure*>& figures, double seconds);

/// True when the library was built with GRAPHLIB_INTERACTIVE.
bool interactive_available();

} // namespace backend
} // namespace graphlib
