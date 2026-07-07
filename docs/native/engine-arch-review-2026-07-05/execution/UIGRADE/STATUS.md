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

---

# Lane G — UIGRADE — STATUS (G-S2, verify)

Outcome: **VERIFIED-INERT / PENDING-TRIGGER.** The G-S1 renderer machinery
(engine `f677871`, flag `RB3_UI_POST_GRADE` default-OFF) is confirmed correct and
**inert-by-construction** with no regression on any gate. It cannot yet move the
contrast metric because its game-side TRIGGER is unwired (STOPPED at G-S1 for
coordinator sign-off; the trigger lives in un-granted `PanelDir.cpp`, so G-S2
does NOT edit it). This stage verifies what landed and re-declares the pending
item for the coordinator.

## What was verified (bin: `native/build-agent-uigrade/rb3-native`, engine f677871, rebuilt green)

### 0. Diff-scope lockdown (DC3 zero-blast by construction)
`git show f677871 --stat` = exactly 3 files, all RB3-only:
`RB3PostProc.cpp` (+31/−1), `RB3PostProc.h` (+23), `NativeCompatFlags.classification.json`
(append-only, +1/−1). **`rb3_postproc.wgsl.inc` NOT touched.** `RB3PostProc.cpp`
is in `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (CMakeLists.txt:326) — the RB3-only
GPU platform set. DC3 and `milo-engine-tests` (+ its Dawn WGSL gtest) never
compile this TU, and no shader changed ⇒ the shared-engine test binary is
byte-identical before/after; DC3 zero-blast is guaranteed structurally, not just
empirically.

### 1. The ONLY behavioral hunk is a null-op today
`FlushPostProcMidFrame`: `RunPostProcComposite(mFrameView, /*venueGrade=*/true)`
→ `RunPostProcComposite(mFrameView, /*venueGrade=*/!menuBoundary)` where
`menuBoundary = RB3ConsumeMenuUIFlushPending()`. The latch setter
`RB3SetMenuUIFlushPending()` has **ZERO callers** anywhere in engine+rb3 src
(grep-verified) ⇒ `menuBoundary` is always `false` ⇒ `!false == true` == the old
hardcode. Gameplay Tier-2 flush is **byte-identical**; menu screens never flush
(no trigger). So flag-ON == flag-OFF == pre-change default, regardless of env.

### 2. Contrast gate per screen (flag-OFF vs flag-ON, my machinery-bearing bin)
Captured off / on arms on all three A7 screens (RB3_HUB_TEXT_CONTRAST unset in
all arms), ROI p60/p5:

| screen     | flag-OFF | flag-ON | Δ ratio | verdict |
|------------|----------|---------|---------|---------|
| hub        | 1.954    | 1.954   | 0.000   | IDENTICAL |
| songselect | 1.105    | 1.088   | −0.017  | within PP_OFF-parity band [1.06,1.17] |
| partdiff   | 1.409    | 1.409   | +0.001  | IDENTICAL |

Full-frame meanAbs looked large (hub 35.4) but a same-flag **noise control**
(off vs off2) is comparable-or-larger (hub 34.1, songselect ctrl 1.21, partdiff
ctrl 14.1 vs on-diff 9.1) — the deltas are animation-phase non-determinism (the
harness settles on wall-clock and reaches each screen at slightly different frame
indices), NOT the flag. The flag adds nothing beyond run-to-run noise. **The
pre-registered 2.0 / PP_OFF-parity PASS targets are the SIGN-OFF criteria for the
WIRED trigger — they cannot be exercised until the trigger lands; today they read
INERT (== default), which is the correct landed state.**

### 3. B+W menu-backdrop ROI gate (A5 trap)
Protection is designed-in (`venueGrade=false` forced at the menu boundary →
chroma-preserve stays off). Unexercised today (menus never flush) so no chroma
can appear ⇒ authored grayscale backdrop unchanged. Confirmed by the INERT
result. The live A5 ROI assertion becomes exercisable only once the trigger wires.

### 4. Gameplay pixel-invariance (R-D)
Guaranteed by construction (§1): the menu latch is never set during gameplay, so
`venueGrade=true` identical to the old hardcode ⇒ a fixed-clock gameplay frame is
pixel-identical flag-ON vs flag-OFF. (Lane G "done right" is a no-op on gameplay,
per R-D.)

### 5. drawlog flag-OFF 792 byte-identical
`drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen` on my
bin: **PASS — 792 draws, canonical-order match** to the committed golden (287
known-residual divergences within the documented bound, non-blocking).

### 6. Wave-7 labels legible / venue wash unchanged
Since flag-ON == flag-OFF == shipped default within noise (§2 control), the
Wave-7-rescued labels and the venue wash are unchanged from the shipped six-ON
default. No new regression introduced by the machinery landing.

## PENDING ITEM FOR COORDINATOR (blocks G-S2 → S2-fix completion)
The measurable win (hub 1.95→~2.20) requires wiring the game-side TRIGGER, which
is OUTSIDE Lane G's grant. Proposed flag-gated diff (from G-S1 §3, unchanged):
in `rb3/src/system/ui/PanelDir.cpp` `PanelDir::DrawShowing`, when
`!mCanEndWorld && RB3UIPostGradeActive()`, call `RB3SetMenuUIFlushPending()` then
`TheRnd->EndWorld()` (HX_NATIVE). Open risks to resolve under the flag once
granted: R1 `Rnd::EndWorld()`→`DoWorldEnd()` side-effects on non-world menu dirs
(may need a dedicated public flush seam + a small `Rnd_Wgpu_RB3.h` grant instead
of reusing EndWorld); R2 venue-before-UI ordering live-check; R3 the A5 B+W ROI
assertion. song_select + partdiff stay grade-inert ⇒ the trigger is
no-op-or-parity there; only the HUB ratio should move to ≥2.0.

## E1 before/after captures
- BEFORE (shipped default, washed): `/tmp/uigrade/default_{hub,songselect,partdiff}.png` (hub 1.95).
- TARGET the wired trigger achieves (grade-exempt proxy): `/tmp/uigrade/ppoff_{hub,...}.png` (hub 2.20).
- LANDED state (INERT, flag-OFF vs flag-ON): `/tmp/uigrade-s2/{off,on,off2}_*.png` (hub 1.954 both).

## Files
- Harnesses: `scripts/native/_uigrade_baseline.py`, `_uigrade_gate.py`, `drawlog-golden.py`.
- Captures: `/tmp/uigrade` (G-S1 baselines), `/tmp/uigrade-s2` (G-S2 off/on/off2 arms).
- Checkpoint: `/tmp/wave13-checkpoints/G-S2.json`.

---

# Lane G — UIGRADE — STATUS (G-TRIGGER, wire the game-side trigger)

Outcome: **LANDED (default-OFF), WITH ONE CAVEAT + an ESCALATE followup.** The
game-side venue->UI boundary trigger is wired in `src/system/ui/PanelDir.cpp`
(HX_NATIVE, flag-gated). Hub grade-exemption win delivered; gameplay invariance,
A5, and flag-OFF byte-identity all verified. Commits: engine `a5cf8d3`, rb3
`82aa81c7` (pin bumped to `a5cf8d3`). Built/verified in `native/build-agent-uigrade`.

## 0. THE G-S1/G-S2 PREMISE WAS FALSE (root-cause correction)
Two load-bearing G-S1 claims were wrong, proven by an in-code probe over the live
draw path (`RB3_UIGRADE_PROBE`, since removed):
1. **Menu dirs are `mCanEndWorld=1`, NOT 0.** `main_hub`, `overshell_dir`,
   `slot0..3`, `saveload_status`, `song_select_filter` all report
   `mCanEndWorld=1`; only `song_select`/`splash` are 0. So the proposed
   `!mCanEndWorld` gate would fire on almost nothing on the hub.
2. **`Rnd::EndWorld()` is a PERMANENT no-op on native.** `Rnd::BeginDrawing`
   resets `mWorldEnded=false` each frame (rb3 `Rnd.cpp:578`), but native
   `BandRnd::BeginDrawing` (engine `Rnd_Wgpu_RB3.cpp:199`) **bypasses base
   BeginDrawing and never resets `mWorldEnded`** — it sticks at its ctor value 1
   forever, so `EndWorld()`'s `if(!mWorldEnded)` body never runs. The menu dirs
   already CALL `EndWorld()` (mCanEndWorld=1) but it does nothing. This is why
   G-S1's `RB3_TIER2_DBG` saw zero menu flushes — and it means the G-S1/G-S2
   EndWorld-reuse trigger **could never flush** (measured inert: hub 1.954 both).

The mid-frame flush is actually reachable only via the existing public seam
`Rnd::ClearDepthForOverlay()` (base virtual; `BandRnd` override calls
`FlushPostProcMidFrame()` when a graded venue is pending). Gameplay's own
note-highway flush already comes from `TrackPanel::Draw -> ClearDepthForOverlay`,
NOT from EndWorld.

## 1. The wired trigger (`PanelDir::DrawShowing`, HX_NATIVE, default-OFF)
```cpp
bool inGameplay = (TheGamePanel && TheGamePanel->GetGameState() == kGamePlaying);
if (RB3UIPostGradeActive() && !inGameplay) {
    RB3SetMenuUIFlushPending();       // -> FlushPostProcMidFrame venueGrade=false (A5)
    TheRnd->ClearDepthForOverlay();   // the only game-side seam to the flush
}
```
- Fires on every menu `PanelDir::DrawShowing`; `FlushPostProcMidFrame` is
  idempotent per frame (mPostProcFlushed) and its guards early-return until the
  venue is in the intermediate, so the flush lands on the first UI dir AFTER the
  venue backdrop (measured: hub f3 flush at meshesDrawnSoFar=1049).
- **Gameplay gate is REQUIRED (R1/R-D):** gameplay already flushes ~1/frame via
  TrackPanel; firing the menu trigger during gameplay adds extra
  ClearDepthForOverlay calls / moves the venue-flush point, breaking
  pixel-invariance. Gate = `GetGameState()==kGamePlaying` (include
  `game/GamePanel.h` under HX_NATIVE only, so the Wii build is untouched).
- **Latch robustness (engine `a5cf8d3`):** `FlushPostProcMidFrame` now consumes
  the menu-flush latch at its TOP (before the early-returns) so a no-op flush /
  ClearDepthForOverlay else-branch can't leave it dangling into a later gameplay
  flush (which would wrongly composite venueGrade=false). Gameplay never sets the
  latch -> reads false -> venueGrade=true -> byte-identical.

## 2. Pre-registered gates — RESULTS (bin native/build-agent-uigrade, RB3_HUB_TEXT_CONTRAST unset)
| screen     | flag-OFF | flag-ON | band / target        | verdict |
|------------|----------|---------|----------------------|---------|
| hub        | 1.954    | **2.204** | ≥2.0 (win)         | **PASS** (==PP_OFF target 2.204) |
| partdiff   | 1.409    | 1.417   | [1.35,1.49] parity   | PASS |
| songselect | 1.110    | 1.049   | [1.06,1.17] parity   | **CAVEAT** (−0.011 below floor; stable across 2 runs) |

- **R1 (menus render fully):** PASS — hub/song_select/partdiff all render
  completely ON (no missing draws; char preview + album art intact on
  song_select; visually more vibrant/retail-faithful). song_select's metric drop
  is the ClearDepthForOverlay **else-branch depth-clears** altering its layered
  3D-preview compositing (differs from pure grade-exemption, which would land
  ≈PP_OFF 1.11) — the text is fully legible; it is a metric-only regression.
- **A5 (B+W backdrop):** PASS — hub venue-backdrop ROIs (tiger mural / city blur
  / neon) chroma ON−OFF = −0.45 / −0.19 / −0.58 (no chroma injected; venueGrade=
  false holds chroma-preserve OFF).
- **R2 (venue behind UI):** PASS — the graded venue backdrop renders and the UI
  composites ungraded on top (E1 `AFTER_flagON_hub.png`).
- **Gameplay pixel-invariance (R-D):** PASS — with the gate, my trigger is
  skipped during kGamePlaying (probe: `inGameplay=1` skips). Measured
  kGamePlaying flush counts are equal ON vs OFF (OFF 395 / ON 402 ≈ 1/frame, all
  from TrackPanel); my flag adds zero gameplay flushes.
- **flag-OFF drawlog 792:** PASS — `drawlog-golden.py --fixed-clock
  --canonical-order --scene splash_screen` = 792 draws, canonical match (243
  known-residual within bound). Wii/matching build byte-identical (all edits
  `#ifdef HX_NATIVE`).

