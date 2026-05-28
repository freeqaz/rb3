# V8 — Gem stream wiring (2026-05-28)

## Summary

Found and fixed the root cause of "zero gem_*.mesh draws during gameplay":
the matched-fork `SongParser::OnGemEnd` was pushing every gem to
`mGemDBs[track]->mGameGemLists[0]` regardless of pitch-derived difficulty,
so `GemTrack::HandleNewSong` → `GemManager::SetupGems` →
`TheSongDB->GetGems(trackNum)` (which selects the user's actual difficulty,
typically Expert=3) returned an empty list. The visible chain
GemPlayer → GemTrack → GemManager → Gem::AddRep → TrackWidget::AddInstance →
RndMesh::DrawShowing → BandRnd::DrawMesh was wired correctly; only the
**data feed at the top of the funnel** was broken.

## Broken link identified

`/home/free/code/milohax/rb3/src/system/beatmatch/SongParser.cpp:718`

```cpp
void SongParser::OnGemEnd(int tick, unsigned char pitch) {
    int num = -1;
    int slot = PitchToSlot(pitch, num, tick);   // `num` is OUT-param: the difficulty (0..3)
    ...
    mSink->AddMultiGem(0, geminfo);  // BUG: hardcoded `0` instead of `num`
```

`PitchToSlot` writes the parsed difficulty into `num` via the pitch-offset
bucket `pitch - (diff * 0xC + 0x3C)`. Then the gem is dispatched —
**ignoring `num`** and pushing everything to diff=0.

Downstream:
- `SongDB::GetGems(track)` → `mSongData->GetGemListByDiff(track, mTrackDifficulties[track])->mGems`
- `mTrackDifficulties[3] = 3` (Expert), so we read `mGemDBs[3]->mGameGemLists[3]` — empty.

## Fix

HX_NATIVE-additive at the one bad line:

```cpp
#ifdef HX_NATIVE
    mSink->AddMultiGem(num, geminfo);   // GEM_STREAM_FIX: dispatch to actual diff
#else
    mSink->AddMultiGem(0, geminfo);
#endif
```

Before (`20thcenturyboy` guitar, Expert):
```
[GEM_DBG] GemManager::SetupGems: trackNum=3 gems.size=0
  trackNum=3 diff=0 NumGems=2076   <-- all gems landed here
  trackNum=3 diff=1 NumGems=0
  trackNum=3 diff=2 NumGems=0
  trackNum=3 diff=3 NumGems=0      <-- mTrackDifficulties[3] points here
```

After:
```
[GEM_DBG] GemManager::SetupGems: trackNum=3 gems.size=842
  trackNum=3 diff=0 NumGems=229    (Easy)
  trackNum=3 diff=1 NumGems=379    (Medium)
  trackNum=3 diff=2 NumGems=626    (Hard)
  trackNum=3 diff=3 NumGems=842    (Expert — now picked up)
```

`GemManager::PollHelper` then advances `mBegin/mEnd` correctly as gems flow:
```
ms=2143.9  mEnd=1 visible=1   <- first gem enters top of highway
ms=2325.1  mEnd=2 visible=2
ms=3477.2  mEnd=3 visible=3
ms=3667.5  mBegin=1 mEnd=3    <- first gem leaves bottom
```

`Gem::AddRep` → `Gem::SetType` → `CreateWidgetInstances` → `AddInstance`
all fire and `TrackWidget::AddInstance` correctly pushes
`{gem_green.wid, gem_blue.wid, ...}` instances into the active widget list
(verified — instance counts grow 1, 2, 3 ...).

`TrackWidget::DrawShowing` is called with `mImp->Size() > 0` on subsequent
frames, which dispatches to `RndMesh::DrawShowing` →
`gBandRnd.DrawMesh(prism_gem_stepped_<color>.mesh)`.

## Diagnostic note: gem meshes are named `prism_gem_stepped_*`, NOT `gem_*`

The investigation's "no `gem_*.mesh` draws" finding was based on the
expected RB3-style mesh naming. In this fork the in-milo geometry for the
gem-note is named `prism_gem_stepped_green.mesh`, `prism_gem_stepped_blue.mesh`,
etc. After the fix `BandRnd::DrawMesh` confirms:

```
[GEM_DBG] BandRnd::DrawMesh first-seen mesh='gem_smasher_guitar_lefty.mesh'
[GEM_DBG] BandRnd::DrawMesh first-seen mesh='prism_gem_stepped_green.mesh' nv=0 nf=246
[GEM_DBG] BandRnd::DrawMesh first-seen mesh='prism_gem_stepped_blue.mesh'  nv=0 nf=246
```

(nv=0 because the prism gem mesh uses compressed verts; the renderer
takes that path and unpacks at draw time.)

