# W4.4-ROWFIX — STAGE B — STATUS

Lane: Wave-15 Lane B (song_select/partdiff focused-row polarity).
Outcome: **DIAGNOSIS-COMPLETE + ESCALATE.** The Wave-15 grant's prime suspect
(z-occluded selection quad, fixable via `RB3PostProc.cpp` menu-flush depth
handling) is **REFUTED at the renderer level.** The real defect is a two-part UI
focus-state gap; Part A (the missing bright fill) is FIXED and proven behind a new
default-OFF flag `RB3_ROWFIX`, Part B (dark focused-row text) is **BLOCKED** by a
separate native RndText color gap outside the granted scope. **The flag stays
default-OFF and must NOT be flipped until Part B lands** (fill-only would worsen
legibility). The granted `RB3PostProc.h:81` stale comment is cleaned up. No depth
code touched → the SETLISTS red-band fix is preserved by construction.

## 0. Method
Native headless (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free port, pgid cleanup),
`native/build-native` under `/tmp/rb3-native-build.lock`. New inert probe
`RB3_ROWFIX_DBG` logs every `UIListWidget::DrawMesh` (name/verts/GeomOwner/
matcolor/tex/blend/zmode). Pixel measurements with PIL on committed captures.

## 1. THE PRIME SUSPECT IS REFUTED (depth is NOT the mechanism)
The kickoff/A3 prime suspect: the focused-row fill is a z-occluded selection quad
that the now-default `RB3_UI_POST_GRADE` menu flush deliberately keeps occluded
(depth `LoadOp::Load`). **Disproven by three independent measurements:**

1. **The two focus-highlight meshes are `zmode=0` (kZModeDisable) — NO depth test
   at all.** `RB3_ROWFIX_DBG`: the song_select `UIListHighlight` draws exactly two
   meshes — `highlight_main.mesh` (`highlight_main.mat`, blend=3 srcAlpha,
   **zmode=0**) and `ml_highlight_glasstopp.mesh` (`ml_highlight_glasstop.mat`,
   blend=4 additive, **zmode=0**). A depth `LoadOp` cannot occlude a mesh that
   never samples depth.
2. **The focused-row fill is identical to a plain song row in BOTH depth
   variants.** Measuring the shipped `LoadOp::Load` capture vs the old
   `LoadOp::Clear` (red-band) capture, focused "25 or 6 to 4" fill = `[26,26,48]`→
   `[60,66,70]` — byte-for-byte the same as the neighbouring plain "Antibodies"
   song row (`[22,22,44]`→`[55,61,65]`). Depth-Clear reveals a generic per-row
   translucent band on EVERY row, not a distinct focus fill.
3. **The SETLISTS red band is a DIFFERENT quad** (measured red `[76,27,27]` only
   on the SETLISTS row under depth-Clear) — the U-CLEAN fix correctly suppresses
   it and is unrelated to the focused-row fill.

⇒ The menu-flush depth grant cannot fix this. **No depth code was touched.**

## 2. ROOT CAUSE (two coupled UI focus-state gaps)
Forcing `highlight_main` to opaque magenta lit only the thin top/bottom EDGES
(interior unchanged) → **`highlight_main` is the yellow FRAME, not a fill.**
Forcing `ml_highlight_glasstopp` to opaque yellow produced a **full-width solid
yellow bar** → **`ml_highlight_glasstopp` is the full-row fill quad**, but authored
as a near-invisible additive sheen (`matcol` white, **alpha 0.08**, additive).

The retail solid bright bar is produced by the list highlight's focus
animation/trigger — `list_song_select_browser.milo` `highlight_bar.grp` /
`highlight_light.trig` / `highlight_yellow.mesh` + `highlight_bar_color.tex`
(`highlight_yellow.mesh` LOADS but has **0 GPU draws** natively; forcing it
`SetShowing(true)` did not restore the fill — the whole focus-anim path is
un-translated). So natively:

- **(A) FILL:** the full-row quad draws only its faint sheen → no bright bar.
- **(B) TEXT:** the focused-row text stays WHITE (should be dark on the bright bar).

## 3. FIX — `RB3_ROWFIX` (default-OFF), game-side, scoped to the fill-bearing list
- **Part A — FILL (WORKS, proven).** `UIListHighlight::Draw`: when
  `RB3_ROWFIX` is set and `mMesh` is `ml_highlight_glasstopp`, repaint it as a
  **solid opaque fill** (`kBlendSrc`, texture dropped) in the highlight's own
  authored color (read from sibling `highlight_main.mat` → the yellow), and latch
  `RB3RowfixSetFillDrawn()`. `kBlendSrc` (not srcAlpha) is required — the quad
  carries a sheen vertex-alpha gradient that srcAlpha would reproduce.
