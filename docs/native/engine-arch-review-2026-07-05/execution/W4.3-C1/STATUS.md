# W4.3-C1 STATUS

## Outcome (2026-07-07)
DIAGNOSED. Both A7 leading candidates DISPROVEN. Root cause is a render/postproc
compositing wash, not a color/focus-state plumbing bug. A faithful sub-fix (bar
alpha clamp) is landed flag-first default-OFF, but it does NOT by itself pass the
C1 contrast gate — the gate-passing fix lives in the fenced render/postproc path
and is ESCALATED to the coordinator.

## Path tracing (the C1 task) — what route the hub items take
1. Hub top-level items `mb_playnow.btn / mb_career / mb_trainers / mb_shop /
   mb_musicstore` are `UILabel`s under `UILabelDir`. They ALL share ONE font
   material instance `Pentatonic_Regular_(9_00)4x.mat` @0x…eda80500.
2. `UILabel::DrawShowing` (src/system/ui/UILabel.cpp:266-294) applies the focus
   state colour: `mLabelDir->GetStateColor(mState, color)` →
   `fontMat->SetColor(color)`. Verified live via a temporary `RB3_UILABEL_DBG`
   probe:
   - `mb_playnow.btn` state=1 (kFocused) → statecol=matcol=**(0.118,0.122,0.035)**
     (a dark olive — the intended dark-on-gold colour, matches retail).
   - `mb_career.btn` state=0 (kNormal) → (0.753,0.753,0.753) bright.
   So the focus-state DARK colour IS applied. **A7 candidate 1 (focus-state colour
   never applied natively) is FALSE.**
3. `RB3MaterialBinder` reads `mat->GetColor()` and plumbs it through. Verified via
   a temporary `RB3_C1_DBG` probe logging final `mu.color` + mesh pointer + world
   pos: the focused glyph mesh at PLAY NOW's screen position (world z=104.4) gets
   `finalcol=(0.118,0.122,0.035)` — the DARK colour reaches the shader unchanged;
   the relaxed UI-text floor does not touch it (R,G > 0.06). Each of the 5 labels
   has a DISTINCT glyph mesh pointer with its own per-instance uniform slot, so
   there is NO shared-material race. **A7 candidate 2 (font-material variant
   bypassing the binder's UI-text branch) is FALSE.**
4. Draw order (temporary `RB3_C1_ORDER` probe, frame 450): highlight bar
   `highlight_main.mesh` draws at seq 299 (col 0.820,0.820,0.169, **alpha=3.560**,
   blend=3 kBlendSrcAlpha, zmode=0), then the dark PLAY NOW text at seq 300
   (col 0.118, alpha 1.0). Text is drawn LAST, on top, opaque-authored. Whole
   frame has only ONE bar draw and ONE PLAY NOW text draw — nothing bright overlaps.

## Why it still renders washed (mechanism, evidenced)
The authored-dark text is plumbed correctly yet renders low-contrast pale olive.
Measured (focused-bar ROI, luma): retail glyph=44 / bar=164 → **4.17 PASS**;
native glyph~115 / bar~223 → **1.95 FAIL** (matches A11's "current ~1.1-1.3" order).
Isolation experiments (all via temporary binder flags, reverted):
- **NOBAR** (force highlight-bar alpha=0): PLAY NOW glyph → **luma 1 (near-black)**.
  Removing the bar makes the text dark. ⇒ the BAR causes the wash.
- **BAR alpha clamp** 3.56→1.0: bar 231→217, text ~unchanged (~113). Gate 1.95→1.81
  (slightly WORSE — clamp dims the p60 reference field more than the text).
- **BAR rgb dim** ×0.6/0.35/0.15: text tracks the bar (text ≈ 0.7-0.8×bar in every
  case) — ratio stays 1.2-1.5. ⇒ NO in-binder bar manipulation can create the
  contrast; the wash scales with the visible bar.
- **RB3_HIGHWAY_BLOOM_OFF**: no effect (halo bloom is gem-confined, as designed).
- **RB3_PP_OFF** (postproc grade off): text 114→94, bar 223→208, gate → **2.20 PASS**.
  The postproc GRADE lifts the dark focused glyphs (+21%) more than the bar (+7%),
  compressing contrast below the gate; it is the tipping factor on top of the base
  bar-through-anti-aliased-text bleed.

Conclusion: the focused item's dark colour is correct end-to-end; the contrast is
destroyed downstream by (a) the focus highlight bar compositing through the
~semi-transparent AA text and (b) the postproc grade lifting the dark text. Both
live in the render/postproc path (Rnd_Wgpu_RB3.cpp draw-flow + RB3PostProc grade),
which is outside this lane's game-side/binder fence and overlaps Lane B's TU.

## Sub-finding (native bug)
The focused hub highlight-bar material alpha is animated to **3.56** (>1) by the
focus pulse anim. Wii's fixed-function blender clamps src alpha to [0,1]; native
does not, over-brightening the bar (bar reads luma ~231 vs retail's ~188). Fixed
flag-first (below). This is a faithful correctness fix but is NOT sufficient for
the C1 gate.

## Landed
- `RB3_HUB_TEXT_CONTRAST` (default-OFF, engine RB3MaterialBinder.cpp): clamps the
  isUiHighlightOverlay alpha to ≤1.0. Registered in classjson (ui/hub, workaround).
  flag-OFF byte-identical. Ticker / focus labels unaffected (only bounds the bar's
  src alpha) — ticker no-regression measured identical flag-OFF vs flag-ON.
- Cleanup: removed my `RB3_C1_ORDER` diagnostic probe that commit 218494a
  (Lane B W2.8g) accidentally swept into HEAD via the shared working tree.

## ESCALATION (for coordinator, per A9/A10)
The gate-passing fix needs a render/postproc change (declared-range grant):
- Option A: draw hub UI text AFTER the postproc grade (or exclude UI from the
  grade). RB3_PP_OFF proves the grade tips the gate (2.20 PASS vs 1.95).
- Option B: make focused UI text fully opaque over the bar / keep the bright bar
  from compositing through the AA text (Rnd_Wgpu_RB3.cpp draw-flow, Lane B TU).
Evidence PNGs: /tmp/wave12-current-state/C1-evidence/ (flagOFF, flagON, NOBAR
proof text→dark, PP_OFF gate 2.20, retail vs native crops).

## Not re-verified this pass
song-select highlighted row + partdiff GUITAR — same font/material family and the
same render/postproc wash mechanism is expected to apply; deferred to the escalated
render/postproc fix (fixing the mechanism fixes all three).