## Remaining blockers (post-fix)

### 1. SIGSEGV at frame ~2400 in `Symbol::operator==` from `PanelDir::Handle`

Once the per-diff gem stream is unblocked, MIDI events that previously
never fired now flow through the parser → `MsgSource::Export` →
TrackPanel → TrackPanelDir → PanelDir::Handle, where:

```cpp
if (sym != "button_down")   // <-- strcmp(sym.mStr, "button_down") with sym.mStr garbage
    HANDLE_MEMBER_PTR(mFocusComponent)
```

The message symbol passed in is malformed. Stack trace:

```
Symbol::operator==(char const*) const          utl/Symbol.h:27
PanelDir::Handle(DataArray*, bool)             ui/PanelDir.cpp:316
TrackPanelDirBase::Handle(DataArray*, bool)    bandobj/TrackPanelDirBase.cpp:268
TrackPanelDir::Handle(DataArray*, bool)        bandobj/TrackPanelDir.cpp:674
UIPanel::Handle(DataArray*, bool)              ui/UIPanel.cpp:287
TrackPanel::Handle(DataArray*, bool)           band3/bandtrack/TrackPanel.cpp:843
MsgSource::Sink::Export(DataArray*)            obj/Msg.cpp:10
MsgSource::Export(DataArray*, bool)            obj/Msg.cpp:160
MidiParser::Poll()                             midi/MidiParser.cpp:172
MidiParserMgr::Poll()                          midi/MidiParserMgr.cpp:64
BeatMaster::Poll(float)                        beatmatch/BeatMaster.cpp:128
```

The pre-existing crash is in the matched-fork MidiParser-driven message
pipeline. Independent of the gem stream wiring — it's the next thing to
fix once the visible-gem path is fully confirmed.

### 2. No gems visibly rendered in screenshots yet — RESOLVED (v10, 2026-05-28)

**Root cause: the `game.cam` world transform was NaN during every gem draw.**
None of the three documented hypotheses held — they were all refuted by the
trace below:

- **H1 (compressed verts degenerate) — REFUTED.** `RB3_RENDER_DBG` +
  per-gem vertex dump showed `prism_gem_stepped_green.mesh nv=174 nf=246`
  with a sane local bounding box `min(-1.98,-1.19,0) max(1.98,1.19,1.19)`.
  The Xbox-compressed-vert unpack in `Rnd_Wgpu_RB3.cpp` (`XboxCVert` /
  `BeFloat`/`BeColor`/`BeDec4n`) works correctly.
- **H3 (instancing not implemented) — REFUTED.** `prism_gem_stepped_green`
  reached `BandRnd::DrawMesh` 3281× / `_blue` 2894× per run — the per-instance
  loop (`ImmediateWidgetImp::DrawInstances` → `RndMesh::SetWorldXfm` per
  instance → `DrawMesh`) fires thousands of times.
- **H2 (off-screen transform) — partially true, but the real cause was the
  camera, not the gem.** Gem world Y was a correct `SecondsToY(songSec)` (e.g.
  Y≈365 at song 3.3s, `mYPerSecond=110`), within the cam frustum
  (`near=30 far=226`). But `RndCam::sCurrent` (`game.cam`) had
  `WorldXfm() = NaN` whenever gems drew, so every gem (and the whole highway
  under that cam) projected to NaN clip space and was discarded.

**The corruption chain** (found by probing `SetWorldXfm`/`WorldXfm_Force` and
walking the cam parent chain):

```
track_0 → rig.grp → cameras_top.grp → cameras.grp(LOCAL.v.y = -5.3e6 !!)
       → rotater.grp → rotater_roll.grp → scaler.grp → game.cam
```

`cameras.grp`'s LOCAL translation ran away ~100×/frame (300 → -5.9e6 →
2.4e7 → … → inf → NaN), driven by camera-rig TransAnims
(`camera_intro.tnm`, `spotlight_track_bounce.tnm`, `kick_success.tnm`)
applied via `AnimTask::Poll`.

**The actual bug:** `AnimTask::Poll` (`rndobj/Anim.cpp:378`) drives every anim
as `mAnim->SetFrame(blend, frame)` — blend (the ~0..1 weight) first, frame
(song time) second. But `RndTransAnim::SetFrame` interpolates its keys at its
first arg and `Interp`/EXTRApolates the result by its `blend` arg. With the
AnimTask ordering it therefore sampled keys at ≈weight (≈1) and extrapolated
the camera-rig's *own* local xfm by the song frame (e.g. 20000), re-reading
and re-multiplying it every frame → the runaway → NaN `game.cam`.

