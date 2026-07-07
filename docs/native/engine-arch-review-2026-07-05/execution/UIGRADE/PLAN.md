# Lane G — UIGRADE (grade-exempt UI compositing) — PLAN

KEY=UIGRADE, STAGE=G-S1. Engine pin `44716f4`. Grant per WAVE13 A6.

## Objective
Menus over-grade their UI: the whole frame (venue backdrop + UI) renders into the
postproc intermediate and is graded ONCE at `BandRnd::EndFrame`, washing the
focused-item text. Fix shape (A5) = GENERALIZE the existing Tier-2
`FlushPostProcMidFrame` to fire at the menu venue→UI boundary, so UI draws
UNGRADED over the graded venue — parameterizing `venueGrade=false` to avoid the
chroma-preserve trap. Flag `RB3_UI_POST_GRADE`, default-OFF.

## Mechanism (S1 first question: why don't menus flush today?) — ANSWERED
- `BandRnd::DoPostProcess` → `FlushPostProcMidFrame` fires only from
  `Rnd::EndWorld()` (rb3 `src/system/rndobj/Rnd.cpp:614-622`, guarded `!mWorldEnded`).
- `Rnd::EndWorld()` is invoked game-side from: `GamePanel.cpp:568` (gameplay),
  world `Dir.cpp:474`, and `PanelDir::DrawShowing` (`src/system/ui/PanelDir.cpp:133-134`)
  — the latter GATED on `mCanEndWorld`, which is the `postprocs_before_draw`
  SYNC_PROP (`PanelDir.cpp:481`; ctor default 1, `:20`).
- LIVE PROOF: `RB3_TIER2_DBG=1` on hub + song_select + partdiff = ZERO
  DoPostProcess/flush events across 2280 frames (`/tmp/rb3-uigrade-probe-*.log`).
  Yet the hub grade IS applied (hub p60/p5 default 1.95 vs RB3_PP_OFF 2.20).
  ⇒ menu UI PanelDirs run with `postprocs_before_draw=false` (`mCanEndWorld=0`)
  and have no separate world-EndWorld boundary, so no mid-frame flush occurs and
  the single `EndFrame` composite (`Rnd_Wgpu_RB3.cpp:1993-1996`,
  `RunPostProcComposite(mFrameView)`, venueGrade default false) grades venue+UI
  together.

## Baselines + A11 gate (captured, arms: default / RB3_PP_OFF; HUB_TEXT_CONTRAST UNSET)
ROI p60(bar field)/p5(text stroke) luma, /tmp/uigrade, `_uigrade_gate.py`:

| screen     | default | PP_OFF | pre-registered PASS |
|------------|---------|--------|---------------------|
| hub        | 1.95    | 2.20   | ratio >= 2.0 (PP_OFF reaches it) |
| songselect | 1.14    | 1.11   | PP_OFF-parity [1.06,1.17] (PP_OFF<2.0; light-text-on-dark row, grade-inert) |
| partdiff   | 1.41    | 1.42   | PP_OFF-parity [1.35,1.49] (PP_OFF<2.0; wash is bar-bleed not grade) |

KEY FINDING: grade exemption is a measurable win ONLY on the hub (1.95→2.20).
song-select + partdiff are grade-INERT (PP_OFF ≈ default) → Lane G's mechanism
must be a NO-REGRESSION (parity) on them; their residual is a separate factor
(bar-through-AA-text bleed / light-text row), NOT grade — out of Lane G scope.

## File ranges (re-derived BY SYMBOL on engine 44716f4)
GRANTED, editing in G-S1:
- `src/platform/RB3PostProc.h` — add `RB3UIPostGradeActive()` + a menu-flush
  pending setter/getter near the other flag decls (after `RB3PPChromaPreserveActive`).
- `src/platform/RB3PostProc.cpp`:
  - `FlushPostProcMidFrame` `:44-89` — body edit ONLY at the `RunPostProcComposite`
    call (`:55`): parameterize `venueGrade` from the menu-flush-pending static.
  - flag accessors block `:189-215` — add `RB3UIPostGradeActive()` +
    `RB3SetMenuUIFlushPending()/RB3ConsumeMenuUIFlushPending()` (file-scope static).
- Classification append: `src/platform/NativeCompatFlags.classification.json`
  (append-only, under lock; NO gen.inc regen).

GRANTED, declared for S2 (NOT edited in G-S1): `Rnd_Wgpu_RB3.cpp:1973-2010`,
`gfx/Shaders/rb3_postproc.wgsl.inc` (no shader change expected — venueGrade=false
already keeps chroma-preserve OFF).

## TRIGGER SITE — OUTSIDE GRANT — STOP FOR COORDINATOR SIGN-OFF
The venue→UI boundary trigger is game-side: `src/system/ui/PanelDir.cpp`
(`DrawShowing`, `:133-134`). Proposed diff in STATUS.md. NO edit until sign-off.
Renderer machinery (flag + parameterized flush) proceeds in granted files;
with flag OFF and trigger unwired, build is byte-identical (verified at G-S1).

## Gates (G-S1 exit)
- Flag default-OFF; granted-file build compiles; flag-OFF path byte-identical
  (static never set → venueGrade=true unchanged for gameplay Tier-2).
- Baselines + pre-registration committed. Trigger declared + STOPPED for sign-off.
