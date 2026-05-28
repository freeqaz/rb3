# APPLY_HANDLER — bridging the milo `N_player_<aspect>` apply path (V31)

**Date:** 2026-05-28 (Opus implementation subagent, V31).
**Goal:** make the gameplay HUD's BandScoreboard plate render at **TOP-CENTER**
(retail) instead of **TOP-RIGHT** (V30 baseline). The trail was: the milo
`N_player_<aspect>` configuration object's `apply` handler is supposed to
re-layout the HUD/highway elements for the active per-player aspect, but the
HUD ended up off-center even though V19–V30 brought the highway/camera and
HUD-visibility paths up.

## TL;DR

The milo `apply` handler **does dispatch and execute** natively — the
`TrackPanelDirBase::SetConfiguration` → `Object::Handle(apply, true)` call
path returns `kDataObject` (= success, NOT `kDataUnhandled`). What is missing
is **part of the authored layout effect**: the BandScoreboard plate is
parented through `right.grp` (the authored *multi-player* right-anchor group),
and nothing on the dispatch path neutralizes `right.grp`'s x-offset for the
single-player layout. The fix is a precise additive HX_NATIVE block in
`TrackPanelDir::ConfigureTracks` (the same function that drives
`scoreboard_to_top.tnm`) that walks the BandScoreboard's parent chain and
zeros the `right.grp` anchor's x for the single-player case.

After the fix, the BandScoreboard plate (with score digits + star-progress
meter) renders at **top-CENTER** across 5+ short-form runs (1600 frames) and
1 long-form run (9000 frames) — all exit 0, no regressions to highway/gems/
strike-plate/applause-meter/venue/band/camera.

The V12 `CAMERA_FRAME_FIX` block remains in place — it solves a different,
independent half of the same systemic gap (the gameplay-camera rig in the
`rotater.grp` chain) and is still load-bearing for the highway framing.

## Trace results — what executes vs what doesn't

The V30 hypothesis was that the milo `apply` handler "doesn't fully execute
natively." The K9_APPLY_DBG trace (added to `TrackPanelDirBase.cpp`) shows the
empirical truth is more nuanced.

### Dispatch trace (`K9_APPLY_DBG=1`)

```
K9_APPLY: SetConfiguration o=0x... name='1_player_wide' class='Object' animate=0
          td=0x... td_type='track_configuration'
K9_APPLY:   td.apply=0x... = (apply ($animate) {$this update $animate})
K9_APPLY:   td.objects=...  sz=1   <- means 0 saved-object entries (array header only)
K9_APPLY:   td.visibles=... sz=1   <- 0 entries
K9_APPLY:   td.xfms=...     sz=1   <- 0 entries
K9_APPLY:     objects[0] type=16 obj=(nil)
K9_APPLY:   o.Dir=0x... name='TrackPanelDir'
K9_APPLY:   Handle(apply) returned type=6 (kDataUnhandled=0)
```

Reading this trace:
- The configuration object active at gameplay start is **`1_player_wide`**
  (selected by `TrackPanelDir::ConfigureTracks` line 230–234 from the current
  aspect + in-use player count). Its `mTypeDef` is the `track_configuration`
  type from `ui/track/track_paneldir.dta` lines 3–425.
- The typedef's `(apply ...)` array is `($animate) {$this update $animate}` —
  it dispatches to the `(update ...)` method defined in the same typedef.
- **The dispatch returns `type=6` (`kDataObject`), not `kDataUnhandled=0`.**
  So the `Hmx::Object::Handle(apply, true)` path through `HANDLE_ARRAY(mTypeDef)`
  (`ObjMacros.h:185-192`) does find `(apply ...)` in `mTypeDef` and does call
  `ExecuteScript`. The handler **executes** natively.