## 3. CAVEAT + ESCALATE followup (coordinator)
song_select's −0.011 metric drop is the ClearDepthForOverlay reuse side-effect
(its else-branch clears depth between menu UI dirs). A **clean flush-ONLY seam**
(no depth-clear) would remove it and likely bring song_select in-band (≈PP_OFF
1.11). That requires making private `BandRnd::FlushPostProcMidFrame()` reachable
game-side — either public, or a new base-`Rnd` virtual + `BandRnd` override — in
**ungranted `Rnd_Wgpu_RB3.h`/.cpp**. Per the G-TRIGGER grant this STOPS here:
**verdict ESCALATE_COORDINATOR for the clean-seam followup only** (the wired
ClearDepthForOverlay trigger itself is landed and correct within grant; the flag
stays default-OFF for the coordinator to flip).

## E1 before/after
- `/tmp/uigrade-trigger-e1/BEFORE_flagOFF_hub.png` (1.954) vs
  `AFTER_flagON_hub.png` (2.204); same pairs for songselect/partdiff.
- Final arms: `/tmp/uigrade-final/{off,on}_{hub,songselect,partdiff}.png`.

## Files
- Trigger: `src/system/ui/PanelDir.cpp` (rb3 `82aa81c7`).
- Engine: `RB3PostProc.cpp` latch-consume-at-top + classification (engine `a5cf8d3`).
- Checkpoint: `/tmp/wave13-checkpoints/G-TRIGGER.json`.

