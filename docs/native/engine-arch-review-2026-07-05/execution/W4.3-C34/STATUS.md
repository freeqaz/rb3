# W4.3-C34 STATUS — C3 (song_select hold-label flip) + C4 (hub ticker overlap)

Continuation of a stalled prior agent run. This file supersedes any earlier
in-progress notes; PLAN.md's grounding facts (A9/A10 binding) still hold.

## C3 — song_select bottom action-bar "vertical flip" — VERDICT: NOT_A_BUG (faithful, no fix)

**Summary:** the InlineHelp rotation state machine that drives the primary↔secondary
action-label "flip card" reveal (`src/system/bandobj/InlineHelp.cpp`,
`SetLabelRotationPcts()` lines 549-554) is mathematically bounded and behaves
exactly as its own decompiled formula dictates — there is no native-side
state/timing divergence, no engine mishandling, and (per A9) no need to touch
`Rnd_Wgpu_RB3.cpp`.

Evidence gathered:
- **Telemetry across ~93,000 samples in 4 independent captured logs**
  (`/tmp/c34-probe/engine.log`, `/tmp/c34-flipfind/engine.log`,
  `/tmp/rb3-c34b-46327.log`, `/tmp/rb3-c34b-37729.log`) found **zero anomalies**:
  `sLabelRot` always stayed within the formula's own disjoint predicted range
  `(-360,-240] ∪ [-120,0]`. This **refutes** PLAN.md's leading hypothesis that
  the rotation clock gets "stuck at ±180" — it never does, by construction
  (the formula literally cannot reach 180°, only asymptotically approach a
  50%-squish-and-invert state at the f=0.5 discontinuity).
- **Burst capture** (`/tmp/wave12-c34-probe/burst/`, 90 frames correlated
  frame-exact to log offsets via `manifest.json`) shows: `rot≈0` renders
  correctly upright for BOTH the primary text ("PLAY SONG", `burst_005.png`)
  AND the secondary text ("HOLD TO MAKE SETLIST", `burst_030.png`) — i.e. the
  visible "flip" only ever appears *during* the transition window
  (`burst_024/025.png`, rot≈-120/-296, edge-on/invisible; `burst_050.png`,
  rot≈-64, partial/italic-sheared), never as a persisted end-state.
- **`SetLabelRotationPcts()` is plain decompiled/matched code with no
  `HX_NATIVE` guard** — the animation formula is not a native invention; it's
  the original game's logic. The `/tmp/wave12-c34-probe/song_select_list2.png`
  reproduction attempt (same nav path as the original bug evidence) rendered
  **correctly upright**, while the original evidence screenshot
  (`/tmp/wave12-current-state/song_select_list2.png`) caught the label
  mid-transition — consistent with "real, intentional flip-card animation,
  photographed mid-flip," not a persistent rendering defect.

