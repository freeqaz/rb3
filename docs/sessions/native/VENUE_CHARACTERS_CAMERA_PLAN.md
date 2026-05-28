# Venue Band-Characters + Camera-Director — pre-plan (read-only)

**Authored:** 2026-05-28 (planning subagent, Opus, READ-ONLY — no build, no edits).
**Purpose:** pre-stage the next two venue increments so they can be dispatched the
moment the **stage GEOMETRY** agent (task #81 "V19", `world/vignette/` +
`world/shared/` proxy instancing) lands. Two increments:

- **A — band-character rendering + animation** during a song.
- **B — venue camera director** (camera cuts between shots during a song).

**Hard prerequisite for BOTH (the long pole the geometry agent owns):** the
cosmetic-venue proxy deferral in `WorldInstance::SyncDir`
(`src/system/world/Instance.cpp:351-358` `IsDeferredVenueProxy` → strstr
`world/vignette/` + `world/shared/`) and the `ObjectDir::PostLoadInlined`
inlined-cached-shared object-resolution gap it documents (`Instance.cpp:304-358`).
Until a real venue `WorldDir` instances and is Polled/Drawn, `BandDirector::mCurWorld`
stays null, no characters proxy into venue slots, and the `CameraManager` never runs.
**This plan assumes that fix lands first.** Everything below is the work that sits
*on top of* it — and most of it is already on the native link line and merely
gated behind `mCurWorld != null`.

---

## Architectural summary (verified by reading the matched fork)

The band, the crowd, the lighting, the post-processing, AND the camera director are
all owned by one object — `BandDirector` (`src/system/bandobj/BandDirector.{h,cpp}`,
singleton `TheBandDirector`) — which itself just wraps the venue `WorldDir`
(`mCurWorld`, set in `EnterVenue` `BandDirector.cpp:476-502`). The flow:

1. `BandDirector::LoadVenue(sym, pos)` (`BandDirector.cpp:1183`) → `GetVenuePath`
   builds `world/venue/<class>/<name>/<name>.milo` (`BandDirector.cpp:509-529`;
   `gVenues[5] = {arena, big_club, festival, small_club, video}` at `:22`) and
   `mVenue.Load()` (`VenueLoader::Load` `:44`) `DirLoader`s it into `mVenue.mDir`
   (a `WorldDir`). **The venue ROOT is under `world/venue/` — NOT covered by the
   `IsDeferredVenueProxy` gate** (which only defers `world/vignette/` + `world/shared/`).
   But the venue pulls its band/crowd/amps from `world/shared/` (verified:
   `world/shared/gen/world_chars.milo_xbox` exists in the extract) — so the venue's
   *shared sub-proxies* still hit the deferral. The geometry agent's fix is what
   un-defers them.
2. `BandDirector::EnterVenue` (`:476`) wires the venue dir as a child of the
   game's `WorldDir`, calls `dir->Enter()`, sets `mCurWorld = dir`, and sends
   `setup_midi_parsers_msg` to it (`:496`). This is where the MIDI-driven char
   animation parsers get attached.
3. `BandDirector::Poll` (`:236`) → `mCurWorld->Poll()` → (`WorldDir::Poll`
   `src/system/world/Dir.cpp:109`) which (a) sends `select_camera_msg` → handled by
   `BandDirector::OnSelectCamera` (`:1193`, advances `mPropAnim`, picks the next
   shot), (b) `mCameraManager.PrePoll()` (start queued shot) and
   `mCameraManager.Poll()` (animate active shot) — both gated by
   `WorldDir::mPollCamera`, (c) `RndDir::Poll()` which polls children including the
   `BandCharacter`s and their `CharDriver`/`CharDriverMidi`.
4. `BandDirector::DrawShowing` (`:310`) → `mCurWorld->DrawShowing()`
   (`WorldDir::DrawShowing` `Dir.cpp:360`) selects the current shot's camera
   (`cam2->Select()` `:378`) and draws the world scene-graph (characters + props +
   crowd) via `RndDir::DrawShowing`.

**Why this matters:** every system in both increments is *one object graph* hanging
off `mCurWorld`. There is no separate "band system" vs "camera system" to bring up —
they co-instantiate. This drives the concurrency conclusion below (A and B largely
**serialize** because they share the same `mCurWorld != null` precondition and the
same `BandDirector` tick path).