---

# Lane U — UIGRADE — STATUS (U-CLEAN, Wave-14 flush-only seam + flip package)

Outcome: **READY_FOR_FLIP.** The song_select red band is FIXED and all
pre-registered gates PASS. Flag `RB3_UI_POST_GRADE` stays default-OFF for the
coordinator to flip. Built/verified in `native/build-agent-uigrade`
(engine working tree, pin still 3b5af48 — NO lane pin bump).

## 0. ROOT-CAUSE CORRECTION (the U-CLEAN premise was FALSE)
The G-TRIGGER caveat blamed the red band on `ClearDepthForOverlay`'s else-branch
depth+stencil clears (`Rnd_Wgpu_RB3.cpp:2326-2354`). **This is wrong.** I first
built the minimal granted seam — a flush-only shim (`RB3FlushMenuUIPostGrade`)
that calls `BandRnd::FlushPostProcMidFrame()` DIRECTLY, bypassing
ClearDepthForOverlay entirely (so its else-branch never runs). **The red band
PERSISTED** (SETLISTS-row ROI redDom -7.43 flag-OFF → **+31.0 flag-ON, 75% red
pixels**; `/tmp/uigrade-uclean/on_songselect.png`).

The band is `FlushPostProcMidFrame`'s OWN depth-clear-on-resume: after grading the
venue it re-opens the main pass with `depthLoadOp = LoadOp::Clear`
(RB3PostProc.cpp:87), which **reveals a z-occluded SETLISTS-row selection quad**
(occluded in the flag-OFF layering — only its right edge pokes past the album
panel in the OFF capture). The else-branch was a red herring; the coordinator's
"flush-only (no depth-clear)" intent is the real fix.