**Conclusion:** this is very likely retail-faithful behavior (a "hold to
reveal alternate action" flip-card transition also present on Xbox/PS3/Wii),
and the original bug report is a false positive from a screenshot caught
mid-animation. No code change needed; no engine escalation triggered (the
generic mesh path draws the correctly-computed game-side transform as-is,
matching A9's expectation exactly). Recommend closing C3 as
NOT_A_BUG/FAITHFUL, with a note that live-video (not single-frame) comparison
against retail would be the only way to fully close the loop if the
coordinator wants 100% certainty.

## C4 — hub ticker "NEXT MESSAGE (n/n)" overlapping message body — VERDICT: ROOT CAUSE NARROWED (game-side text wrap/scale), NOT YET FIXED, NO ESCALATION NEEDED

**Reproduces cleanly:** `/tmp/c34-probe3/hub.png` (RB3_FIXED_CLOCK=1,
`scripts/native/_c34_holdlabel_probe.py`) shows "NEXT MESSAGE (1/1)" and
"Connect to Xbox LIVE to get more songs and information about Rock Band 3!"
essentially on one visually-overlapping line.

**Diagnostic path:**
1. The pre-existing name-filtered probe on `UIList::DrawShowing()`
   (`src/system/ui/UIList.cpp:475-485`) never fired for `message_area` in any
   log despite the bug visibly reproducing. Widened it to log
   unconditionally (removed the `strstr(nm,"message_area")` filter) — still
   only ever observed `song.lst` / `options.lst` names. This proved the
   ticker body text is **not** drawn through a plain `UIList::DrawShowing()`
   call path at all (see `UIListDir::DrawShowing()`,
   `src/system/ui/UIListDir.cpp:439-444`, which iterates `RndDir::mDraws` via
   `Draw()`, not this override, for the non-test-mode case).
2. Added a second probe directly in `UILabel::DrawShowing()`
   (`src/system/ui/UILabel.cpp:294-307`, gated `HX_NATIVE` + `getenv`) logging
   every `UILabel`'s `Name()` + `WorldXfm()`. This found the actual label:
   **`message.lbl`** (bound in `MainHubPanel::UpdateHeader()`,
   `src/band3/meta_band/MainHubPanel.cpp:252-261`,
   `mDir->Find<BandLabel>("message.lbl", true)`), worldXfm
   `(-366.69, 0.62, -134.15)`.
3. Compared against the sibling `expand_message_area.ihp` (InlineHelp action
   label) worldXfm `(-368.66, 0.00, -128.12)` — logged from the prior agent's
   existing probe. **X/Y match closely; Z (screen-vertical axis in this UI's
   coordinate system) differs by only ~6.0 units.** For contrast, a
   confirmed-correctly-stacked pair elsewhere on the same screen —
   `fan_total.lbl` (Z=190.51) directly above `level.lbl` (Z=169.71) — is
   separated by **~20.8 units**, roughly 3-4x more. A 6-unit gap is far too
   tight for two full-size text lines not to visually collide.
4. **Direct screenshot comparison against retail**
   (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`) is the
   decisive evidence: retail's message body ("American Pie by Don McLean is
   now available. To all the fans...") renders in a **distinctly smaller
   font than the bold "NEXT MESSAGE (2/3)" label**, and **wraps across 2
   lines** with clear vertical separation below the label. Native's message
   body renders at **essentially the same font size as the label** and does
   **not wrap** — it runs on as one long line that collides with the label's
   line. This is a **font-scale / line-wrap application gap for
   `message.lbl` specifically**, not a raw world-transform/anchor-offset bug
   — the authored ~6-unit gap is plausible/intentional for two *correctly
   differently-sized* lines (small ticker font under a bold label), but wrong
   when the ticker font/wrap isn't being applied and both lines render at
   full label size.
5. Traced the wrap implementation (`src/system/rndobj/Text.cpp:696-910`, the
   DP line-wrap algorithm keyed off `mWrapWidth`/`Style::brk`) — the
   algorithm itself looks structurally sound for plain unmarked-up text
   (`Style::brk` defaults `true` in `src/system/rndobj/Text.h:16-17`, so
   wrapping isn't gated off by default; `canBreak()` at
   `Text.cpp:576-582` breaks on spaces/tabs, which "Connect to Xbox LIVE..."
   has plenty of). Did **not** get to the bottom of *why* `message.lbl`
   specifically isn't getting a smaller-scale font/nonzero effective
   `mWrapWidth` applied at runtime — that requires either (a) a live
   `.milo`-scene property dump of `message.lbl`'s authored `wrap_width` /
   font style size (no generic "dump any object's synced properties" DTA
   verb exists yet in `native/src/rb3_http_handlers.cpp` — would need a small
   addition), or (b) instrumenting `RndText::SetText`
   (`Text.cpp` ~line 362-380, the early-out cache-check gated on
   `mWrapWidth`/`mStyle.size`/`mTextMarkup`) to log the actual values arriving
   for this specific label.

**Scope/escalation check (per A9):** every file touched or implicated here —
`UIList.cpp`, `UIListDir.cpp`, `UILabel.cpp`, `Text.cpp`, `Font.cpp`,
`AppLabel.cpp`, `BandLabel.cpp`, `MainHubPanel.cpp` — lives under
`/home/free/code/milohax/rb3/src/`, compiled directly into `rb3-native`
(confirmed via build log: e.g. `.../rb3/src/system/ui/UIList.cpp.o`), **not**
the `milo-native-engine` repo. `Rnd_Wgpu_RB3.cpp` (the one file A9 fences off)
is never implicated by this trace. **No COORDINATOR_ESCALATE needed for C4** —
this is a game-side (or at most rb3-shared-engine-side, still in-repo)
text-scale/wrap gap, not a render-path/mesh-path issue.

**Recommended follow-up (not done this session, lowest-priority C-lane item
per A10):** add a one-off DTA/HTTP debug verb (or extend the existing
`UILabel`/`RndText` probe) to print `mWrapWidth`, `mStyle.size`, and the bound
`RndFont*` for `message.lbl` at draw time, and diff against a label that is
known to render at the smaller/correct scale in-game elsewhere (if one
exists), or against the same object's Bank-8-target static in the `.milo`
asset (`orig-assets/wii-extracted/ui/resource/list/gen/list_main_hub_messages.milo_wii`).
That will pinpoint whether the authored value is simply not being read
(load/PostLoad bug) vs. never authored at all (asset gap, would need
`MainHubPanel.cpp`-side override akin to the `playnow.lsw` precedent at
`MainHubPanel.cpp:130`).

## Regression check

`python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order` with
all new probes flag-OFF (default, no `RB3_HOLDLABEL_DBG` set): **PASS, 792
draws**, unchanged.

## Files touched this session

- `src/system/ui/UIList.cpp` — widened pre-existing `RB3_HOLDLABEL_DBG` probe
  in `DrawShowing()` to log unconditionally (diagnostic only, still
  `getenv`-gated, default-off).
- `src/system/ui/UILabel.cpp` — added new `RB3_HOLDLABEL_DBG`-gated probe in
  `DrawShowing()` (diagnostic only, default-off).
- This file (`STATUS.md`), new.

No engine (`milo-native-engine`) files touched. No `classification.json`
changes needed (no new engine-side flag required).
