# N7 — Make the loading screen perceptible (implementation plan)

**Authored:** 2026-05-29 (planning subagent, Opus, READ-ONLY — no source edits, no
build, no commit). Companion to `LOAD_SCREEN_RENDER.md` (root-cause) and item N7
in `STATUS_AND_NEXT_GOALS.md`.

**One-line outcome:** hold `preloading_screen` (and its `tv3_b` cinematic
vignette) for a configurable number of EXTRA FRAMES so the vignette PropAnim
actually animates and a screenshot sweep can perceive the loading sequence.
Opt-in, default no-op.

---

## 1. Root-cause confirmation

Confirmed against the code + assets (not just the prior doc):

- **The transition chain is real and screen names are correct.**
  - `preloading_screen` is a real `BandScreen` — `ui/loading/loading.dta:62`
    (`(panels meta preload_panel transition_fallback_panel)`, `(focus
    preload_panel)`, `(show_fallback FALSE)`).
  - `on_preload_ok` (`ui/loading/loading.dta:37-46`) runs
    `{ui goto_screen {gamemode get game_screen}}` when the current screen is
    `preloading_screen`.
  - `tv3_b` is the cinematic transition vignette wrapping
    `preloading_screen → game_screen` (`config/vignettes.dta:91-113`,
    `DIFFICULTY_TO_GAME_TRANSITION` → `("tv3_b")` for a video-class venue). It is
    committed under the screen name `tv3_b_screen` (per the
    `LOAD_SCREEN_RENDER.md` `UISCREEN_DBG` trace; the literal `"tv3_b"` token is
    the vignette key in `vignettes.dta`).

- **Why the load is instant.** The `preloading_screen` commits only while its
  `preload_panel` (a `BandPreloadPanel`) reports not-loaded. The gate is:
  - `BandPreloadPanel::IsLoaded()` (`src/band3/meta_band/BandPreloadPanel.cpp:40`)
    `= PreloadPanel::IsLoaded() && !mLockInProgress`.
  - `PreloadPanel::IsLoaded()` (`src/system/meta/PreloadPanel.cpp:123`)
    `= UIPanel::IsLoaded() && mPreloadResult != kPreloadInProgress`.
  - `PreloadPanel::PollForLoading` (`:99`) flips `mPreloadResult` to
    `kPreloadSuccess` as soon as `sCache->DoneCaching()` — and the native
    `CheckFileCached` HX_NATIVE patch (`PreloadPanel.cpp` ~`:204`) makes the
    file-cache staging a no-op (direct `FileStream`, no disc latency), so the
    panel reports loaded within ~1 poll. The `LockStepMgr` resolves locally on
    the same/next frame offline (`mLockInProgress` clears immediately).
  - Net: `preloading_screen->CheckIsLoaded()` is true within ~1 poll; the UI
    state machine (`UIManager::Poll`/`PollTransition`, `src/system/ui/UI.cpp:514`
    /`567`) advances `kTransitionTo → kTransitionFrom`, the
    `UITransitionCompleteMsg` fires `on_preload_ok`
    (`PreloadPanel::OnMsg(UITransitionCompleteMsg)`, `PreloadPanel.cpp:227-230`)
    → `goto_screen game_screen`. The whole `part_difficulty → preloading →
    tv3_b → game_screen` chain is ~3 frames (the documented f451→456).

- **What the vignette needs to animate.** The `tv3_b` vignette is a `PropAnim`
  driven off `TheTaskMgr` UI-time (`kTaskUISeconds`); RndAnimatable rate→units
  maps UI anims onto `kTaskUISeconds` (`src/system/rndobj/Anim.cpp:14`). At ~3
  frames it gets ~3×(1/30 s) of UI-time = ~0.1 s and is captured frozen on one
  pose (f454/455/456 byte-identical). It needs *many more poll cycles on
  `preloading_screen`* to play through.

---

## 2. Frame-hold vs wall-clock decision — RESOLVED: **frame-hold (extra ticks)**

**Decision: the hold MUST be expressed as N extra FRAME TICKS pinned on
`preloading_screen`, NOT a wall-clock `sleep()`/`RB3_LOAD_HOLD_MS`. A wall-clock
sleep would animate NOTHING headless.**

Decisive evidence — the headless UI clock is fake and frame-quantised:

