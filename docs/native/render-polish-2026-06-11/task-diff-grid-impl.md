# task-diff-grid-impl — Center instrument-icon glyph on difficulty-grid dot rows

**Issue key:** `diff-grid`  ·  **Status:** DONE (verified)  ·  **Wave:** 2 implementer
**Implementer:** opus, 2026-06-11  ·  **Ports used:** 8721–8727

---

## SUMMARY

Implemented the diff-grid scout's A/B-proven fix. In the song-select per-instrument
difficulty grid, the instrument-icon glyphs (guitar/bass/drums/keys/vox) rendered
~22–28px **below** their dot rows. They now render **vertically centered on the
dot rows**, matching retail. Icon shape is preserved (full circles, not squished),
the wide-atlas CellDiff correction is not regressed, and the devil/expert bars and
dots still render. Normal UI text (song list, main-menu Pentatonic labels,
gameplay HUD) is provably unshifted. The Wii build is byte-identical (all edits
behind `#ifdef HX_NATIVE`).

---

## WHAT CHANGED (files + why)

Both files are in the **rb3 src tree** (compiled into the engine via
`add_subdirectory`), **NOT** `milo-native-engine` — so **no engine-repo change
and no `MILO_ENGINE_PIN` bump**.

### 1. `src/system/rndobj/Text.cpp` — `SetupCharVerts` (free fn ~line 1159)

Replaced the four `vert[N].pos.Set(...)` calls (which hard-coded `f5` / `f5 - f6`
as the quad top/bottom Z) with `topZ` / `botZ` locals. Under `#ifdef HX_NATIVE`,
for the color-icon font only, both are shifted **up** by `0.5 * (f6 - f6_raw)` —
exactly the height the wide-atlas CellDiff correction added — re-centering the
grown glyph on its (milo-authored, dot-aligned) line anchor while keeping the
corrected height `f6` so the icon SHAPE is unchanged.

- `f6` is the call-site height `style.size * definingFont->CellDiff()` (corrected).
- `f6_raw = style.size * font->RawCellDiff()` is the byte-matched (Wii) height.
- Font discriminator: `font->GetMat()->Name()` contains `"icon"` — identical to
  the engine's existing `isColorIconFont` heuristic
  (`Rnd_Wgpu_RB3.cpp` line 4347). Letter fonts (`Pentatonic_*`) carry no `"icon"`
  in their material name, so all normal text is untouched. Null-guarded on
  `iconMat` and `matName`.

Also added `#include <cstring>` under `#ifdef HX_NATIVE` for `std::strstr`
(the file already `#include <string.h>`; this guarantees the `std::` name).

### 2. `src/system/rndobj/Font.h` — added `RndFont::RawCellDiff()` (HX_NATIVE only)

`float RawCellDiff() const { return mCellSize.y / mCellSize.x; }` — the
byte-matched glyph aspect (CellDiff WITHOUT the non-square-atlas correction).
SetupCharVerts uses `CellDiff() - RawCellDiff()` to compute the exact re-center
delta. Added inside the existing `#ifdef HX_NATIVE` block in the class, so it does
not exist in the Wii build.

### Deviation from the scout doc

