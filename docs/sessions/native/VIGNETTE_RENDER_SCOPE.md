# Vignette-world rendering — scoping + tractability verdict (read-only)

> **PROBE UPDATE 2026-05-29 — the "BOUNDED camera-drive" verdict below (§0) was
> OVERTURNED by a follow-up probe.** Env-gated traces (`RB3_TV3_DBG`) across
> `WorldDir::Poll/DrawShowing`, `CamShot::SetFrame`, `CamShotFrame::BuildTransform`,
> `RndPropAnim::SetFrame` proved:
> - The tv3 `WorldDir` **IS** polled and its CameraManager **IS** driving a shot
>   (`BandCamShot.shot`, `SetFrame` advancing). So it is NOT an unpolled-anim gap.
> - **There is NO camera animation** — the shot's only PropAnim animates a material
>   (`showtonight_poster.mat`), not the camera. The `BandCamShot.shot` keyframe has
>   `mWorldOffset=(0,0,0)`, `parent=nil`, no targets → identity-at-origin pose. This
>   loads CORRECTLY; the `(0,0,0)` cam is **genuine authored-absent data** in the
>   loaded sub-milo (`postnobills_closeup_01`), not an LP64 bug. (Working shell
>   vignettes sv3/sv4/sv8 carry non-zero offsets `(110,60,300)` etc. and render.)
> - Both §5 seams (poll the anim / V22 CamOverride-retarget) assume an authored shot
>   to point at; there isn't one. A control experiment forcing a synthetic standoff
>   cam framed only stray venue geometry near origin, NOT the restaurant/menu scene.
>
> **Revised verdict: the tv3 transition is NOT a bounded camera-drive fix.** The real
> tv3 camerawork lives in the parent `tv3_a.milo` sub-shot **sequencer**
> (`trans_index`/`EventTrigger`/`vignette_transition`), which never runs natively
> because `InterstitialPanel::Exiting()` short-circuits and advances the screen in
> ~3 frames. Making N7 perceptible needs that **sequencing** work (let the transition
> play its full sub-shot sequence) — a larger, multi-step effort — NOT a one-block
> camera pose. Probe code was discarded (matched-fork wipe surface); findings kept here.
> The engine-RTT cloud-dome item (§3, §5 end) remains the other separate larger piece.
>
> **PROBE UPDATE 2 (sequencer attempt, 2026-05-29) — confirms tv3 is a MULTI-SESSION
> data-sequencer rework, NOT a screen-hold.** A second probe (`RB3_TV3_PLAY` +
> `RB3_TV3SEQ_DBG` in `InterstitialPanel.cpp`) established:
> - The tv3 sub-shot sequencer is **data-driven inside `tv3_a.milo`** — an `EventTrigger`
>   `vignette_transition` (`trans0..trans4`/`trans_index`/`new_index`) maps an index to
>   the 3 sub-shots (`postnobills_closeup_01/02/03_ao`) and re-points `world.cam`. It is
>   NOT C++ (`trans_index`/`vignette_transition` appear in no source file). Screen wiring
>   is `ui/vignettes.dta` `new_transition_vignette`.
> - **What advances the transition natively is NOT the InterstitialPanel gate** — it's
>   `game_screen` finishing its load. `BandUI::GetTargetScreen` redirects `goto game_screen`
>   → `tv3_a_screen`; the swap completes when `game_screen` (game + sync_audio_net panels)
>   loads — ~3 frames headless vs seconds on console (which is why retail has time to play
>   the cinematic). So tv3 is `cur` only f453-455 by default.
> - **HOLD experiment (decisive):** restoring the retail gate (`RB3_TV3_PLAY=1`,
>   `!mCamshotDone || unk88<3`) holds the panel but **STALLS PERMANENTLY** — across ~250
>   held frames `SetCamshotDone` NEVER fires, `world.cam` stays at origin, all frames black.
>   The `vignette_transition` EventTrigger sequencer **never advances `trans_index`** — its
>   advance mechanism does not tick natively (no `vignette_start.trig` kick; the WorldDir
>   sub-shot sequencer is never driven). So the gate is a STALL, not a fix.
> - **To actually fix tv3:** drive the milo's `vignette_transition` EventTrigger sequencer
>   (fire `vignette_start.trig` on screen-enter → advance `trans_index` through posed
>   sub-shots → post `transition_camshot_done`) AND decouple the tv3→game swap from
>   `game_screen`'s instant headless load. That spans WorldDir/CameraManager/BandCamShot/
>   EventTrigger + InterstitialMgr screen-flow timing = multi-session. Probe code discarded
>   (default-off was byte-identical + regression-clean; the opt-in is a proven stall).

