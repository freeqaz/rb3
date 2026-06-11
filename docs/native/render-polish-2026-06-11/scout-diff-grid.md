# scout-diff-grid — Song-select per-instrument difficulty grid misaligned

**Issue key:** `diff-grid`  ·  **Status:** ROOT CAUSE (proven by A/B in worktree)
**Scout:** opus, 2026-06-11  ·  **Ports used:** 8601–8605

---

## SYMPTOM

On the Music Library (song_select) screen, the right-hand panel shows the
per-instrument difficulty grid (instrument icon + difficulty dots / NO PART /
NO REVIEW, in two columns: core instruments left, PRO variants right). In the
native/web port the **instrument icons sit ~22–28px BELOW the dot rows they
belong to** — the dots/labels are vertically offset (appear *above*) their icon.
In retail the icon and its dot-row are vertically centered on the same line.

**Repro**

```bash
python3 scripts/native/song-select-capture.py --port 8601 \
        --depths 16 --out /tmp/rp-diff-grid --verbose
```

(boots to `song_select_screen`, scrolls 16 rows, screenshots at 1280×720).

**Evidence (all under `/tmp/rp-diff-grid/`)**

- `native_depth_16.png` — current native song-select (baseline).
- `nat_grid_overlay.png` — native right-panel grid w/ Y-gridlines. Measured
  left-column: guitar icon center ≈ y432 / its dots ≈ y410; bass ≈ 478 / 450;
  drums ≈ 510 / 490; keys ≈ 560 / 535. **Dots are uniformly ~22–28px above the
  icon.**
- `ref_grid_overlay.png` — retail `yt_qRagnZCIMzk_song_select_diff_ratings.png`
  right-panel w/ Y-gridlines. Icon center == dot-row center on every row
  (guitar 388/388, bass 428/428, drums 468/468, vox 505/505, keys 542/542).
- `sidebyside_panel.png` — retail (left) vs native (right) right panel.
- `probe2_cmp.png` — CellDiff-correction ON (left) vs OFF (right): toggling it
  moves the icon and squishes its shape → proves the icon vertical position is
  governed by the font CellDiff.
- `probe3_cmp.png` / `probe3_zoom.png` — the fix probe (icon glyph re-centered):
  icons rise toward the dot rows.