- The typedef's saved `[objects]/[visibles]/[xfms]` arrays are **empty in the
  loaded milo asset** (size = 1 = array header only). So even if the `update`
  script's `[objects]` else-branch fired, there would be no saved positions to
  apply. (For an active config it doesn't fire anyway — see below.)
- The `(update $animate)` script's `{if {$this is_active} ...}` then-branch
  iterates `mGemTracks`, sets each gem-track's `screen_aspect`, `total_slots`,
  `track_slot`, then calls `update_track_position` on each (chain documented
  in `ui/track/track_paneldir.dta:166-273`). That ultimately animates the
  gem-track-internal `track_size.anim`/`track_shift.anim`/`track_cam_rect.anim`
  PropAnims (`(..)/(..)/system/run/band/band_objects.dta:2686-2725`).

### What does and doesn't move

The gem-track PropAnims that get animated **drive the gem track's own
rig + camera** — they do not drive trackpanel siblings like the BandScoreboard
plate, the applause meter, or the MTV overlay. Those are at the trackpanel
level, parented through `aspect_ratio.grp/<anchor>.grp/...` and positioned by
**a different code path**: `TrackPanelDir::ConfigureTracks` (lines 245–254)
calls `Find<RndTransAnim>("scoreboard_to_top.tnm")->SetFrame(f19=1.0, 1.0)` and
similarly for `applause_meter_to_top.tnm` / `mtv_overlay_to_top.tnm`.

The K9 trace, dumping the BandScoreboard's transform chain immediately after
those `SetFrame(1.0, 1.0)` calls, shows:

```
K9_APPLY:   sbtt.trans=0x... name='scoreboard.trans'
K9_APPLY:   sbtt.trans local=(0.000, 0.000, 4.000)  world=(11.713, 22.000, 4.164)
K9_APPLY:     parent[0] name='right.grp'        local=(11.713, 0.000, 1.764) world=(11.713, 22.000, 1.764)
K9_APPLY:     parent[1] name='aspect_ratio.grp' local=(0.000, 22.000, 0.000) world=(0.000, 22.000, 0.000)
K9_APPLY:     parent[2] name='draw_order.grp'   local=(0.000,  0.000, 0.000) world=(0.000,  0.000, 0.000)
```

So `scoreboard_to_top.tnm` animates `scoreboard.trans` to local=(0,0,4) — the
"top, slight depth offset" position — but the **parent `right.grp` has
local.x=11.713** (the authored multi-player right-anchor offset) and the
world position lands at `x=11.713`, i.e. ~half a screen-width to the right.
That is exactly the visible "top-RIGHT" behavior reported in V30.

In retail's single-player layout this `right.grp` is supposed to be moved to
`(0, *, *)` so the scoreboard ends up at world x=0 (top-center). That
neutralization is **not** part of any C++ code path on the native build, and
not part of the `update` script's gem-tracks branch (which only animates
gem-track-internal anims). It would be either:
1. An authored saved-objects record on `1_player_wide` (the `[objects]/
   [visibles]/[xfms]` array) — but that array is empty in this milo asset
   (per K9 trace), OR
2. A milo-side `(post_sync ...)` / `(apply ...)` hook on `right.grp` itself
   the asset would carry, OR
3. A separate authored TNM/Trigger we haven't located.

Either way, **the visual gap on native today is the un-neutralized
`right.grp` for the single-player layout** — and that is the smallest piece
to bridge for the top-CENTER win.

## The fix (file:line)

`src/system/bandobj/TrackPanelDir.cpp` — `TrackPanelDir::ConfigureTracks`
(additive `#ifdef HX_NATIVE` block, immediately after the existing
`scoreboard_to_top.tnm` / `applause_meter_to_top.tnm` / `mtv_overlay_to_top.tnm`
`SetFrame(f19, 1.0f)` calls at lines 245–254). The fix walks the parent chain
of each of those three TNM's driven `mTrans`, finds the nearest group named
`right.grp` or `left.grp` (the authored multi-player anchor groups), and
zeros its **x-translation** when the in-use player count is 1.

Key constraints:
- **Only acts when `nPlayers == 1 && unk24c == 1`** (single in-use gem track).
  Multi-player layouts get the authored fan-out unchanged.
- **Only zeros `x`** — leaves `y` and `z` alone so vertical placement and
  depth keying inherited from the milo asset are preserved.
