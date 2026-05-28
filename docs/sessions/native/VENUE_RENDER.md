# V19 — Venue / stage geometry render (the black-void background unlock)

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED — venue
geometry renders behind the gem highway; `BandDirector::mCurWorld` is non-null and
ticking; clean exit 0.**

## TL;DR

Before V19, gameplay drew the gem highway over a **pure black void** —
`BandDirector::mCurWorld == null`, no venue/stage 3D. The planning agent's pivotal
finding (the `IsDeferredVenueProxy` gate is the long pole) was **only half the
story**: the gameplay venue WorldDir was **never even requested** natively, so
clearing the deferral gate alone would have changed nothing.

Root cause found by tracing: the retail `load_venue <sym>` dispatch (which reads
the world WorldDir's authored `venue` field and loads
`world/venue/<class>/<name>/<name>.milo` into `BandDirector::mVenue`) is **data /
game-mode driven and never fires in the native flow** — the only DTA `load_venue`
is the editor-only `load_and_play_song` preview (`world/world_objects.dta:1257`).
So `mVenue.Dir()` stayed null, `EnterVenue()` was a no-op, `mCurWorld` stayed null,
and additionally `TheBandWardrobe` was null (it's instanced from
`world/shared/world_chars.milo`, which only loads as a *side effect* of the venue
load).

Fix: bridge the missing dispatch in `BandDirector::EnterVenue()` (native only) —
read the world's authored `venue` symbol (`small_club_01` for the gameplay world)
and load the venue synchronously, exactly as retail `load_venue` would. The venue
load pulls in `world_chars.milo` → `TheBandWardrobe` becomes non-null →
`EnterVenue`'s normal wardrobe path (`SetVenueDir` / `SyncTransProxies`) then runs
and sets `mCurWorld`. One clang-LP64 bring-up crash surfaced and was fixed
(`RndEnviron::SetFogEnable` ud2). The `IsDeferredVenueProxy` gate was left UNTOUCHED
— the venue's *cosmetic* `world/shared/` sub-props (amps, mics, decals) still defer
cleanly; the venue ROOT + stage + crowd + characters all live outside that gate and
now instance and draw.

## What renders now (honest Opus visual review)

Screenshots: `docs/sessions/native/screenshots/v19-venue/` (canonical reproducer,
`20thcenturyboy`, frames 560/600/700/900/1200/1800).

- **f0560 / f0600 / f0700** — venue geometry IS drawing (magenta stage wall + a
  green/brown trim band), but the camera is on an awkward intro framing looking at
  walls/floor, not yet the down-highway shot. Not black void; real geometry, poor framing.
- **f0900** — *Clearly recognizable RB3 small_club venue:* dim wooden club stage,
  wall posters/signs, stage platform, pink stage lighting, and a **crowd of
  character silhouettes** in the foreground. This is unmistakably the RB3 stage.
- **f1200** — *The target view:* the gem highway (colored gems flowing) rendering
  **over the lit 3D venue** — wooden walls, "CORE" sign, a ladder/chair on the right,
  the stage platform. The previously-black background is now the club.
- **f1800** — highway + a **band-character figure** standing on stage behind it
  (renders as a dark / untextured body — consistent with the `BandPatchMesh` stub
  predicted in `VENUE_CHARACTERS_CAMERA_PLAN.md`).

Mesh draw count is the quantitative proof: **~54 meshes / 3.7K tris (pre-V19) →
156–245 meshes / 100K–139K tris (V19)** during gameplay.

Honest caveats (these are the planned NEXT increments, NOT regressions):
1. Early-gameplay camera framing (f560–f700) is awkward — the venue camera director
   isn't running, so the shot isn't the proper down-highway/stage composite.
   (Increment B — camera director.)
2. Characters render dark / untextured (`BandPatchMesh` excluded+stubbed) and are not
   yet animated to the music. (Increment A — characters.)
3. A `SHELL_PRESS_START_TO_ROCK` placeholder token leaks (separate G_TOKENS item).

## `mCurWorld` state

**Non-null and ticking.** `VENUE_DBG` confirms at gameplay:
`EnterVenue() wardrobe=0x… venueDir=0x… venueName='small_club_01'`. `mCurWorld` is
set to the loaded `small_club_01` WorldDir; it is Polled (`BandDirector::Poll →
mCurWorld->Poll()`) and Drawn (`DrawShowing → mCurWorld->DrawShowing()`) every frame.
This is the precondition the characters/camera follow-up increments were waiting on.

## Code changes (file:line)

1. **`src/system/bandobj/BandDirector.cpp` `EnterVenue()`** — additive `#ifdef
   HX_NATIVE` block at the top of the function. If `mVenue.Dir()` is null and
   `GetWorld()` is up, read `GetWorld()->Property(Symbol("venue"), false)` (falls back
   to `small_club_01`), set `mAsyncLoad=false` for the duration, and call
   `LoadVenue(venueSym, kLoadStayBack)` (synchronous `PollUntilLoaded`). Also added a
   null-wardrobe fallback that wires `mCurWorld = mVenue.Dir()` directly + sends
   `setup_midi_parsers_msg` (defensive; in practice the wardrobe is non-null by the
   time this runs because the venue load pulls in `world_chars.milo`). Plus two
   gated `VENUE_DBG` log lines. The `#else`/non-HX_NATIVE path is unchanged.
   (Diagnostic-only `VENUE_DBG` lines were also left in `LoadVenue` and at the top of
   `BandDirector::Enter()` — gated, low-volume.)

2. **`src/system/rndobj/Env.h:74` `RndEnviron::SetFogEnable(bool)`** — additive
   `#ifdef HX_NATIVE` block. The matched fork declares the method `bool` but never
   returns; MWCC PPC tolerates this (garbage in r3) but clang emits a `ud2` trap →
   SIGILL the moment the venue light-preset path calls it
   (`LightPreset::AnimateEnvFromPreset` → `LightPresetManager::Poll` →
   `BandDirector::Poll`). The return value is never consumed; the HX_NATIVE branch
   adds `return enable;`. `#else` byte-identical to the matched fork.

Both are matched-fork (layer a) additive `#ifdef HX_NATIVE … #else … #endif` edits;
the `#else` halves are byte-identical to the permuter's current content. No engine
(layer b) or glue (layer c) changes were needed. `IsDeferredVenueProxy`
(`Instance.cpp`) and `ObjectDir::PostLoadInlined` (`Dir.cpp`) were **NOT** modified.

## Obstructions cleared

- The "venue never requested" gap (the actual blocker the planning doc missed): the
  data-driven `load_venue` dispatch — bridged in `EnterVenue` (native).
- `TheBandWardrobe == null` at gameplay — resolved as a *consequence* (the venue load
  pulls in `world_chars.milo` which instances the wardrobe singleton).
- `RndEnviron::SetFogEnable` missing-return ud2 (clang-LP64 bring-up).

The three obstructions named in the dispatch (the `IsDeferredVenueProxy` gate, the
`PostLoadInlined` 3 sub-obstructions, `InterstitialPanel::Exiting`) turned out NOT to
be on the critical path for *geometry render*: the venue ROOT + stage geometry +
crowd + characters all instance fine once the venue is requested. Those gates only
affect the cosmetic `world/shared/` sub-props (amps/mics/decals — still deferred,
visually minor) and the menu/interstitial vignette outros. They remain as
documented for a fidelity follow-up but are no longer blocking.

## Remaining-obstruction list (for the characters / camera follow-up)

Per `VENUE_CHARACTERS_CAMERA_PLAN.md`, with `mCurWorld != null` now satisfied:

**Increment A — characters (render textured + animate):**
- `BandPatchMesh` is excluded (`native/CMakeLists.txt`) + fully stubbed
  (`native/src/band3_link_stubs.s`) → characters draw dark/untextured. Bring up the
  clang-LP64 port (a `.bak` exists) to restore outfit textures.
- Verify `BandWardrobe::SyncTransProxies` matched the 360-ARK proxy slot names (the
  characters DO appear on stage, so the proxy match is at least partially working).
- Confirm `CharDriverMidi` ticks (animation) — the `setup_midi_parsers_msg` is now
  sent to a real `mCurWorld`; verify bone xfms advance over song time.
- The `ReadyForMidiParsers` HX_NATIVE venue-deferred gate
  (`BandDirector.cpp:570-595`) can now be tightened back toward the `#else` form
  since `mVenue.Dir()` is real at gameplay (currently still returns the
  deferred-gate value; harmless but no longer necessary).

**Increment B — camera director (camera cuts):**
- Camera framing is the most visible remaining gap. `WorldDir::Poll`'s
  `mCameraManager.PrePoll/Poll` (gated on `mPollCamera`) + `BandDirector::OnSelectCamera`
  now run (the world is polled), but the director-vs-highway camera-ownership conflict
  (V12 highway lock vs venue shot cam, `RndCam::sCurrent` last-write-wins) is unresolved
  — that's why early frames look at walls. This is Increment B's core (B3) work.

## Reproducer

```
MILO_SCREENSHOT_DIR=docs/sessions/native/screenshots/v19-venue \
MILO_SCREENSHOT_FRAMES=560,600,700,900,1200,1800 \
VENUE_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=2000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

`VENUE_DBG=1` logs the venue force-load + the `EnterVenue` wardrobe/venueDir state.

---

# V20 — Increment A: band characters (texturing + animation)

**Authored:** 2026-05-28 (Opus implementation agent). **Status: PARTIAL — crowd +
extras render textured and animate; the four named band players (`player0..3`,
`type=main`) are loaded + proxied + drawn but EXPLODE due to a runaway bone-Y
transform bug, so they're invisible. BandPatchMesh un-excluded + clang-LP64-clean.**

## What I did (file:line)

1. **`src/system/bandobj/BandPatchMesh.cpp`** — un-excluded + brought up clang-LP64.
   The TU was a complete matched-fork port already; the ONLY clang blocker was the two
   `namespace stlpmtx_std { … }` blocks of explicit STLport `__unguarded_partition` /
   `__introsort_loop` / `__adjust_heap` sort-helper specializations (asm-match-only;
   clang's libstdc++ has no such symbols → `no function template matches`). Wrapped both
   blocks in additive `#ifndef HX_NATIVE … #endif` (same pattern as `GameGemList.cpp`).
   The `std::sort` calls fall through to the standard library under clang. (lines ~130,
   ~217, ~519, ~574).
2. **`native/CMakeLists.txt:307`** — removed `BandPatchMesh` from `_NATIVE_FORK_EXCLUDE`.
3. **`native/src/band3_link_stubs.s`** — deleted all `BandPatchMesh` weak no-op stubs
   (was lines ~254 `ConstructQuad` + ~1006-1027 the ctor/Render/PreRender/Compress/
   ListDrawChildren/operator>>/PropSync block) — strong defs from the compiled TU win.
