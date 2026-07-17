---
name: new-artist
description: End-to-end recipe for adding a new plot type or drawable to graphlib (an Artist subclass and/or Axes method — bar, hist, imshow, errorbar, fill_between, …). Use whenever implementing a visual feature from the roadmap, to guarantee a complete vertical slice (API, autoscale, legend, tests, example, parity row).
---

# Adding an artist / plot method

## 0. Ground truth first

Run the `mpl-parity` skill: signature, defaults, draw semantics, and edge cases (empty input, NaN,
single point, log-scale interaction). Decide the supported-kwargs subset for this milestone.

## 1. API sketch — header first

- Options struct in the artist's public header; one field per mpl kwarg; defaults literally from
  rcParams (cite the rc key in a comment, e.g. `double capsize = 0.0;  // errorbar.capsize`).
- Axes method mirrors mpl and returns references to what it created:
  `BarContainer& Axes::bar(std::span<const double> x, std::span<const double> height, const BarOpts& = {});`

## 2. Implementation checklist

- [ ] Artist subclass stores geometry in **data coordinates**; converts to `Path`(s) lazily in
      `draw()`. No pixel math outside `draw()`.
- [ ] `draw(Renderer&)` uses only the Renderer interface + GraphicsContext; honors
      alpha/zorder/visible/clip.
- [ ] Correct default zorder (images 0, patches/collections 1, lines 2, text 3 — DESIGN §2).
- [ ] **Data limits:** contribute to the Axes dataLim on add so autoscale works (margins are the
      Axes' job, not yours). Sticky edges where mpl has them: bar/hist stick to 0, imshow to its
      extent — no margin applied on a stuck side.
- [ ] **Color cycling:** color unset ⇒ consume the next property-cycle color via the Axes.
- [ ] **NaN policy:** NaN vertex splits the path (gap), matching mpl.
- [ ] Label set ⇒ register a legend handle (line/marker/patch swatch) so `legend()` finds it.
- [ ] fmt shorthand only if mpl's method takes one (basically `plot` only).

## 3. Tests — all three kinds

- Unit: geometry, limits, option/token parsing (+ oracle fixture if numeric — `mpl-parity` §4).
- Image: one baseline per meaningful visual variant, **both svg and png** (see `image-tests`
  skill). Include one edge-case render (empty data must produce an empty axes, not a crash).
- Conformance: if you extended the Renderer interface itself, update the backend conformance
  scene (see `new-backend` skill) — every backend must handle the new primitive.

## 4. Ship the slice

- [ ] Example `examples/NN_<name>.cpp` — short, gallery-style, ends in `savefig` (svg + png).
- [ ] docs/PARITY.md row → `done`, noting unsupported kwargs.
- [ ] CHANGELOG.md fragment under Unreleased.
- [ ] Doxygen on the public API; first line matches the mpl docstring summary.

## Definition of done

`ctest --preset dev` green · example output opened and eyeballed · PARITY updated · no TODO
without a milestone tag (`TODO(v0.4): …`).