- **Acts on the nearest anchor only** (`break` after the first match) — to
  avoid reaching past the per-widget anchor into structural parents like
  `aspect_ratio.grp` or `draw_order.grp`.
- **Idempotent** — the next `ConfigureTracks` call applies the same value, so
  no drift / oscillation.
- **Gated by `RB3_APPLY_HANDLER_FIX_OFF=1`** (opt-OUT, default ON) for clean
  A/B testing without rebuild. Verified the gate works: with `OFF=1` the
  output reproduces the V30 top-RIGHT baseline exactly.

Quoted snippet (post-build, in-source comment retained as the load-bearing
doc):

```cpp
// V31 APPLY_HANDLER_FIX: re-center the BandScoreboard plate (top-RIGHT
// → top-CENTER) for the single-player lone-track layout. [full comment in
// TrackPanelDir.cpp around lines 258-294]
if (!getenv("RB3_APPLY_HANDLER_FIX_OFF")) {
    int nPlayers = (mTrackPanel ? mTrackPanel->GetNumPlayers() : 1);
    if (nPlayers == 1 && unk24c == 1) {
        auto neutralizeAnchor = [](RndTransAnim *tnm) {
            if (!tnm) return;
            static Message qTrans("trans");
            DataNode n = tnm->Handle(qTrans.mData, false);
            if (n.Type() != kDataObject) return;
            RndTransformable *rt = dynamic_cast<RndTransformable*>(n.UncheckedObj());
            if (!rt) return;
            int depth = 0;
            for (RndTransformable *p = rt->TransParent();
                 p && depth < 8;
                 p = p->TransParent(), ++depth) {
                const char *nm = p->Name();
                if (!nm) continue;
                if (std::strcmp(nm, "right.grp") == 0 ||
                    std::strcmp(nm, "left.grp") == 0) {
                    Transform lx(p->LocalXfm());
                    if (lx.v.x != 0.0f) {
                        lx.v.x = 0.0f;
                        p->SetLocalXfm(lx);
                    }
                    break;
                }
            }
        };
        neutralizeAnchor(Find<RndTransAnim>("scoreboard_to_top.tnm", false));
        neutralizeAnchor(Find<RndTransAnim>("applause_meter_to_top.tnm", false));
        neutralizeAnchor(Find<RndTransAnim>("mtv_overlay_to_top.tnm", false));
    }
}
```

The K9 trace probe (the `K9_APPLY_DBG=1` blocks) is retained in
`TrackPanelDirBase::SetConfiguration` / `TrackPanelDirBase::ReapplyConfiguration`
and `TrackPanelDir::ConfigureTracks` for future re-verification.

## HUD position — before / after

| Run | Frame | BandScoreboard plate position | Highway / strike-plate | Applause meter | Exit |
|---|---|---|---|---|---|
| V30 (baseline; `RB3_APPLY_HANDLER_FIX_OFF=1`) | 1400 | **TOP-RIGHT** (~x=0.85 in screen-space) | centered, V12 fix | top-LEFT | 0 |
| V31 after_run1 | 1400 | **TOP-CENTER** (~x=0.50) | centered, V12 fix | top-LEFT | 0 |
| V31 after_run2 | 1400 | TOP-CENTER | centered, gems streaming | top-LEFT | 0 |
| V31 after_run3 | 1400 | TOP-CENTER | centered | top-LEFT | 0 |
| V31 after_run4 | 1400 | TOP-CENTER | centered | top-LEFT | 0 |
| V31 after_run5 | 1400 | TOP-CENTER | centered, gems streaming | top-LEFT | 0 |
| V31 after_long1 | 3500 | TOP-CENTER | centered, gems streaming, gem-tail trails | top-LEFT | 0 |
| V31 after_long1 | 7500 | TOP-CENTER | centered, gems streaming | top-LEFT | 0 |

Sample numerical confirmation (from K9_APPLY_DBG):
- V30: `sbtt.trans world=(11.713, 22.000, 4.164)` (x=11.713 → top-right)
- V31: `sbtt.trans world=(0.000,  22.000, 4.164)` (x=0.000  → top-center)

