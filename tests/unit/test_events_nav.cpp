#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "graphlib/figure.hpp"

using namespace graphlib;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr Size kCanvas{640.0, 480.0}; // default figure at dpi 100
} // namespace

TEST_CASE("EventRegistry dispatches by name, disconnect works mid-dispatch", "[events]") {
    EventRegistry reg;
    int presses = 0;
    int moves = 0;
    const int cid = reg.connect(event_names::button_press, [&](const Event&) { ++presses; });
    reg.connect(event_names::motion_notify, [&](const Event&) { ++moves; });

    Event press;
    press.name = std::string(event_names::button_press);
    reg.emit(press);
    CHECK(presses == 1);
    CHECK(moves == 0);

    // A handler that disconnects itself while running must not crash dispatch.
    int self_removed_calls = 0;
    int cid2 = 0;
    cid2 = reg.connect(event_names::button_press, [&](const Event&) {
        ++self_removed_calls;
        reg.disconnect(cid2);
    });
    reg.emit(press);
    reg.emit(press);
    CHECK(self_removed_calls == 1);
    CHECK(presses == 3);

    reg.disconnect(cid);
    reg.emit(press);
    CHECK(presses == 3);
}

TEST_CASE("process_event hit-tests axes and fills data coordinates", "[events]") {
    Figure fig;
    Axes& ax = fig.add_subplot(); // box (80, 52.8) - (576, 422.4), view (0,1)^2
    Axes* seen_axes = nullptr;
    double seen_xdata = -1;
    fig.mpl_connect(event_names::button_press, [&](const Event& e) {
        seen_axes = e.inaxes;
        seen_xdata = e.xdata.value_or(-1);
    });

    Event ev;
    ev.name = std::string(event_names::button_press);
    ev.x = (80.0 + 576.0) / 2.0; // axes center
    ev.y = (52.8 + 422.4) / 2.0;
    fig.process_event(ev, kCanvas);
    CHECK(seen_axes == &ax);
    CHECK_THAT(seen_xdata, WithinAbs(0.5, 1e-9));

    ev.x = 10.0; // outside the axes box
    seen_axes = &ax;
    fig.process_event(ev, kCanvas);
    CHECK(seen_axes == nullptr);
}

TEST_CASE("process_event undoes log scaling for xdata", "[events]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_xscale("log");
    ax.set_xlim(1.0, 100.0);
    double xdata = 0;
    fig.mpl_connect(event_names::motion_notify,
                    [&](const Event& e) { xdata = e.xdata.value_or(0); });
    Event ev;
    ev.name = std::string(event_names::motion_notify);
    const Bbox box = ax.bbox_pixels(kCanvas);
    ev.x = (box.x0() + box.x1()) / 2.0; // halfway across == one decade in
    ev.y = (box.y0() + box.y1()) / 2.0;
    fig.process_event(ev, kCanvas);
    CHECK_THAT(xdata, WithinRel(10.0, 1e-9)); // sqrt(1*100) on a log axis
}

TEST_CASE("pan shifts the view by pixel deltas (linear + log)", "[nav]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_xlim(0.0, 1.0);
    ax.set_ylim(0.0, 1.0);
    const double box_w = ax.bbox_pixels(kCanvas).width();
    ax.pan(box_w / 10.0, 0.0, kCanvas); // drag right by 10% of the box
    auto [x0, x1] = ax.get_xlim();
    CHECK_THAT(x0, WithinAbs(-0.1, 1e-9));
    CHECK_THAT(x1, WithinAbs(0.9, 1e-9));

    Axes& lax = fig.add_axes(Bbox::from_extents(0.1, 0.1, 0.9, 0.9));
    lax.set_xscale("log");
    lax.set_xlim(1.0, 100.0);
    const double lw = lax.bbox_pixels(kCanvas).width();
    lax.pan(-lw / 2.0, 0.0, kCanvas); // drag left by half the box: one decade up
    auto [lx0, lx1] = lax.get_xlim();
    CHECK_THAT(lx0, WithinRel(10.0, 1e-9)); // multiplicative pan on log axes
    CHECK_THAT(lx1, WithinRel(1000.0, 1e-9));
}

TEST_CASE("zoom_at keeps the anchor point fixed and scales the span", "[nav]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    ax.set_xlim(0.0, 1.0);
    ax.set_ylim(0.0, 1.0);
    // Anchor at data (0.25, 0.5).
    const Point anchor_px = ax.trans_data(kCanvas).apply({0.25, 0.5});
    ax.zoom_at(2.0, anchor_px, kCanvas);
    auto [x0, x1] = ax.get_xlim();
    CHECK_THAT(x1 - x0, WithinAbs(0.5, 1e-9)); // span halved
    // The anchor is still at the same pixel:
    const Point anchor_after = ax.trans_data(kCanvas).apply({0.25, 0.5});
    CHECK_THAT(anchor_after.x, WithinAbs(anchor_px.x, 1e-9));
    CHECK_THAT(anchor_after.y, WithinAbs(anchor_px.y, 1e-9));
}

TEST_CASE("save_home / restore_home round-trips the view", "[nav]") {
    Figure fig;
    Axes& ax = fig.add_subplot();
    const std::vector<double> v{0.0, 10.0};
    ax.plot(v, v);
    const auto before = ax.get_xlim();
    ax.save_home();
    ax.zoom_at(4.0, {320.0, 240.0}, kCanvas);
    CHECK(ax.get_xlim() != before);
    ax.restore_home();
    CHECK(ax.get_xlim() == before);
}