> **PROBE UPDATE 3 (implementation pass, 2026-05-29, branch `nwt-tv3seq`) — the
> "HOLD stalls permanently" verdict (UPDATE 2) is PARTIALLY OVERTURNED on the
> current build (engine pin 9f635b7 / RndTex). Env-gated work (default-off,
> byte-identical) on `nwt-tv3seq` (commits 451da7c2 / 96061a5f / e2792685):**
> - **The sequencer DOES advance + complete when held.** `RB3_TV3_PLAY=1` restores
>   the retail `InterstitialPanel::Exiting` gate. With the tv3 screen held, the
>   data-driven `vignette_transition` sequencer (defined in
>   `world/world_objects.dta` as the `vignette_transition` WorldDir type, NOT C++)
>   runs to completion: the WorldDir `enter` handler fires `do_transition(0)`
>   (sets `trans_index=0`, `new_index=1`); `WorldDir::Poll`'s
>   `HandleType(select_camera_msg)` runs the `select_camera` handler (fires
>   `vignette_start` + `force_camera` on the current sub-shot); each sub-shot's
>   `BandCamShot.shot` (`StartAnim`, dur≈25-35) reaches `mDuration` →
>   `CamShot::SetShotOver` (`shot_over_msg`) → data `next_camera` → advances
>   `trans_index` 0→1→2(→3). After the last sub-shot, `next_camera` hits the
>   no-more-trans branch → `{ui transition_camshot_done}` → `SetCamshotDone` →
>   `unk88` counts 0..3 in Draw → `Exiting()` returns false → screen swaps to
>   `game_screen`. Observed completing at ~f1023 (tv3_b) → reaches gameplay,
>   exit 0. So **steps "advance trans_index" + "decouple the swap" both work; no
>   permanent stall** (every run exits 0 with forward progress).
> - **Two genuinely-open remaining gaps (clearly scoped, NOT stalls):**
>   1. **Held-panel poll-starvation makes completion-timing unreliable.** The held
>      tv3 WorldDir is polled only ~70-130×/1500-2400 frames (the shot `CalcFrame`
>      only advances when `WorldDir::Poll`'s `kProcessPost` flag is set; during the
>      held game_screen load it is throttled). So some vignettes complete fast
>      (tv3_b@f1023) while others poll-starve and exceed a 4000-frame budget. Making
>      the swap reliably bounded needs the held-panel shot clock to advance steadily
>      (UI poll/clock path — regression-sensitive).
>   2. **Vignette cameras are authored at/near origin → geometry instances but is
>      not framed → black.** `CamShotFrame::BuildTransform` for the `_ao` closeup
>      sub-shots reads `mWorldOffset=(0,0,0)`, `parent=(none)`, `nTargets=0`,
>      `path=(nil)` → `tf.v=(0,0,0)`. The intended props DO draw (`amp.mesh`,
>      `prop_guitarcord`, `microphone`, `mic_stand`… — confirmed via DrawMesh) but
>      the origin camera doesn't frame them. Same family as the V22/V23 venue
>      closeup target-resolution work. (3 traces left default-off under
>      `RB3_TV3SEQ_DBG`: `WorldDir::Poll`, `Hmx::Object::HandleType`,
>      `CamShot::StartAnim/SetShotOver/BuildTransform`.)
> - **Default-off regression: CLEAN.** With no env, tv3 swaps fast (f453→f456,
>   unchanged) and boot→gameplay is byte-identical, exit 0; zero trace output.