Screenshots: `docs/sessions/native/screenshots/v31-apply-handler/`
- `trace1/02_f1400.png`            — pre-fix baseline (top-RIGHT).
- `trace2/02_f1400.png`            — same, with extended K9 probe (top-RIGHT).
- `fix_off/01_f1400.png`           — `RB3_APPLY_HANDLER_FIX_OFF=1` regression check (top-RIGHT, as expected).
- `after_run1`..`after_run5/02_f1400.png` — 5 short-form runs with fix (top-CENTER, no regression).
- `after_long1/01_f1200.png`..`05_f8500.png` — 9000-frame song with fix (top-CENTER throughout).

## Regression status

- **Highway / strike plate / gems / gem-tails**: INTACT. The highway is still
  centered (V12 `CAMERA_FRAME_FIX` is still doing the `rotater.grp` /
  `rotater_roll.grp` / `screenRect.x` neutralization on the *camera* chain;
  V31 only touches the *scoreboard / applause-meter / mtv-overlay* HUD chains,
  which is a disjoint subtree). 5/5 short-form + 1 long-form runs show
  identical highway framing to V30.
- **Applause meter top-LEFT**: INTACT (its `left.grp` was already at x=0
  per K9 trace, so the V31 neutralization had no effect there — the lambda
  is a no-op on already-zero anchors).
- **PRESS START prompt**: INTACT (mid-left, identical to V30).
- **Score data path**: INTACT — the BandScoreboard still reads "0" (correct
  for headless no-hit `nofail` runs); its `SetScore`/`SetNumStars` calls in
  `TrackPanel::Poll` are unchanged.
- **Clean exits**: 7/7 runs (1 trace + 5 after + 1 long + 1 fix_off) exit 0.
- **Builds clean**: both `rb3-dta` and `rb3-native` link without warnings.

## V12 `CAMERA_FRAME_FIX` disposition

**Keep V12 in place.** Independently load-bearing for the highway/camera, not
made redundant by V31.

V12 lives in two pieces:
1. `TrackPanelDir::ConfigureTracks` lines 348–379 — sets
   `gHxNativeNumUsedGemTracks = unk24c` (used by piece 2) and explicitly
   hides unused-pool gem tracks (`gt->SetShowing(false)` for `!mInUse`).
2. `TrackDir::DrawShowing` lines 208–273 — walks the *gameplay camera*
   (`i6 = GetCam()`)'s parent chain, finds `rotater.grp` and `rotater_roll.grp`
   (the per-player FAN-OUT groups), and neutralizes them
   (rotater.grp.x → `NativeSettings.camRotX` (-4.0 default); rotater_roll.grp →
   identity rotation); then resets the camera's `mScreenRect.x` to 0.

This operates on a **completely different subtree** than V31 (the *camera
rig* under the gem-track proxy vs the *HUD plate parents* under
`draw_order.grp/aspect_ratio.grp`). Confirmed by inspecting parent chains:
- V12 target: `game.cam` → `rotater_roll.grp` → `rotater.grp` → ... (inside
  the gem-track proxy / TrackDir).
- V31 target: `scoreboard.trans` → `right.grp` → `aspect_ratio.grp` →
  `draw_order.grp` (at the trackpanel-dir level, not under any gem track).

Removing V12 would re-introduce the off-axis highway camera regression
documented in `VENUE_RENDER.md` (V19–V23) and explicitly flagged by the
dispatch as the precious working baseline. V31 does not bridge V12's gap and
should not. **Decision: leave V12 untouched.**

(Test attempted briefly to add a `CAMERA_FRAME_FIX_OFF=1` opt-out gate to V12
for symmetry with V31 — deferred; not in scope for this milestone.)

## What remains for follow-up

