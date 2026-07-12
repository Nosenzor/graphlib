---
name: release
description: Cut a graphlib milestone release (v0.X.0 or a patch) — from scope verification to an annotated tag with playable artifacts and release notes. Use when a ROADMAP milestone's exit criteria are met, or when the user asks to release.
---

# Release checklist

## 1. Preflight

- [ ] ROADMAP.md exit criteria for this milestone: walk the list literally, check each.
- [ ] CI green on **all** tier-1 targets, including the sanitizer job.
- [ ] `grep -rn "TODO(v0.X" src include` — empty for the version being released.
- [ ] docs/PARITY.md statuses updated for everything shipped (planned → done).
- [ ] Examples: build and run on at least macOS + Linux; regenerate `docs/gallery/`; eyeball
      every image.
- [ ] **Fresh-consumer smoke test** (the release *is* this working): new empty dir, a 10-line
      CMakeLists with `FetchContent` pinned to the candidate SHA, plus the hello example →
      configure, build, run, open the output.

## 2. Version & notes

- [ ] Bump `project(VERSION …)` in CMakeLists.txt; confirm generated `graphlib/version.hpp`
      matches.
- [ ] CHANGELOG.md: move Unreleased → `v0.X.0 — YYYY-MM-DD`; group feat/fix/perf; add a 3-line
      human Highlights paragraph.
- [ ] ROADMAP.md: mark the milestone done. AGENTS.md §Status: point at the next milestone.

## 3. Tag & publish

```bash
git tag -a v0.X.0 -m "graphlib v0.X.0 — <codename>"
git push origin v0.X.0
```

GitHub release notes template:

- **Highlights** — 3 lines, human.
- **New in this release** — API list (from CHANGELOG).
- **Parity delta** — "now covers N pyplot functions" + link to PARITY.md.
- **Known gaps** — honest list.
- **Play with it** — exact clone → cmake → run commands, plus 2 hero images from the gallery
  embedded in the notes. Attach the zipped gallery so people see output without building.

## 4. After

- Open the next-milestone tracking issue from its ROADMAP scope list; move anything deferred
  into it explicitly (no silent drops).
- Fresh `## [Unreleased]` section back into CHANGELOG.md.