> **PROBE UPDATE 4 (FIX, 2026-05-29, branch `nwt-tv3seq`, commit 7018c1bc) — the
> "_ao closeup cameras authored at origin" verdict (UPDATE 3 gap #2) is OVERTURNED.
> The cameras were NEVER authored at origin; a matched-fork decomp bug collapsed
> them. The tv3 cinematic now FRAMES its scene (non-black).**
> - **Root cause (decisive):** `CamShotFrame::Interp` builds two keyframe transforms
>   and interpolates. The rb3 (MWCC) matched fork calls `this->BuildTransform` TWICE
>   (`CameraShot.cpp` ~L1108-1110); the verified-correct sibling decomps
>   (`dc3-decomp`/`rb3-xenon` CameraShot.cpp:513-515) call `other.BuildTransform`
>   for the SECOND. For a single-keyframe shot, `CamShot::SetFrame` invokes
>   `nullFrame.Interp(*frame50)`, so BOTH builds came from `nullFrame`
>   (`mWorldOffset=0`) → camera at origin → black. The keyframe IS authored:
>   `key[0] wOff=(18.59,-86.90,85.72)` (tv3_a) / non-zero for every sub-shot; the
>   `BandCamShot` resolves 4 targets (`player0..3`) at non-origin world positions.
> - **Fix:** HX_NATIVE, opt-in `RB3_TV3_PLAY`, default-off byte-identical (`#else`
>   keeps the asm-matched double-`this` build). When set, the second build is
>   `frame.BuildTransform`.
> - **RESULT (RB3_TV3_PLAY=1, canonical flow):** tv3 `postnobills_closeup` /
>   `ampplugin`/`footpedal`/`guitarstrum`/`miccheck` sub-shots POSE `world.cam` to
>   non-origin (e.g. `(-85.77,89.59,40.26)`, animating) and FRAME the authored props
>   (mic/mic_stand/water-bottle/cymbal/amp). tv3 frames render **110-124 KB content**
>   (was 36970-byte black). With a 6000-frame budget the sequencer advances
>   trans_index 0→1→2→3 through all sub-shots and reaches `game_screen` (exit 0).
> - **Default-off: CLEAN.** No env → boot→gameplay byte-identical, fast tv3 swap
>   (f453→game_screen f456), exit 0.
> - **Remaining (gap #1, unchanged):** the held-panel completes only with a large
>   frame budget (poll-starvation throttles the held shot clock); NOT a stall — every
>   run exits 0 with forward progress. Bounding the swap reliably is the regression-
>   sensitive UI-clock piece, left for a follow-up.

**Authored:** 2026-05-29 (Opus deep-investigation subagent, READ-ONLY: no source
edits, no build, no commit; ran the game read-only for diagnostics).
**Reproducer:** the canonical `RB3_GAME_INPUT` boot→song flow (see §6); evidence
captured in `/tmp/vignette-scope/` (`run_renderdbg.log`, `run_menuvoid.log`,
`01_f0007.png`, `03_f0454.png`).

---

## 0. TRACTABILITY VERDICT (read this first)

**The dispatch's stated root cause (`IsDeferredVenueProxy` proxy-deferral +
`PostLoadInlined` gap) is NOT what blocks vignette rendering. Runtime data
disproves it.** Across the entire boot→hub→song-select→tv3-transition run, **ZERO
`world/vignette/**` proxies ever reach `WorldInstance::SyncDir`'s deferral** (the
`MENU_VOID_DBG=1` "deferring" log is empty; `grep -c deferring` = 0). The only
proxy ever deferred is `world/shared/amps/classic_blacktriple` (an amp prop). The
vignette worlds load fine, instance their meshes, and reach `DrawMesh`.

The problem splits into **two independent, already-mostly-solved or
narrowly-bounded issues — neither is the structural proxy-instancing rework the
prior docs feared:**

| Symptom | True root cause (data-confirmed) | Status / verdict |
|---|---|---|
| **Hub/menu backdrop void** (N3) | A single opaque-black depth-writing occluder mesh `worldcenter.mesh` (in `sv8_a.milo_xbox`) + the unpainted `clouds_rnd.tex` render-target. **NOT proxy deferral.** | **ALREADY FIXED at HEAD** (v43, `src/system/rndobj/Draw.cpp` occluder-skip, default ON). f7 now renders the rooftop city + BAND3 logo + cloud sky. Residual: animated cloud-dome RT unpainted (engine RTT stub). |
| **tv3_* transition renders pure black** (N7) | The vignette's **`world.cam` is never animated — it stays collapsed at pose `(0,0,0)`** (identity). The 15 tv3 meshes DO instance and DO reach `DrawMesh`, but through an origin camera with no valid view → all geometry off-frame → black. The vignette's camera PropAnim / `.trg` chain (which would also fire `transition_camshot_done`) never runs. **NOT proxy deferral, NOT a load failure.** | **BOUNDED, OPEN.** This is a camera-activation / PropAnim-drive gap, in the same family as the venue camera work (V19/V22), not the proxy core. |

**Verdict: this is a BOUNDED problem, NOT a multi-session structural rework.** The
`IsDeferredVenueProxy` deferral is a red herring for rendering (it only defers
cosmetic `world/shared/` props; the V19 venue precedent already proved vignette/
venue ROOTS instance fine without touching the gate). The genuinely-open piece —
the tv3 transition black — is a contained "drive the vignette world's camera"
problem with a clear smallest-valuable first step (§5). **Do NOT spend a week on
the `PostLoadInlined` many-to-one shared-dir abstraction for rendering; it is not
on the render critical path.**

