#include "graphlib/rc.hpp"

#include <algorithm>
#include <utility>

#include "graphlib/errors.hpp"
#include "graphlib/style.hpp"

namespace graphlib {

namespace {

// The matplotlib 3.10.8 defaults for every key graphlib understands
// (docs/DESIGN.md §12). Grows as features land; unknown keys are KeyErrors so
// typos fail loudly, like matplotlib.
std::vector<std::pair<std::string, RcValue>> default_entries() {
    using SV = std::vector<std::string>;
    return {
        {"axes.edgecolor", std::string("black")},
        {"axes.facecolor", std::string("white")},
        {"axes.grid", false},
        {"axes.labelcolor", std::string("black")},
        {"axes.labelpad", 4.0},
        {"axes.labelsize", std::string("medium")},
        {"axes.linewidth", 0.8},
        {"axes.prop_cycle",
         SV{"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2",
            "#7f7f7f", "#bcbd22", "#17becf"}},
        {"axes.titlepad", 6.0},
        {"axes.titlesize", std::string("large")},
        {"axes.xmargin", 0.05},
        {"axes.ymargin", 0.05},
        {"errorbar.capsize", 0.0},
        {"figure.dpi", 100.0},
        {"figure.facecolor", std::string("white")},
        {"figure.figsize", std::vector<double>{6.4, 4.8}},
        {"figure.subplot.bottom", 0.11},
        {"figure.subplot.hspace", 0.2},
        {"figure.subplot.left", 0.125},
        {"figure.subplot.right", 0.9},
        {"figure.subplot.top", 0.88},
        {"figure.subplot.wspace", 0.2},
        {"font.size", 10.0},
        {"grid.alpha", 1.0},
        {"grid.color", std::string("#b0b0b0")},
        {"grid.linewidth", 0.8},
        {"hist.bins", 10.0},
        {"legend.borderaxespad", 0.5},
        {"legend.borderpad", 0.4},
        {"legend.edgecolor", std::string("0.8")},
        {"legend.fontsize", std::string("medium")},
        {"legend.framealpha", 0.8},
        {"legend.frameon", true},
        {"legend.loc", std::string("best")},
        {"lines.linewidth", 1.5},
        {"lines.markeredgewidth", 1.0},
        {"lines.markersize", 6.0},
        {"patch.edgecolor", std::string("black")},
        {"patch.facecolor", std::string("C0")},
        {"patch.linewidth", 1.0},
        {"scatter.edgecolors", std::string("face")},
        {"scatter.marker", std::string("o")},
        {"text.color", std::string("black")},
        {"xtick.color", std::string("black")},
        {"xtick.labelsize", std::string("medium")},
        {"xtick.major.pad", 3.5},
        {"xtick.major.size", 3.5},
        {"xtick.major.width", 0.8},
        {"xtick.minor.size", 2.0},
        {"xtick.minor.width", 0.6},
        {"ytick.color", std::string("black")},
        {"ytick.labelsize", std::string("medium")},
        {"ytick.major.pad", 3.5},
        {"ytick.major.size", 3.5},
        {"ytick.major.width", 0.8},
        {"ytick.minor.size", 2.0},
        {"ytick.minor.width", 0.6},
    };
}

// mpl font_manager.font_scalings
double named_size_factor(std::string_view name) {
    if (name == "xx-small") {
        return 0.579;
    }
    if (name == "x-small") {
        return 0.694;
    }
    if (name == "small" || name == "smaller") {
        return 0.833;
    }
    if (name == "medium") {
        return 1.0;
    }
    if (name == "large" || name == "larger") {
        return 1.2;
    }
    if (name == "x-large") {
        return 1.44;
    }
    if (name == "xx-large") {
        return 1.728;
    }
    throw ValueError("unknown named font size '" + std::string(name) + "'");
}

} // namespace

RcParams::RcParams() { reset(); }

RcParams& RcParams::instance() {
    static RcParams params;
    return params;
}

void RcParams::reset() {
    entries_.clear();
    for (auto& [k, v] : default_entries()) {
        entries_.push_back({std::move(k), std::move(v)});
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.key < b.key; });
}

const RcParams::Entry* RcParams::find(std::string_view key) const {
    const auto it = std::lower_bound(
        entries_.begin(), entries_.end(), key,
        [](const Entry& e, std::string_view k) { return e.key < k; });
    if (it == entries_.end() || it->key != key) {
        return nullptr;
    }
    return &*it;
}

