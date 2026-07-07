# Lane G — UIGRADE — STATUS (G-S1)

Outcome: **DONE (G-S1).** Baselines captured + A11 gate pre-registered per screen;
"why don't menus flush" answered with live proof; flag-first `RB3_UI_POST_GRADE`
(default-OFF) renderer machinery landed in granted files, build green, flag-OFF
AND flag-ON both byte-identical to default (trigger unwired). The game-side
TRIGGER is declared below and **STOPPED for coordinator sign-off** before S2.

## 0. Baselines + A11 percentile gate (A7) — DONE
Captured hub focused / song-select highlighted row / partdiff GUITAR at
`default` and `RB3_PP_OFF=1` (RB3_HUB_TEXT_CONTRAST UNSET in all arms).
Harness `scripts/native/_uigrade_baseline.py`; gate `scripts/native/_uigrade_gate.py`.
PNGs `/tmp/uigrade/{default,ppoff}_{hub,songselect,partdiff}.png`.

ROI luma ratio = p60(bar field)/p5(text stroke):

| screen     | default | PP_OFF | pre-registered PASS (S2) |
|------------|---------|--------|--------------------------|
| hub        | **1.95** | **2.20** | ratio >= 2.0 (PP_OFF reaches it; grade exemption is the win) |
| songselect | 1.14    | 1.11   | PP_OFF-PARITY: ON within [1.06,1.17] (PP_OFF<2.0, grade-inert) |
| partdiff   | 1.41    | 1.42   | PP_OFF-PARITY: ON within [1.35,1.49] (PP_OFF<2.0, wash is bar-bleed) |

**KEY FINDING (feeds coordinator):** grade exemption is a MEASURABLE win only on
the HUB (1.95→2.20). song-select (light text on dark row) and partdiff (venue
behind a details panel) are grade-INERT — PP_OFF ≈ default — so Lane G's
mechanism cannot move them; their gate is no-regression parity. Their residual is
the OTHER C1 factor (bright bar compositing through semi-transparent AA text /
light-text-on-dark polarity), NOT the grade, and is out of Lane G's scope
(shape-c opaque-text or a separate lane).

## 1. Why don't menus flush today? (A6 first question) — ANSWERED w/ live proof
- The mid-frame flush `BandRnd::FlushPostProcMidFrame` runs only via
  `BandRnd::DoPostProcess` ← `Rnd::EndWorld()` (rb3 `src/system/rndobj/Rnd.cpp:614`,
  guarded `!mWorldEnded`).
- `Rnd::EndWorld()` is called game-side from `GamePanel.cpp:568` (gameplay),
  world `Dir.cpp:474`, and `PanelDir::DrawShowing` (`src/system/ui/PanelDir.cpp:133-134`)
  — the last GATED on `mCanEndWorld`, i.e. the `postprocs_before_draw` SYNC_PROP
  (`PanelDir.cpp:481`; ctor default 1 `:20`).
- LIVE PROOF: `RB3_TIER2_DBG=1` across hub + song_select + partdiff = **ZERO**
  DoPostProcess/flush events over 2280 frames (`/tmp/rb3-uigrade-probe-*.log`),
  while the hub grade is demonstrably applied (1.95 washed vs 2.20 PP_OFF).
  ⇒ menu UI PanelDirs run with `postprocs_before_draw=false` (`mCanEndWorld=0`),
  no world-EndWorld boundary fires, and the single `EndFrame` composite
  (`Rnd_Wgpu_RB3.cpp:1993-1996`, venueGrade default false) grades venue+UI
  together. THAT is the wash.

## 2. Flag-first landing (granted files) — DONE
Engine (pin 44716f4, my build `native/build-agent-uigrade`, BUILD OK):
- `RB3PostProc.h` — `RB3UIPostGradeActive()` (RB3_UI_POST_GRADE, default-OFF) +
  menu-flush latch `RB3SetMenuUIFlushPending()`/`RB3ConsumeMenuUIFlushPending()`.