**Secondary / out-of-scope-for-diff-grid (note, don't fix here):**
- The **red "devil" bar** on some rows is `image/difficulty_button_devil.png`
  (`devil.tex`/`devil.mat` in `instrument_difficulty_display.milo_xbox`) — the
  **expert-difficulty** indicator. It is *intentional*, not a bug. Verify it
  still renders for expert songs after the fix.
- "FRIEND RANKINGS" text overlapping the grid + a flat **grey album-art box**
  (measured RGB ≈ 177,177,177, uniform) in the top-right are *separate*
  panel-state / album-art-not-loading problems shared with the broader
  song_select render gap. Not the diff-grid misalignment. Flag to whichever
  agent owns album-art / panel z-order.

---

## ROOT CAUSE

The difficulty grid widget is `ui/resource/gen/instrument_difficulty_display.milo_xbox`.
Its parts (from `strings`):

- `dot01.mesh … dot05.mesh` — the 5 difficulty dots: **fixed-transform RndMeshes**
  positioned by the milo (textures `difficulty_off/on_nomip.bmp`).
- `vocal_part.mesh`, `devil.mat` — vocal-part / expert indicators.
- `instrument.lbl` — the "NO PART"/"NO REVIEW" text **BandLabel**.
- **`instrument_icon.lbl` — a `BandLabel` whose glyph IS the guitar/bass/drums/
  vox/keys icon, drawn from the `instrument_icons_small*` color-icon font**
  (the same font the engine's `isColorIconFont` path in
  `Rnd_Wgpu_RB3.cpp:4347` handles).

So on every row the dots are **meshes at a fixed Y**, and the icon is a
**font glyph** whose Y is computed from font metrics. They diverge because of a
glyph-height vs glyph-anchor interaction:

1. `src/system/rndobj/Text.cpp` `SetupCharVerts` builds each glyph quad
   **hanging DOWN from the line anchor**: top verts at `z = f5`, bottom verts at
   `z = f5 - f6`, where `f6 = style.size * font->CellDiff()` is the glyph height
   (lines 1185–1188, called at 1225–1237 with `f5 = curLine.unk28.v.z`).
2. `RndFont::CellDiff()` (`src/system/rndobj/Font.h:97`) has an `#ifdef HX_NATIVE`
   non-square-atlas correction (commits `3cb8a41a` tall-atlas, **`0ee42279`
   wide-atlas**). The `instrument_icons_small*` atlas is wide (512×256), so the
   matched formula `mCellSize.y/mCellSize.x` returns ~HALF the true aspect; the
   correction returns the true (larger) value. **This is correct for glyph SHAPE**
   (without it the icons render as squished half-height ovals — see
   `probe2_cmp.png`, right).
3. **But growing `f6` while the quad stays top-anchored at `f5` pushes the
   icon's visual CENTER down by ≈ Δf6/2.** The milo authored the dot meshes at
   the icon's intended *center*, so the icon now sits ~22–28px below the dots.

The Wii target never hit this: its atlas is square → `CellDiff()` returns the
matched value → glyph height matches what the milo was authored against. The bug
is purely a native-port side-effect of the (otherwise-correct) wide-atlas
CellDiff fix.

**Why this is not a line-spacing bug:** `Text.cpp:947–957` (WrapText) computes
line `ratio` from the *raw* `mCellSize.y/mCellSize.x` (an intentional inline,
NOT `CellDiff()`). I A/B'd switching that to `CellDiff()` — **no pixel change**
(`/tmp/rp-diff-grid/probe/`), because the icon label is single-line and its
vertical anchor doesn't flow through the `topY` (kMiddle) path here. The
mispositioning is entirely in the per-glyph quad geometry, not line layout.

### A/B proof (worktree `scout-diff-grid`, native build)

| Probe | Change | Result |
|---|---|---|
| 1 | `Text.cpp:953` raw→`CellDiff()` (line ratio) | **no change** → not line spacing |
| 2 | disable `Font.h` non-square branch (force raw CellDiff) | icons SQUISH to half-height ovals but their center rises toward the dots → **confirms CellDiff height drives icon Y** |
| 3 | center the `*icon*`-material glyph quad in `SetupCharVerts` (`z∈[f5−f6/2, f5+f6/2]`) | **icons rise to the dot rows** (`probe3_zoom.png`); a full f6/2 slightly overshoots → magnitude needs tuning |

---

## FIX DESIGN

**File:** `src/system/rndobj/Text.cpp`, function `SetupCharVerts`
(the free function at line 1159). This file lives in the **rb3 src tree**
(compiled into the engine via `add_subdirectory`), **NOT** the
`milo-native-engine` repo — so the fix is an rb3 change, no engine-repo edit and
no `MILO_ENGINE_PIN` bump needed.

**Approach (recommended):** vertically center the glyph quad for the color-icon
font only, under `#ifdef HX_NATIVE`. The discriminator is the font material name
containing `"icon"` — identical to the engine's existing `isColorIconFont`
heuristic (`Rnd_Wgpu_RB3.cpp:4347`), and to the `instrument_icons_small*` font
the `Font.h` CellDiff comment already calls out. Sketch (proven in probe 3):

```cpp
// SetupCharVerts, replacing the four vert[N].pos.Set(...) lines:
float topZ = f5, botZ = f5 - f6;
#ifdef HX_NATIVE
RndMat *m = font->GetMat();              // rndobj/Mat.h already included
if (m && m->Name() && std::strstr(m->Name(), "icon")) {
    topZ = f5 + 0.5f * f6;               // center the taller corrected glyph
    botZ = f5 - 0.5f * f6;               // on its (dot-aligned) anchor
}
#endif
vert[0].pos.Set(fref + f7,        f4, topZ);
vert[1].pos.Set(fref - f7,        f4, botZ);
vert[2].pos.Set(f1 + (fref - f7), f4, botZ);
vert[3].pos.Set(f1 + fref + f7,   f4, topZ);
```

**Tuning:** a full `f6/2` shift slightly overshoots in probe 3 (icon center ends
a hair *above* the dots). The exact offset is "Wii glyph height vs corrected
Xbox height, halved" — i.e. shift up by `0.5 * (f6_corrected − f6_raw)` rather
than `0.5 * f6_corrected`. Concretely: compute `f6_raw = size * (mCellSize.y/
mCellSize.x)` and shift the whole quad up by `0.5f*(f6 − f6_raw)` (keep height =
f6). That re-centers *exactly* the amount the correction added, leaving square-
atlas (Wii-shape) glyphs untouched. **Implementer should iterate on the
song-select capture until icon-center == dot-row-center matches retail.**

**Alternative (also valid):** fix it in `UILabel` vertical positioning so the
icon label re-centers when its glyph height changes (it already has a
`CellDiff()`-based `mHeight` in `AdjustHeight`, `UILabel.cpp:650`, but that only
runs for `mFitType == kFitWrap`, which the single-glyph icon label is not). The
`SetupCharVerts` route is more localized and was the one A/B-proven.

**Risk / match-neutrality**
- Wrap everything in `#ifdef HX_NATIVE`; the `#else` keeps the byte-matched Wii
  geometry verbatim → **zero match impact** (`Text.o` is 98.78%, unchanged).
- Scoped to `*icon*`-material fonts → letter fonts (Pentatonic_*) and all normal
  text are untouched (verify by screenshotting the main hub / song list text
  after the change — no vertical shift expected).
- `std::strstr` per glyph: trivially cheap; the icon font draws ≤10 glyphs/frame.
- `m->Name()` / `m` guarded against null.

---

## VERIFICATION

```bash
# Build (in a worktree to avoid main-repo concurrency):
tools/setup-worktree.sh impl-diff-grid
cd .claude/worktrees/impl-diff-grid/native
cmake -B build-native -S . \
  -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build-native --target rb3-native -j"$(nproc)"

# Capture (use the implementer's assigned port, not 8601-8609):
python3 ../../../scripts/native/song-select-capture.py --port <P> \
  --bin "$PWD/build-native/rb3-native" --depths 16 --out /tmp/diff-grid-verify --verbose
```

**Pass criteria** (compare `/tmp/diff-grid-verify/native_depth_16.png` to
`images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`):
1. On every grid row, the instrument-icon center is on the SAME horizontal line
   as its dot-row / NO-PART label center (within ~2–3px). Use the gridline-
   overlay helper from `/tmp/rp-diff-grid/` to measure.
2. Icon SHAPE stays correct (full circle, not squished — do NOT regress the
   CellDiff fix).
3. No vertical shift of normal text: re-shoot main-hub menu + song-list rows and
   diff against a pre-change capture (`scripts/native/song-select-capture.py`
   depth 0 list rows).
4. The red devil/expert bar and the dots still render (functional, not just
   aligned).

**Match check:** `git -C <repo> diff --stat` shows only `Text.cpp`; rebuild the
Wii target and confirm `Text.o` match % is unchanged (the change is fully behind
`#ifdef HX_NATIVE`).

---

## REFERENCE SCREENSHOTS NEEDED

**None.** `images/retail-screenshots/yt_qRagnZCIMzk_song_select_diff_ratings.png`
is a clean Wii capture of exactly this panel and is sufficient ground truth
(icon center == dot center on every row). A higher-res 360/PS3 diff-ratings shot
would make sub-pixel tuning easier but is not required.