```
// src/system/ui/UI.cpp:518-525  (UIManager::Poll)
#ifdef HX_NATIVE
    static bool sHeadless = !!getenv("MILO_HEADLESS");
    if (sHeadless) {
        sHeadlessFakeUISeconds += 1.0f / 30.0f;          // +1/30 s PER Poll()
        TheTaskMgr.SetUISeconds(sHeadlessFakeUISeconds, false);
    } else
#endif
        TheTaskMgr.SetUISeconds(Timer::CyclesToMs(mTimer.mCycles)/1000.0f, false);
```

- Under `MILO_HEADLESS=1` (the N7 reproducer), `kTaskUISeconds` advances by a
  fixed `1/30 s` **per `UIManager::Poll()` call**, i.e. once per native frame
  loop iteration (`App.cpp:537`). It is deliberately decoupled from wall clock so
  the headless run is deterministic and frame-indexed.
- Therefore the vignette PropAnim only advances when the frame loop iterates and
  calls `TheUI.Poll()`. A `usleep()`/`nanosleep()` inside `RB3GameInputPoll`
  would burn real time while `sHeadlessFakeUISeconds` does not move and
  `TheUI.Poll()` is not re-entered → **the vignette stays frozen and the load
  still completes in the same 3 frames.** A wall-clock knob is a no-op headless.
- (Non-headless / windowed the UI clock *is* wall-clock via `mTimer`, so a
  wall-clock hold would work there — but the deliverable harness is headless, and
  the windowed path is not how this is verified. We standardise on frames.)

**Mechanism that actually animates the vignette:** keep `preloading_screen` the
committed screen for `N` additional frame-loop iterations. Each extra iteration
runs `TheUI.Poll()` → `+1/30 s` UI-time → the `tv3_b` PropAnim advances one step.
After `N` frames the natural advance to `game_screen` is allowed to proceed.

**Units of the knob:** FRAMES, not ms. Name `RB3_LOAD_HOLD_FRAMES` (default `0`).
Rationale: the harness is frame-indexed; a frame count maps 1:1 to UI-time at
`1/30 s` each (so `RB3_LOAD_HOLD_FRAMES=120` ≈ 4 s of vignette UI-time, a
visible multi-second loading sequence). Naming it `_FRAMES` (not `_MS`) makes the
frame-shift side-effect honest to the caller. (If a future windowed/real-time
build wants ms, that is a separate additive knob; do NOT pretend ms works
headless.)

**Side-effect on frame-indexed `@N` verbs and `MILO_SCREENSHOT_FRAMES` — MUST be
documented to the user:**

- The frame-hold inserts `N` extra iterations of the `for (int frame=...)` loop
  (`App.cpp:535`) BETWEEN the `preloading_screen` commit and the `game_screen`
  commit. The loop counter `frame` (used for `@N` verbs) and the engine
  `mFrameCount` (used for screenshots, `Rnd_Wgpu_RB3.cpp:219-262`, one increment
  per `EndDrawing`) BOTH advance during the hold.
- **Consequence:** every `RB3_GAME_INPUT @N` verb scheduled to fire AFTER the
  load (here `@nofail`/anything post-`end_override_flow`) and every
  `MILO_SCREENSHOT_FRAMES` value that is meant to land on `game_screen` content
  shifts LATER by `N`. To capture in-game frame `X` you now schedule frame
  `X + N`. The pre-load verbs (`@10:start … @450:end_override_flow`) are
  unaffected.
- **Two redeeming details that keep this manageable:**
  1. The post-load verb in the canonical script is `@500:nofail`, whose
     readiness predicate gates on `MetaPerformer::Current()` existing (it is a
     `>= @N` MINIMUM, not an exact trigger — see the state-driven queue,
     `rb3_game_input.cpp:947-992`). So `nofail` simply fires a few frames later;
     it does not break.
  2. The screenshot scheduler already reschedules any shot whose `target` lands
     before the first stable scene cam to `firstSceneCam + 4`
     (`Rnd_Wgpu_RB3.cpp:224-235`). The hold delays `firstSceneCam`, so early
     game-screen shots auto-defer; later shots still shift by `N` and the user
     must budget for it.