- **Part B — TEXT (BLOCKED).** `UIListSlot::Draw`: on the highlighted-display
  element of the latched list, temporarily darken the label `UIColor` around the
  one draw. It **fires** (probe: darkened applied 3756×) and `UILabel::Draw` does
  set `fontMat->SetColor(dark)` (`UILabel.cpp:269`), **but the native RndText/glyph
  shader ignores the font-material color** — the text stays luma **167** with AND
  without the restore. This is a separate **native text-color gap** (same family
  as the engine's useAlphaAsRGB glyph path), in engine TUs OUTSIDE the
  `RB3PostProc.cpp` grant. Left in-tree (inert, default-OFF) so the flag encodes
  the complete intended fix and lights up once the native gap is closed.
- Per-list scoping: `UIList::DrawShowing` resets the latch before drawing widgets
  → only the list that drew the `ml_highlight` fill darkens its highlighted text.

## 4. GATES (bin `native/build-native/rb3-native`)
| gate | result |
|------|--------|
| Directional — focused FILL bright | flag-OFF p60=**94** (dark, RED) → flag-ON p60=**179** (bright yellow) **PASS-A** |
| Directional — text darker than fill | flag-ON text p5=**168** ≈ fill 179 → **FAIL-B** (native text-color gap, Part B blocked) |
| SETLISTS red-band (both arms) | **0% red both arms** (redDom −1.6/−1.5) — preserved (no depth edit) |
| flag-OFF drawlog | `drawlog-golden --fixed-clock --canonical-order --scene splash_screen` = **792 draws, canonical PASS** |
| flag-OFF byte-identical | all edits `getenv`-gated / `#ifdef HX_NATIVE` → Wii + flag-OFF unchanged |
| hub ≥2.0 / gameplay pixel-invariance | not affected — fix scoped to the `ml_highlight` fill quad (hub is not a UIList of this kind; no UIList highlight in gameplay) |
| fail-red control | flag-OFF reads RED on today's default (fill p60=94, below bright threshold) |

**Verdict:** ship-blocked on Part B. Directional gate is GREEN on the FILL axis
and RED on the TEXT axis; because the two are coupled, the flag must stay OFF.

## 5. WHAT THE COORDINATOR SHOULD DO
1. **Retire the depth-occlusion hypothesis for this screen** (§1). The
   `RB3PostProc.cpp` per-quad/LoadOp grant is not the lever; the depth handling is
   already correct and the red-band fix stands.
2. **Open a native RndText/list-label color-override item** (Part B) — the fix
   that makes `RB3_ROWFIX` flippable. Likely engine `Rnd_Wgpu_RB3.cpp` glyph
   material-color binding (out of Lane B's `RB3PostProc.cpp` grant).
3. Alternatively, the fully faithful route is to translate the list highlight
   focus anim/trigger (`highlight_bar.grp`/`highlight_light.trig`) so
   `highlight_yellow.mesh` + `highlight_bar_color.tex` compose the bar as authored
   — a UI-animation subsystem task.
4. `partdiff` GUITAR: not re-characterized this stage (time spent nailing the
   song_select mechanism); it sits on a different layout (yellow EASY bar + dark
   text) and should get its own ROI/polarity pass — but the depth prime suspect is
   equally inapplicable there (same zmode=0 UI highlight family).

## Files
- rb3: `src/system/ui/UIListHighlight.cpp` (Part A), `UIListSlot.cpp` (Part B),
  `UIList.cpp` (latch reset), `UIListWidget.cpp`/`.h` (`RB3Rowfix*` + `RB3_ROWFIX_DBG`).
- engine: `src/platform/RB3PostProc.h` (stale `:81` comment cleanup),
  `NativeCompatFlags.classification.json` (append `RB3_ROWFIX`). **`FxSendNative.cpp`
  UNTOUCHED** (its `M` is a concurrent agent's, not staged).
- E1: `e1/BEFORE_flagOFF_songselect.png` (dark fill, white text) vs
  `e1/AFTER_flagON_songselect.png` (bright yellow fill, Part A) vs retail
  `images/retail-screenshots/yt_qRagnZCIMzk_song_select_list.png`;
  `e1/depthClear_redband_reference.png` (the DIFFERENT red-band quad).
- Checkpoint: `/tmp/wave15-checkpoints/B.json`.
