# Menu selection-highlight mispositioned — root cause + fix

Status: **DONE / VERIFIED** for the static UIList overshell screens (choose-instrument,
choose-difficulty). The hub "focused item doesn't enlarge" is a *separate* mechanism
(BandButton focus-anim) — see "Hub note" below; the stray detached box on the hub is
resolved by this same fix.

Branch: `wt-task-menu-highlight`  Commit: `89e3beef`
File changed: `src/system/rndobj/Text.cpp` (+16, all `#ifdef HX_NATIVE`-gated)
Wii build: **byte-identical** (`RndText::WrapText` fuzzy match unchanged 92.69526% → 92.69526%).

---

## Symptom (user)

> "The test for the 'highlighted' item is wrong. On the main menu it's stuck randomly
> in the middle of the screen … Song selection in the library works fine. The
> difficulty/instrument select is wrong (doesn't highlight properly either)."

Observed in `rb3-native` (reproduces identically on web): on **choose-instrument**
(GUITAR/BASS) and **choose-difficulty** (EASY/MEDIUM/HARD/EXPERT) the yellow selection
bar sat ~half an element-spacing **below** the focused row — e.g. with HARD selected
the bar straddled HARD/EXPERT, mostly on EXPERT. The song library list highlighted
fine.

## Root cause — NOT the UIList position math

The hypothesis going in was a decomp regression in `UIListDir::ElementSpacing` /
`BuildDrawState` / `UIListState::BuildScroll`, or in `BandList::DrawShowing`'s
HighlightObject loop. All three were **refuted**:

- The recent permuter commits to `BuildDrawState` (`a44c1f16`, `3530046c`) and the
  `volatile` cast in `BandButton::SetState` (`47e2bd22`) are behavior-neutral
  (temp-extraction / load-barrier only). `BandList::DrawShowing`'s
  `z = -(space*SelectedDisplay() - z)` has been stable across the churn.
- The overshell lists have **empty** `mHighlightObjects`, so `BandList::DrawShowing`'s
  highlight loop never runs (instrumented: 0 hits). The visible bar is the
  **`UIListHighlight`** widget (`highlight.mesh`) positioned from `drawState.mHighlightPos`.
- **Decisive instrumentation** (`UIListHighlight::Draw` vs `UIListSlot::Draw`, env-gated
  `RB3_HL_DBG`, headless) on choose-difficulty with HARD (disp 2) selected:
  - highlight final `xfm2.v = (-71.30, 122.01, -157.29)`
  - selected element final `tfa8.v = (-71.30, 122.01, -157.29)` — **identical.**

  The highlight mesh is placed **exactly** on the selected element's transform origin.
  The position math is correct. What was wrong is **where the text glyphs render
  relative to that origin**.

### The actual bug: `RndText::WrapText` vertical centering uses the wrong cell-diff

`src/system/rndobj/Text.cpp`, `RndText::WrapText` (the line-layout pass):

```cpp
// vertical-centering offset for mAlign & 0x20 (middle) / & 0x40 (bottom)
float ratio = 0.0f;
for (... fonts ...) {
    float cx = font->mCellSize.x;
    float diff = font->mCellSize.y / cx;   // <-- RAW cell aspect (RawCellDiff)
    if (diff > ratio) ratio = diff;
}
ratio *= style.size;
float topY = 0.0f;
if (mAlign & 0x20) topY = 0.5f * ratio * ((lines-1)*mLeading + 1.0f);  // middle
```

But the glyph **height** in `SetupCharVerts` is `f6 = style.size * definingFont->CellDiff()`
(`Text.cpp:1261`) — the **native-corrected** `CellDiff()`.

On the native port, `RndFont::CellDiff()` (Font.h, `#ifdef HX_NATIVE`) applies a
wide/tall-atlas correction for the **non-square Xbox font atlases**; `RawCellDiff()`
(= `mCellSize.y/mCellSize.x`) does not. So for a `kMiddleCenter` (0x22) single-line
label on a non-square atlas:

- centering `topY = 0.5 * RawCellDiff * size`
- glyph spans `topY` down to `topY − CellDiff*size`
- visual center = `0.5*size*(RawCellDiff − CellDiff)` — **non-zero** → block drifts off
  origin by ~half a line. For the overshell label font (wide atlas, `CellDiff < Raw`)
  the center lands **above** origin → text renders high, so the correctly-placed
  highlight box appears low. Exactly the reported symptom.

Alignment `0x22` decodes as **kMiddleCenter** (bit `0x20` = vertical-middle).

## Fix

Make the centering metric use the **same** cell-diff the glyph height uses, under
`HX_NATIVE` only:

```cpp
#ifdef HX_NATIVE
        float diff = font->CellDiff();      // matches SetupCharVerts glyph height
#else
        float cx = font->mCellSize.x;        // unchanged Wii path
        float diff = font->mCellSize.y / cx;
#endif
```

Square-atlas / letter fonts have `CellDiff() == RawCellDiff()`, so this is a **no-op**
for normal UI text and the song library list (which is why those never showed the bug).
Only non-square-atlas middle/bottom-aligned blocks are corrected.

This is a port-correctness fix in the native path, not a hack: it removes a metric
mismatch the Wii code never had (square atlases there).

## Before / after evidence (headless, `scripts/native/keyboard-to-gameplay.py`)

choose-difficulty, HARD selected, measuring yellow-bar center vs the four difficulty
text-row centers (px, 1280×720):

| | bar center | text rows (EASY..EXPERT) | HARD vs bar |
|---|---|---|---|
| BASELINE | 610 | 488 / 549 / **588** / 627 | bar 22px below HARD (≈ on EXPERT) |
| FIXED    | 610 | 532 / 571 / **610** / 650 | **0px — HARD centered in bar** |

The bar never moved (it was always at the right origin); the **text** moved down to meet
it. Visual crops confirm HARD perfectly framed; choose-instrument GUITAR likewise (same
font + same `choose_*.lst` code path).

Song library (`01_song_select.png`): **unchanged** — pixel diff is only the animated
background/album-art panel; the song-row text + highlight alignment is identical
baseline↔fixed (that font/alignment doesn't hit the corrected branch).

Evidence dir: `/tmp/menu-hl-fix/` (`baseline-partdiff/`, `fixed-flow/`, `final-verify/`,
`*_crop.png`).

## Wii byte-identity / match delta

- Change is entirely inside `#ifdef HX_NATIVE … #else … #endif`; `HX_NATIVE` is **not**
  defined in the Wii (`SZBE69_B8`) build, so the preprocessed Wii TU is unchanged.
- `RndText::WrapText` (`WrapText__7RndText…`) fuzzy match: **92.69526%** before
  (main-repo `report.json`) and **92.69526%** after (worktree `report.json` regenerated).
  No code/data % movement.

## Hub note (out of scope of this fix, but related)

The main hub (`main_hub_panel`) is **not** a UIList — its items are `BandButton`s with
`(focus "mb_playnow.btn")`. In retail the focused button's text **enlarges** via
`BandButton::SetState → mFocusAnim->Animate()` (the milo `focus_anim`). Native not
enlarging the focused hub item is a *separate* mechanism (BandButton focus-anim
loading/firing), not the text-centering bug — left for a follow-up. The stray detached
yellow **box** that appeared over the hub menu in the baseline is gone after this fix
(it was the same mis-centered-text artifact feeding the focus highlight mesh).

## Landing notes

- Exact change: `src/system/rndobj/Text.cpp`, `RndText::WrapText`, the
  `ratio`-accumulation loop (~line 950): wrap the `diff` computation in
  `#ifdef HX_NATIVE` (use `font->CellDiff()`) / `#else` (original
  `font->mCellSize.y / font->mCellSize.x`). +16 lines incl. comment.
- Shared engine `src/` (compiled by both rb3 native + web + Wii) — the gate keeps Wii
  byte-identical; web inherits the fix automatically (same `HX_NATIVE` TU).
- No engine-repo (`milo-native-engine`) change; no `MILO_ENGINE_PIN` bump needed.
- Build: `cmake --build native/build-native --target rb3-native -j` (clean).
- Diagnostic harness used (not committed): env-gated `RB3_HL_DBG` prints in
  `UIListHighlight::Draw` + `UIListSlot::Draw` to dump highlight-vs-element transforms;
  reverted before commit. Reusable repro: `scripts/native/keyboard-to-gameplay.py`
  reaches choose-difficulty reliably; `/tmp/menu-highlight-repro.py` for the hub.
- Branch `wt-task-menu-highlight`, commit `89e3beef`.