- `RB3PostProc.cpp` — accessor + file-scope latch; **A5-trap fix**:
  `FlushPostProcMidFrame` now grades with `venueGrade = !RB3ConsumeMenuUIFlushPending()`
  (menu boundary → false → chroma-preserve stays OFF, authored B+W look intact;
  gameplay Tier-2 → latch unset → venueGrade=true, unchanged).
- `NativeCompatFlags.classification.json` — appended `RB3_UI_POST_GRADE`
  (append-only textual insert under lock; no gen.inc regen).
- Signature of `FlushPostProcMidFrame` UNCHANGED → `Rnd_Wgpu_RB3.h` (ungranted)
  untouched.

INERT verification (new binary): hub gate flag-OFF **1.954**, flag-ON **1.954**
(== default baseline). Flag-ON is inert because the trigger is unwired.

## 3. TRIGGER SITE — OUTSIDE GRANT — NEEDS COORDINATOR SIGN-OFF (STOP)
The venue→UI boundary trigger must come from the game-side UI draw path:
`rb3/src/system/ui/PanelDir.cpp`, `PanelDir::DrawShowing` (`:133-134`). Proposed
diff (flag-gated, HX_NATIVE), mirroring how gameplay's `TrackPanel::Draw` →
`ClearDepthForOverlay` triggers Tier-2:

```cpp
void PanelDir::DrawShowing() {
#ifdef HX_NATIVE
    // Wave-13 Lane G (RB3_UI_POST_GRADE): menu UI dirs author
    // postprocs_before_draw=false, so their venue backdrop + UI are graded
    // together at EndFrame (washing focused text). Flush the venue grade at this
    // venue->UI boundary (venueGrade=false) so UI draws ungraded. Inert unless a
    // postproc + intermediate are live (FlushPostProcMidFrame early-returns).
    if (!mCanEndWorld && RB3UIPostGradeActive()) {
        RB3SetMenuUIFlushPending();
        TheRnd->EndWorld();          // -> BandRnd::DoPostProcess -> FlushPostProcMidFrame
    }
#endif
    if (mCanEndWorld)
        TheRnd->EndWorld();
    ...
```

Open risks for sign-off (S2 to resolve under the flag):
- (R1) `Rnd::EndWorld()` also runs `DoWorldEnd()` (world-cam copy bookkeeping) on a
  UI PanelDir that may not be a world dir. The flush body itself is guarded
  (`mRenderedToIntermediate && RndPostProc::Current() && !mPostProcFlushed`) so it
  no-ops when there's no graded venue; but the `DoWorldEnd`/`mWorldEnded` side
  effects on menu dirs need an A/B check. If unsafe, S2 adds a dedicated public
  flush seam instead of reusing EndWorld (that would need a small `Rnd_Wgpu_RB3.h`
  grant).
- (R2) Ordering: the trigger fires at the TOP of the UI PanelDir's DrawShowing —
  the venue must already be in the intermediate by then (drawn by a prior dir this
  frame). Verified only that no flush fires today; the venue-before-UI ordering
  needs a live check once wired.
- (R3) B+W menu-backdrop ROI gate (A5) — pin a hub venue ROI (excluding UI) and
  assert ON≈OFF, since the flush must not tint the authored B+W look. venueGrade
  is already forced false at the flush, so chroma-preserve stays off by
  construction; the gate is the belt-and-suspenders check.

Because song-select/partdiff are grade-inert, S2 should also pre-register that
this trigger is a NO-OP-or-parity there (the flush no-ops if the screen isn't
rendered into a graded intermediate), and that the HUB is the only screen where
the ratio should move (target ≥2.0).

## Files
- Harness: `scripts/native/_uigrade_baseline.py`, gate `scripts/native/_uigrade_gate.py`.
- Baselines: `/tmp/uigrade/`; probe log `/tmp/rb3-uigrade-probe-*.log`.
- Checkpoint: `/tmp/wave13-checkpoints/G-S1.json`.
