#pragma once
// Mirrors matplotlib.animation.FuncAnimation — the live-window subset.
// Frames redraw fully (blitting is a v0.7 perf topic, deviation D15).
#include <functional>

#include "graphlib/backend/interactive.hpp"

namespace graphlib {

class Figure;

/// kwargs of FuncAnimation.
struct AnimOpts {
    int interval = 200; // ms between frames (mpl default)
    int frames = -1;    // -1: run until the window closes
};

/// Makes an animation by repeatedly calling a function (mpl FuncAnimation).
class FuncAnimation {
public:
    /// `figure` must outlive the animation. `update` is called with frame =
    /// 0, 1, 2, … and mutates artists in place; run() repaints the figure and
    /// pumps window events for ~`interval` ms after each call.
    FuncAnimation(Figure& figure, std::function<void(int frame)> update, AnimOpts opts = {})
        : figure_(&figure), update_(std::move(update)), opts_(opts) {}

    /// Open the window and drive frames until done or closed (mpl runs its
    /// timer inside show(); ours is an explicit blocking call — PARITY D16).
    void run() {
        int frame = 0;
        while (opts_.frames < 0 || frame < opts_.frames) {
            update_(frame++);
            if (!backend::pause({figure_}, opts_.interval / 1000.0)) {
                return; // every window closed
            }
        }
        backend::show({figure_}); // frames done: stay interactive
    }

private:
    Figure* figure_;
    std::function<void(int)> update_;
    AnimOpts opts_;
};

} // namespace graphlib