- **Net guidance to bake into the verification recipe:** when
  `RB3_LOAD_HOLD_FRAMES=N` is set, add `N` to every game-phase
  `MILO_SCREENSHOT_FRAMES` entry (and to post-load `@` verbs if exact timing
  matters). The plan's verification recipe (§6) does exactly this and also adds
  shots WITHIN the hold window to prove animation.

---

## 3. EXPLICIT files-to-edit list

| File | Layer | What changes | N8 serialization note |
|------|-------|--------------|-----------------------|
| `rb3/native/src/rb3_game_input.cpp` | (c) glue | Add the hold logic (see §4). | See below — exact scope. |

**Preferred implementation is glue-only (one file).** No matched-fork edit and no
engine edit are required for the recommended approach in §4 (Approach A). The
fragile-but-pure-glue Approach B is also one-file. Only the rejected Approach C
would touch matched fork.

**Exact functions/lines touched in `rb3_game_input.cpp` (for N8 serialization):**

- ADD one anonymous-namespace helper near the other `Exec*` helpers (around
  `rb3_game_input.cpp:450-470`, after `ExecNoFail`) **or** inline state in
  `RB3GameInputPoll`. New module-static state: `gLoadHoldFrames` (parsed once),
  `gLoadHoldStartFrame` (-1), `gLoadHoldActive` (bool).
- ADD a self-contained block INSIDE `void RB3GameInputPoll(int frame)`
  (`rb3_game_input.cpp:691`), placed **immediately after the screen-flow trace
  block (after line ~703, before the N4 fix block at ~717)**, reading
  `curName`/`cur` which are already computed at `:696-697`.
- Do NOT modify: the N4 block (`:717-731`), the N6 block (`:754-827`), the
  state-driven verb dispatch (`:947-992`), or any `Exec*`/`VerbReady` function
  bodies. N8 (hit/flame FX) will add a synthetic note-hit verb — i.e. it edits
  the `ParseScript` grammar (`:194-300`), the `Verb`/`VerbKind` enum
  (`:141-153`), `VerbReady` (`:477`), and `DispatchVerb` (`:549`). **N7 touches
  NONE of those.** N7 lives entirely in the per-frame body between the screen
  trace and the N4 block + a small static-state addition. The two items are
  line-disjoint; serialize only the final merge (both edit the same file), not
  the design.

---

## 4. Fix approach

### Knob
- `RB3_LOAD_HOLD_FRAMES` — integer, **default `0` (no behavior change)**, units =
  extra native frames to hold on the load screen. Parsed once via `getenv`/`atoi`
  in `RB3GameInputPoll` (guard: `<= 0` disables). Optional opt-out alias not
  needed since default already off.

### Approach A — **RECOMMENDED: glue re-pin of `preloading_screen` for N frames**

The honest constraint discovered in the code: `rb3_game_input.cpp` runs AFTER
`TheUI.Poll()` each frame and cannot veto the matched-fork `on_preload_ok` goto,
nor flip the private `mPreloadResult`. The only lever a pure-glue file has over
the committed screen is the **public virtual** `TheUI.GotoScreen(...)`
(`src/system/ui/UI.h:134`, confirmed callable). So:

```
// (sketch — final code in rb3_game_input.cpp)
static int  gLoadHoldFrames    = -2;     // -2 = unparsed
static int  gLoadHoldStart     = -1;     // frame the hold began
if (gLoadHoldFrames == -2)
    gLoadHoldFrames = getenv("RB3_LOAD_HOLD_FRAMES")
                        ? atoi(getenv("RB3_LOAD_HOLD_FRAMES")) : 0;

if (gLoadHoldFrames > 0 &&
    (curName == Symbol("preloading_screen") ||
     curName == Symbol("tv3_b_screen"))) {
    if (gLoadHoldStart < 0) gLoadHoldStart = frame;       // entered the window
    int held = frame - gLoadHoldStart;
    if (held < gLoadHoldFrames && !TheUI.InTransition()
        && curName == Symbol("preloading_screen")) {
        // Re-pin: if the engine just advanced toward game_screen, send it back
        // to preloading_screen so the vignette keeps polling/animating.
        // (Only re-pin from a settled state; never stack on an in-flight swap.)
        TheUI.GotoScreen("preloading_screen", false, false);
    }
    // Once held >= gLoadHoldFrames, do nothing → the natural on_preload_ok
    // advance to game_screen proceeds on the next poll.
}
```