### Link-line reality check (important — most of this already compiles)

`native/CMakeLists.txt:242-243` globs the **entire** `src/system/bandobj/*.cpp`
(`ENGINE_BANDOBJ`) and `src/system/char/*.cpp` (`ENGINE_CHAR`); `:236` globs
`src/system/world/*.cpp` (`ENGINE_WORLD`). The `_NATIVE_FORK_EXCLUDE` list
(`CMakeLists.txt:307-313`) only drops, from these trees: **`BandPatchMesh`**
(bandobj) and **`ClipDistMap`** (char). So:

- `BandDirector.cpp`, `BandCharacter.cpp`, `BandWardrobe.cpp`, `BandCamShot.cpp`,
  `CameraManager.cpp`, `CameraShot.cpp`, `CharDriver.cpp`, `CharDriverMidi.cpp`,
  `CharClipDriver.cpp`, `CharClip.cpp`, `Character.cpp` are **ALL on the native
  link line, clang-compiled.**
- The weak stubs at `band3_link_stubs.s:214-217` (`BandDirector::HarvestDircuts`,
  `BandDirector::ReadyForMidiParsers`) and `dta_link_stubs.s:264-265`
  (`CameraManager::Poll`) are **DEAD** — the globbed strong defs win. (Confirm with
  `nm` once a binary exists; no binary at `build-native/rb3-native` right now.)
  These stale stubs should be *removed* during this work for audit clarity, but they
  are not currently active obstructions.
- The only genuine link-line gaps are **`BandPatchMesh`** (character outfit
  texture/patch compositing — stubbed at `band3_link_stubs.s:254-255,1006-1027`,
  every method a no-op) and **`ClipDistMap::Draw`** (`band3_link_stubs.s:159-160`,
  a LOD distance-fade draw helper — off the critical path).

So both increments are far less "bring up new code" and far more "the existing code
is short-circuited by `mCurWorld == null` and one HX_NATIVE gate."

---

## Increment A — band characters (render + animate during a song)

### A. Retail mechanism (file:function citations)

- **Who the characters are / where they live.** Four `BandCharacter`s
  (`BandCharacter : Character, BandCharDesc, MergeFilter,
  Rnd::CompressTextureCallback`, `BandCharacter.h:27`) held in
  `BandWardrobe::mTargets[4]` (`BandWardrobe.h:116`, singleton `TheBandWardrobe`).
  The character `ObjectDir`s are loaded from `world/shared/gen/world_chars.milo`
  (the no-wardrobe fallback path `BandDirector.cpp:1170` loads
  `world/shared/world_chars.milo` into `mChars`; the normal path is via
  `BandWardrobe`). `BandWardrobe::Load` resolves `mTargets[i] =
  Dir()->Find<BandCharacter>("player%d", i)` (`BandWardrobe.cpp:881`).
- **How they get instanced into the venue.** `BandDirector::EnterVenue`
  (`BandDirector.cpp:476`) → `TheBandWardrobe->SetVenueDir(dir)`
  (`BandWardrobe.cpp:225`), which calls `SetDir(dir)`, then `SyncTransProxies`
  (`BandWardrobe.cpp:326`) — this walks every `RndTransProxy` in the venue dir and
  `it->SetProxy(mTargets[i])` when the proxy's name matches a venue target name
  (`mVenueNames`). **This is the load-bearing instancing seam:** the characters
  are not free-standing meshes — they're proxied into named venue slots
  (`crowd_male01`, `player0`, etc.). `SetVenueDir` also finds + wires crowd
  characters: `dir->Find<Character>("crowd_%s%02d", ...)` (`BandWardrobe.cpp:233`).
- **How they get LOADED.** `OnFileLoaded`/`OnLoadSong` path:
  `BandDirector.cpp:1103-1166` → `TheBandWardrobe->LoadCharacters(mVenue.Name(),
  mAsyncLoad)` (`BandWardrobe.cpp:456`) → `LoadMainCharacters`
  (`BandWardrobe.cpp:503`) → per-target `mTargets[i]->StartLoad(...)`
  (`BandWardrobe.cpp:719`, via `StartClipLoads` `:700`). `AllCharsLoaded`
  (`BandWardrobe.cpp:342`) polls `bc->IsLoading()`. **This already runs natively** —
  `GamePanel::PollForLoading` (`GamePanel.cpp:183-184`) gates the gameplay load on
  `TheBandWardrobe->AllCharsLoaded()` and the game DOES reach `kReady`, so the
  characters are being loaded; they're just not instanced/drawn (their venue proxies
  are deferred).