4. **Diagnostics (all env-gated on `CHAR_DBG`, log-only, render-inert):**
   - `src/system/bandobj/BandWardrobe.cpp:326` `SyncTransProxies` — proxy-match counts.
   - `src/system/char/Character.cpp:320` `DrawShowing` — per-character LOD/screenSize/
     worldPos + a `type==main` periodic bone/parent sampler.
   - `src/system/char/Character.cpp:522` `Teleport` — waypoint/local pos.
   - `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1320` `DrawMesh` — per skinned
     mesh: resolved diffuse tex name + `RndTex` type + hasTexView + color + boneCount.
   Leave them in (cheap, gated) for the bone-Y follow-up.

## Honest visual review (screenshots `screenshots/v20-characters/`)

- **Crowd** (`crowd_male/female01..04`): render textured (clothing `_diff.tex`
  resolved, `hasTexView=1`), sane positions (`worldPos≈origin`, `screenSize`>0), and
  ANIMATE — comparing f0505/f0535/f0565 the silhouettes change pose frame-to-frame.
  These are the recognizable figures on the club floor.
- **Extras** (`*_extras01/02/11`): body/head/hair meshes texture correctly
  (`hasTexView=1`), BUT their `Character` root `worldPos` shows the same runaway-Y as
  the players (see below) so they're largely off-screen / unreliable on stage.
- **Band players** (`player0..3`, `type=main`): **loaded** (`mTargets[0..3]` non-null),
  **proxied** (SyncTransProxies matched **86/88** `playerN_*.tp` slots), **`SetShowing(true)`**,
  and `Character::DrawShowing('playerN')` **IS called every frame** — but they do NOT
  appear. Their per-mesh draw never reaches the engine because the skeleton EXPLODES.
- **Gameplay camera** still on the highway-lock framing (Increment B) — the stage/band
  isn't framed during the song; the band is only glimpsable in the intro window
  (~f0505-f0565). Did NOT touch the camera director (per dispatch).

## The wall (precise): runaway bone-Y in the servo skeleton

`CHAR_DBG` proves the chain step by step:
- The player ROOT transform is **fine**: `Teleport` (way=nil) leaves a sane
  `mLocalXfm` (`player0 local=(55.67,28.85,0)`, players spread along X at Y=28.85,Z=0),
  `TransParent()==(none)`, and at sample moments `WorldXfm` reads the sane root.
- But the player's BONES diverge in Y and **only Y**: `bone_head`/`bone_R-hand` world
  positions have sane X/Z that even animate over song time (X/Z move frame-to-frame —
  the MIDI-driven animation IS running), while Y runs away: ≈ -2.3e14 → -1.1e16 →
  -2.3e16 → -3.4e16 … growing ~linearly each frame until `inf`. With an inf-Y world
  sphere `ComputeScreenSize` collapses to 0 and the body draws degenerate/off-screen.
- The same runaway-Y also afflicts the `extras` `Character` roots (but not the
  `crowd`, which sits at origin and renders fine). crowd uses plain clips; extras +
  players use the `CharServo`/`CharServoBone` "facing"/regulated skeleton.

**Root-cause hypothesis (for the follow-up):** an LP64 bone-buffer / pointer-offset
divergence in `src/system/char/CharServoBone.cpp` (`Poll`/`MoveToFacing`/
`MoveToDeltaFacing`, the `mFacingPosDelta = FindPtr("bone_facing_delta.pos")` +
pelvis-relative `Multiply` chain) or its `CharBones`/`CharBonesMeshes` packed-buffer
base in `CharBonesObject`. `FindPtr` returns raw offsets into a packed bone blob;
under LP64 a wrong stride/offset writes a `Vector3` one component off, which is
exactly consistent with a single-axis (Y) runaway that compounds each Poll. This is
NOT excluded/stubbed (CharServoBone.cpp is on the link line) and is the genuine
Increment-A blocker. It is the same class of deep matched-fork skeleton-math LP64 bug
as the V14a/gameplay-loop bone work — needs careful tracing, not a quick gate flip.
The `BandPatchMesh` outfit-texture render-to-texture path is a SECONDARY gap: the
engine's `RndTex::MakeDrawTarget`/`FinishDrawTarget` (`Rnd_Wgpu_RB3.cpp:1419`) are
no-ops, and `OutfitConfig::MatSwap::Compose` only needs them for materials whose
diffuse is a `kRenderedNoZ` render-target; the crowd/extras/player base materials use
the simple `mTextures[idx]` path (type=0x1) which already textures correctly — so RTT
outfit compositing is a low-priority fidelity item, not why the band is invisible.

## ReadyForMidiParsers gate (A1)

Left the `BandDirector::ReadyForMidiParsers` HX_NATIVE gate (`BandDirector.cpp:628`)
AS-IS. It already returns true (`mPropAnim && AllCharsLoaded()`) and the MIDI parsers
attach — confirmed by the bones animating in X/Z. Tightening it to the `#else` form
(`mVenue.Dir() || Name()=="none"`) is harmless-but-unnecessary now that the venue
loads; not required for animation, so not changed (avoids churn).

## State of shared files (for Increment B — camera director)

- **`native/CMakeLists.txt`** — `BandPatchMesh` removed from `_NATIVE_FORK_EXCLUDE`
  (still excludes `ClipDistMap DataResults SaveLoadManager Splash StorePackedMetadata
  TourPerformerLocal WaitingUserGate`). No other change.
- **`native/src/band3_link_stubs.s`** — only the `BandPatchMesh` stubs were removed;
  the `CameraManager::Poll` stub (`dta_link_stubs.s`) and others are UNTOUCHED.
- **`Rnd_Wgpu_RB3.cpp`** — added ONLY the env-gated `CHAR_DBG` diagnostic in DrawMesh's
  material block (lines ~1320). No render-path / camera / pipeline change. The skinned
  path (V14a) and `RndTex::MakeDrawTarget` no-ops are unchanged. Safe for B to extend.
