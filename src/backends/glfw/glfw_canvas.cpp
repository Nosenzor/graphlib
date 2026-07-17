// GLFW interactive backend (GRAPHLIB_INTERACTIVE=ON). One window per figure;
// each frame is rendered by AggRenderer at framebuffer resolution (retina-
// aware) and blitted with glDrawPixels — deliberately loader-free legacy GL,
// fine for a pixel blit (ADR-0002; a GL3 path is a v0.7 perf topic).
//
// GLFW must run on the main thread (macOS requirement); graphlib is
// thread-confined anyway.
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "graphlib/backend/agg.hpp"
#include "graphlib/backend/interactive.hpp"
#include "graphlib/errors.hpp"
#include "graphlib/figure.hpp"

namespace graphlib::backend {

namespace {

std::string key_name(int key, int mods) {
    std::string name;
    if ((mods & GLFW_MOD_CONTROL) != 0) {
        name += "ctrl+";
    }
    if ((mods & GLFW_MOD_ALT) != 0) {
        name += "alt+";
    }
    switch (key) { // the specials mpl names; printables via glfwGetKeyName
    case GLFW_KEY_LEFT:
        return name + "left";
    case GLFW_KEY_RIGHT:
        return name + "right";
    case GLFW_KEY_UP:
        return name + "up";
    case GLFW_KEY_DOWN:
        return name + "down";
    case GLFW_KEY_ESCAPE:
        return name + "escape";
    case GLFW_KEY_ENTER:
        return name + "enter";
    default:
        break;
    }
    const char* printable = glfwGetKeyName(key, 0);
    return printable != nullptr ? name + printable : name;
}

class Canvas {
public:
    explicit Canvas(Figure& figure) : figure_(&figure) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); // legacy blit path
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        const int w = static_cast<int>(std::lround(figure.figsize[0] * figure.dpi));
        const int h = static_cast<int>(std::lround(figure.figsize[1] * figure.dpi));
        window_ = glfwCreateWindow(w, h, "graphlib", nullptr, nullptr);
        if (window_ == nullptr) {
            throw Error("interactive: glfwCreateWindow failed (no display?)");
        }
        glfwSetWindowUserPointer(window_, this);
        glfwSetCursorPosCallback(window_, [](GLFWwindow* win, double x, double y) {
            self(win).on_cursor(x, y);
        });
        glfwSetMouseButtonCallback(window_, [](GLFWwindow* win, int button, int action, int) {
            self(win).on_button(button, action);
        });
        glfwSetScrollCallback(window_, [](GLFWwindow* win, double, double yoff) {
            self(win).on_scroll(yoff);
        });
        glfwSetKeyCallback(window_, [](GLFWwindow* win, int key, int, int action, int mods) {
            if (action == GLFW_PRESS) {
                self(win).on_key(key, mods);
            }
        });
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* win, int fbw, int fbh) {
            self(win).on_resize(fbw, fbh);
        });
        glfwSetWindowCloseCallback(window_, [](GLFWwindow* win) { self(win).on_close(); });
        for (const auto& ax : figure.axes()) {
            ax->save_home(); // 'h' returns here
        }
    }

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    ~Canvas() {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
    }

    [[nodiscard]] bool open() const {
        return window_ != nullptr && glfwWindowShouldClose(window_) == 0;
    }
    void mark_dirty() { dirty_ = true; }

    void render_if_dirty() {
        if (!open() || !dirty_) {
            return;
        }
        dirty_ = false;
        glfwMakeContextCurrent(window_);
        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(window_, &fbw, &fbh);
        if (fbw <= 0 || fbh <= 0) {
            return;
        }
        // Retina: render at framebuffer resolution with proportionally scaled
        // dpi so points map to the same physical size.
        int ww = 0;
        int wh = 0;
        glfwGetWindowSize(window_, &ww, &wh);
        const double scale = ww > 0 ? static_cast<double>(fbw) / ww : 1.0;
        AggRenderer renderer(fbw, fbh, figure_->dpi * scale);
        figure_->draw(renderer);

        glViewport(0, 0, fbw, fbh);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        // AGG buffer rows are top-down; flip with a negative pixel zoom.
        glRasterPos2f(-1.0F, 1.0F);
        glPixelZoom(1.0F, -1.0F);
        glDrawPixels(fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, renderer.buffer().data());
        glfwSwapBuffers(window_);

        Event ev;
        ev.name = std::string(event_names::draw);
        figure_->process_event(ev, {static_cast<double>(fbw), static_cast<double>(fbh)});
    }

