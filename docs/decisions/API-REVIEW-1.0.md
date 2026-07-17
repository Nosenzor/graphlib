# API review for the 1.0 freeze (2026-07-17)

Two independent header reviews (46 findings) were adjudicated; everything
breaking-to-fix-later was fixed before the freeze:

**Contract hardening**
- Renderer: non-copyable; `open_group` gained `gid` (future `set_gid`);
  `draw_image` gained mpl's optional affine `transform` parameter (reserved —
  all backends throw ValueError when set); redundant default args removed from
  overrides; `draw_text`'s mathtext-in-string contract documented.
- Every public header compiles standalone (verified mechanically).

**Return types (unfixable-post-1.0)**
- `hist` -> `HistResult{counts, edges, patches}` (mpl `(n, bins, patches)`).
- `pie` -> `PieResult{wedges, texts}`; `errorbar` -> `ErrorbarResult{line,
  caplines, barlines}` (mpl containers).
- `colorbar` -> `Colorbar&` handle class (grows tick/label control later).

**Type unification**
- One `CoordSys` enum for Text/Annotation coordinate systems.
- `LegendLoc` enum replaces the magic int loc code.
- `savefig` takes `std::filesystem::path`; annotate takes `Point`;
  `GridSpec(int, int, GridSpecOpts)` replaces the positional 5-arg ctor;
  PieOpts label/color vocabularies unified; ColorbarOpts::label ->
  string_view.
- Renames: `Text::rotation_deg` -> `rotation`, `Figure::axes_list` -> `axes`;
  `set_aspect_equal` deleted (use `set_aspect("equal")`);
  `Axes::yaxis_tick_right` -> `Axis::tick_right`.

**Encapsulation**
- Privatized with friends: share-group/layout wiring, legend plumbing,
  `transparent_render`, artist blended-transform flags; dedicated `is_twin_`.

**Deliberate exceptions** (PARITY D21-D24): enum ha/va, boolean bold,
bool sharex/sharey, HistOpts::edges.