The scout sketched a full `f6/2` shift (`topZ = f5 + 0.5f*f6`) and **explicitly
flagged that it slightly overshoots** ("a full f6/2 slightly overshoots… the
exact offset is Wii glyph height vs corrected Xbox height, halved — i.e. shift up
by `0.5 * (f6_corrected − f6_raw)`"). I implemented the scout's **recommended
tuned variant** (`0.5 * (f6 − f6_raw)`), not the overshooting sketch. This is the
scout's own stated target, so it is a documented refinement, not a divergence in
approach. It also makes the change a strict no-op for square-atlas fonts
(`CellDiff() == RawCellDiff()` → delta 0), which is cleaner than gating on shape.

---

## BRANCH + COMMITS

- **rb3 worktree branch:** `wt-task-diff-grid`
  (worktree at `/home/free/code/milohax/rb3/.claude/worktrees/task-diff-grid`)
- **Commit:** `0522796b54dc9c2e57b134be186fc48039284df6`
  `fix(native): center instrument-icon glyph on difficulty-grid dot rows`
- **Engine commits:** none (rb3-src-only change).
- Branched from rb3 master `979401c2`.

---

## EVIDENCE (screenshots)

All under `/tmp/rp2-diff-grid/`:

| Path | What |
|---|---|
| `before/native_depth_16.png` | BEFORE full song-select w/ difficulty grid (main binary) |
| `after/native_depth_16.png` | AFTER full song-select (worktree binary) |
| `before_grid_zoom.png` / `after_grid_zoom.png` | right-panel grid, 3× zoom — icons drop below dots (before) vs centered (after) |
| `sidebyside_grid_before_after.png` | **before (left) vs after (right)** composite — the money shot |
| `ref_grid_zoom.png` | retail `yt_qRagnZCIMzk_song_select_diff_ratings.png` right panel, same crop (ground truth: icon center == dot row) |
| `before/native_depth_00.png` / `after/native_depth_00.png` | depth-0 song-select normal text (unshifted) |
| `hub_before.png` / `hub_after.png` | main-hub Pentatonic menu labels (PLAY NOW / QUICKPLAY / …) — unshifted |
| `game_after/06_game_screen.png`, `07_playing.png` | gameplay HUD renders correctly (worktree binary) |
| `game_after/02_part_difficulty.png` | part-difficulty screen text correct |

---

## VERIFICATION RESULTS — `verified=true`

**(1) Difficulty grid aligned — PASS.** In `after_grid_zoom.png` every
instrument-icon center sits on the same horizontal line as its dot row, matching
the retail reference relationship. In `before_grid_zoom.png` the icons clearly
hang below. Pixel-diff of the grid region (x[820,1120] y[380,560]):
`before vs after` mean diff **18.95** (max 227) vs a boot-to-boot noise floor of
**2.34** — i.e. the icons demonstrably moved, localized to the grid.

**(2) Icon SHAPE preserved — PASS.** Icons remain full circles in the after zoom;
the wide-atlas CellDiff correction is not regressed (the change only translates
the quad in Z, height unchanged).

**(3) Normal text NOT shifted — PASS (quantified).** I captured the SAME unchanged
main binary twice (`before` and `before2`) to establish the boot-to-boot noise
floor (the animated venue/attract background bleeds through behind menus and
animates non-deterministically across boots). In the left song-list text region
(x[0,640] y[60,560]):

| comparison | max | mean | nonzero px |
|---|---|---|---|
| before vs before2 (noise floor) | 85 | 11.473 | 260064 |
| before vs after (my change) | 84 | 11.452 | 259838 |

The two are statistically identical → my change adds **zero** text shift beyond
inherent boot noise. Main-hub menu-text region and depth-0 song-list confirm the
same (Pentatonic labels in identical positions in `hub_before` vs `hub_after`).

**(4) Devil/expert bars + dots still render — PASS.** Red devil bars and all
difficulty dots present in `after_grid_zoom.png`.

**(5) Three required scenes (song select / main menu / gameplay HUD) — all clean
in AFTER.** Song select fixed; main hub labels correct + unshifted; gameplay HUD
(`06_game_screen.png`) and part-difficulty screen render with correct text.

**Match-neutrality — PASS.** `git diff --stat` shows only `Text.cpp` + `Font.h`.
Built Wii `Text.o` in the worktree via `tools/ninja-locked`:
`SetupCharVerts` = **99.752%** and `CreateLines` = **100%** — IDENTICAL to the
master baseline (same two values from the main-repo report.json). All edits are
behind `#ifdef HX_NATIVE`, so the Wii preprocessor output is unchanged.

### Caveats / honest notes
- The right-side difficulty grid is partially obscured at depth 16 by the
  "FRIEND RANKINGS" overlay text and a flat grey album-art box. The scout flagged
  both as **separate, out-of-scope** panel-state / album-art problems (not the
  diff-grid misalignment) belonging to whoever owns album-art / panel z-order.
  My fix does not touch or worsen them; the icon-vs-dot alignment is clearly
  measurable around/under them.
- Gameplay-HUD burst frames were truncated by a tmpfs **disk-quota** limit
  (environment-wide, not my doing) after the key screens were already captured;
  `06_game_screen.png` / `07_playing.png` confirm the HUD is fine.

---

## LANDING NOTES (for the orchestrator)

- **Cherry-pick `0522796b` from `wt-task-diff-grid`.** rb3-src only; NO engine
  commit, NO `MILO_ENGINE_PIN` bump.
- **Conflict surface:** touches `src/system/rndobj/Text.cpp` (`SetupCharVerts`)
  and `src/system/rndobj/Font.h` (adds `RawCellDiff` in the existing HX_NATIVE
  block). These are shared text-rendering files. If a sibling render-polish task
  also edits Text.cpp/Font.h, watch for conflicts:
  - In `Text.cpp` the edit is localized to the `vert[N].pos.Set(...)` block inside
    `SetupCharVerts` plus an added `#include <cstring>` (HX_NATIVE) near the top.
  - In `Font.h` the edit adds one method right after the `CellDiff()` HX_NATIVE
    body (before the `#else`), and is purely additive.
  - No sibling wave-2 task in PLAN.md is assigned to Text.cpp/Font.h, so a clean
    cherry-pick is expected.
- **After landing, no Wii rebuild concern:** match% is provably unchanged
  (`SetupCharVerts` 99.752 / `CreateLines` 100), but re-confirm if the lander runs
  a full report.
- The fix is **default-on** with no opt-out env var. If a regression is ever
  suspected, the entire effect is gated to fonts whose material name contains
  `"icon"` (only the `instrument_icons_small*` grid font in practice), so blast
  radius is minimal.