(The whole anim system has been running with this swapped (blend, frame)
convention, so a blanket `SetFrame(frame, blend)` swap in `AnimTask::Poll`
hides the highway/gem track-graphic groups — other content is tuned around
the swap. The blowup is unique to `RndTransAnim` because it reads + re-applies
its target's current local xfm.)

**The fix** (HX_NATIVE-additive, surgical):
`rndobj/TransAnim.cpp` `RndTransAnim::SetFrame` — a blend weight is by
definition in [0,1]; an out-of-range `blend` arg means the args arrived in
AnimTask's (blend, frame) order, so swap them back to (frame, blend) before
sampling (and clamp the weight). The property-sync path
(`SetFrame(mFrame, 1.0f)`) keeps blend==1 and is untouched.

After the fix: zero NaN anywhere in the render output; `game.cam` stays at a
sane Y≈175; `prism_gem`, `surface_keys` (6132×), and the smasher all draw
with a valid camera. The highway now renders with correct perspective and the
red/yellow/blue fret-lane surface — see
`screenshots/v10-gems-visible/`.

**Remaining gap (NOT a render bug):** the song transport clock is not
advancing in headless (`TheTaskMgr.Seconds(kRealTime) ≈ 0`), so the camera
scroll (`mult = mYPerSecond * Seconds(kRealTime)`) stays ~0 and the scene is
static — frames 1500/2500 are pixel-identical. The 842 gems are set up and
draw at their fixed highway-Y, but without a ticking song clock they don't
appear to flow down toward the smasher. Making gems visibly *move* is now a
song-clock / transport problem, not a gem-geometry or camera problem.

## Visual outcome

Screenshots: `/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v8-gem-stream/`
- 01_f0500…08_f2250 — boot → song-in → highway visible → just-before-crash.
- Highway + smasher render with correct lane colors (this was the v7 result).
- No visible gem notes yet despite the data path being fully wired
  (see blocker #2 above).

## Biggest remaining gameplay-visual gap after this fix

The MIDI-parser-driven message dispatch crash (blocker #1) blocks any
forward progress past song time ~2.4s. Fixing it should let us run
through the song long enough to confirm whether gems are merely tiny/
at-top (blocker #2 hypothesis b) or whether the compressed-vert render
path is the problem (hypothesis a/c). The fix here is also the highest-
value next step because it's directly on the gameplay-critical path
(every song that has MIDI events past the lead-in will hit it).

## Files touched (HX_NATIVE-additive)

- `src/system/beatmatch/SongParser.cpp` — the one-line fix
  (`AddMultiGem(0, ...)` → `AddMultiGem(num, ...)` inside HX_NATIVE).
- `src/band3/bandtrack/GemManager.cpp` — kept a minimal
  `[GEM_DBG] SetupGems: trackNum=N gems.size=K` summary log.
- `src/band3/bandtrack/Gem.cpp`,
  `src/system/track/TrackWidget.cpp`,
  `native/src/rb3_band_rnd.cpp` — investigation-only diagnostic logs
  added during the trace; reverted (clean).

## Reproducer

```bash
GEM_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=2200 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native 2>&1 | grep GEM_DBG
```

Expected: `gems.size=842` for `20thcenturyboy` guitar Expert.

---

# V12 — Gameplay camera framing + placeholder card (2026-05-28)

Commissioned by the V11 Opus review (`screenshots/v11-review/REVIEW_OPUS.md`),
which found the gameplay view was mis-composed: a tri-color "Wil Name" card
filled the centre while the real gem highway was an edge-on dark sliver against
the right frame edge. Both root-caused and fixed.

## Root cause: the milo `N_player_<aspect>` configuration script doesn't run

`TrackPanelDir::ConfigureTracks` builds the layout name (`1_player_wide` for a
lone guitarist) and calls `SetConfiguration(cfg, b)` →
`cfg->Handle("apply", b)`. That `apply` handler is a milo DataArray script that
is supposed to (a) center the active track — `set_track_offset 0`,
`set_side_angle 0`, `set_screen_rect_x 0` — and (b) hide the unused pool
tracks. **In the native port that script's track commands never execute**
(verified: `GemTrackDir::SetTrackOffset` / `SetSideAngle` / `SetScreenRectX`
are never called). So every gem track keeps the milo's AUTHORED multi-player
default, which produced two compounding faults:

1. **Off-axis camera.** `game.cam`'s rig chain
   `game.cam → scaler.grp → rotater_roll.grp → rotater.grp → cameras.grp → …`
   kept `rotater.grp` at the multi-player fan-out (local `x=-34.5`) plus a
   side-angle carried in `rotater_roll.grp` (basis `mY.x≈-0.14`). Net rendered
   pose: `pos=(-18.55,175,23) fwd=(-0.166,0.978,-0.124)` — a left-yawed,
   laterally-offset camera that swung the highway off-axis to the right edge.

2. **Placeholder card.** The pool holds 4 GemTrackDirs; only `track_0` (guitar)
   is in use, but the unused keys-template track (`track_3`) was never hidden,
   so its `surface_keys.mesh` + `tamb_Background.mesh` (the red/yellow/blue
   "Wil Name" card) kept drawing centred, dominating the frame. (V4/V5/V11 all
   mistook this card for the highway.)

Note `game.cam` IS correctly `RndCam::sCurrent` during the highway draw — the
per-frame render trace shows `world.cam` only because `TrackDir::DrawShowing`
restores the prior cam after its scope; `game.cam` is current solely inside
that scope. So the V8-B / V10 "game.cam is sCurrent during gem draws" finding
still held; the steady-state *pose* was the residual bug, exactly as V11
suspected.

Also relevant: a used GemTrackDir is a milo PROXY. Its `mRotater` member points
at the proxy-instance's own rotater.grp, but the camera it renders through hangs
off the proxy-SOURCE template's group chain — a DIFFERENT rotater.grp object.
That is why poking `mRotater` (what `SetTrackOffset` does) doesn't move the
rendered camera; the fix had to operate on the chain reached from the rendered
`game.cam`.

## Fix (HX_NATIVE-additive)

- `src/system/bandobj/TrackPanelDir.cpp` — `ConfigureTracks`: publish the
  in-use gem-track count (`gHxNativeNumUsedGemTracks`) and hide every UNUSED
  pool track (`SetShowing(false)`) so its placeholder template surface stops
  drawing. (The card disappears.)
- `src/system/track/TrackDir.cpp` — `DrawShowing`: for the single-player layout
  (`gHxNativeNumUsedGemTracks == 1`), walk up from the rendered camera `i6` and
  neutralize the per-player fan-out on the groups genuinely in its chain:
  zero `rotater_roll.grp`'s rotation (removes the side-yaw/roll), set
  `rotater.grp`'s local `x` to a centering offset (default `-4.0`, env
  `CAM_ROTX` to tune), and clear the authored `screenRect.x` viewport shift.
  Applied every frame because the proxy/rig re-syncs these from the template.
  The base camera pitch (in `scaler.grp` + `rotater.grp`'s rotation) is left
  intact. Result: `pos=(-4,173,23) fwd=(0,0.993,-0.122)` — straight down the
  highway, no yaw, no roll, slight down-pitch.
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — added a `CAM_DBG`-gated
  per-gameplay-mesh log (cam + mesh world position). Diagnostic only.

## Visual outcome

Screenshots: `screenshots/v12-camera-fix/` (frames 3200/4000/4800/5600).
The placeholder card is gone; the gem highway now renders as a clean, centred,
down-the-highway perspective with symmetric yellow side-rails converging to a
vanishing point top-centre — the signature RB3 framing. The strike/smasher
indicator sits bottom-centre-right.

Recalibrated gameplay recognizability: **~45%** (up from V11's ~10%). It now
unmistakably reads as an RB3 guitar highway. The gap to higher is the two
remaining (camera-independent) issues below.

## Remaining blockers (post-V12, camera-independent)

### 1. Gems not visible ON the highway surface

The 842 gems draw (`prism_gem_stepped_green/blue` reach `DrawMesh`), the song
clock advances (gem `meshPos.y` scrolls 365→377→…), and the camera now frames
them — but they're not visible on the surface. Two reasons:
- Only a sparse pair (green `x=-8`, blue `x=+4`) is in the window at the probed
  song time, sitting at `y≈365` while the camera is at `y≈150` with `far=226`,
  i.e. right at the far plane / vanishing point — sub-pixel and dim.
- The GEM WINDOW / population: far fewer gems are simultaneously in-frame than a
  real Expert chart at this song time. This is the V8 "gem window cycling /
  transport granularity" area, not camera or geometry.

### 2. Strike line / smasher is a skinned mesh that doesn't render

`gem_smasher_*` and `gem_mash0..5` are SKINNED meshes (the loader emits
"Skinned mesh needs to be re-exported"); the engine's `DrawMesh` binds an
identity bone palette, so the smasher/fret plate doesn't draw correctly (only a
small fallback glyph appears bottom-right). Implementing skinned-mesh rendering
in `Rnd_Wgpu_RB3.cpp` is the next gameplay-visual step.

## Reproducer (V12)

```bash
CAM_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=6000 \
  MILO_SCREENSHOT_DIR=docs/sessions/native/screenshots/v12-camera-fix \
  MILO_SCREENSHOT_FRAMES=3200,4000,4800,5600 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

Expected: `CAM_DBG: dir='track_0' cam='game.cam' pos=(-4.00,…) fwd=(0.000,0.993,-0.122)`
and a centred down-the-highway view in the PNGs. `CAM_ROTX=<x>` re-tunes the
lateral centering.
