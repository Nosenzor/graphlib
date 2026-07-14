#pragma once
// Mirrors matplotlib's event system: canonical event names, a flat event
// payload, and the mpl_connect callback registry. Pure core — no window
// toolkit types; backends translate native input into these events, and tests
// synthesize them directly.
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace graphlib {

class Axes;
class Figure;

/// Canonical names (mirrors matplotlib): "button_press_event",
/// "button_release_event", "motion_notify_event", "scroll_event",
/// "key_press_event", "resize_event", "draw_event", "close_event".
namespace event_names {
inline constexpr std::string_view button_press = "button_press_event";
inline constexpr std::string_view button_release = "button_release_event";
inline constexpr std::string_view motion_notify = "motion_notify_event";
inline constexpr std::string_view scroll = "scroll_event";
inline constexpr std::string_view key_press = "key_press_event";
inline constexpr std::string_view resize = "resize_event";
inline constexpr std::string_view draw = "draw_event";
inline constexpr std::string_view close = "close_event";
} // namespace event_names

/// One flat payload for every event kind (field names mirror matplotlib's
/// event attributes; unused fields stay at their defaults).
struct Event {
    std::string name;
    Figure* figure = nullptr;

    // Mouse/key position in display pixels (origin bottom-left, y-up).
    double x = 0.0;
    double y = 0.0;
    // Filled by Figure::locate_event when the position falls inside an axes.
    Axes* inaxes = nullptr;
    std::optional<double> xdata;
    std::optional<double> ydata;

    int button = 0;    // 1 left, 2 middle, 3 right (mpl numbering)
    double step = 0.0; // scroll: positive is 'up'
    std::string key;   // "a", "ctrl+s", "left", ...

    int width = 0; // resize_event, in pixels
    int height = 0;
};

using EventCallback = std::function<void(const Event&)>;

/// Callback registry (mirrors canvas.mpl_connect/mpl_disconnect).
class EventRegistry {
public:
    int connect(std::string_view name, EventCallback callback);
    void disconnect(int cid);
    void emit(const Event& event) const;

private:
    struct Entry {
        int cid;
        std::string name;
        EventCallback callback;
    };
    std::vector<Entry> entries_;
    int next_cid_ = 1;
};

} // namespace graphlib