1. **`right.grp`/`left.grp` y/z neutralization (V31.1).** V31 only zeros x.
   For deep aesthetic parity, retail single-player might also place the
   scoreboard at slightly different y/z. K9 trace shows the current
   z=4.164 / y=22.000 land at a reasonable "top-of-screen" point but a
   strict comparison to retail screenshots could surface a small offset.
   Out-of-scope for the visibility/centering primary; a polish item.

2. **The empty `[objects]/[visibles]/[xfms]` save-arrays (V31.2).** The
   K9 trace shows these are empty on the loaded `1_player_wide` config
   object. Investigate whether (a) the milo author intentionally left them
   empty (relying on script-only positioning), (b) they should be populated
   from a different milo asset path (e.g. the `save_objects` editor command
   wasn't run before shipping), or (c) the native binstream `PostLoad` path
   is dropping them. If (c), populating them would let the script's
   `[objects]` else-branch position the HUD widgets directly and supersede
   V31. This is real-engine work, not a one-off fix.

3. **The `tutorial` / `bre` / `unison` TNMs.** Strings dump of trackpanel.milo
   lists `scoreboard_tutorial.tnm`, `bre_position.tnm`,
   `unison_position.tnm` as additional TransAnims targeting the same
   `right.grp` (or sibling anchors). If a tutorial / BRE / unison phase
   re-triggers any of those during a song, V31's idempotent
   `right.grp.x = 0` could conflict if they expect a non-zero anchor. Not
   reproduced in the headless quickplay+nofail reproducer; flag for any
   future tutorial/BRE/unison verification runs.

4. **V12 `CAMERA_FRAME_FIX` opt-out gate (V31.3).** Worth adding for
   symmetry with V31's `RB3_APPLY_HANDLER_FIX_OFF=1`, so future A/B testing
   of "the highway/camera fix half" is cleaner. Not done in this task.

## Reproducer

Canonical (after-fix, default ON, exit 0, BandScoreboard top-CENTER):
```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=1600 \
  MILO_SCREENSHOT_DIR=/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v31-apply-handler/<runN> \
  MILO_SCREENSHOT_FRAMES="1200,1400" \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

Toggles:
- `RB3_APPLY_HANDLER_FIX_OFF=1` — disable V31, reverts to V30 top-RIGHT layout.
- `K9_APPLY_DBG=1` — dump the dispatch trace + scoreboard.trans world position
  before/after the fix.

## State of shared files

- `src/system/bandobj/TrackPanelDir.cpp` — additive `#ifdef HX_NATIVE` block
  around the existing `scoreboard_to_top.tnm` / `applause_meter_to_top.tnm` /
  `mtv_overlay_to_top.tnm` `SetFrame(f19, 1.0f)` calls and the V12
  `CAMERA_FRAME_FIX` block at lines 348–379 (unchanged from V30). Top of file
  picks up `<rndobj/TransAnim.h>` outside the HX_NATIVE guard; HX_NATIVE
  helpers (`<cstdio>` etc.) are inside their own `#ifdef HX_NATIVE` block.
- `src/system/bandobj/TrackPanelDirBase.cpp` — additive `#ifdef HX_NATIVE`
  block around `SetConfiguration` / `ReapplyConfiguration` (the K9_APPLY_DBG
  probe). Behavior identical to the permuter form when `K9_APPLY_DBG` is unset.
- Engine (`milo-native-engine/src/**`) — UNTOUCHED.

Both files are additive-HX_NATIVE shape — the `#else` branch is byte-identical
to the permuter form. If the permuter rewrites either file mid-session, the
edits are easy to re-apply.

## Conclusion

GOAL MET. The BandScoreboard plate renders at **TOP-CENTER** in the V31 build,
matching retail layout, across 5 short-form runs and 1 long-form (9000-frame)
run. The fix is the minimal additive HX_NATIVE neutralization of the
authored `right.grp` anchor for the single-player layout, applied in the same
function (`TrackPanelDir::ConfigureTracks`) that already drives the
`scoreboard_to_top.tnm` animation. No regressions to the highway, gems,
strike-plate, applause meter, venue, band, or camera. V12 `CAMERA_FRAME_FIX`
left untouched (independent subtree, still required for highway/camera).