- **How they DRAW.** Through the scene graph: `BandDirector::DrawShowing`
  (`:310`) → `mCurWorld->DrawShowing` → `RndDir::DrawShowing` polls the venue's
  drawables, which include the proxied characters → `Character::DrawShowing`
  (`Character.cpp:291`) / `Character::DrawLodOrShadow` (`Character.cpp:228`) →
  per-LOD `RndMesh::DrawShowing` → engine `BandRnd::DrawMesh`. The engine's V14a
  skinned path (`Rnd_Wgpu_RB3.cpp:1070` `owner->IsSkinned()` → `GpuVertexSkinned`
  88-byte layout → bone palette `BoneOffsetAt(b) * boneTrans->WorldXfm()` at
  `:1243-1292` → `vs_skinned` pipeline) **already draws skinned character meshes.**
- **How they ANIMATE (dance/performance, slaved to song time).** Each character
  carries a `CharDriverMidi` (`char/CharDriverMidi.cpp`, on the link line). At
  venue-enter the `setup_midi_parsers_msg` (`BandDirector.cpp:496`) attaches the
  song's MIDI parsers; the venue MIDI track + `song.anim` feed
  `CharDriverMidi::OnMidiParser` (`CharDriverMidi.cpp:88`) which selects a `CharClip`
  and plays it slaved to beat/seconds (`BeatToSeconds(... + TheTaskMgr.Beat())`
  `:103`, `clip->AverageBeatsPerSecond()` `:104`). The animation is then ticked each
  frame by `Character::Poll` (`Character.cpp:217`) → `RndDir::Poll` → the driver's
  `PollDeps`/`SetFrame`, which rewrites the bone `WorldXfm`s. **The engine reads
  those bone xfms fresh every frame** (`Rnd_Wgpu_RB3.cpp:1256` `bt->WorldXfm()`), so
  once the characters are polled, animation renders with no engine change.
  `BandDirector::mPropAnim` (`BandDirector.h:152`) is the venue `RndPropAnim` that
  also drives stagekit/lighting/shot-category keys; it's advanced from
  `OnSelectCamera` (`BandDirector.cpp:1201` `mPropAnim->SetFrame(songTime*30, 1)`).

### A. Native obstructions

1. **(BLOCKING, owned by geometry agent) `mCurWorld == null`.** The whole draw +
   poll chain hangs off `mCurWorld`, which is null because the venue's
   `world/shared/` character/prop sub-proxies are deferred
   (`Instance.cpp:351-358`). No fix here without the geometry fix. `SetVenueDir` /
   `SyncTransProxies` never run because `EnterVenue` only proceeds when
   `mVenue.Dir()` is non-null (`BandDirector.cpp:478-479`).
2. **(BLOCKING) `ReadyForMidiParsers` HX_NATIVE venue-deferred gate**
   (`BandDirector.cpp:570-589`). The native block treats the venue as satisfied
   (`mPropAnim != 0 && AllCharsLoaded()`) precisely *because* the venue is deferred —
   it bypasses `mVenue.Dir() != null`. Once the venue instances, this gate should
   revert to the `#else` form (`mPropAnim && (mVenue.Dir() || mVenue.Name()=="none")
   && AllCharsLoaded()`) so the parsers actually attach to a real world. **Action:**
   tighten/retire this HX_NATIVE block once `mVenue.Dir()` is real.
3. **(MEDIUM) `BandPatchMesh` is excluded + fully stubbed**
   (`CMakeLists.txt:308`, `band3_link_stubs.s:254,1006-1027`). `BandPatchMesh`
   composites the character outfit textures/decals (`BandCharacter::GetPatchMesh`/
   `GetPatchTex`/`AddOverlays`, `BandCharacter.h:71-75`). With it stubbed, characters
   may draw with **missing/blank outfit textures** (untextured or default-material
   bodies) but should still render as posed, animated geometry. Bring-up is a
   clang-LP64 port of `BandPatchMesh.cpp` (there's a `.bak` at
   `bandobj/BandPatchMesh.cpp.bak`, and it was actively edited 05-28, so it may be
   close). Treat as a **fidelity follow-up**, not a blocker for "characters appear
   and move."
