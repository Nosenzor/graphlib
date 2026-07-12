# ADR-0002 — Rendering stack

Date: 2026-07-12 · Status: **accepted** (re-validate at v0.2 kickoff before vendoring)

## Decision

| Concern | Choice | Why |
|---------|--------|-----|
| Vector output first | Hand-rolled **SVG writer** (v0.1) | zero deps, human-inspectable, perfect for golden tests |
| Rasterization | **Vendored AGG 2.4** — the same Anti-Grain Geometry engine matplotlib ships (v0.2) | battle-tested AA/stroking/dashing, pixel semantics match the library we're mimicking, plain C++ (runs on all arches), BSD-style license as vendored by mpl |
| PNG encode | **stb_image_write** | public domain, single file |
| Font rasterization & metrics | **stb_truetype** + embedded **DejaVu Sans/Bold/Mono** (mpl's default family) | deterministic metrics on all platforms; FreeType optional later (v0.6) for hinting quality |
| Interactive canvas | **GLFW + OpenGL 3.3 core**, blitting the AGG framebuffer as a texture (v0.5) | one code path on all platforms; GPU is a *presenter*, not a renderer — no per-platform drawing variance |
| PDF | Own minimal writer + TrueType subsetting (v0.6) | scope-controlled; mpl's own PDF backend is the reference |

## Alternatives rejected

- **Blend2D**: excellent modern rasterizer (JIT, x86+AArch64) — heavier dep, different stroke/AA
  fidelity than mpl. Revisit post-1.0 as an optional speed backend.
- **Skia**: enormous build; against the dependency policy.
- **Cairo**: C dependency + platform packaging pain; LGPL friction for static linking.
- **Write our own rasterizer**: high risk/low reward when AGG exists and IS matplotlib's engine.
- **Native GUI backends (Cocoa/Win32/GTK)**: 3× maintenance for pre-1.0; GLFW gives one loop.
  Native integration (Qt/ImGui embedding) is a post-1.0 goal via the FigureCanvas abstraction.