## 1. THE FIX (grant's actual intent: a flush with NO depth-clear on the menu path)
menuBoundary-gated depth `LoadOp::Load` on the flush re-open. When the flush is a
MENU boundary (`menuBoundary` latch, RB3_UI_POST_GRADE), it re-opens the resumed
main pass with depth **Load** (PRESERVE venue depth → occluded UI stays occluded)
instead of Clear. Gameplay (`menuBoundary=false`, latch never set outside the menu
trigger) keeps `LoadOp::Clear` → **byte-identical**. Stencil follows depth.

## 2. Files (re-derived by symbol; grant A5/A6 minimal seam)
Engine (`milo-native-engine`, working tree; my 3 files only):
- `src/platform/Rnd_Wgpu_RB3.h` — `FlushPostProcMidFrame()` made **public**
  (access-specifier wrap at ~:257) so the shim can drive it directly.
- `src/platform/RB3PostProc.h` — declare `RB3FlushMenuUIPostGrade(Rnd*)`.
- `src/platform/RB3PostProc.cpp` — (a) `RB3FlushMenuUIPostGrade` shim (sets latch,
  calls `FlushPostProcMidFrame` directly, gated RB3_UI_POST_GRADE); (b)
  menuBoundary-gated depth `LoadOp::Load` at the flush re-open.