4. **(LOW) `ClipDistMap::Draw` stubbed** (`band3_link_stubs.s:159`) — LOD
   distance-fade; off the path; only affects far-LOD crossfade. Ignore for V1.
5. **(VERIFY) freed-object / proxy lifetime.** Characters proxied via
   `RndTransProxy::SetProxy` interact with the inlined-proxy lifetime the geometry
   agent is repairing; the `HxNoteFreedAddr` ring guard
   (DIVERGENCE_AUDIT legitimate-divergence #3) exists for exactly the
   `CharBonesObject` virtual-base teardown case — watch for it during bring-up.

### A. Concrete ordered implementation plan

| # | Step | Layer | Effort |
|---|------|-------|--------|
| A0 | **Wait for geometry agent.** Confirm a venue `WorldDir` instances and `BandDirector::mCurWorld` is non-null at gameplay (check via `GAME_DBG` ReadyForMidiParsers log `BandDirector.cpp:584` once it shows `venueDir != 0`). | — | — |
| A1 | Tighten/retire the `ReadyForMidiParsers` HX_NATIVE venue gate (`BandDirector.cpp:570-589`) so parsers attach to the real `mVenue.Dir()`. Additive `#ifdef HX_NATIVE … #else … #endif`. | (a) matched-fork | S |
| A2 | Verify `SetVenueDir`/`SyncTransProxies` (`BandWardrobe.cpp:225,326`) actually find the `RndTransProxy` slots + crowd characters in the instanced venue dir; log proxy-match counts. If venue names mismatch the 360-ARK proxy names, reconcile (likely a glue-side name check, mirroring DIVERGENCE Pattern-4 asset-schema tolerances). | (c) glue diag first; possibly (a) | S–M |
| A3 | Confirm characters appear (static pose) — screenshot review (Opus). If untextured, that's the `BandPatchMesh` gap (A5), not a draw failure. | review | S |
| A4 | Confirm `CharDriverMidi` ticks: verify `setup_midi_parsers_msg` attached parsers and `Character::Poll` advances bone xfms over song time (gems already prove the song clock + MIDI parse run). Screenshot sweep across song time to confirm motion. | review + (a) diag | M |
| A5 | **(fidelity follow-up)** Bring up `BandPatchMesh.cpp` clang-LP64 (port from `.bak`), remove from `_NATIVE_FORK_EXCLUDE` (`CMakeLists.txt:308`) + delete its stubs (`band3_link_stubs.s:254,1006-1027`). Restores outfit textures/decals. | (a) matched-fork + (c) CMake/stub | M |
| A6 | Remove the now-dead `BandDirector::HarvestDircuts`/`ReadyForMidiParsers` stubs (`band3_link_stubs.s:214-217`) for audit clarity. | (c) glue (.s) | S |

**Effort:** core (A1-A4) **M** (1-3 days, mostly verification + small gating fixes,
since the code is already linked); +M for `BandPatchMesh` outfit textures (A5).

**Agent fit:** **Opus** — subjective "do the band members look right / are they
animating to the music" is Opus-only per user pref; the proxy-name reconciliation
(A2) is multi-system. A5 (`BandPatchMesh` mechanical clang port) could be a Sonnet
sub-task with an Opus visual confirm.

---

## Increment B — venue camera director (camera cuts during a song)

### B. Retail mechanism (file:function citations)

- **The shot objects.** `BandCamShot : CamShot` (`BandCamShot.h:16`) — a camera
  shot with targets, keyframes, a duration, a category, and a `mNextShots` list
  (`BandCamShot.h:118`). Shots live in the venue `WorldDir` and are organized by
  category in `CameraManager::mCameraShotCategories`
  (`CameraManager.h:73`, `world/CameraManager.cpp`). `CamShot::SetFrame`
  (`CameraShot.cpp:192`) animates the shot's `RndCam` (`GetCam` `:177` returns the
  PanelDir's `mCam`) by interpolating keyframes → builds the camera transform
  (`CamShotFrame::BuildTransform` `:1191`, `SetFrustum` `:1042`).
- **Shot selection / sequencing (the "director").** Driven by `BandDirector`:
  - `OnSelectCamera` (`BandDirector.cpp:1193`, invoked each frame via
    `select_camera_msg` from `WorldDir::Poll` `Dir.cpp:125`) advances `mPropAnim`
    to song time (`:1201`), then if no `mNextShot` and the dwell timer `unke0`
    elapsed, picks the next: `mNextShot = FindNextDircut()` (the MIDI `DIRECTED_CUT`
    track, `:1209`) else `FindNextShot()` (`:1211`), then `PlayNextShot()` (`:1214`).
  - `FindNextShot` (`:398`) queries `dir->mCameraManager.FindCameraShot(category,
    filters)` for a shot in the current `mShotCategory`.
  - `PlayNextShot` (`:432`) commits `mCurShot = mNextShot`, computes the dwell
    deadline `unke0` from shot duration / `DirectedCut`/`BFTB` category
    (`:437-465`), then `wdir->mCameraManager.ForceCameraShot(curshot)` (`:470`) and
    `mCurWorld->Handle(cam_cut_msg)` (`:472`).
  - `HarvestDircuts` (`:692`) pre-scans `mPropAnim`'s shot-category `SymbolKeys`
    track at venue-enter and builds the `mDircuts` keyframe list (`:719-729`),
    including remapping for coop modes; also disables shots that don't match the
    genre/gender/play-mode (`:711-716`). `PickIntroShot` (`:652`) picks the opening
    shot.
- **The camera SWITCH (how the active camera changes).** `CameraManager::PrePoll`
  (`CameraManager.cpp:301`) consumes `mNextShot` → `StartShot_` (`:246`,
  `EndAnim` old + `StartAnim` new + record `mCamStartTime`). `CameraManager::Poll`
  (`:324`) calls `mCurrentShot->SetFrame(CalcFrame(), 1.0f)` to animate. The actual
  camera selection happens in `WorldDir::DrawShowing` (`Dir.cpp:360-378`): it takes
  `mCameraManager.CurrentShot()`'s cam and calls `cam2->Select()`. The engine then
  honors whatever `RndCam::sCurrent` is (`Rnd_Wgpu_RB3.cpp:196` `RndCam::sCurrent ?
  RndCam::sCurrent : mDefaultCam`).
- **The MIDI VENUE / DIRECTED_CUT events.** Shot-category changes and directed cuts
  are authored in the song's MIDI VENUE track + the venue `song.anim` `RndPropAnim`;
  `HarvestDircuts` reads them into `mDircuts`, `FindNextDircut` (`:1490`) returns the
  next one by song time, and `OnSelectCamera` consumes them. `DirectedCut(sym)` /
  `BFTB(sym)` (`BandDirector.h:92-93`) classify a category.

### B. Native obstructions

1. **(BLOCKING, owned by geometry agent) `mCurWorld == null` again.** Same root.
   The `CameraManager` lives in the venue `WorldDir`; `WorldDir::Poll`
   (`Dir.cpp:109`) only runs the director when the venue is polled, and that only
   happens via `BandDirector::Poll → mCurWorld->Poll()` (`BandDirector.cpp:239`).
2. **(VERIFY) `WorldDir::mPollCamera` gate** (`Dir.cpp:126,130`). The
   `mCameraManager.PrePoll()/Poll()` calls are gated on `mPollCamera`. Confirm it's
   true for the gameplay venue (it's a serialized world field; should be set by the
   venue load). If false natively, the director silently no-ops even with a live
   world — a likely small glue/gate fix.
3. **(BLOCKING — the central conflict) camera ownership: director vs. the gameplay
   highway camera.** Native currently drives the gameplay highway from
   `game.cam` / `TrackPanelDir::GetCam` (`GamePanel.cpp:244`) and `TrackDir`'s many
   `i6->Select()` calls (`TrackDir.cpp:199-373`), with the V12 work neutralizing
   `TrackDir`/`TrackPanelDir` camera moves to lock the down-highway framing. **The
   venue camera director wants to `Select()` a DIFFERENT camera (the venue shot cam)
   every cut.** In retail RB3 the player sees BOTH — the band/venue from a directed
   camera AND the highway composited on top (the highway is drawn in the
   `TrackPanelDir`'s own pass/overlay, not the venue cam). Natively, since the last
   `Select()` wins (`RndCam::sCurrent`), whichever of {venue director,
   TrackDir/TrackPanelDir} selects last in the frame owns the camera. **This must be
   reconciled deliberately:** the highway is a screen-space-ish overlay locked to its
   own cam while the 3D venue uses the directed cam. The V12 neutralization may need
   to be revisited so the highway draws in its own camera/pass and does NOT fight the
   venue director for `RndCam::sCurrent`. This is the core design decision of
   Increment B and is where it touches the **engine `Rnd_Wgpu_RB3.cpp`** (multi-pass
   / camera-per-drawable) if a single `sCurrent` can't express both.
4. **(LOW, dead) `CameraManager::Poll` stub** (`dta_link_stubs.s:264`) — dead
   (strong def from globbed `CameraManager.cpp` wins). Remove for clarity.
5. **(VERIFY) directed-cut MIDI availability.** `HarvestDircuts` reads the
   `mPropAnim` shot-category track; confirm the song's VENUE/anim data is parsed
   natively (the gem MIDI parse already works, so the parser stack is alive — but the
   VENUE track specifically must be present + read). If `mDircuts` is empty, the
   director falls back to `FindNextShot` category cycling, which still produces cuts.

### B. Concrete ordered implementation plan

| # | Step | Layer | Effort |
|---|------|-------|--------|
| B0 | **Wait for geometry agent** (same `mCurWorld != null` precondition as A0). Also requires A0-A2 to have a venue with real `BandCamShot`s + a `CameraManager` populated. | — | — |
| B1 | Verify `WorldDir::mPollCamera` is true for the gameplay venue; if not, set it (glue or additive matched-fork gate). Confirm `CameraManager::PrePoll/Poll` run (log shot start/frame). | (c) glue diag, maybe (a) | S |
| B2 | Verify `HarvestDircuts` populated `mDircuts` from the song VENUE/anim track (log count). If empty, confirm fallback `FindNextShot` category cycling produces shots. | (a) diag | S |
| B3 | **Resolve the camera-ownership conflict (the real work).** Decide the compositing model: venue director owns `RndCam::sCurrent` for the 3D stage; the gem highway draws locked to its own framing without stomping `sCurrent`. Likely revisit the V12 `TrackDir`/`TrackPanelDir` neutralization so the highway uses a dedicated cam/pass. May require an engine change in `Rnd_Wgpu_RB3.cpp` (a second camera/pass for the highway overlay) if one `sCurrent` can't serve both. | (a) matched-fork (TrackDir camera) + **(b) engine `Rnd_Wgpu_RB3.cpp`** if multi-pass needed | **L** |
| B4 | Confirm cuts happen on song beats / DIRECTED_CUT events; screenshot sweep across song time showing distinct camera angles (Opus visual review). | review | M |
| B5 | Remove dead `CameraManager::Poll` stub (`dta_link_stubs.s:264-265`). | (c) glue (.s) | S |

**Effort:** **L** — dominated by B3 (the camera-ownership reconciliation vs the V12
highway lock + possible engine multi-pass). B1/B2/B4/B5 are S–M.

**Agent fit:** **Opus** — B3 is a multi-system design decision (matched-fork +
engine renderer) plus subjective "do the cuts look like RB3" visual review (Opus-only).

---

## Concurrency, serialization, and shared file scope

### A vs B — **serialize, do A first**

A and B share the **same `mCurWorld != null` precondition** and the **same
`BandDirector` tick path**, and B's most important step (B3) depends on characters
already being visible to judge framing. They are NOT independent.

- **Do A first** (get characters instanced + animating under the locked V12
  highway camera — visually verifiable on its own).
- **Then B** (introduce the camera director, which by design *moves* the camera A's
  characters are framed by). B3's camera-ownership decision is much easier to reason
  about and review once characters are visibly on stage.

### Shared file scope (the serialization map)

| File | Geometry agent (#81) | Increment A | Increment B | Rule |
|------|----------------------|-------------|-------------|------|
| `src/system/world/Instance.cpp` | **owns** (the proxy fix) | — | — | Geometry agent only. A/B consume the result. |
| `src/system/obj/Dir.cpp` (`PostLoadInlined`) | **owns** | — | — | Geometry agent only. |
| `src/system/world/Dir.cpp` (`WorldDir::Poll`/`mPollCamera`) | maybe | maybe (A: poll path) | **yes (B1: mPollCamera gate)** | **Serialize** A↔B; coordinate with geometry agent (it may already touch WorldDir). |
| `src/system/bandobj/BandDirector.cpp` | maybe (ReadyForMidiParsers note) | **yes (A1 gate)** | yes (director already lives here; B reads it) | **Serialize** — A edits the `ReadyForMidiParsers` HX_NATIVE gate; B relies on the director methods. A owns the edits; B should not edit it concurrently. |
| `src/system/bandobj/BandWardrobe.cpp` | — | **yes (A2 proxy/name reconcile)** | — | A only. |
| `src/system/bandobj/BandPatchMesh.cpp` + `CMakeLists.txt` exclude | — | **yes (A5)** | — | A only; CMake edit serializes with any other TU bring-up. |
| `src/system/track/TrackDir.cpp` / `bandobj/TrackPanelDir*.cpp` | — | — | **yes (B3: revisit V12 cam lock)** | B only. |
| **engine `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`** | maybe (geometry may extend draw/lit) | unlikely (skinned path already landed V14a) | **maybe (B3: multi-pass / 2nd camera)** | **SERIALIZE** all engine-render edits across geometry agent + B (+ G_FX/G_GEMPOLISH from the parity roadmap). Single owner at a time. |
| `native/src/band3_link_stubs.s` | maybe | **yes (A5 BandPatchMesh, A6 dead stubs)** | yes (B5 CameraManager::Poll) | **SERIALIZE** `.s` edits (also shared with G_SCORE/G_FX). Batch all stub deletes into one pass. |
| `native/src/dta_link_stubs.s` | — | — | **yes (B5)** | Serialize with any other `.s` owner. |
| `native/CMakeLists.txt` | maybe | **yes (A5 un-exclude BandPatchMesh)** | — | **SERIALIZE** CMake edits. |
| `native/src/rb3_game_input.cpp` | — | maybe (synthetic verbs for longer sweeps) | maybe | Serialize with G1/G_SCORE/G_FX owners per the parity roadmap. |

### Relationship to the existing RETAIL_PARITY_ROADMAP `G_VENUE` item

`RETAIL_PARITY_ROADMAP.md`'s **G_VENUE** is the umbrella for all of this (venue core
+ "+M for band characters"). This doc refines G_VENUE into the geometry (geometry
agent #81) + A (characters) + B (camera director) sub-increments and adds the
camera-ownership finding (B3) that the roadmap did not call out.

---

## Recommended dispatch ordering (for the coordinator)

1. **(in flight) Geometry agent #81** — the `Instance.cpp`/`Dir.cpp` proxy fix.
   **Everything below gates on this.** Do not start A/B implementation until a
   gameplay venue `WorldDir` instances and `BandDirector::mCurWorld != null` (watch
   the `GAME_DBG` `venueDir=` log).
2. **Increment A (characters)** — dispatch **Opus**, the moment geometry lands.
   Mostly verification + small gating fixes (A1-A4) because the band/char code is
   already linked; `BandPatchMesh` outfit textures (A5) is a fidelity follow-up that
   can trail. A runs largely self-contained in the bandobj/char/wardrobe matched-fork
   tree; its only shared-file risk is `WorldDir::Poll`/`BandDirector.cpp` (coordinate
   with the geometry agent) and the `.s`/CMake stub cleanups (batch them).
3. **Increment B (camera director)** — dispatch **Opus**, **after A** lands and is
   visually confirmed. B's core (B3 camera-ownership vs the V12 highway lock, with a
   possible engine `Rnd_Wgpu_RB3.cpp` multi-pass) is the **L** long pole of the two
   and needs A's characters on screen to judge framing. **Serialize B's engine-render
   edits** behind whatever the geometry agent / G_FX last touched in
   `Rnd_Wgpu_RB3.cpp`.

**A and B do NOT run concurrently** — they share the `mCurWorld`/`BandDirector` tick
path and B intentionally moves the camera that frames A. Serialize: geometry → A → B.

**Mechanical sub-tasks that CAN be peeled to Sonnet** (with an Opus visual confirm):
A5's `BandPatchMesh` clang-LP64 port (from the `.bak`), and the dead-stub deletions
(A6/B5) — but only after the owning Opus dispatch has settled the shared `.s`/CMake
files, to avoid serialization conflicts.

---

*Planned read-only; no source files modified. No binary present at
`build-native/rb3-native` at authoring time, so symbol-strength claims (dead stubs)
are from CMake glob analysis — confirm with `nm` once a binary exists.*