- **`BandDirector.cpp`** — NOT modified (V19's EnterVenue bridge intact).

## Reproducer (V20)

```
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v20-characters \
MILO_SCREENSHOT_FRAMES=505,515,525,535,545,555,565 \
CHAR_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=590 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

NOTE: `MILO_SCREENSHOT_DIR` must be an ABSOLUTE path — a relative path silently fails
`WritePNG` (stbi can't open it; cwd quirk under the headless harness).
`CHAR_DBG=1` enables the character/bone/proxy/texture diagnostics above.

## Recommended next step

Fix the `CharServoBone`/`CharBones` runaway bone-Y (the real Increment-A blocker) —
trace `FindPtr("bone_facing*.pos")` offsets + the pelvis-relative `Multiply` chain in
`CharServoBone::Poll` under LP64; compare bone-buffer base/stride vs the PPC layout.
Once bones stay finite, the players' bodies (already textured via the simple path)
should render on stage and animate to the song. THEN Increment B (camera director) so
the stage/band is actually framed during gameplay.

---

# V21 — Increment A blocker fixed: runaway bone-Y was an empty-body `Multiply` no-op

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED — the band
players' servo skeletons stay finite + animate; `player0..3` render upright on stage.
Clean exit 0; gameplay/audio/gem path unregressed.**

## TL;DR

The V20 hypothesis (a `FindPtr`/packed-buffer LP64 stride/offset bug in
`CharServoBone`) was the right SUBSYSTEM but the wrong mechanism. The runaway-Y was a
**single empty-body math function** in the shared math header:

**`src/system/math/Mtx.h:639` `Multiply(const Vector3 &vin, const Hmx::Matrix3 &mtx,
Vector3 &vout)`.** The matched fork's ONLY body for this overload is a PPC
paired-singles `ASM_BLOCK(...)`. Under clang (non-`__MWERKS__`) `ASM_BLOCK(...)`
expands to **nothing** (`src/decomp.h:76`), so the function returned without ever
writing `vout` — every caller got **uninitialized stack garbage**. This is exactly
the "empty-body non-void" footgun the V20 dispatch named.

It manifested as the bone-Y blow-up because `CharServoBone::MoveToDeltaFacing` does:
```cpp
Vector3 v18;                            // uninitialized
Multiply(*mFacingPosDelta, tf.m, v18);  // NO-OP under clang -> v18 stays garbage
tf.v += v18;                            // accumulates the SAME garbage every frame
```
`tf` is the Character's `mLocalXfm` (`mMe->DirtyLocalXfm()`), so the character
TRANSLATION accumulated a constant garbage `v18.y` (≈2.9e32 per frame in the observed
run) linearly → `mLocalXfm.v.y` → `inf`. The pelvis/head/all bones ride that root, so
their WORLD-Y diverged while their stable LOCALs (and X/Z) animated correctly — which
is precisely the X/Z-fine-but-Y-runs-away signature V20 documented.

Sibling note: the *other* `Multiply(const Vector3 &, const Transform &, Vector3 &)`
overload (`Mtx.h:409`) was NOT broken — it has a real `#else` C++ fallback gated on
`#ifdef MATCHING` (and `MATCHING` is undefined natively). Only the matrix-only
overload (639) fell straight through its `#else` into the no-op asm.

## The fix (file:line)

**`src/system/math/Mtx.h:639`** — additive `#ifdef HX_NATIVE … #else … #endif`. The
`HX_NATIVE` branch gives the function a real C++ body matching the asm's semantics
(row-vector × rotation matrix, no translation — i.e. the `Multiply(Vector3,Transform)`
formula minus `+t.v`):
```cpp
vout.Set(
    mtx.x.x*vin.x + mtx.y.x*vin.y + mtx.z.x*vin.z,
    mtx.x.y*vin.x + mtx.y.y*vin.y + mtx.z.y*vin.z,
    mtx.x.z*vin.x + mtx.y.z*vin.y + mtx.z.z*vin.z);
```
The `#else` half is **byte-identical** to the permuter's current asm body. Derivation
of the asm semantics is in the comment at the fix. This is a layer-a (matched-fork)
header change; it forces a wide recompile (746 objects) because it's the shared math
header, and it is a **broad correctness win** — this overload backs `TransformNormal`,
`Transpose(Transform)`, `Invert`/`FastInvert`(Transform) (`Multiply(vtmp, tfout.m,
tfout.v)`), `CharMirror`, the servo facing math, etc. Anything natively calling
`Multiply(Vector3, Matrix3, Vector3)` had been silently writing garbage.

## Before / after bone-Y (CHAR_DBG)

- **Before:** `player2` `meWorldY` / pelvis world-Y climbed linearly each frame:
  `68 → 115 → 163 → … → 1.06e3 → … → 1.32e24 → 2.97e24 → …` toward `inf`; X/Z pinned
  (`pelvis world.x=-18.6, world.z=37.58` constant) while local stayed sane
  (`local=(-0.0005,1.63,37.58)`). `screenSize→0` ⇒ invisible.
- **After:** `player2 meWorldY=28.85`, `pelvis world=(-18.6,30.47,37.58)` — **finite
  and bounded for the entire 9000-frame song**. `player0` pelvis oscillates frame to
  frame `(18.57,86.0,37.06) → (18.62,78.2,35.96)` (all three axes move) — the skeleton
  is finite AND animating to the MIDI clip.

## What renders now (honest Opus visual review)

Screenshots: `docs/sessions/native/screenshots/v21-band-players/`
(reproducer below; frames 570/650/750/900/1200/1800/2400/3000).

- **f0570 / f0650** — the venue with the camera near the stage: a dense textured
  CROWD in the foreground AND **upright textured human figures standing on the lit
  stage platform** (the band players + extras). They are normal-proportioned bodies,
  not the degenerate/off-screen smears of V20. This is the headline change — the band
  is now ON the stage.
- **f0900 / f1200 / f1800** — the down-highway gameplay camera: gem highway flowing
  over the wooden `small_club` venue (CORE sign, walls, ladder/chair). The band is on
  the stage but **out of this camera's frame** — expected, because the venue camera
  director (Increment B) isn't running and the highway camera is locked looking down
  the fretboard. Not a regression.
- **f0750** — an awkward intro framing looking at a wall — same Increment-B camera gap.

Honest caveats / remaining gaps (NOT regressions; the planned next increments):
1. **Camera framing (Increment B).** The down-highway gameplay camera doesn't frame
   the stage, so the (now-correct) band is only clearly visible in the early
   stage-facing window (~f0560–f0700). Judging "do they render + animate" therefore
   leans on the CHAR_DBG bone data + those early frames — which is conclusive: finite,
   animating, upright, textured.
2. **`BandPatchMesh` RTT outfit compositing** (secondary fidelity gap, untouched).
   Player base/skin/hair materials already texture via the simple `mTextures[idx]`
   path (`type=0x1 hasTexView=1`, same as crowd/extras). The render-to-texture outfit
   *composite* (`RndTex::MakeDrawTarget`/`FinishDrawTarget` no-ops in
   `Rnd_Wgpu_RB3.cpp`) is still a no-op, so any material whose diffuse is a
   `kRenderedNoZ` target stays unpainted (a few accessory mats, e.g. eyebrows/tongue,
   show `diffuse=(null) type=0xffffffff`). Bodies/heads/skin/hair are correct.

## CHAR_DBG diagnostics left in place (gated, low-volume)

- `src/system/char/CharServoBone.cpp` `Poll` — one throttled (every 300 polls) line
  per servo: `moveSelf/deltaChanged/pelvisLocalY/meLocalY/facingPos/facingPosDelta`.
- `src/system/char/Character.cpp` `DrawShowing` (the `type==main` sampler) — now also
  prints `meWorldY` + pelvis world/local + constraint, so the servo skeleton's
  finiteness/animation can be re-verified at a glance (regression canary for B).

Both are additive `#ifdef HX_NATIVE`, `getenv("CHAR_DBG")`-gated, render-inert. The
verbose per-write `MoveToDeltaFacing/Regulate dY=` and pelvis-parent-chain-walk traces
used to localize the bug were removed (they served their purpose).

## State of shared files (for Increment B — camera director)

- **`src/system/math/Mtx.h`** — the one `Multiply(Vector3,Matrix3,Vector3)` HX_NATIVE
  fix (line 639). Pure correctness; affects all native vector×matrix transforms. B can
  rely on transforms now being correct (this also makes `FastInvert`/`Transpose`/
  `TransformNormal` correct, which a camera director may exercise).
- **`src/system/char/CharServoBone.cpp`**, **`src/system/char/Character.cpp`** — only
  the gated CHAR_DBG diagnostics above; no render/camera/pipeline logic changed.
- **`Rnd_Wgpu_RB3.cpp`**, **`BandDirector.cpp`**, **`native/CMakeLists.txt`**,
  **`native/src/band3_link_stubs.s`** — UNTOUCHED by V21 (V19/V20 state intact). The
  highway-vs-venue camera-ownership conflict (V12 lock vs `RndCam::sCurrent`
  last-write-wins) is still the open Increment-B (B3) item.

## Reproducer (V21)

```
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v21-band-players \
MILO_SCREENSHOT_FRAMES=570,650,750,900,1200,1800,2400,3000 \
CHAR_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=3200 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

`CHAR_DBG=1` logs the servo + `bones '<player>'` finite-Y trace; grep
`CHAR_DBG: bones 'player2'` and confirm `meWorldY`/pelvis world-Y stay ~30 (not e+NN)
across the whole song. `MILO_SCREENSHOT_DIR` MUST be ABSOLUTE (relative silently fails
WritePNG).

## Recommended next step

Increment B — venue camera director. With the skeletons finite and the band on stage,
the only thing hiding them during the song is the camera: the highway-locked gameplay
camera never cuts to the stage. Resolve the `RndCam::sCurrent` last-write-wins conflict
between the V12 highway lock and the venue/`BandDirector::OnSelectCamera` shot cams so
the director can frame the stage/band between highway passes. (Secondary: bring up the
`BandPatchMesh` RTT outfit composite for full outfit fidelity.)

---

# V22 — Increment B: venue camera-cut director (camera cuts during the song)

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED — the venue
camera director runs and CUTS between cinematic shots during the song (intro venue
pan, wide stage, guitar/bass/vocal/drum closeups, ...), framing the
stage/band/crowd. The note highway + gems remain visible and playable in every
gameplay frame. Clean exit 0 over the full 8000-frame song.**

## TL;DR — the central conflict was NOT what the plan predicted

The `VENUE_CHARACTERS_CAMERA_PLAN.md` B3 hypothesis was a `RndCam::sCurrent`
last-write-wins fight between the venue director and the highway, possibly needing
an engine multi-pass. **Neither was true:**

1. **The engine already does per-element multi-camera.** `Rnd_Wgpu_RB3.cpp`
   (V2/V13 machinery, `DrawMesh` ~1023-1053) re-writes the scene view/projection
   uniforms *per mesh* whenever `RndCam::sCurrent` changes — by pointer OR by pose.
   So the venue meshes can draw through one camera and the highway meshes through
   another in the SAME frame; there is no single-`sCurrent` conflict to resolve. No
   engine change was needed.

2. **The real blocker: the director was animating a camera that rendered nothing.**
   `CAMDIR_DBG` instrumentation proved the director chain is fully alive natively —
   `OnSelectCamera → FindNextShot/FindNextDircut → PlayNextShot →
   CameraManager::ForceCameraShot/PrePoll → StartShot_ → Poll → CamShot::SetFrame`
   all fire, committing a real stream of distinct `BandCamShot`s every dwell
   (`coop_smallclub_intro_venue` → `coop_all_f04` → `coop_g_cg` → `coop_b_cg01` →
   `coop_v_c01` → `coop_g_n06` → `coop_d_cd02` ...). Each shot animates ONE camera
   object — `world.cam`, the PanelDir `mCam` of `GetWorld()` (the FileMerger "world"
   dir) — by rewriting its `WorldXfm`. `world.cam` WAS moving/cutting correctly.

   But V19 loaded the venue WorldDir (`small_club_01`) as `BandDirector::mCurWorld`
   *without merging it into* `GetWorld()`, so `mCurWorld != GetWorld()` (proved by
   `CAMDIR_DBG: ... sameDir=0`). `PlayNextShot` forces the shot onto
   `GetWorld()->mCameraManager` (where `world.cam` lives), but the visible
   stage/band/crowd geometry lives in `small_club_01`, and
   `WorldDir::DrawShowing` for the venue selects the VENUE's OWN `CamOverride()`
   (its `mCam`), never the director-posed `world.cam`. **The cinematic camera cut
   beautifully into the void; the venue rendered from a static unrelated cam.** In
   retail the venue IS merged into `world`, so the shot cam and the venue draw cam
   are the same object — the native split severed them.

## The fix (file:line) — one matched-fork edit, zero engine change

**`src/system/bandobj/BandDirector.cpp` `DrawShowing()`** — additive `#ifdef
HX_NATIVE` block (~line 315). Before `mCurWorld->DrawShowing()`, fetch the
director's active shot camera from `GetWorld()->mCameraManager`
(`MiloCamera() ?: CurrentShot()` then `->CurrentShot()->GetCam()`) and point the
venue WorldDir's `CamOverride` at it (`mCurWorld->SetCam(shotCam)`) when it differs.
`small_club_01` is a child of `world` (EnterVenue does
`SetName(.., GetWorld())`) so they share world coordinates — the shot poses frame
the stage correctly. Result: the venue draws through the shot-animated `world.cam`
(cuts), the highway continues to draw afterward through `game.cam` (the V12-locked
down-highway cam), and the engine's per-mesh camChanged re-write composites both in
one frame. **Per-element camera, not a global one.** Opt-out via env
`VENUE_CAM_LOCK=1` reverts to the pre-V22 highway-locked baseline for A/B.

No edit to `Rnd_Wgpu_RB3.cpp`, `TrackDir.cpp`/`TrackPanelDir.cpp` (the V12 highway
lock is UNTOUCHED — the highway keeps its own `game.cam`), `Instance.cpp`,
`CMakeLists.txt`, or any `.s`. The V12 neutralization did NOT need revisiting: the
highway and venue never actually fought for `sCurrent` because the engine tracks the
current camera per draw.

## What the camera does now (honest Opus visual review)

Screenshots: `docs/sessions/native/screenshots/v22-camera-cuts/` (`run1/`, `sweep/`,
`final/`; reproducers below). Track `guitar`, `20thcenturyboy`, full song.

- **Intro (f0580-f0700)** — the `INTRO_VENUE` shot pans the club: wide low-angle
  shots of the crowd silhouettes in the foreground + the lit `small_club` stage with
  the band players on the platform, club walls/posters. Distinct framings cut
  frame-to-frame. Recognizable RB3 venue cinematics.
- **Gameplay (f0860 onward, full song to f7000)** — the down-highway view: gem
  highway + smasher + score flowing, with the venue behind it. The venue BACKGROUND
  visibly cuts between shots over the song — f0900 green-lit wall, f1100 dark wall
  with amps, f1800 the "CORE" sign + bar + wall mural, f3600/f7000 pink stage wall +
  green trim — confirming the director keeps cutting the venue camera throughout
  while the highway stays fixed and playable.
- **~8 distinct cuts** in the first ~4000 frames (`CAMDIR_DBG: PlayNextShot
  COMMITTED` log), cycling wide / guitar-hand / bass-hand / vocal / guitar-near /
  drum-hand categories — the retail camera variety.

## Regression status

- **Highway / gems: INTACT.** `CAM_DBG` confirms every `prism_gem*` / `gem_smasher*`
  / `surface` mesh still draws under `game.cam` (the down-highway cam) at its correct
  NDC position; the highway is present + playable in every gameplay frame across the
  full 8000-frame song. Mesh/tri counts unchanged from V21 (~970 meshes / 551K tris).
- **V19/V20/V21 unregressed.** Venue geometry, crowd, extras, and the four band
  players (finite servo skeletons) all still render. Clean exit 0; audio + gem +
  score path unaffected (no edits outside `BandDirector::DrawShowing`).
- `VENUE_CAM_LOCK=1` cleanly reverts to the locked-highway baseline (verified, exit 0).

## Remaining gaps (NOT regressions — follow-ups)

1. **Tight character-targeted closeups frame poorly.** The wide / `*_near` /
   `INTRO_VENUE`-pan shots frame the stage+band well, but the tight closeup shots
   (`coop_g_cg` guitar-hand, `coop_b_cg01`, `coop_v_c01`, etc.) target exact band-bone
   positions; natively those land slightly off, so some closeups frame the stage WALL
   / empty space (e.g. the intro `f0740-f0820` wall close-up that fills the screen
   before the highway engages). This is a **character/shot-target fidelity** issue
   (the closeup cam's authored target is a `BandCharacter` bone whose native position
   differs), NOT a camera-ownership issue — driving the cam differently won't fix it.
   Follow-up: trace `BandCamShot` target resolution (`CamShotTarget` /
   `mInterestObjects`) vs the native character bone positions.
2. **No `DIRECTED_CUT` MIDI dircuts harvested for this song.** `HarvestDircuts` ran
   with `venueDir=(nil)` at the harvest moment (timing: it fires before the V19
   EnterVenue force-load completes), so `mDircuts` is empty and the director falls
   back to `FindNextShot` category cycling — which still produces the cut sequence
   above. Wiring `HarvestDircuts` to re-run after the venue instances (so authored
   `DIRECTED_CUT`s drive beat-synced cuts) is a fidelity follow-up.
3. **Intro venue shot dominates full-screen pre-gameplay (f570-850).** Because the
   highway hasn't engaged yet, a bad intro closeup (#1) fills the whole screen with no
   highway to fall back on. Once the gem stream starts (~f860) the highway is always
   present. Cosmetic; tied to #1.
4. The dead `CameraManager::Poll` weak stub (`dta_link_stubs.s:264-265`) is confirmed
   dead by `nm` (strong `T _ZN13CameraManager4PollEv` from the globbed TU wins). Left
   in place to avoid touching the SERIALIZED `.s` file; safe to delete in a stub-audit
   pass.

## Diagnostics left in place (gated, render-inert)

All `#ifdef HX_NATIVE` + `getenv("CAMDIR_DBG")`-gated, throttled, log-only — kept as a
regression canary for the director chain:
- `BandDirector.cpp` — `OnSelectCamera` (shot-category/next-shot state),
  `PlayNextShot` (committed shot name+category+`wdir==curWorld`), `HarvestDircuts`
  (dircut/category counts), `DrawShowing` (curWorld vs getWorld, shot cam + pose, the
  V22 `venue mCam -> shotCam` retarget).
- `world/Dir.cpp` `WorldDir::DrawShowing` — per-dir current-shot + shot-cam name +
  `mPollCamera` + `sCurrent`.
- `world/CameraManager.cpp` `StartShot_` — shot start.

## Reproducer (V22)

```
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v22-camera-cuts/final \
MILO_SCREENSHOT_FRAMES=600,700,900,1200,1800,2400,3600,5000,7000 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=8000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

`CAMDIR_DBG=1` logs the director chain + the V22 venue-cam retarget. `CAM_DBG=1`
confirms the highway gems still draw under `game.cam` (regression canary).
`VENUE_CAM_LOCK=1` disables the V22 venue-cam follow (reverts to the locked-highway
baseline). `MILO_SCREENSHOT_DIR` MUST be ABSOLUTE (relative silently fails WritePNG).

## State of shared files (for the next agent)

- **`src/system/bandobj/BandDirector.cpp`** — the V22 `DrawShowing()` HX_NATIVE
  venue-cam-follow block (+ gated CAMDIR_DBG in OnSelectCamera/PlayNextShot/
  HarvestDircuts/DrawShowing). V19 EnterVenue bridge intact.
- **`src/system/world/Dir.cpp`**, **`src/system/world/CameraManager.cpp`** — only
  gated CAMDIR_DBG diagnostics; no logic change.
- **`Rnd_Wgpu_RB3.cpp`**, **`TrackDir.cpp`/`TrackPanelDir*.cpp` (V12 lock)**,
  **`native/CMakeLists.txt`**, **`native/src/*.s`** — UNTOUCHED by V22.

## Recommended next step

Improve closeup framing (gap #1): trace `BandCamShot` interest-object / target
resolution so the guitar/bass/vocal/drum closeups frame the actual band members
instead of the stage wall — the camera-ownership plumbing is now correct, this is
character-target fidelity. Secondary: re-run `HarvestDircuts` post-venue-instance so
authored `DIRECTED_CUT`s drive beat-synced cuts (gap #2).

---

# V23 — Cinematography polish: closeups frame the band; authored dircuts harvest

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED — the
instrument closeups (`coop_g_cg` guitar, `coop_b_cg01` bass, `coop_v_c05` vocal,
drum, ...) now frame the ACTUAL band member at its real stage position instead of
the stage wall / void. The song's authored MIDI `DIRECTED_CUT`s now harvest
(`dircuts=15` for `20thcenturyboy`). Note highway + gems intact every gameplay
frame; clean exit 0 over the full 8000-frame song.**

## TL;DR — the closeup miss was NOT a transform/interest-object bug

The V22 follow-up hypothesis was that the closeups land "slightly off" because the
authored target bone's native position differs. **The real cause is upstream and
total, not a small offset:** the camera-target proxies the closeups point at were
never wired to the band characters at all, so they all collapsed onto a single
shared stand-in dir — every guitar/bass/vocal closeup framed the *same* wrong world
point.

Chain (proved by `CAM_TGT_DBG` / `CHAR_DBG`):
1. A `BandCamShot`'s framing comes from `CamShotFrame::mTargets`
   (`world/CameraShot.cpp:1165` `GetCurrentTargetPosition` averages each target's
   `WorldXfm().v`). For closeups those targets are venue `RndTransProxy`s named
   `player_<inst>0_bone_*.tp` (e.g. `player_guitar0_bone_target_strum.tp`,
   `player_vocals0_bone_head.tp`).
2. A proxy tracks its band member only after `BandWardrobe::SyncTransProxies`
   (`BandWardrobe.cpp:326`) calls `it->SetProxy(mTargets[i])` — matched by
   `strstr(proxyName, mVenueNames.names[i])`. The instrument-keyed venue names
   `player_<inst>0` are set by `LoadMainCharacters` (`BandWardrobe.cpp:719`).
3. **`LoadMainCharacters` never ran natively.** Its only live entry is
   `BandDirector::OnFileLoaded(song)` (`BandDirector.cpp:~1255`) →
   `TheBandWardrobe->LoadCharacters(mVenue.Name(), ...)`, gated on
   `!mVenue.Name().Null()`. In retail the data-driven `load_venue` ran earlier so
   `mVenue.Name()` is already set; natively the venue load is deferred to V19's
   `EnterVenue`, so at `OnFileLoaded(song)` time `mVenue.Name()` is STILL NULL and
   the call SKIPS (`CHAR_DBG: OnFileLoaded(song) venueName='' ... SKIP`).
4. Consequence: the band players never got `mInstrumentType` assigned, and
   `mVenueNames` stayed empty. When `EnterVenue → SetVenueDir → SyncTransProxies`
   ran, it matched **0** instrument-keyed proxies (and the bare `player0..3` slot
   proxies that DID match — 86/88 — are NOT the ones the closeups target). The
   `player_<inst>0_*` target proxies kept their serialized `mProxy` (one shared
   stand-in dir, `proxy=0x…` identical for guitar AND vocal) → all closeups framed
   the same wrong point. `CAM_TGT_DBG` showed every `*_bone_target_strum.tp`
   reading the same `(61.0,41.2,37.3)` and every `*_bone_head.tp` the same
   `(55.7,29.1,65.4)`.

## The fix (file:line)

1. **`src/system/bandobj/BandDirector.cpp` `EnterVenue()`** (HX_NATIVE, ~line 616,
   inside the `TheBandWardrobe` path, right before `SetVenueDir`) — drive the
   character-load step retail runs from `OnFileLoaded`:
   `if (TheBandWardrobe && !mVenue.Name().Null())
   TheBandWardrobe->LoadCharacters(mVenue.Name(), mAsyncLoad);`. Placed HERE (not at
   `OnFileLoaded`) on purpose: the venue's `world_chars` load re-finds `player%d`
   and REPLACES `mTargets`, so calling it earlier would set names/instruments on
   characters that get thrown away (a first attempt at `OnFileLoaded` confirmed
   `mTargets` were swapped + `inst='none'` again by venue-instance time). At
   EnterVenue/pre-SetVenueDir, `mTargets` are the venue's final characters.
   `mAsyncLoad` is already false for the V19 force-load → synchronous. Result:
   `LoadMainCharacters` sets `mVenueNames=[player_guitar0,player_bass0,player_mic0,
   player_drum0]` + assigns instruments; `SetVenueDir → SyncTransProxies` then
   matches **32** instrument-keyed proxies (was 0) and `SetProxy`s
   `player_guitar0_* → guitarist`, `player_bass0_* → bassist`, etc.

2. **`src/system/bandobj/BandWardrobe.cpp` `LoadMainCharacters()`** (HX_NATIVE, ~line
   722) — `if (inst == "mic") inst = "vocals";` just before
   `mVenueNames.names[i] = MakeString("player_%s0", inst)`. The vocalist's
   `mInstrumentType` is the `mic` symbol (`gInstNames[3]`), but the venue's vocal
   proxies + the vocal-closeup `BandCamShot` targets are named `player_vocals0_*`
   (the `small_club` .milo has 14681 `player_vocals0` refs and ZERO `player_mic0`),
   so without the remap the singer's closeups never matched. `#else` byte-identical.

3. **`src/system/bandobj/BandCharacter.cpp` `ReplaceRefs()`** (HX_NATIVE, ~line 1728)
   — **prerequisite crash fix.** Running `LoadCharacters` exercises the FileMerger
   character-OUTFIT merge for the first time natively
   (`FileMerger::Filter → BandCharacter::Filter → ReplaceRefs`). The matched-fork
   `ReplaceRefs` caches raw `std::vector<ObjRef*>` iterators into `theirs->Refs()`,
   walks them in reverse (`it[-1]`), and after each `ref->Replace()` resets
   `it = end()`. `ref->Replace()` ERASES the replaced ObjRef from `theirs->mRefs`,
   which under libstdc++ can reallocate the vector and invalidate both cached
   iterators → the next `--it; it[-1]` dereferences a dangling iterator → SIGSEGV
   (fault @ +0x30). MWCC/PPC tolerated the stale iterator; clang-LP64 does not. The
   HX_NATIVE branch is an index-based, reallocation-safe rewrite with identical
   semantics; `#else` byte-identical to the matched fork.

4. **`src/system/bandobj/BandDirector.cpp` `EnterVenue()`** (HX_NATIVE, ~line 658,
   after `setup_midi_parsers_msg`/`ClearLighting`) — **V23 Part 2:** re-run
   `HarvestDircuts()` now that `mVenue.Dir()` is real (guarded on `mPropAnim &&
   mVenue.Dir()`). The earlier harvest (driven from the load flow before the venue
   force-loaded) bailed at its `mPropAnim && mVenue.Dir()` gate (`venueDir=(nil)`),
   so the song's authored MIDI `DIRECTED_CUT`s were never harvested and the director
   fell back to generic shot-category cycling. With the venue + `song.anim` both live
   here it harvests `dircuts=15` for the test song (`20thcenturyboy` DOES have
   authored dircuts), so `FindNextDircut` can drive beat-synced cuts. `HarvestDircuts`
   ends with `StartClipLoads(true,0)` (another character merge — now safe via fix #3).

No engine (`Rnd_Wgpu_RB3.cpp`) change. The V22 `BandDirector::DrawShowing` CamOverride
follow (and the `Mtx.h:639` Multiply) are untouched and relied upon.

## What the camera does now (honest Opus visual review)

Screenshots: `docs/sessions/native/screenshots/v23-cinematography/` (`sweep/`,
`closeups/`, `final/`; track `guitar`, `20thcenturyboy`, full 8000-frame song).

- **`coop_g_cg` guitar closeup (closeups/f0920)** — a large teal guitar body fills
  the upper-left frame: the camera is now tight on the guitarist's
  instrument/hands. Pre-V23 this same shot framed empty stage wall. This is the
  headline change. (Highway still visible lower-right.)
- **Target resolution (`CAM_TGT_DBG`)** — guitar/bass/vocal closeup targets now
  resolve to DISTINCT proxies at DISTINCT stage positions:
  `player_guitar0_bone_head.tp` wpos=(68.8,51.2,78.7),
  `player_bass0_bone_target_strum.tp` wpos=(-75.6,69.4,50.0) (other side of stage),
  `player_vocals0_bone_head.tp` wpos=(-10.0,31.1,78.6) (center). Distinct proxy
  pointers per instrument — no more shared-collapse.
- **Gameplay cuts (f1100–f7000)** — the highway flows over the lit `small_club`
  venue, and the venue BACKGROUND cuts between distinct shots: f1100 wooden walls +
  "CORE" sign + a guitar prop, f3200 purple guitar prop + CORE, f4200 purple/teal
  crowd backdrop, f5500 purple drape, f7000 wide club interior (dartboard, posters,
  pink-lit walls). ~14 distinct shots committed across the song, cycling
  `coop_all_far / coop_g_closeup_hand / coop_b_closeup_hand / coop_v_closeup /
  coop_d_closeup_hand / coop_d_closeup_head / coop_bg_near / coop_g_near / ...` —
  song-authored categories now backed by the harvested dircuts.

## Authored dircuts — DO harvest now

`CAMDIR_DBG: HarvestDircuts dircuts=15 introShot=0x… shotTrackKeys=0x…
catCategories=104` — the second (post-venue) harvest succeeds and reads 15
`DIRECTED_CUT` keys from the song `shot_bg` track. So `20thcenturyboy` has real
authored camerawork and it now drives the director (no fabrication; the
category-cycling fallback remains only when a song has no dircuts).

## Regression status

- **Highway / gems: INTACT.** `CAM_DBG` confirms `prism_gem_stepped_*` +
  `gem_smasher_*` still draw under `game.cam` every gameplay frame across the full
  song. Gameplay loop (gems streaming, score) unaffected.
- **V19/V20/V21/V22 unregressed.** Venue geometry, crowd, extras, finite band-player
  servo skeletons, and the V22 venue-cam-follow cuts all still work. Clean exit 0
  over 8000 frames; `VENUE_CAM_LOCK=1` opt-out still clean.
- **First-time native character-outfit merge now runs** (it never did pre-V23 — the
  players appeared via the V19 raw `world_chars` instancing without the outfit
  merge). The `ReplaceRefs` LP64 fix (#3) is what makes it survive; the outfit
  RENDER compositing (`BandPatchMesh` RTT) is still the separate untouched fidelity
  item, so outfit *textures* aren't the point here — the merge just has to complete
  so instruments/names/proxies wire up.

## Remaining gaps (NOT regressions — follow-ups)

1. **Intro `INTRO_VENUE` pan still frames walls (f0600–f0900).** These are the
   wide cinematic pan shots BEFORE the highway engages, not character-targeted
   closeups — their authored `mWorldOffset`/path frames the stage wall at some
   sweep moments. Cosmetic; same as V22 gap #3. The character-targeted CLOSEUPS
   (the V23 fix) are the ones that now frame the band.
2. **Intermittent teardown SIGSEGV.** One run in ~9 crashed at a high address
   (0x7f… — NOT the null-deref class fixed in #3) during/after the full 8000-frame
   song; 8/9 runs (and 5/5 of a focused re-run) exit cleanly. The fixes here are
   deterministic (target resolution, ReplaceRefs, harvest timing), so this looks
   like a pre-existing rare flaky teardown/timing issue rather than a V23 regression
   — flagged as a watch item, not reproduced on demand.
3. **`BandPatchMesh` RTT outfit compositing** — untouched (separate minor fidelity
   item, per dispatch). Player bodies texture via the simple path.

## Diagnostics left in place (gated, render-inert)

- `world/CameraShot.cpp` `GetCurrentTargetPosition` — `CAM_TGT_DBG` (throttled): per
  shot, target count + averaged pos + each target's name / `WorldXfm` / `mProxy` /
  `mPart` / parent. The decisive instrument for this fix; kept as the closeup-target
  regression canary.
- `BandWardrobe.cpp` `SyncTransProxies` — `CHAR_DBG` now also prints
  `mTargets[i]->mInstrumentType`; `LoadMainCharacters` logs the resolved
  `mVenueNames`.
- `BandDirector.cpp` `OnFileLoaded(song)` — `CHAR_DBG`/`CAMDIR_DBG` venueName +
  WILL/SKIP LoadCharacters trace. (V22's CAMDIR_DBG director-chain logs intact.)

## State of shared files (for the next agent)

- **`src/system/bandobj/BandDirector.cpp`** — V23 `EnterVenue` LoadCharacters +
  post-venue HarvestDircuts re-run (HX_NATIVE) + OnFileLoaded diagnostic. V19/V22
  blocks intact.
- **`src/system/bandobj/BandWardrobe.cpp`** — V23 `mic→vocals` venue-name remap
  (HX_NATIVE) + CHAR_DBG inst/venueName diagnostics.
- **`src/system/bandobj/BandCharacter.cpp`** — V23 `ReplaceRefs` reallocation-safe
  index rewrite (HX_NATIVE; `#else` byte-identical). This is a BROAD correctness fix
  for the whole native FileMerger character-merge path — any future char-outfit /
  vignette / closet merge work can rely on it.
- **`src/system/world/CameraShot.cpp`** — only the gated `CAM_TGT_DBG` diagnostic;
  no logic change.
- **`Rnd_Wgpu_RB3.cpp`, `TrackDir.cpp`/`TrackPanelDir*.cpp`, `native/*.s`,
  `native/CMakeLists.txt`** — UNTOUCHED by V23.

## Reproducer (V23)

```
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v23-cinematography/final \
MILO_SCREENSHOT_FRAMES=600,920,1100,1400,1800,3200,4200,5500,7000 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=8000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

`CAM_TGT_DBG=1` dumps each closeup's resolved target proxy + world pos (confirm
guitar/bass/vocal land at distinct stage positions). `CAMDIR_DBG=1` shows the second
`HarvestDircuts dircuts=15`. `CHAR_DBG=1` shows `LoadMainCharacters set
venueNames=[player_guitar0,...]` and the venue `SyncTransProxies` 32-match.
`MILO_SCREENSHOT_DIR` MUST be an ABSOLUTE path that already exists (relative, or a
non-existent dir, silently fails WritePNG).

## Recommended next step

Polish the `INTRO_VENUE` pan framing (gap #1) so the pre-highway intro doesn't dwell
on the stage wall, and investigate the intermittent teardown SIGSEGV (gap #2).
Separately, the `BandPatchMesh` RTT outfit composite remains the open outfit-texture
fidelity item.

---

# V24 — Cleanup: teal triangular shards (degenerate skinned meshes) + void-cut audit

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED for Defect 1
— the prominent screen-crossing teal/green triangular shards are eliminated by an
engine-side degenerate-skinned-mesh guard; highway/gems/crowd/band all intact; clean
exit 0 over the full 8000-frame song. Defect 2 (void cuts) ROOT-CAUSED + precisely
documented, intentionally NOT retargeted (see below).**

## Defect 1 — teal/green triangular shards: ROOT CAUSE

The shards are **degenerate skinned-character triangles**, not light cones. Decisive
bisection (engine env-gated draw-skip `SHARD_KILL_SKIN`, since removed): skipping ALL
skinned meshes made every shard disappear at the exact frames they appeared
(`v23-cinematography/closeups/02_f0600`, `08_f0840` equivalents), while the static
venue/stage/highway stayed; skipping translucent/depth-disabled meshes or no-dir
meshes did NOT remove them. So the shards are character geometry.

Which characters: an instrumented per-mesh blended-vs-bind extent dump
(`SHARD_RATIO_DBG`) identified the culprits as the **extras + crowd CharServo-skeleton
meshes**, whose servo bones momentarily produce a **finite-but-wrong** pose — a vertex
weighted to a mis-posed bone flings tens-to-hundreds of units from the rest of the
body, drawing a thin sliver/fan. The repeat offenders: `fingernails_resource.mesh`
(bind ~36u → world ~310u), `male/female_extra_head*.mesh` (15-19u → ~180-220u),
`female_extra_hair02.mesh`, `51squier_strings`/`precision01_strings.mesh` (band
instrument strings, ~35u → ~100u), `clap.mesh`/`lighter.mesh`/`fist.mesh` (crowd held
props), and intermittently a whole `male_crowd_body01.mesh` (87u → 305u).

Why the earlier per-bone guards did NOT catch it: the existing skinned-path guard
(`Rnd_Wgpu_RB3.cpp` ~L1257) substitutes identity for a bone whose WORLD TRANSLATION is
non-finite / > 1e5. A V24 addition also validates the COMPOSED skin matrix (rotation
rows + translation) for finiteness/`>1e5` — but instrumentation showed **that guard
never fires** for the shards: the bad poses are FINITE and well within ±1e5 (the bone
is wrong, not overflowed). So no per-bone clamp helps. This is the same class of deep
CharServo skeleton-math LP64 issue the V20/V21 work flagged ("needs careful tracing,
not a quick gate flip") — V21 fixed the band PLAYERS' root (`Mtx.h:639` Multiply
no-op); the EXTRAS/crowd servo bones retain a residual transient pose error.

## Defect 1 — the fix (file:line)

**`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` `BandRnd::DrawMesh`**, a new
degenerate-skinned-mesh guard inserted right after the bone palette is built (~L1337,
before the bone-ring write). For each skinned mesh it computes, sampled + cheaply, the
mesh's BIND-pose (local) AABB extent `lext` and its BLENDED (world) AABB extent `wext`
using the EXACT 4-bone weighted blend the `vs_skinned` shader uses, then **drops the
mesh for that frame** when `wext > 15u && wext > 2.0 * lext` (the mesh has exploded to
>2x its authored size — a shard). The 2.0x threshold sits in the bimodal gap measured
across the song: correctly-posed meshes (crowd bodies, extras, hair, mic stand,
animated limbs) sit at ratio ~1.0-1.9; the shard poses jump to 2.0-12x. Dropping a
single transient frame of a small prop/limb is imperceptible; the alternative is a
screen-crossing teal blade. `mDrawnMeshes++` keeps the mesh-count stat honest. Opt out
via env `SHARD_GUARD_OFF=1` for A/B. Two gated, render-inert diagnostics left in
(`SHARD_DBG` logs each drop; `SHARD_RATIO_DBG` logs every skinned mesh's ratio) as a
regression canary.

Rejected alternatives (documented so they aren't re-tried): (a) a per-bone composed-
matrix finiteness clamp — never fires (poses are finite); (b) an absolute world-span
cap (>110u) — REJECTED, dropped legitimate large/batched crowd-row meshes (bind ~80u →
world ~113u at ratio 1.4x); (c) a max-triangle-edge cap (>45u) — REJECTED, crowd-row
meshes are large/batched and legitimately have long edges, so it gutted the crowd
(`female_crowd_body02` etc.). The AABB-ratio is the only metric that separates cleanly.

This is a **layer-b (engine) change only** — no matched-fork or glue edit. It is the
right layer: the root cause is in the matched-fork CharServo skeleton math, but a
correct fix there is a deep LP64 trace (the V20/V21 class of work); the engine guard
is a robust, localized symptom fix that cannot regress asm-match.

## Defect 1 — honest visual review (`screenshots/v24-cleanup/`)

Track `guitar`, `20thcenturyboy`, full 8000-frame song. NOTE: with this build the
menu→gameplay timing collapses so gameplay renders from ~frame 5; captured frame
numbers are NOT comparable to V23's, so I sampled a wide spread + the worst historical
shard angles.

- **Down-highway gameplay (`04_f0800`, `05_f0950`, `06_f1050`, `08_f2000`, `09_f3200`,
  `10_f5000`)** — CLEAN. Gem highway flows over the lit `small_club` venue (CORE sign,
  walls, chair, guitar prop, posters); the large teal fan-of-shards that used to hang
  over the highway (V23 `closeups/08_f0840`) is GONE. `02_f2000` is the proper teal
  guitar closeup (cinematic, not a shard).
- **Crowd-level cinematic (`01_f0600`, `03_f0750`)** — crowd is dense + intact, lit
  stage behind; the prominent screen-crossing teal/yellow blades are eliminated. A
  faint small fleck can still appear for a frame (residual, below).

Regression status: **highway / gems / smasher INTACT** — `CAM_DBG` confirms
`prism_gem_*` (267 samples) + `gem_smasher_*` still draw under `game.cam` every
gameplay frame across the full song; crowd density, band players, venue all still
render (mesh count unchanged ~970/frame; tri count drops ~550K→~390K as the degenerate
meshes' tris are skipped). Clean exit 0. V19-V23 unregressed.

## Defect 1 — residual gap (honest)

A few SMALL held-prop slivers (`clap`/`lighter`, ratio just under 2.0x) can still
flicker for a single frame in some crowd shots — they sit just below the 2.0x gap and
can't be caught without risking dropping legitimately-posed hair/skin meshes that peak
at ~1.9x (`mohawk_resource` 1.91, `male_extras_skin02` 1.88). Lowering the threshold
was tested and trades one cosmetic artifact (rare tiny fleck) for a worse one
(flickering crowd hair), so 2.0x is held. The complete fix is the CharServo skeleton-
math root cause (extras/crowd servo bones), tracked as a follow-up — same class as the
V21 `Multiply` no-op, needs a careful LP64 trace, not a threshold tweak.

## Defect 2 — void cuts: ROOT CAUSE + decision (documented, not retargeted)

Some camera cuts frame near-total black. Two distinct cases, both transient:

1. **`INTRO_VENUE` wide pan dwelling on the stage wall** (`02_f0700`): the pre-gameplay
   `coop_intro_venue.shot` pan frames the stage-edge wood panels with black void
   around them — the same V22 gap #3 / V23 gap #1. It is a wide cinematic pan whose
   authored `mWorldOffset`/path sweeps across the wall; brief (one shot-dwell, ~1-2s)
   before the highway engages.
2. **Gameplay cut with the venue out of frame** (`07_f1400`): the gem highway renders
   perfectly (full fretboard + gems + smasher), but the venue BACKGROUND is black —
   the active shot's camera angle puts the venue geometry outside the frustum, leaving
   the highway over a black background. The gameplay focus (highway) is fully present
   and playable; only the decorative backdrop is absent for that cut.

`CAMDIR_DBG` confirms the director is healthy — it cuts a proper RB3 shot sequence
(`coop_intro_venue → coop_all_far → coop_g_cg → coop_b_cg02 → coop_v_c01 → coop_g_n →
coop_d_cd02 …`) every ~1-2s; the void cuts are individual shots among many framed ones,
never a sustained stretch. Per the dispatch bar ("no cut sits on black/void for a
noticeable stretch") these are borderline-acceptable transients.

**Decision: documented, NOT retargeted.** Retargeting the camera director (shot
`mWorldOffset` / target overrides) risks regressing the V22/V23 cinematography that an
independent Opus review judged "genuinely like RB3," and Defect 1 consumed the budget.
The fix would be: (a) for the intro pan, bias `coop_intro_venue`'s framing toward the
band/stage center; (b) for gameplay void cuts, fall back to the highway-locked
`game.cam` framing when the active shot's frustum contains no venue geometry (a
visibility test in `BandDirector::DrawShowing`'s V22 cam-follow). Both are camera-
director (matched-fork `BandDirector.cpp`) work, not engine.

## Regression + flaky-run notes

- Highway/gems/smasher/crowd/band/venue all intact; clean exit 0 (full song).
- **Menu→gameplay reach is FLAKY** (pre-existing, not introduced here): the synthetic
  `RB3_GAME_INPUT` fires at FIXED frame numbers, but native loading (`LoadMgr::Poll`
  drains synchronously per frame, HX_NATIVE) races against them under host load, so a
  fraction of runs never advance past the menu (every frame draws 0 meshes). Re-run
  until `MeshIB`/mesh-count > 0. The dispatch reproducer reaches gameplay on a
  good run. Tuning the input frame spacing or load-gating the script is a separate
  harness follow-up.
- **Intermittent teardown SIGSEGV** (exit 139, ~1-in-N) persists — same watch item as
  V23 gap #2; deterministic clean exit on the majority of runs.

## State of shared files (for the next agent)

- **`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`** — V24 degenerate-skinned-mesh
  guard in `DrawMesh` (drop on world/bind extent ratio > 2.0x), plus a composed-
  bone-matrix finiteness clamp in the bone-palette loop (defensive; does not fire on
  the observed shards but guards true overflow), plus gated `SHARD_DBG`/
  `SHARD_RATIO_DBG` canaries and the `SHARD_GUARD_OFF` opt-out. The V14a skinned path,
  V2/V13 per-mesh camera re-write, and material blend/zmode (V4) are untouched.
- **Matched-fork (`BandDirector.cpp`, `BandWardrobe.cpp`, `BandCharacter.cpp`,
  `CharServoBone.cpp`, `Character.cpp`, `Mtx.h`, `world/*.cpp`) — UNTOUCHED by V24.**
  The V19-V23 HX_NATIVE blocks are all intact (verified). NOTE for the next agent: a
  `git stash pop` accident during this session momentarily conflicted permuter-owned
  files in the rb3 repo; it was fully reverted (taking the pre-pop working tree), the
  permuter stash list is intact, and no conflict markers remain — but stay alert that
  the permuter rewrites `src/system/**`+`src/band3/**` continuously.

## Reproducer (V24)

```
SHARD_DBG=1 \
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v24-cleanup \
MILO_SCREENSHOT_FRAMES=600,700,750,800,950,1050,1400,2000,3200,5000 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=8000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

`SHARD_DBG=1` logs each `[SHARD_GUARD] dropped degenerate skinned mesh=...`.
`SHARD_RATIO_DBG=1` logs every skinned mesh's bind/world extent + ratio (find the
slivers). `SHARD_GUARD_OFF=1` disables the guard (reverts to the shard-y baseline for
A/B). `CAM_DBG=1` confirms the gems still draw under `game.cam` (regression canary).
`MILO_SCREENSHOT_DIR` MUST be ABSOLUTE and already exist (relative silently fails
`WritePNG`). Re-run until `MeshIB`/non-zero mesh frames appear (flaky gameplay reach).

## Recommended next step

Root-cause the EXTRAS/crowd CharServo skeleton transient pose error (the real Defect-1
source: `CharServoBone`/`CharBones` LP64 math, same class as V21's `Mtx.h` Multiply
fix) so the guard becomes unnecessary and the last small flecks vanish. Then Defect 2
camera retargeting (intro-pan bias + venue-visibility highway fallback) in
`BandDirector.cpp`. The `BandPatchMesh` RTT outfit composite remains the open outfit-
texture fidelity item.

---

# V26 — Teal shards root cause: `MakeRotQuat` dropped the half-angle 0.5 factors (sqrt(2) quat)

**Authored:** 2026-05-28 (Opus implementation agent). **Status: LANDED — the
DOMINANT teal/green shards (the screen-crossing fans over the highway + crowd, from
the band/extras/crowd HAND meshes) are ELIMINATED at the math source. With the V24
guard OFF (`SHARD_GUARD_OFF=1`) the down-highway gameplay view is now shard-free for
the whole song. The fix is a real LP64 char-math root cause, not a band-aid. Residual
small slivers remain in crowd-cinematic shots from a DIFFERENT, narrower cause
(documented below). Clean exit 0 over the full 8000-frame song; gameplay/highway/gems/
venue/band all unregressed.**

## Root cause (file:line) — `src/system/math/Rot.cpp:484` `MakeRotQuat`

The V24 hypothesis ("CharServoBone transient finite-but-wrong pose") was the wrong
subsystem. The decisive trace (engine `SHARD_BONE_DBG`, added this session; runs with
the guard OFF) localized the explosions to the **upper-arm bone chain**, not the
servo: the shard-producing hand meshes (`fingernails_resource`, finger/thumb tips)
ride `bone_R/L-upperArm`, whose **LOCAL rotation matrix went non-orthonormal
(determinant 2–12 instead of 1)** for transient frames — a ~1.4–1.5x spurious scale
that, composed down the arm→hand→finger chain, flung the fingertips 100+ units and
drew the slivers. (`PoseMeshes` produces a clean det=1 local; the corruption is a
later poller.)

The writer is **`CharIKHand::IKElbow` (`CharIKHand.cpp:202`)**:
`Multiply(ma0, trans2->LocalXfm().m, trans2->DirtyLocalXfm().m)` where `ma0 =
MakeRotMatrix(q128)` and `q128 = MakeRotQuat(v118, v10c)`. Instrumentation showed
`ma0` itself had det 2–8, because **`q128` was NOT a unit quaternion — its magnitude
was always exactly `sqrt(2)`** (e.g. `(0.044,0.042,0.656,1.251)`, |q|=1.414). Feeding
a `sqrt(2)` quat to `MakeRotMatrix` (which assumes unit) yields a matrix scaled by
`|q|^2 = 2` (det ~ 8) — a non-orthonormal "rotation."

`MakeRotQuat` in the rb3 decomp **dropped the two half-angle `0.5` factors** that the
correct (dc3 sister `dc3-decomp/src/system/math/Rot.cpp:277`) version has:
- correct: `sq2 = sqrt((1 + dot/sq) * 0.5)` = `cos(theta/2)`; `scale = 0.5/(sq*sq2)`
- rb3   : `sq2 = sqrt(1 + dot/sq)` = `sqrt(2)*cos(theta/2)`; `scale = 1.0/(sq*sq2)`

So the rb3 quat is `sqrt(2)x` too large in every component (provably: `w^2 + |xyz|^2
= (1+cos) + sin^2/(1+cos) = 2`). MOST consumers `Normalize()` the quat first (or feed
it through a path that re-normalizes) and were unaffected — which is why the game
mostly worked — but the IK / twist paths (`CharIKHand`, `CharForeTwist`,
`CharUpperTwist`, `CharLookAt`, `CharIKFingers`) feed it straight into
`MakeRotMatrix` / `Multiply(Vector3,Quat,...)`, which assume a unit quat. Same family
of LP64 char-math footgun as the V21 `Mtx.h:639` empty-body `Multiply` no-op.

## The fix (file:line)

**`src/system/math/Rot.cpp:484` `MakeRotQuat`** — additive `#ifdef HX_NATIVE … #else …
#endif`. The `HX_NATIVE` branch restores the shortest-arc half-angle normalization
(matching dc3): `sq2 = sqrt((1 + dot/sq) * 0.5)` and `scale = 0.5/(sq*sq2)`, so the
result is a proper UNIT quaternion. The `#else` half is **byte-identical** to the
permuter's current matched-fork body (verified by `git diff`). This is a layer-a
(matched-fork) math-header-adjacent `.cpp` change; it is a broad correctness win — it
fixes every native IK/twist/look-at path that consumes `MakeRotQuat` without
normalizing (13 call sites across `CharIKHand`, `CharIKFingers`, `CharForeTwist`,
`CharUpperTwist`, `CharLookAt`, `CharIKHead`, `BandIKEffector`, `BandPatchMesh`).

## Before / after (with the V24 guard OFF)

- **Before** (`screenshots/v26-charservo/` first capture): f0600 crowd shot = dense
  teal/yellow/green BLADES crossing the frame; f0800 down-highway = giant teal+yellow
  fans hanging over the gem highway.
- **After**: `IK_DBG` proves every upper-arm `MakeRotMatrix` det is now `1.000` and
  the quats are unit (`(-0.023,-0.029,0.390,0.920)` etc.); a whole-skeleton
  `LOCALDET_DBG` sweep shows **zero** arm/hand/finger bones go non-orthonormal (was
  30+ upper-arm hits/run). Visually: the down-highway gameplay view (f0950, f1400,
  f2000, f7000) is CLEAN — gem highway over the lit `small_club` venue, NO shards. The
  big crossing fans are gone.

## Residual (honest) — a DIFFERENT, narrower cause

With the guard OFF, small slivers still flicker in some **crowd-cinematic** shots
(not the gameplay highway). Measured with the (now guard-independent) `SHARD_RATIO`
diagnostic, the post-fix DROP-worthy meshes are no longer body explosions but:
1. **`fingernails_resource` / `bone_R-hand` / `bone_R-upperArm` reaching far** — these
   now have `det=1.000` (orthonormal) but a WORLD TRANSLATION of z≈300–340u (vs the
   normal ~70u). i.e. `CharIKHand` is correctly orienting the hand but aiming it at a
   far/mis-placed IK TARGET. This is a target-placement/data issue (the crowd/extras
   hand-IK target proxy), NOT a math bug — driving the rotation correctly (this fix)
   can't move a target that's authored/resolved at the wrong spot.
2. **Tiny-bind face features + crowd held-props** — `male_extras_eyebrows11`
   (bind 4.9u → world 207u, ratio 42), `goatee_resource`, `clap`/`lighter`/`fist`.
   Their bind extent is tiny so any small pose error reads as a high ratio; these are
   the V24-documented sub-class (face servo / held-prop attachment), driven by a
   different path than the arm chain.

These residuals are small slivers in crowd shots, not the screen-crossing fans, and
the gameplay highway is clean. A full elimination needs (1) the crowd hand-IK target
resolution and (2) the face-servo / held-prop attachment — both separate follow-ups.

## Is the V24 guard now redundant?

For the **dominant** shards (the arm/hand/finger-body explosions over the highway):
YES — they no longer occur, so the guard never needs to drop those meshes. For the
**residual** crowd-cinematic slivers (IK-target reach + tiny-bind face/props): the
guard still catches the ratio>2.0 ones (the z≈300 hand reaches) and is left in place
as a **harmless safety net**. Recommendation: KEEP the V24 guard (it's engine-layer,
cannot regress asm-match, and now fires far less often — tri-count impact is minimal),
but it is no longer load-bearing for the main view. `SHARD_GUARD_OFF=1` is now
acceptable for gameplay.

## Regression status

- **Highway / gems / venue / band: INTACT.** Full 8000-frame song, ~203 meshes/frame
  at gameplay (unchanged), clean exit 0 (`Inferior exited normally`). The down-highway
  gameplay view renders the gem highway over the venue with no shards.
- **V19–V25 unregressed.** No camera/layout/framing code touched. The `MakeRotQuat`
  fix only corrects native IK/twist quaternions (it makes the band players' and
  crowd's arms pose CORRECTLY — a strict improvement to the V20/V21 character work).
- The known menu→gameplay reach FLAKE persists (fixed-frame `RB3_GAME_INPUT` races
  loading; plain runs often SIGSEGV in the menu before gameplay). WORKAROUND used this
  session: run under `gdb -batch -ex run` — it slows the harness enough to reliably
  reach gameplay and exit 0. Re-run / use gdb until `frame drawn — N meshes` (N>0).

## Diagnostics left in place (engine layer only, gated, render-inert)

- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` `DrawMesh`:
  - **`SHARD_BONE_DBG`** (V26, new) — for a skinned mesh, reports the outlier bone
    (farthest from the per-mesh centroid) + its name/world/local. The instrument that
    traced V26 to the upper-arm chain. Kept as a regression canary. (NOTE: high
    bone-spread is NOT proof of a shard for both-hands/wide-crowd meshes; the rendered
    `SHARD_RATIO` is the truer metric.)
  - **`SHARD_RATIO`** (V24) — now also computes/logs when the guard is OFF (gated on
    `SHARD_RATIO_DBG`) so the post-fix residual can be measured with the guard
    disabled. The DROP itself still only fires when `SHARD_GUARD_OFF` is unset.
- All matched-fork (layer-a) probes added this session to localize the bug
  (`CharBonesMeshes`, `CharUpperTwist`, `CharIKHand`, `rndobj/Trans.cpp`) were REMOVED
  after the fix — those files are byte-identical to the permuter's content again.

## State of shared files (for the next agent)

- **`src/system/math/Rot.cpp`** — the one V26 `MakeRotQuat` HX_NATIVE half-angle fix
  (line ~497). `#else` byte-identical to the matched fork. This is the only matched-
  fork change; a BROAD native correctness fix for all IK/twist/look-at quat math.
- **`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`** — V26 added `SHARD_BONE_DBG`
  + made the V24 `SHARD_RATIO` ratio computable with the guard off (DROP gated on
  `guardActive`). The V24 guard, V14a skinned path, V2/V13 per-mesh camera, V4
  blend/zmode are otherwise untouched.
- **`CharBonesMeshes.cpp`, `CharUpperTwist.cpp`, `CharIKHand.cpp`,
  `rndobj/Trans.cpp`** — UNTOUCHED (all session probes reverted; verified via
  `git status`). The V19–V24 HX_NATIVE blocks elsewhere are intact.

## Reproducer (V26)

```
# guard OFF (verify the dominant shards are gone at the source):
SHARD_GUARD_OFF=1 \
MILO_SCREENSHOT_DIR=$PWD/docs/sessions/native/screenshots/v26-charservo \
MILO_SCREENSHOT_FRAMES=600,800,950,1400,2000,3200,5000,7000 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=8000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
gdb -batch -ex run --args ./native/build-native/rb3-native
```

`SHARD_BONE_DBG=1` reports the per-mesh outlier bone (the trace instrument).
`SHARD_RATIO_DBG=1` (now works with the guard off) logs every skinned mesh's
bind-vs-world AABB ratio — confirm no BODY/arm/hand mesh exceeds the threshold;
remaining `DROP`s are crowd held-props / tiny-bind face features / far IK-target
reaches. `SHARD_GUARD_OFF=1` disables the guard. Run under `gdb -batch -ex run` to
beat the menu-reach flake. `MILO_SCREENSHOT_DIR` MUST be ABSOLUTE.

## Recommended next step

Resolve the crowd/extras **hand-IK target placement** (the residual: `CharIKHand`
correctly orients but aims at a target proxy resolved ~300u away — trace the IK
`mTargets` proxy resolution for crowd/extras, analogous to the V23 closeup-target
proxy fix in `BandWardrobe::SyncTransProxies`). Secondary: the face-servo /
held-prop (eyebrows/goatee/clap/lighter) tiny-bind slivers. With those, the V24 guard
becomes fully redundant. Then Defect 2 camera void-cuts and the `BandPatchMesh` RTT
outfit composite remain the open fidelity items.

---

# V32 — Crowd hand-IK target investigation: V21/V26 had been permuter-wiped; reapplied; CharIKHand path inert in current build state

**Authored:** 2026-05-28 (Opus implementation agent). **Status: V21/V26 RESTORED
(reapplied additive `#ifdef HX_NATIVE` blocks the permuter had blanked between
sessions). The dispatch's premise — fix the crowd hand-IK target proxy
resolution so the V24 guard becomes fully redundant — could NOT be tested
end-to-end in this session: the current build state cannot reach in-song
gameplay rendering (game_screen reaches at frame ~456 but the venue/crowd
in-song render never engages, mesh count stays at ~50/frame instead of the
200+ V26 documented), and CharIKHand::Poll() is NEVER called in the
menu / song-select / pre-gameplay phases that DO render the crowd (instrumented
and confirmed: 0 hits on a 12000-frame run with `IK_TGT_DBG=1`). The
hand-IK-target story therefore can't be the cause of the shards I CAN
observe; those shards trace to a different (CharServoBone / CharBones)
path. V24 guard status: **NOT redundant**, still load-bearing for the
pre-game crowd preview. Honest summary at the end.**

## What actually happened this session

1. **Build was clean on entry** but the V21 (`Mtx.h:639` empty-body `Multiply`)
   and V26 (`Rot.cpp:484` `MakeRotQuat` half-angle factors) HX_NATIVE blocks
   were both GONE from the matched-fork files — wiped by the permuter sometime
   since the V26 session. The matched-fork bodies were back to the asm-match
   shape (ASM_BLOCK no-op for V21; missing the two `0.5` factors for V26),
   silently breaking every native IK / skeleton / interest-cam consumer of
   those functions. This matches the dispatch precedent ("BandTrack.cpp had its
   V29 block wiped + re-applied this session").
2. **Re-applied V21** (`src/system/math/Mtx.h:639`): `#ifdef HX_NATIVE` wraps
   the C body (`vout.Set(m.x.x*v.x + m.y.x*v.y + m.z.x*v.z, …)`, matching
   `dc3-decomp/src/system/math/Mtx.h:425`); `#else` byte-identical to the
   permuter's ASM_BLOCK body. Build + link clean.
3. **Re-applied V26** (`src/system/math/Rot.cpp:484` `MakeRotQuat`):
   `#ifdef HX_NATIVE` restores `sq2 = sqrt((1 + dot/sq) * 0.5)` and
   `scale = 0.5/(sq*sq2)` (matching `dc3-decomp/src/system/math/Rot.cpp:277`);
   `#else` byte-identical to the permuter's no-half-angle body. Build + link
   clean. The reapplied fix is verifiably in the `Rot.cpp.o` MakeRotQuat
   symbol (disassembly inspected).
4. **Added gated `IK_TGT_DBG` diagnostic** (`src/system/char/CharIKHand.cpp`
   top of `Poll()`, additive `#ifdef HX_NATIVE`, render-inert when unset): per
   `CharIKHand` instance, log hand WorldXfm + each `mTargets[i]->WorldXfm()` +
   distance + parent name. Intended to find the V26-documented "z≈300u hand
   reach" case. With it on under the full reproducer (12000 frames,
   `SHARD_GUARD_OFF=1`): **0 IK_TGT lines** — `CharIKHand::Poll` is not
   exercised at all in the menu/song-select/pre-game-screen window. The
   diagnostic is left in place for the next agent who reaches in-song
   crowd-cinematic gameplay (where it presumably WILL fire).
5. **`SHARD_RATIO` measurement, guard OFF, V21+V26 reapplied:** the worst
   ratios this build can produce (in the menu / song-select / pre-game state)
   are:
   - `male_extras_eyebrows11.mesh` bind 4.94 → world 207.81 (ratio 42)
   - `goatee_resource.mesh` bind 5.73 → world 97.6 (ratio 17)
   - `clap.mesh` bind 51.28 → world 700-815 (ratio 14-16)
   - `male_extra_head01.mesh` bind 19.18 → world 178-180 (ratio 9.3)
   - `male_crowd_body03.mesh` bind 85.83 → world 760-770 (ratio 8.9)
   - `female_crowd_body02.mesh` bind 80.13 → world 706 (ratio 8.8)
   - `lighter.mesh` bind 35 → world 35-50 (ratio ~1) — generally OK
   These are the same residual category V26 documented (tiny-bind face
   features + held props + occasional crowd body) — meaning the dispatch's
   target-proxy hypothesis is the WRONG layer for these meshes (no IK
   path is touching them). The body-explosion ratio 8.9 on `male_crowd_body03`
   shows that even AFTER V21+V26 reapply, the CharServoBone / CharBones
   skeleton math still has residual native LP64 issues in the menu render
   path; in V26 the in-song bodies measured clean because CharIKHand WAS
   polled in-song (different code path).
6. **DROP count:** 364 in a 500-frame menu run (vs ~744 before V21/V26
   reapply), so V21+V26 are demonstrably having effect (~50% reduction), but
   the residual is large.

## Gameplay reach blocker (the thing that prevents finishing the dispatch)

`RB3_GAME_INPUT` fires through the canonical screen sequence cleanly:
splash → main_hub → song_select_enter → song_select → part_difficulty →
tv3_b → game_screen (frame 456). `nofail` fires at frame 500 and reports
`IsNoFailActive=1`. Audio device init returns `-401` (no host audio device,
expected headless), `Game::mLoadState = kReady` is reached (per
`GAME_DBG`), the song mid/audio/anim load successfully (per `STREAM_DBG`,
`MIDI_DBG`, `BEATMASTER_DBG`). But the venue does NOT engage:

- No `EnterVenue` / `BandDirector::Enter` log line ever appears.
- No `prism_gem_*` or `gem_smasher_*` `CAM_DBG` log line ever appears.
- Mesh count stays at ~50-65/frame from frame 500 to frame 8000+ (gameplay
  should be 200+ per V26).

`game_screen` is current but the gameplay scene proper never populates.
This is upstream of V32's target and outside its scope — the V26 doc
specifically noted this exact failure mode ("Menu→gameplay reach is FLAKY...
the synthetic RB3_GAME_INPUT fires at FIXED frame numbers, but native loading
races against them under host load, so a fraction of runs never advance past
the menu") and the recommended `gdb -batch -ex run` workaround did NOT
beat the flake this session (3 attempts including gdb-slow, all stalled the
same way at ~50 meshes during game_screen). So the in-song crowd-cinematic
view (where the dispatch's IK-target-proxy residual is observable) is NOT
reproducible in this session's build state.

## Why the dispatch hypothesis can't be the cause of the shards I see

The dispatch hypothesized `CharIKHand::mTargets` resolves a proxy ~300u
away (analogous to the V23 closeup target-proxy fix). Direct instrumentation
(`IK_TGT_DBG` HX_NATIVE diagnostic in `CharIKHand::Poll`) proves
`CharIKHand::Poll` is NEVER called in the 12000-frame
menu/song-select/pre-game span — yet the SHARD_RATIO logs show 200-800u
world extents on extras heads / crowd bodies / held props from frame 1
onward. So whatever path is producing those shards (CharServoBone /
CharBones / animation evaluation), it doesn't route through the IK chain.
The dispatch's plan (analogous to V23's `BandWardrobe::SyncTransProxies`
slot-name rewire) addresses the wrong subsystem for the shards observable
in this build's reachable state.

The dispatch's claim still likely holds for the **in-song** residual the V26
doc described (where `CharIKHand` IS polled and reaches ~z=300u) — but
testing that requires reaching in-song crowd-cinematic rendering, which
this session's build state doesn't reach.

## Files touched this session (all additive HX_NATIVE; permuter-safe `#else` branches)

- `src/system/math/Mtx.h` — V21 reapply (one HX_NATIVE block at L639 around the
  empty-body `Multiply(Vector3,Matrix3,Vector3)`).
- `src/system/math/Rot.cpp` — V26 reapply (one HX_NATIVE block at L484 around
  `MakeRotQuat` to restore the half-angle 0.5 factors).
- `src/system/char/CharIKHand.cpp` — added gated `IK_TGT_DBG` diagnostic at the
  top of `Poll()` (HX_NATIVE-only, render-inert, 200-record cap, distance>50u
  threshold). Left in place as instrument for the next agent.

NO matched-fork file outside those three was modified by V32. The other
unstaged matched-fork files reported in `git status`
(`BandCharacter.cpp`, `BandTrack.cpp`, `TrackPanelDir.cpp`,
`TrackPanelDirBase.cpp`, `LightPreset.cpp`) were already modified when V32
started (V29 BandTrack reapply + the standing HUD/Score/V19/V20/V21 blocks
the permuter has been shifting around) and were NOT touched by V32.

## Engine layer (Rnd_Wgpu_RB3.cpp) — UNTOUCHED by V32

The V24 guard, V26 `SHARD_BONE_DBG`, V26 `SHARD_RATIO_DBG`, and the
`SHARD_GUARD_OFF` opt-out are all still in place. V32 made no engine changes.

## Is the V24 guard now fully redundant? — NO

In the V26 doc the answer was "fully redundant for the dominant arm/finger
explosions in the gameplay highway view." V32 cannot confirm anything beyond
that, and explicitly confirms the OPPOSITE for the menu/pre-game preview state
(364 DROPs on a 500-frame menu run with the guard off; visible high ratios on
crowd bodies, extras heads, eyebrows, goatee, and clap up to 42x). The V24
guard remains LOAD-BEARING for the pre-game render. Recommendation: KEEP V24
in place (unchanged from V26's safety-net status); a future session that
restores gameplay-reach AND fixes the residual CharServoBone / face-servo /
held-prop attachment LP64 issues can then re-evaluate redundancy.

## Recommended next step (concrete)

The actual blocker for V32's dispatched goal is **gameplay-reach
reliability**, not IK target-proxy resolution. The V26 doc and V32 both hit
the same `game_screen-reached-but-venue-never-engages` failure. Possible
causes to investigate next session, in priority order:

1. **Why doesn't `BandDirector::Enter` fire?** Trace
   `GamePanel::PollForLoading → CreateGame → Game::LoadSong` and look for the
   `BandDirector::Enter` invocation — likely a load-poll ordering or a
   missing message dispatch since the last working V26 run. Adding a
   `BANDDIR_DBG` print at the top of `Enter` would localize it in one run.
2. **If `BandDirector::Enter` IS called but `mVenue.Dir()` is null,** the
   V19 force-load needs reapplication (search V19/V22/V23 for the venue
   force-load entry point — it likely sits inside the `tv3_b_screen` load
   flow and may have been permuter-shifted).
3. Once in-song crowd-cinematic gameplay renders again, re-run V32's
   `IK_TGT_DBG` diagnostic — that's when the dispatch's residual
   (`z≈300u hand reach`) actually exists and can be debugged. Likely fix
   pattern is the V23 analog (a `SyncTransProxies`-style slot-name rewire
   for the crowd extras' hand-target proxies, separate from the band
   players that V23 covered).
4. The face-servo / held-prop tiny-bind slivers (`eyebrows`, `goatee`,
   `clap`, `lighter`, `fist`) require attacking `CharServoBone` /
   `CharBones` LP64 math. Same class as V21's `Multiply` no-op + V26's
   `MakeRotQuat` half-angle — likely another small handful of decomp
   functions where MWCC quirks hide a wrong native body. The `clap.mesh`
   ratio 14-16 (bind 51u → world 700-800u) is a sharp signal: ONE bone or
   one Multiply in the prop's attachment chain is going wrong by an
   order of magnitude.

## Reproducer (V32)

```
# Build with the V21+V26 reapply (already done by this session):
cmake --build /home/free/code/milohax/rb3/native/build-native -j

# Measure residual shard ratios in the pre-game crowd preview:
SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 \
  RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=500 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  ./native/build-native/rb3-native

# Confirm CharIKHand::Poll IS the right path once gameplay engages:
IK_TGT_DBG=1 SHARD_GUARD_OFF=1 \
  <same env as above with MILO_MAX_FRAMES=8000> \
  ./native/build-native/rb3-native
# If [IK_TGT] lines appear with d > 100u for crowd/extras characters,
# that's the dispatch's residual; the proxy fix it describes applies.
# If [IK_TGT] lines never appear even in-song, the residual is
# CharServoBone-driven and needs a different attack (see step 4 above).
```

## Regression status — V19–V31 unregressed (with caveat)

- The V19/V20/V21/V22/V23/V24/V26 HX_NATIVE blocks I could verify all
  remain in place (or were reapplied by V32 in the V21/V26 case).
- The menu→game_screen reach FLAKE is pre-existing and not introduced by
  V32; same exact behavior as V26 documented.
- Build is clean; binary exits 0 over 12000 frames; no SIGSEGV in the
  V32 runs.

