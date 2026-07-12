#pragma once
// SVG backend — deterministic output for golden tests (docs/DESIGN.md §13):
// coordinates rounded to 2 decimals, fixed attribute order, counter-based ids.
// Renders at 72 dpi with a pt-sized canvas, matching matplotlib's SVG backend.
#include <map>
#include <string>
#include <vector>

#include "graphlib/backend/renderer.hpp"

namespace graphlib {

class SvgRenderer final : public Renderer {
public:
    SvgRenderer(double width_px, double height_px);

    void draw_path(const GraphicsContext& gc, const Path& path, const Affine2D& transform,
                   const std::optional<Color>& face = std::nullopt) override;
    void draw_markers(const GraphicsContext& gc, const Path& marker,
                      const Affine2D& marker_transform, const Path& positions,
                      const Affine2D& transform,
                      const std::optional<Color>& face = std::nullopt) override;
    void draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                   const FontProperties& font, double angle_deg, HAlign ha, VAlign va) override;
    [[nodiscard]] Size canvas_size() const override { return {width_, height_}; }
    void open_group(std::string_view tag) override;
    void close_group() override;

    /// Complete SVG document; call once, after drawing.
    [[nodiscard]] std::string finalize() const;

private:
    [[nodiscard]] Point flip(Point p) const { return {p.x, height_ - p.y}; } // single y-flip site
    [[nodiscard]] std::string path_data(const Path& path, const Affine2D& transform);
    [[nodiscard]] std::string clip_attr(const GraphicsContext& gc);
    [[nodiscard]] std::string stroke_attrs(const GraphicsContext& gc) const;
    [[nodiscard]] static std::string fill_attrs(const std::optional<Color>& face);

    double width_;
    double height_;
    std::string body_;
    std::string defs_;
    std::map<std::string, int> group_counters_;
    std::map<std::string, std::string> clip_ids_;   // rect key -> clipPath id
    std::map<std::string, std::string> marker_ids_; // geometry key -> path id
    int clip_counter_ = 0;
    int marker_counter_ = 0;
};

} // namespace graphlib
