#include "graphlib/colormaps.hpp"

#include <cmath>
#include <initializer_list>
#include <vector>

#include "graphlib/errors.hpp"

namespace graphlib {

namespace {

struct CmapDef {
    std::string_view name;
    std::vector<std::uint32_t> lut;
    bool qualitative;
};

#include "colormap_luts.inc"

Color unpack(std::uint32_t rgb) {
    return {static_cast<double>((rgb >> 16) & 0xFF) / 255.0,
            static_cast<double>((rgb >> 8) & 0xFF) / 255.0,
            static_cast<double>(rgb & 0xFF) / 255.0, 1.0};
}

} // namespace

Colormap::Colormap(std::string_view name, std::span<const std::uint32_t> lut, bool /*qualitative*/)
    : name_(name), lut_(lut) {
    under = unpack(lut_.front());
    over = unpack(lut_.back());
}

Color Colormap::operator()(double x) const {
    if (std::isnan(x)) {
        return bad;
    }
    if (x < 0.0) {
        return under;
    }
    if (x > 1.0) {
        return over;
    }
    const auto n = static_cast<double>(lut_.size());
    auto idx = static_cast<size_t>(x * n);
    if (idx >= lut_.size()) {
        idx = lut_.size() - 1; // x == 1.0
    }
    return unpack(lut_[idx]);
}

const Colormap& get_cmap(std::string_view name) {
    static const std::vector<Colormap> registry = [] {
        std::vector<Colormap> out;
        out.reserve(std::size(kColormaps));
        for (const auto& def : kColormaps) {
            out.emplace_back(def.name, def.lut, def.qualitative);
        }
        return out;
    }();
    for (const auto& cmap : registry) {
        if (cmap.name() == name) {
            return cmap;
        }
    }
    throw ValueError("Unknown colormap '" + std::string(name) +
                     "' (see graphlib/colormaps.hpp for the registry)");
}

double LogNorm::operator()(double v) const {
    if (v <= 0.0 || vmin <= 0.0 || vmax <= 0.0) {
        return std::nan(""); // maps to the 'bad' color, like mpl masked values
    }
    return std::log10(v / vmin) / std::log10(vmax / vmin);
}

} // namespace graphlib