---

## 1. Why are vignette worlds "deferred"? (the gate, and why it's a red herring)

`WorldInstance::SyncDir` (`src/system/world/Instance.cpp:361-375`) has an
HX_NATIVE early-return: if the proxy's source `mDir.GetFile()` matches
`IsDeferredVenueProxy` (`Instance.cpp:351-358`: `strstr "world/vignette/"` ||
`strstr "world/shared/"`), it skips the proxy-instancing loop and leaves `this` an
empty proxy (`Instance.cpp:366-374`).

**What it skips:** the instancing loop at `Instance.cpp:392-459` that, for each
object in the shared source dir, calls `FindObject` (`:413`) and on miss does
`NewObject`+`CopyObject` (`:415-419`), then `MILO_ASSERT(p->from->Dir(), 0x2CA)`
(`:428`). The assert fires because `Hmx::Object::Copy` does not copy `mDir`, so
the fresh copy has a null `Dir()`. The deferral exists to dodge that assert + a
downstream `DeleteTransientObjects` "Could not find …mesh" teardown crash. The
three documented obstructions (Instance.cpp:326-350): (a) `LoadPersistentObjects`
save/restores `mDir->Dir()` (`:191-212`); (b) the shared dir is many-to-one across
proxies (DirLoader cache hit, `Dir.cpp:404-407`); (c) `Copy` drops `mDir`.

