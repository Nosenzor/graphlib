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
    AggRenderer(int width_px, int height_px, double dpi);
    ~AggRenderer() override;

    void draw_path(const GraphicsContext& gc, const Path& path, const Affine2D& transform,
                   const std::optional<Color>& face = std::nullopt) override;
    // draw_markers/draw_text: inherited implementations (stamped fast path: v0.7)

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
