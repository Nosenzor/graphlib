#pragma once
// PDF backend — deterministic vector output with selectable text
// (docs/DESIGN.md §13, PARITY D20). No timestamps, no /ID, fixed object
// order, fixed float formatting: the same figure produces byte-identical
// files on every platform, so PDFs are golden-testable like SVGs.
// Text embeds the DejaVu faces as Type0/CIDFontType2 (Identity-H) with a
// ToUnicode CMap; mathtext runs render as outlines. 72 units/inch, y-up
// (native PDF coordinates — no flip).
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "graphlib/backend/renderer.hpp"

namespace graphlib {

class PdfRenderer final : public Renderer {
public:
    /// Page size in 72-dpi device units — numerically points, PDF's native
    /// unit (savefig passes figsize inches * 72; SaveOpts dpi does not apply).
    PdfRenderer(double width_pt, double height_pt);
    ~PdfRenderer() override;

    void draw_path(const GraphicsContext& gc, const Path& path, const Affine2D& transform,
                   const std::optional<Color>& face) override;
    void draw_text(const GraphicsContext& gc, Point pos, std::string_view text,
                   const FontProperties& font, double angle_deg, HAlign ha,
                   VAlign va) override;
    void draw_image(const GraphicsContext& gc, const Bbox& dest, const ImageBuffer& image,
                    Interp interpolation,
                    const std::optional<Affine2D>& transform) override;
    [[nodiscard]] Size canvas_size() const override { return {width_, height_}; }

    /// Complete PDF file bytes; call once, after drawing.
    [[nodiscard]] std::string finalize() const;

private:
    struct FontSlot; // per-face embedding state (used glyphs, metrics)
    struct PdfImage;

    std::string gstate_name(double stroke_alpha, double fill_alpha);
    std::string set_color_ops(const GraphicsContext& gc, const std::optional<Color>& face);
    FontSlot& slot_for(bool bold, bool italic);

    double width_;
    double height_;
    std::string content_;
    std::map<std::string, std::string> gstates_;      // "CA,ca" -> /GsN
    std::vector<std::unique_ptr<FontSlot>> fonts_;    // lazily created, fixed order
    std::vector<PdfImage> images_;
    int next_gs_ = 0;
};

} // namespace graphlib