RcParams::Entry* RcParams::find(std::string_view key) {
    return const_cast<Entry*>(std::as_const(*this).find(key));
}

const RcValue& RcParams::at(std::string_view key) const {
    const Entry* e = find(key);
    if (e == nullptr) {
        throw KeyError("rcParams: unknown key '" + std::string(key) + "'");
    }
    return e->value;
}

void RcParams::set(std::string_view key, RcValue value) {
    Entry* e = find(key);
    if (e == nullptr) {
        throw KeyError("rcParams: unknown key '" + std::string(key) + "'");
    }
    if (e->value.index() != value.index()) {
        // One relaxation mpl users expect: numeric font sizes for named-size keys.
        const bool size_key = std::holds_alternative<std::string>(e->value) &&
                              std::holds_alternative<double>(value) &&
                              (key.ends_with("size") || key == "legend.fontsize");
        if (!size_key) {
            throw ValueError("rcParams: type mismatch for key '" + std::string(key) + "'");
        }
    }
    e->value = std::move(value);
}

double RcParams::number(std::string_view key) const {
    const RcValue& v = at(key);
    if (const auto* d = std::get_if<double>(&v)) {
        return *d;
    }
    throw ValueError("rcParams: '" + std::string(key) + "' is not a number");
}

bool RcParams::flag(std::string_view key) const {
    const RcValue& v = at(key);
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b;
    }
    throw ValueError("rcParams: '" + std::string(key) + "' is not a bool");
}

const std::string& RcParams::str(std::string_view key) const {
    const RcValue& v = at(key);
    if (const auto* s = std::get_if<std::string>(&v)) {
        return *s;
    }
    throw ValueError("rcParams: '" + std::string(key) + "' is not a string");
}

Color RcParams::color(std::string_view key) const { return to_color(str(key)); }

std::vector<Color> RcParams::color_cycle() const {
    const RcValue& v = at("axes.prop_cycle");
    const auto& specs = std::get<std::vector<std::string>>(v);
    std::vector<Color> out;
    out.reserve(specs.size());
    for (const auto& s : specs) {
        out.push_back(to_color(s));
    }
    return out;
}

double RcParams::fontsize(std::string_view key) const {
    const RcValue& v = at(key);
    if (const auto* d = std::get_if<double>(&v)) {
        return *d;
    }
    return named_size_factor(std::get<std::string>(v)) * number("font.size");
}

namespace style {

void use(std::string_view name) {
    RcParams& params = rc();
    params.reset();
    if (name == "default") {
        return;
    }
    using SV = std::vector<std::string>;
    if (name == "ggplot") { // key diff vs default, from the mpl style sheet
        params.set("axes.edgecolor", std::string("white"));
        params.set("axes.facecolor", std::string("#E5E5E5"));
        params.set("axes.grid", true);
        params.set("axes.labelcolor", std::string("#555555"));
        params.set("axes.labelsize", std::string("large"));
        params.set("axes.linewidth", 1.0);
        params.set("axes.prop_cycle", SV{"#E24A33", "#348ABD", "#988ED5", "#777777", "#FBC15E",
                                         "#8EBA42", "#FFB5B8"});
        params.set("axes.titlesize", std::string("x-large"));
        params.set("grid.color", std::string("white"));
        params.set("patch.edgecolor", std::string("#EEEEEE"));
        params.set("patch.facecolor", std::string("#348ABD"));
        params.set("patch.linewidth", 0.5);
        params.set("xtick.color", std::string("#555555"));
        params.set("ytick.color", std::string("#555555"));
        return;
    }
    if (name == "dark_background") {
        params.set("axes.edgecolor", std::string("white"));
        params.set("axes.facecolor", std::string("black"));
        params.set("axes.labelcolor", std::string("white"));
        params.set("axes.prop_cycle", SV{"#8dd3c7", "#feffb3", "#bfbbd9", "#fa8174", "#81b1d2",
                                         "#fdb462", "#b3de69", "#bc82bd", "#ccebc4", "#ffed6f"});
        params.set("figure.facecolor", std::string("black"));
        params.set("grid.color", std::string("white"));
        params.set("patch.edgecolor", std::string("white"));
        params.set("text.color", std::string("white"));
        params.set("xtick.color", std::string("white"));
        params.set("ytick.color", std::string("white"));
        return;
    }
    throw KeyError("style: unknown style '" + std::string(name) +
                   "' (available: default, ggplot, dark_background)");
}

std::vector<std::string_view> available() { return {"default", "ggplot", "dark_background"}; }

} // namespace style

} // namespace graphlib