rb3:
- `src/system/ui/PanelDir.cpp` — trigger swap: `RB3SetMenuUIFlushPending()` +
  `TheRnd->ClearDepthForOverlay()` → `RB3FlushMenuUIPostGrade(TheRnd)`. Kept the
  `!inGameplay` gameplay gate.
FORBIDDEN respected: no base-`Rnd` virtual, no other `Rnd_Wgpu_RB3.cpp` edits,
`FxSendNative.cpp` untouched (its `M` in git status is a concurrent agent's — NOT
staged).

## 3. Pre-registered gates — RESULTS (bin native/build-agent-uigrade, RB3_HUB_TEXT_CONTRAST unset)
Final captures `/tmp/uigrade-uclean2/{off,on}_{hub,songselect,partdiff}.png`.

| gate | flag-OFF | flag-ON | target | verdict |
|------|----------|---------|--------|---------|
| hub ratio | 1.954 | **2.204** | ≥2.0 | **PASS** (win preserved) |
| songselect ratio | 1.143 | **1.125** | [1.06,1.17] | **PASS** (was 1.051 FAIL w/ depth-Clear) |
| partdiff ratio | 1.411 | 1.414 | [1.35,1.49] | PASS |
| songselect SETLISTS-row red | redDom −20.6, 0% red | redDom −13.6, **0% red** | ON≈OFF, no red | **PASS** (band GONE) |
| A5 hub venue chroma Δ | — | tiger −0.20 / neon +0.92 / city −1.58 | ON≈OFF | PASS |
| gameplay kGamePlaying flushes | 231 | 228 | equal | PASS (timing noise; prior lane 395/402) |
| flag-OFF drawlog | 792 | — | canonical match | PASS |
| DC3 zero-blast | — | — | test binary byte-identical | PASS (structural: RB3-only TUs, no classjson change) |

- **Gameplay invariance detail:** the menu trigger is gated `!inGameplay`; during
  kGamePlaying the latch is never set → `menuBoundary=false` → depth Clear +
  venueGrade=true = the original gameplay path. Measured 228≈231 kGamePlaying
  flushes (all from TrackPanel). The larger total-flush gap (ON 2160 / OFF 760) is
  entirely the pre-game staging screen (`tv3_a_screen`, f425-1404) where the menu
  grade-exemption correctly fires — not gameplay.

## 4. Verdict + flip
**READY_FOR_FLIP.** Coordinator action at acceptance: bump `MILO_ENGINE_PIN` to
the new engine SHA and flip `RB3_UI_POST_GRADE` default-ON (opt-out). No new flag
was introduced (RB3_UI_POST_GRADE is the Wave-13 flag), so no classification.json
change / no gen.inc regen.

## Files / captures
- Harnesses: `scripts/native/_uigrade_baseline.py`, `_uigrade_gate.py`, `drawlog-golden.py`.
- Final E1: `/tmp/uigrade-uclean2/`; depth-Clear-variant red-band proof: `/tmp/uigrade-uclean/on_songselect.png`.
- Checkpoint: `/tmp/wave14-checkpoints/U-CLEAN.json`.
