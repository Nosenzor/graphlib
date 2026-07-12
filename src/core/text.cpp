#include "graphlib/text.hpp"

#include "graphlib/axes.hpp"

namespace graphlib {

void Text::draw(Renderer& renderer) {
    if (!visible || text.empty()) {
        return;
    }
    Point pos = position;
    if (coords == Coords::data) {
        pos = axes->trans_data(renderer.canvas_size()).apply(pos);
    }
    GraphicsContext gc;
    gc.color = color;
    if (alpha) {
        gc.color.a = *alpha;
    }
    renderer.open_group("text");
    renderer.draw_text(gc, pos, text, FontProperties{fontsize, bold, false}, rotation_deg, ha,
                       va);
    renderer.close_group();
}

} // namespace graphlib