- **Why this animates the vignette:** while held, each extra frame iteration runs
  `TheUI.Poll()` (+1/30 s UI-time) before this glue runs, so the `preloading_screen`
  panels (and the `tv3_b` vignette PropAnim, once it is the transition screen)
  advance. The re-pin keeps the committed screen from racing ahead to
  `game_screen`.
- **Caveat (must be validated in §6, see §7):** re-issuing `GotoScreen` may
  restart the `tv3_b` wipe each time rather than letting it play through once.
  The first verification pass MUST inspect whether the vignette *progresses* vs
  *restarts*. If it restarts, fall back to Approach A′ (below) or accept Approach
  C′s tiny matched-fork gate.

### Approach A′ — glue hold of the SYNTHETIC start verb is NOT sufficient
Delaying the `@…:end_override_flow` verb (which kicks the flow) only delays WHEN
the load starts; the load is still instant once started, so `preloading_screen`
still flashes. Documented here so a future implementer does not waste a cycle on
it. Reject.

### Approach C — tiny additive matched-fork gate (fallback if A re-pin is ugly)
If §6 shows Approach A re-pin restarts the vignette instead of advancing it, the
clean alternative is a 3-line additive `#ifdef HX_NATIVE` gate in
`PreloadPanel::IsLoaded()` (`src/system/meta/PreloadPanel.cpp:123`) **OR**
`BandPreloadPanel::IsLoaded()` (`BandPreloadPanel.cpp:40`): hold the return
`false` for `N` frames after the panel first reports loaded (count frames via a
member or a file-static), reading the same `RB3_LOAD_HOLD_FRAMES`. This keeps
`preloading_screen` committed *naturally* (no re-goto), so the vignette plays
through once cleanly. Cost: it is layer (a) — permuter-owned; the edit must be
additive `#ifdef HX_NATIVE … #else … #endif` with a byte-identical `#else`, and
is subject to permuter-wipe (re-apply). **Use only if A is visually wrong.** This
is exactly the seam `LOAD_SCREEN_RENDER.md` §Fix-plan option 1 named.

**Recommendation:** implement Approach A first (pure glue, permuter-safe,
single-file, default-off). Verify per §6. If the vignette restarts rather than
advances, switch to Approach C (the natural-commit matched-fork gate), which is
known-correct but costs a permuter-tracked block.

---

## 5. Regression risks

- **Default no-op:** with `RB3_LOAD_HOLD_FRAMES` unset/`0`, the new block is
  fully skipped (`gLoadHoldFrames > 0` guard) — byte-for-byte the current
  behavior. This is the single most important property; the default reproducer,
  all existing `MILO_SCREENSHOT_FRAMES`, and every regression run are unchanged.
- **No matched-fork churn (Approach A):** glue-only, permuter never touches it.
  (Approach C would add a permuter-tracked HX_NATIVE block — fall-back only.)
- **Headless harness integrity:** the hold uses ONLY extra frame iterations of
  the existing loop; it never sleeps, never spins a busy-wait, never blocks
  `TheUI.Poll()`/`EndDrawing()`. The screenshot readback and the `@N` verb queue
  keep running normally — they just see more frames. No desync of capture vs
  draw (both are driven by the same per-iteration `mFrameCount`/`frame`).
- **Frame-shift is opt-in and bounded:** only when the knob is set do downstream
  `@N`/`MILO_SCREENSHOT_FRAMES` shift by `N`; the §6 recipe accounts for it.
- **Re-pin safety (Approach A):** re-goto is gated on `!TheUI.InTransition()` and
  `curName == preloading_screen`, so it never stacks on an in-flight swap (the
  documented SIGSEGV class). Once `held >= N` it stops, guaranteeing forward
  progress to `game_screen` (no infinite hold / hang).
- **N8 merge:** line-disjoint from N8's verb-grammar edits (see §3); only a
  textual merge in the same file is needed.

---

## 6. Verification recipe (prove the vignette ANIMATES during the hold)

Goal: show the `tv3_b` vignette progressing across multiple frames inside the
hold window (NOT byte-identical), then confirm `game_screen` still commits.

