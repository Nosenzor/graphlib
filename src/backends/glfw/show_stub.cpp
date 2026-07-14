// Built when GRAPHLIB_INTERACTIVE=OFF: show()/pause() explain how to enable
// the window backend instead of failing obscurely.
#include <vector>

#include "graphlib/backend/interactive.hpp"
#include "graphlib/errors.hpp"

namespace graphlib::backend {

namespace {
[[noreturn]] void unavailable() {
    throw ValueError("plt::show(): graphlib was built without the interactive backend — "
                     "reconfigure with -DGRAPHLIB_INTERACTIVE=ON (fetches GLFW)");
}
} // namespace

void show(const std::vector<Figure*>&) { unavailable(); }

bool pause(const std::vector<Figure*>&, double) { unavailable(); }

bool interactive_available() { return false; }

} // namespace graphlib::backend
