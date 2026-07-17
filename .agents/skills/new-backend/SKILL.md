---
name: new-backend
description: Implementing a new rendering backend or extending the Renderer interface (SVG, AGG raster, PDF, GLFW window, …). Use when adding an output format, an interactive canvas, or a new Renderer primitive/fast path — covers the contract, determinism rules, conformance suite, and savefig registration.
---

# Backend implementation guide

## The contract

`Renderer` (in `include/graphlib/backend/renderer.hpp`) mirrors matplotlib's `RendererBase` —
full rationale in DESIGN.md §7.

Required: `draw_path`, `draw_text`, `draw_image`, `get_canvas_size`, `points_to_pixels`.
Optional fast paths with default loop implementations — override only for perf/fidelity:
`draw_markers`, `draw_path_collection`, `draw_quad_mesh`, `open_group`/`close_group`.

Semantics you must honor:

- Device space is pixels. The artist layer produces display coordinates **origin bottom-left**
  (y-up). A y-down backend (SVG, PNG buffer) converts **exactly once**, at emission — one flip
  site per backend, marked with a comment. Never double-flip text vs paths.
- `points_to_pixels(pt) = pt × dpi / 72` for raster; a 72-units-per-inch vector format (PDF)
  returns pt unchanged.
- Apply **all** of GraphicsContext: rgba, alpha semantics (forced vs multiplied), linewidth
  (pt→px), dashes (offset + pattern, already ×lw), capstyle (butt|round|projecting→square),
  joinstyle (miter|round|bevel), antialiased flag, clip rect, clip path.
- Fills: nonzero winding; stroke and fill are both optional per call.
- **Never measure text yourself** — FontManager owns metrics; you only emit (native text element /
  glyph outlines / raster bitmaps). Rotation is CCW degrees about the (x, y) anchor.

## Determinism (vector backends — golden tests depend on this)

Floats via the shared `util::fmt_float` (`std::to_chars`, capped precision) — never printf `%f`.
Fixed attribute order. Element ids from a per-canvas counter, never pointer values. Deterministic
iteration (no unordered containers in emission order). Goal: byte-identical files on all platforms.

## Checklist

- [ ] `class XxxRenderer final : public Renderer` under `src/backends/xxx/`; backend headers never
      leak into core or artists.
- [ ] Canvas glue: register the file extension in the single `savefig` dispatch point
      (`".pdf"` → PdfCanvas). Unknown extension ⇒ `ValueError` listing supported ones.
- [ ] **Conformance scene**: `tests/image/conformance_scene.cpp` draws the canonical scene
      (dashed/capped/joined strokes, béziers, markers, rotated + aligned text, clipping, an image,
      alpha overlap). Add baselines for the new backend; eyeball once before committing.
- [ ] Backend-specific unit tests (SVG: minimal well-formedness/structure checks; PDF: xref +
      "opens in a viewer" smoke; raster: corner pixels of a known scene).
- [ ] CI: if the backend needs a system dep (GLFW → xvfb job on Linux), extend the matrix.
- [ ] PARITY.md + example + CHANGELOG fragment.

## Extending the Renderer interface itself

Adding a primitive (e.g. `draw_quad_mesh`) ⇒ provide a default implementation in terms of existing
primitives so every backend keeps working, then override where it pays. Update DESIGN.md §7 and
the conformance scene in the same PR.

## Interactive backends (v0.5+) additionally

Translate native events to graphlib Events with matplotlib's names (`button_press_event`,
`motion_notify_event`, `scroll_event`, `key_press_event`, `resize_event`, `draw_event`,
`close_event`). Redraw on damage only — no busy loop. Resize ⇒ new canvas size ⇒ figure re-layout
⇒ redraw. Support blitting (save/restore region) for animation. Keep the event core unit-testable
with a fake canvas (no GLFW in those tests).
