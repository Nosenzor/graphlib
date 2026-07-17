---
name: image-tests
description: Writing, running, debugging, and regenerating graphlib's image-comparison tests (PNG RMS + golden SVG). Use when adding visual coverage for a feature, when an image test fails and the diff needs triage, or after an intentional rendering change requires new baselines.
---

# Image test workflow

## Layout & harness

`tests/image/test_<area>.cpp` — Catch2 tests through the harness macro:

```cpp
GRAPHLIB_FIGURE_TEST("lines_dashed") {          // name == baseline filename
    auto& ax = fig.add_subplot();
    ax.plot(x, y, "--");
}   // harness saves svg+png and compares against tests/image/baselines/{svg,png}/lines_dashed.*
```

Fixed environment: default rcParams, 6.4×4.8 in @ dpi 100 unless the test overrides; embedded
DejaVu only. Anything environmental leaking into output is a determinism bug (DESIGN §13).

## Tolerance policy — do not bump casually

- **SVG: byte-equal** after harness normalization. A platform-dependent SVG is *always* a bug
  (float formatting, unordered iteration, pointer-derived ids — go hunting).
- **PNG: RMS over RGBA ≤ 1.0** (0–255 scale) default. Per-test override up to 3.0 **only** with a
  comment naming the cause (e.g. libm ULP differences on arm vs x86 shifting an AA edge). Never
  raise a tolerance to hide a layout shift.

## Run & triage

```bash
ctest --preset dev -R image
```

Failures dump artifacts to `build/dev/image_failures/<name>/` — `expected.png`, `got.png`,
`diff.png`, `report.html`. **Open the report and look before touching anything.** Classify:

1. Real regression → fix the code.
2. Intentional change → regenerate baselines (below).
3. Platform-only drift → determinism hunt (fonts? float formatting? uninitialized alpha?).
   CI uploads the `image_failures` artifact, so remote diffs are inspectable locally.

## Regenerating baselines

```bash
cmake --build --preset dev --target update-baselines
git diff --stat tests/image/baselines
```

Eyeball every changed image (expected vs new, side by side), commit baselines **together with**
the code change, and say why rendering changed in the PR description. Never hand-edit a baseline;
never regenerate to silence a failure you don't understand.

## Writing good image tests

- One concept per test; the smallest figure that shows it.
- Prefer numeric unit tests for numeric things (tick *values* belong in a ticker unit test; the
  image test covers that they're *drawn* correctly).
- Fold cheap visual edge cases into an existing figure (NaN gap, clip at edge, empty series)
  rather than one baseline each — baselines have review cost.