private:
    static Canvas& self(GLFWwindow* win) {
        return *static_cast<Canvas*>(glfwGetWindowUserPointer(win));
    }

    [[nodiscard]] Size fb_size() const {
        int fbw = 0;
        int fbh = 0;
        glfwGetFramebufferSize(window_, &fbw, &fbh);
        return {static_cast<double>(fbw), static_cast<double>(fbh)};
    }

    /// Cursor in display coords: framebuffer pixels, origin bottom-left (mpl).
    [[nodiscard]] Point cursor_display(double x, double y) const {
        int ww = 0;
        int wh = 0;
        glfwGetWindowSize(window_, &ww, &wh);
        const Size fb = fb_size();
        const double scale = ww > 0 ? fb.width / ww : 1.0;
        return {x * scale, fb.height - y * scale};
    }

    Axes* axes_at(Point display) const {
        const Size fb = fb_size();
        for (auto it = figure_->axes().rbegin(); it != figure_->axes().rend(); ++it) {
            if ((*it)->bbox_pixels(fb).contains(display)) {
                return it->get();
            }
        }
        return nullptr;
    }

    void on_cursor(double x, double y) {
        const Point pos = cursor_display(x, y);
        if (dragging_ && nav_axes_ != nullptr) {
            nav_axes_->pan(pos.x - last_.x, pos.y - last_.y, fb_size());
            mark_dirty();
        }
        last_ = pos;
        Event ev;
        ev.name = std::string(event_names::motion_notify);
        ev.x = pos.x;
        ev.y = pos.y;
        figure_->process_event(ev, fb_size());
    }

    void on_button(int button, int action) {
        Event ev;
        ev.name = std::string(action == GLFW_PRESS ? event_names::button_press
                                                   : event_names::button_release);
        ev.x = last_.x;
        ev.y = last_.y;
        ev.button = button == GLFW_MOUSE_BUTTON_LEFT    ? 1
                    : button == GLFW_MOUSE_BUTTON_MIDDLE ? 2
                                                         : 3; // mpl numbering
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            dragging_ = action == GLFW_PRESS;
            nav_axes_ = dragging_ ? axes_at(last_) : nullptr;
        }
        figure_->process_event(ev, fb_size());
    }

    void on_scroll(double yoff) {
        if (Axes* ax = axes_at(last_)) {
            ax->zoom_at(std::pow(1.2, yoff), last_, fb_size()); // zoom at the cursor
            mark_dirty();
        }
        Event ev;
        ev.name = std::string(event_names::scroll);
        ev.x = last_.x;
        ev.y = last_.y;
        ev.step = yoff;
        figure_->process_event(ev, fb_size());
    }

    void on_key(int key, int mods) {
        // mpl-style keymap: 'h' home, 's' save, 'q' close.
        if (key == GLFW_KEY_H) {
            for (const auto& ax : figure_->axes()) {
                ax->restore_home();
            }
            mark_dirty();
        } else if (key == GLFW_KEY_S) {
            figure_->savefig("graphlib_figure.png");
        } else if (key == GLFW_KEY_Q) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        Event ev;
        ev.name = std::string(event_names::key_press);
        ev.key = key_name(key, mods);
        ev.x = last_.x;
        ev.y = last_.y;
        figure_->process_event(ev, fb_size());
    }

    void on_resize(int fbw, int fbh) {
        mark_dirty();
        render_if_dirty(); // live relayout while dragging the window edge
        Event ev;
        ev.name = std::string(event_names::resize);
        ev.width = fbw;
        ev.height = fbh;
        figure_->process_event(ev, {static_cast<double>(fbw), static_cast<double>(fbh)});
    }

    void on_close() {
        Event ev;
        ev.name = std::string(event_names::close);
        figure_->process_event(ev, fb_size());
    }

    Figure* figure_;
    GLFWwindow* window_ = nullptr;
    bool dirty_ = true;
    bool dragging_ = false;
    Point last_{};
    Axes* nav_axes_ = nullptr;
};

// Global canvas registry: pause() must find the windows show()/pause created.
std::map<Figure*, std::unique_ptr<Canvas>>& canvases() {
    static std::map<Figure*, std::unique_ptr<Canvas>> map;
    return map;
}

void ensure_glfw() {
    static bool initialized = false;
    if (!initialized) {
        if (glfwInit() == GLFW_FALSE) {
            throw Error("interactive: glfwInit failed (no display available?)");
        }
        initialized = true;
    }
}

void ensure_canvases(const std::vector<Figure*>& figures) {
    ensure_glfw();
    for (Figure* fig : figures) {
        auto it = canvases().find(fig);
        if (it == canvases().end()) {
            canvases().emplace(fig, std::make_unique<Canvas>(*fig));
        }
    }
}

void prune_closed() {
    for (auto it = canvases().begin(); it != canvases().end();) {
        it = it->second->open() ? std::next(it) : canvases().erase(it);
    }
}

bool render_all_and_poll(double wait_seconds) {
    for (auto& [fig, canvas] : canvases()) {
        canvas->render_if_dirty();
    }
    if (wait_seconds > 0) {
        glfwWaitEventsTimeout(wait_seconds);
    } else {
        glfwWaitEvents();
    }
    prune_closed();
    return !canvases().empty();
}

} // namespace

void show(const std::vector<Figure*>& figures) {
    ensure_canvases(figures);
    while (render_all_and_poll(0.0)) {
    }
}

bool pause(const std::vector<Figure*>& figures, double seconds) {
    ensure_canvases(figures);
    for (auto& [fig, canvas] : canvases()) {
        canvas->mark_dirty(); // animation frames redraw unconditionally
    }
    return render_all_and_poll(std::max(1e-3, seconds));
}

bool interactive_available() { return true; }

} // namespace graphlib::backend
