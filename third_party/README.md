# third_party — vendored dependencies (AGENTS.md tier 1)

Never modify in place; patches must be documented here. Licenses ship next to the code.

| Component | Source | Pinned | License |
|-----------|--------|--------|---------|
| `agg/` | matplotlib's patched AGG 2.4 (`extern/agg24-svn`), the engine matplotlib itself renders with | matplotlib@`bde111fb4ee0023852b48d48f14436647400663d` | `agg/copying` (AGG 2.4 permissive) |
| `stb/` | stb_truetype.h, stb_image_write.h, stb_image.h from github.com/nothings/stb | `31c1ad37456438565541f4919958214b6e762fb4` | public domain / MIT (header footers) |
| `fonts/` | DejaVu Sans + Bold from the local matplotlib 3.10.8 `mpl-data/fonts/ttf` (matplotlib's default family) | DejaVu 2.37 (as shipped by mpl 3.10.8) | `fonts/LICENSE` (Bitstream Vera + public-domain changes) |

Notes:
- `agg/src` contains exactly the 8 translation units matplotlib compiles (see their
  `meson.build`); `agg/include` is complete.
- Fonts are embedded into the library at build time (`cmake/EmbedResource.cmake`) —
  no runtime file lookups, per the determinism rules (docs/DESIGN.md §13).
- stb_image.h is **test-only** (PNG decode for the image-comparison harness).
