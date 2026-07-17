#pragma once
// Raster backend over vendored AGG 2.4 — the same Anti-Grain Geometry engine
// matplotlib renders with (ADR-0002). AGG types stay behind the pimpl; the
// backend never leaks into the artist layer (ground rule #2).
#include <memory>
#include <span>
#include <string>

#include "graphlib/backend/renderer.hpp"

namespace graphlib {

class AggRenderer final : public Renderer {
public:
    /// A canvas of exactly width_px x height_px pixels. `dpi` never resizes
    /// it — it only scales point-sized attributes (px = pt * dpi / 72:
    /// linewidths, fonts, markers); savefig derives both from figsize * dpi.
    AggRenderer(int width_px, int height_px, double dpi);
    ~AggRenderer() override;

    void draw_path(const GraphicsContext& gc, const Path& path, const Affine2D& transform,
                   const std::optional<Color>& face) override;
    void draw_image(const GraphicsContext& gc, const Bbox& dest, const ImageBuffer& image,
                    Interp interpolation,
                    const std::optional<Affine2D>& transform) override;
    /// Stamped fast path: the marker is rasterized once and its scanlines
    /// replayed per position, snapped to whole pixels (mpl _backend_agg).
    void draw_markers(const GraphicsContext& gc, const Path& marker,
                      const Affine2D& marker_transform, const Path& positions,
                      const Affine2D& transform,
                      const std::optional<Color>& face) override;
    // draw_text: inherited glyph-outline implementation

    [[nodiscard]] Size canvas_size() const override;

    /// RGBA8, straight (non-premultiplied) alpha, rows top-down — PNG-ready.
    [[nodiscard]] std::span<const unsigned char> buffer() const;

    /// Encode to PNG via stb_image_write; throws graphlib::Error on IO failure.
    void write_png(const std::string& filename) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace graphlib
