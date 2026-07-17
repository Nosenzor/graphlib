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
#include <vector>

#include "graphlib/color.hpp"
#include "graphlib/graphics_context.hpp"
#include "graphlib/path.hpp"
#include "graphlib/transforms.hpp"

namespace graphlib {

/// Canvas dimensions in device pixels. The vector backends (SVG, PDF) render
/// at a fixed 72 dpi, so their device pixels are numerically points.
struct Size {
    double width = 0;
    double height = 0;
};

/// RGBA8 pixels, row-major, row 0 at the TOP (image convention).
struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba; // width * height * 4, straight alpha
};

/// Image resampling mode for draw_image (mpl interpolation subset, D12).
enum class Interp { nearest, bilinear };

/// Font selection for draw_text. Family is the embedded DejaVu Sans (D2);
/// the oblique face backs mathtext italics.
struct FontProperties {
    double size_pt = 10.0; // rc font.size
    bool bold = false;
    bool italic = false;
};

/// Horizontal text anchor (mpl `ha`).
enum class HAlign { left, center, right };
/// Vertical text anchor (mpl `va`); `baseline` anchors the first line's
/// baseline — the matplotlib default.
enum class VAlign { baseline, bottom, center, top };

class Renderer {
public:
    explicit Renderer(double dpi) : dpi_(dpi) {}
    virtual ~Renderer() = default;
    Renderer(const Renderer&) = delete; // polymorphic base: no slicing
    Renderer& operator=(const Renderer&) = delete;

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
    /// `text` may contain $...$ mathtext runs — that is part of this contract.
    /// The default implementation lays out with FontManager metrics (math
    /// included) and fills glyph outlines through draw_path — matplotlib's
    /// svg.fonttype='path' behavior, identical on every backend. Backends with
    /// native text (PDF) may override; they must still handle or delegate the
    /// mathtext case (see PdfRenderer, which falls back to outlines for it).
    virtual void draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                           const FontProperties& font, double angle_deg, HAlign ha, VAlign va);

    /// Stretch `image` into `dest` (device px, y-up; image row 0 lands at the
    /// TOP of the rect). Honors gc.clip_rect and the image's alpha.
    /// `transform` mirrors mpl RendererBase.draw_image's optional affine for
    /// rotated/skewed images; no backend implements it yet — every backend
    /// throws ValueError when it is set (reserved at the 1.0 freeze so adding
    /// the capability later is not a signature break).
    virtual void draw_image(const GraphicsContext& gc, const Bbox& dest, const ImageBuffer& image,
                            Interp interpolation,
                            const std::optional<Affine2D>& transform = std::nullopt) = 0;

    /// Canvas size in device pixels (== points on the 72-dpi vector backends).
    [[nodiscard]] virtual Size canvas_size() const = 0;

    /// Structure hooks for vector formats (SVG <g> nesting). Ids are generated
    /// by the backend from deterministic counters; a nonempty `gid` (future
    /// Artist::set_gid) overrides the generated id.
    virtual void open_group(std::string_view /*tag*/, std::string_view /*gid*/ = {}) {}
    virtual void close_group() {}

    /// px = pt * dpi / 72 — the single unit conversion of the library.
    [[nodiscard]] double points_to_pixels(double pt) const { return pt * dpi_ / 72.0; }
    /// Raster resolution; fixed at 72 for the vector backends.
    [[nodiscard]] double dpi() const { return dpi_; }

private:
    double dpi_;
};

} // namespace graphlib
