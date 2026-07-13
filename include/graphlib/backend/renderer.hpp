#pragma once
// The backend contract — distilled from matplotlib's RendererBase
// (docs/DESIGN.md §7, new-backend skill). Artists draw through this interface
// only; no backend types may leak into the artist layer.
//
// Device space: pixels, origin bottom-left, y-up (matplotlib display coords).
// A y-down backend (SVG, raster) flips exactly once at emission.
#include <optional>
#include <string>
#include <string_view>

#include "graphlib/color.hpp"
#include "graphlib/graphics_context.hpp"
#include "graphlib/path.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

struct Size {
    double width = 0;
    double height = 0;
};

struct FontProperties {
    double size_pt = 10.0; // rc font.size
    bool bold = false;
    bool italic = false;
    // family is DejaVu Sans only pre-0.6 (deviation D2)
};

enum class HAlign { left, center, right };
enum class VAlign { baseline, bottom, center, top };

class Renderer {
public:
    explicit Renderer(double dpi) : dpi_(dpi) {}
    virtual ~Renderer() = default;

    /// Stroke (and optionally fill) `path` after mapping through `transform`.
    virtual void draw_path(const GraphicsContext& gc, const Path& path,
                           const Affine2D& transform,
                           const std::optional<Color>& face = std::nullopt) = 0;

    /// Stamp `marker` (marker units -> px via marker_transform) at every vertex
    /// of `positions` (-> px via transform). Default implementation loops over
    /// draw_path; backends override for compact output / speed.
    virtual void draw_markers(const GraphicsContext& gc, const Path& marker,
                              const Affine2D& marker_transform, const Path& positions,
                              const Affine2D& transform,
                              const std::optional<Color>& face = std::nullopt);

    /// Draw text at `pos` (device px), rotated `angle_deg` CCW about the anchor.
    /// The default implementation lays out with FontManager metrics and fills
    /// glyph outlines through draw_path — matplotlib's svg.fonttype='path'
    /// behavior, identical on every backend. Backends with native text
    /// (PDF, GLFW glyph atlas) may override.
    virtual void draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                           const FontProperties& font, double angle_deg, HAlign ha, VAlign va);

    [[nodiscard]] virtual Size canvas_size() const = 0;

    /// Structure hooks for vector formats (SVG <g> nesting). Ids are generated
    /// by the backend from deterministic counters.
    virtual void open_group(std::string_view /*tag*/) {}
    virtual void close_group() {}

    [[nodiscard]] double points_to_pixels(double pt) const { return pt * dpi_ / 72.0; }
    [[nodiscard]] double dpi() const { return dpi_; }

private:
    double dpi_;
};

} // namespace graphlib