1. **Pick a hold of `N = 120` frames** (≈4 s UI-time). The load window currently
   starts ~f451 and commits `game_screen` ~f456; with the hold,
   `preloading_screen`/`tv3_b` stay committed across ~f452 … f452+120.

2. **Capture DENSE shots inside the hold window** (this is the proof):
   ```
   RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
     RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
     RB3_LOAD_HOLD_FRAMES=120 \
     MILO_MAX_FRAMES=24120 \
     MILO_SCREENSHOT_DIR=/abs/out/n7-hold \
     MILO_SCREENSHOT_FRAMES=455,470,490,510,540,575 \
     RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@620:nofail" \
     native/build-native/rb3-native
   ```
   - `MILO_MAX_FRAMES` bumped by `N` (24000→24120) so the full song still runs.
   - `@nofail` moved 500→620 (past the hold) since it must land on
     `game_screen` / a live `MetaPerformer`.
   - The six shots (455…575) straddle the hold window. **PASS = consecutive shots
     are visibly DIFFERENT** (the `tv3_b` maroon-band / teal-floor geometry
     panning/animating, camera moving) — i.e. NOT the byte-identical frozen pose
     `LOAD_SCREEN_RENDER.md` documented at f454/455/456.

3. **Diff adjacent captures** (objective check):
   ```
   cmp 01_f0455*.png 02_f0470*.png   # MUST differ (was identical pre-fix)
   ```
   If `cmp` reports differences and the screen trace shows
   `currentScreen = preloading_screen`/`tv3_b_screen` held across f452…f572, the
   vignette is animating.

4. **Confirm forward progress:** the log must still show
   `RB3 screen: frame ~57x currentScreen = 'game_screen'` after the hold, and the
   deep-song shots (e.g. `MILO_SCREENSHOT_FRAMES=…,700,1220` i.e. retail-frame +
   `N`) must still show gameplay highway + gems. Clean exit 0.

5. **Default-off regression:** re-run WITHOUT `RB3_LOAD_HOLD_FRAMES` and confirm
   the original v18/v34 screenshots reproduce frame-for-frame (no shift).

6. **A/B the re-pin artifact (Approach A only):** in step 2, if adjacent shots
   differ but show the wipe RESTARTING (same early-wipe pose recurring) rather
   than the camera progressing through the vignette, that is the §7 risk
   materialising → switch to Approach C and re-verify.

---

## 7. Honest assessment

- **The timing diagnosis is solid and the fix is genuinely a hold, not a render
  patch.** The vignette content draws correctly; it only lacks poll cycles. This
  is confirmed by the headless fake-UI-clock (`UI.cpp:521`) + the PropAnim/
  TaskMgr UI-time coupling.

- **A pure-glue hold is achievable but has ONE real risk:** `rb3_game_input.cpp`
  cannot cleanly keep `preloading_screen` committed except by re-issuing
  `TheUI.GotoScreen("preloading_screen")` (Approach A). Re-issuing a goto each
  frame *may* restart the `tv3_b` wipe rather than let its PropAnim play through
  once. **I could not prove from static reading alone that the re-pin advances vs
  restarts the vignette** — that requires the §6 capture. If it restarts, the
  honest fix is the tiny additive matched-fork gate in `PreloadPanel::IsLoaded()`
  (Approach C), which holds the screen *naturally* (no re-goto) and is
  known-correct, at the cost of one permuter-tracked HX_NATIVE block.

- **What is NOT in doubt:** the vignette PropAnim DOES advance whenever
  `preloading_screen`/`tv3_b_screen` stays the polled screen for extra frames
  (the clock + anim wiring is present and runs every Poll). So the *mechanism*
  (extra frame ticks) is correct; the only open question is the *cleanest lever*
  to keep the screen committed (glue re-pin vs matched-fork natural hold). This
  is a single verification capture away from resolution and is called out in §6
  step 6.

- **No deeper render/anim bug is suspected.** Unlike a "PropAnim never advances
  at all" failure, here the anim is simply starved of poll cycles; feeding it
  cycles is sufficient. If §6 shows the vignette still frozen *with* a clean hold
  (Approach C, natural commit, 120 frames), THEN it escalates to a real
  RndPropAnim/UITransition timing bug — but the code wiring gives no reason to
  expect that.