**Why it is a red herring for vignette rendering** (the key data finding): the
shell-vignette + tv3 milos are loaded as `WorldInstance` proxies whose **source
file is the per-vignette root** (`world/vignette/shell/gen/sv8_a.milo_xbox`,
`world/vignette/transition/gen/tv3_a.milo_xbox`) — but the `MENU_VOID_DBG` trace
shows **no `world/vignette/` file ever reaches the deferral predicate** on the
real game path. The vignette ROOTs are instanced through a non-deferred path (the
same way V19's `small_club_01` venue root instances fine — `VENUE_RENDER.md:108-114`
documents the gate "turned out NOT to be on the critical path for geometry
render"). Only the genuinely-shared `world/shared/` sub-props (amps/mics/decals)
ever hit `IsDeferredVenueProxy`, and those are visually negligible. The
`PostLoadInlined` "gap" (`Dir.cpp:152-163`, `:390-454`) is real but only matters
for those shared sub-props — it is **not** why vignettes render black.

This was already empirically established by the v41 attempt
(`MENU_VOID_RESULT.md:78-95`): narrowing the deferral and carrying `mDir` on the
copy changed NOTHING visually for the hub, because no vignette proxy was being
deferred in the first place. This investigation re-confirms it at HEAD.

## 2. Why does the in-song venue (small_club) render but vignettes don't?

They render by the **same mechanism**; the difference is **whether the world's
camera is driven to a valid pose**, not whether geometry instances.

- **In-song venue (V19, `VENUE_RENDER.md`):** `BandDirector::EnterVenue`
  (`BandDirector.cpp` HX_NATIVE) force-loads the venue, sets `mCurWorld`, and —
  critically — the **venue camera director runs** (V22, `BandDirector::DrawShowing`
  `:~315`, points the venue WorldDir's `CamOverride` at the director's active shot
  cam). So the venue draws through an animated, valid camera. Mesh count
  178-245/frame; renders.
- **Shell-vignette backdrop (hub, song-select):** instances fine AND its
  `world.cam` is animated by the vignette's own PropAnim — the render trace shows
  `world.cam pos=(-0.32,-0.11,0.23)` varying frame-to-frame at f7/f270-453,
  drawing 212-230 meshes. **Renders.** (The hub void was a separate occluder-mesh
  issue, fixed v43.)
- **tv3 transition vignette:** instances its 15 meshes (`restaurant_sign`,
  `brick_wall`, `menu_01..07`, `corkboard`, `showtonightposter` — all genuine
  tv3_a content, confirmed via `strings` on the milo), and they DO reach
  `DrawMesh` (`run_renderdbg.log` f454-456). **But `world.cam pos=(0.00,0.00,0.00)`
  — the camera collapsed to origin/identity.** No valid view transform → geometry
  off-frame → pure black (`03_f0454.png` is 37 KB pure black vs 727 KB for the
  rendered hub f7).

So: **vignette instances-AND-draws, but its camera is never animated.** The tv3
vignette is a `WorldDir` (`world.cam`, `char.grp`, `geometry.grp`, `smoke.grp`,
`paper.anim`) whose `world.cam` should be posed by the vignette's transition
camera animation — the same animation that fires `transition_camshot_done`
(`ui/vignettes.dta:47`). That animation does not run natively, which is also
exactly why `InterstitialPanel::Exiting()` had to be short-circuited
(`InterstitialPanel.cpp:21-47`: native drops the `!mCamshotDone || unk88<3` gate
because `transition_camshot_done` never fires).

## 3. What would un-blocking vignettes actually take?

It is **(c) — not the proxy/inline-cached-shared rework.** Specifically:

- **Hub/menu (N3): DONE.** No further work for the user-visible void. Optional
  fidelity: implement `RndTex::MakeDrawTarget`/`FinishDrawTarget`
  (`src/platform/Rnd_Wgpu_RB3.cpp:1642-1643`, currently `{}` no-ops) so the
  animated `clouds_rnd.tex` render-target paints. That is an **engine RTT
  feature**, decoupled from the proxy/vignette question.
- **tv3 transition (N7): drive the vignette world's `world.cam`.** The geometry is
  present; only the camera is dead. Two candidate seams (needs a probe to pick):
  1. The tv3 `WorldDir` needs its camera-animation PropAnim Polled/activated when
     `tv3_*_screen` is the committed screen (the `BandScreen`/`InterstitialPanel`
     path that should tick the vignette's `world.cam` track). The headless
     UI-clock (`UI.cpp:518-525`, +1/30 s per Poll) means the anim WILL advance if
     the screen is held and the anim is actually wired to a polling task — the N7
     plan's frame-hold premise. The open question (N7 honest-negative,
     `N2_N4_N7_VERIFY_RESULT.md:48-79`) was that holding showed only black: that is
     because the anim/camera isn't driven AT ALL, not that it lacks poll cycles.
  2. If the vignette camera is meant to be posed by a director/`CamOverride` (like
     the venue), the fix mirrors V22: ensure the drawn `world.cam` is pointed at
     the vignette's authored camera shot. The `cam=world.cam pos=(0,0,0)` collapse
     is the same "drawing through an un-posed camera" class V22 fixed for the
     venue.

**Blast radius / what relies on the gate:** the `IsDeferredVenueProxy` gate is in
the proxy-instancing CORE (`SyncDir` runs for the in-song venue's shared props
too), so naively un-deferring it risks the V19-V23 venue path
(`MENU_VOID_PLAN.md:298-316` flags this as HIGH-risk; the v41 attempt confirmed an
unrelated UI list-panel copy crash when `Hmx::Object::Copy` carried `mDir`
globally — `MENU_VOID_RESULT.md:45-66`). **But the tv3 fix does not touch the
gate or `Copy` at all** — it touches the vignette camera-drive path, whose blast
radius is the transition screens + (positively) retires the
`InterstitialPanel::Exiting` short-circuit (`InterstitialPanel.cpp:21-47`) once
the camshot anim fires naturally. Why the deferral was added: to avoid the
`:0x2CA` assert + teardown crash on the shared sub-props — that reason is still
valid for `world/shared/`, so leave the gate as-is.

## 4. Runtime diagnostics (the evidence)

All from the canonical reproducer (§6), `/tmp/vignette-scope/`:

- **Deferral never fires for vignettes:** `MENU_VOID_DBG=1` "deferring" lines = 0;
  `grep -i vignette run_menuvoid.log` empty. Only `world/shared/amps/
  classic_blacktriple` is ever deferred. → **disproves the proxy-deferral
  hypothesis.**
- **tv3 instances + draws but camera is dead** (`RB3_RENDER_DBG=1`,
  `run_renderdbg.log`):
  - f450-453 (song-select shell): `world.cam pos=(-0.49..-0.47, …)` animated, 198-212 meshes → renders.
  - f454-456 (`tv3_a_screen`, confirmed committed screen): `world.cam
    pos=(0.00,0.00,0.00)`, **15 meshes / 1400 tris** (`restaurant_sign`,
    `brick_wall`, `menu_01..07`, `corkboard`, `showtonightposter`, frames, tape —
    all tv3_a content). Meshes reach `DrawMesh`; camera at origin → black.
  - f457+ (game): cam still `(0,0,0)` but venue/V22 cam-follow re-points the draw
    cam → 178-191 meshes render.
- **tv3 milo loads cleanly:** no "Could not load" for any `tv3`/`vignette`/`sv`
  file; `tv3_a_screen` is the committed screen at f453.
- **Hub renders at HEAD:** `01_f0007.png` (727 KB) shows the rooftop cityscape +
  "ND3"/BAND3 neon logo + cloud sky — the v43 occluder-skip
  (`src/system/rndobj/Draw.cpp`, default ON) is in effect.
- **tv3 black confirmed:** `03_f0454.png` is 37 KB, pure black.

## 5. Smallest valuable first step + file scope

**Smallest valuable step: get the tv3_* transition to render its (already-loaded)
geometry by driving its `world.cam` to a non-origin pose.** This is the bounded
win that unblocks N7 ("perceptible load screen") and retires the
`InterstitialPanel::Exiting` short-circuit. Concretely (probe-then-fix, ~1 focused
session):

1. **Probe (S):** add an env-gated trace in the transition-screen path to dump,
   per frame while `tv3_*_screen` is committed: (a) is the tv3 `WorldDir` being
   Polled at all? (b) does it have a camera-animation PropAnim/`.anim` task, and
   is it ticking? (c) what `RndCam` is `world.cam` and is anything writing its
   `WorldXfm`? This distinguishes "anim exists but isn't Polled" (→ wire the Poll,
   mirroring `WorldDir::Poll`'s `mCameraManager`) from "no director posing the
   cam" (→ a V22-style `CamOverride` retarget to the vignette's authored shot).
2. **Fix (M):** drive the vignette camera by the chosen seam. Either is a
   contained HX_NATIVE block; neither touches `IsDeferredVenueProxy`,
   `Hmx::Object::Copy`, or `PostLoadInlined`.

What it unblocks: N7 load screen becomes perceptible; once the vignette camera
anim runs it fires `transition_camshot_done`, so the
`InterstitialPanel::Exiting()` HX_NATIVE short-circuit
(`InterstitialPanel.cpp:21-47`) and the `BackdropPanel::Exiting()` outro
short-circuit (`:81-117`) can be tightened back toward the `#else` form
(DIVERGENCE hacks #2a/#2b retired naturally).

**File scope (likely):**
- `/home/free/code/milohax/rb3/src/system/world/Dir.cpp` — `WorldDir::Poll` /
  `DrawShowing` (vignette camera Poll / `CamOverride`), HX_NATIVE. *(matched-fork,
  permuter-owned, additive `#ifdef`.)*
- `/home/free/code/milohax/rb3/src/band3/meta_band/InterstitialPanel.cpp` — once
  the camshot fires, tighten `Exiting()`/`BackdropPanel::Exit/Exiting`
  short-circuits. *(matched-fork.)*
- Possibly `/home/free/code/milohax/rb3/src/system/ui/UIPanel.cpp` /
  `BandScreen` path — to ensure the vignette `WorldDir` is Polled while the
  transition screen is shown. *(matched-fork.)*
- **NOT in scope:** `src/system/world/Instance.cpp` (`IsDeferredVenueProxy`),
  `src/system/obj/Object.cpp` (`Copy`), `src/system/obj/Dir.cpp`
  (`PostLoadInlined`) — these are the proxy core the prior plan feared; the data
  shows they are not on the vignette render path.

**Blast radius:** transition/backdrop screens; the venue path (V19-V23) is
untouched because the fix is on the vignette `WorldDir`/screen camera, not the
proxy-instancing core. A camera-drive change cannot regress geometry instancing.
Gate behind an env opt-out and A/B per `visual-reviews-opus-only`.

**Engine fidelity follow-up (separate, larger, optional):** implement
`RndTex::MakeDrawTarget`/`FinishDrawTarget` (engine RTT) so the hub's animated
cloud-dome and any vignette render-target layers paint. This is the one genuinely
larger item, but it is an engine feature, not the vignette/proxy question, and the
user-visible voids are already resolved without it.

## 6. Reproducer

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=470 \
  MILO_SCREENSHOT_DIR=/tmp/vignette-scope MILO_SCREENSHOT_FRAMES=7,280,454 \
  MENU_VOID_DBG=1   # (proves NO vignette proxy is deferred — log is empty) \
  RB3_RENDER_DBG=1  # (per-frame cam pose + DrawMesh names — shows the (0,0,0) cam) \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```
(f7 = hub backdrop [renders], f280 = song-select shell [renders], f454 = tv3
transition [pure black, cam at origin]. `MILO_SCREENSHOT_DIR` MUST be absolute.)
