# Scout: `crowd` — crowd merged into a single location, not animating

Wave-1 scout, 2026-06-11 (fable). Issue: "Crowd characters render merged into a
single location and do not animate."

**TL;DR — root cause found and probe-proven.** The crowd's per-instance placement,
polling, and per-draw GPU uniforms are all healthy. What's broken: the crowd
archetypes' **skinned body/outfit meshes compose bone palettes that mix bones from
TWO same-named `Character` instances** (the gameplay venue's `crowd_male01` etc.
AND the tv3 intro-vignette theater's `crowd_male01`, ~2000 units apart). The
blended mesh tears across that span (bindExt ≈ 80u → worldExt ≈ 2100-2240u, ratio
≈ 25x), so the engine's V24 shard-guard **drops every `*_crowd_body*` and crowd
outfit mesh ~20,000 times per song** — the crowd you see is just the surviving
fragments (heads/props/odd silhouettes) bunched together and apparently static.
This is the *same bug class* as the band-character shared-skeleton fix
(`acd9c19a`, `BandCharacter::RebindOutfitBonesToOwnSkeleton`) — **which was
applied band-only and never to crowd-type Characters.** Fix = rebind crowd
meshes' bones to the owning archetype's own skeleton (rb3-side, no engine change
required for the primary fix).

Two adjacent latent issues are documented below with fix designs: the Wii 2D
imposter pipeline is structurally dead on native (§2.4, matters for arena/bowl
venues), and native gameplay can currently ONLY load `small_club_01` (§2.5).

---

## 1. SYMPTOM (repro + evidence)

Repro (any song — the native venue bridge always loads `small_club_01`, see §2.5):

```bash
python3 scripts/native/keyboard-to-gameplay.py --port 8621 --diff hard \
    --out /tmp/rp-crowd --game-burst 24 --verbose
# frame the crowd on a live instance:
curl -X POST localhost:8621/api/dta/eval -d '{band_director force_shot "coop_dir_crowd.shot"}'
```

Screenshots (`/tmp/rp-crowd/`):

- `crowdshot_07.png` — two flat-green statue-like figures by the highway (the
  surviving fraction of the front-row crowd).
- `crowdshot_06.png` — green disembodied heads bunched mid-right, bodies missing.
- `crowdshot_13.png` — scattered thin/skeletal remnants on the club floor.
- `burst_*.png`, `live_*.png`, `probe*/`, `arena2/` — assorted runs.

Probe build: worktree `.claude/worktrees/scout-crowd` (uncommitted; `git diff` =
env-gated `CROWD_DBG` fprintf probes in `src/system/world/Crowd.cpp` +
`src/system/char/Character.cpp` only — no engine edits). Engine logs contain
binary bytes — **always `grep -a`**:

- `/tmp/rb3-kbd2game-8624.log` — lifecycle probes
- `/tmp/rb3-kbd2game-8626.log` — bone-vs-root probe
- `/tmp/rb3-kbd2game-8627.log` — de-aliased poll census + `SHARD_DBG`/`SHARD_RATIO_DBG`

## 2. ROOT CAUSE

### 2.1 What is actually on screen — bodies are shard-guard-dropped  [PROVEN]

Run with `SHARD_DBG=1 SHARD_RATIO_DBG=1` (engine-side diagnostics that already
exist, `Rnd_Wgpu_RB3.cpp:4111+`), full song in small_club_01:

```
$ grep -a SHARD_GUARD /tmp/rb3-kbd2game-8627.log | awk -F"'" '{print $2}' | sort | uniq -c | sort -rn | head
20059 talldocsfolded_resource.mesh        19951 shortspikes_resource.mesh
19501 male_crowd_body01.mesh              19258 male_crowd_body02.mesh
17652 greaser_resource.mesh               17100 wovensteppers_skin.2.mesh
16942 female_crowd_body02.mesh            16748 female_crowd_body03.mesh
16541 vestandtank_skin.1.mesh             ... (every crowd body/hair/outfit mesh)

$ grep -a SHARD_RATIO ... | grep crowd_body
[SHARD_RATIO] mesh='male_crowd_body01.mesh'        bindExt=87.35 worldExt=88.06   ratio=1.01        (passes sometimes)
[SHARD_RATIO] mesh='male_crowd_body01.mesh'        bindExt=87.35 worldExt=2238.94 ratio=25.63 DROP  (dropped ~20k times)
[SHARD_RATIO] mesh='female_crowd_body01_lod02.mesh' bindExt=79.62 worldExt=2095.06 ratio=26.31 DROP
```

So the crowd bodies are being **dropped nearly every draw of every frame** by the
engine's degenerate-skinned-mesh guard. The visible "crowd" is only the meshes
that pass (heads, eyes, some lods/props) — bunched fragments that read as
"merged into a single location and not animating."

### 2.2 Why the bodies are degenerate — bone palette spans TWO worlds  [PROVEN by geometry]

worldExt ≈ 2095–2239 with bindExt ≈ 80 means the blended vertices of ONE draw
span ~2100+ units. The probe (CHAR-DRAW lines, 8626 log) shows the two crowd
char populations in memory:

- gameplay venue floor instances: rootPos ≈ `(±150, -30..-300, 3.6/4.5)`
- tv3 intro-vignette THEATER instances (same names `crowd_male01`…): rootPos ≈
  `(-1350..-1455, 640..1390, -1016.3)`

Distance between the clusters ≈ **2040u — exactly the observed worldExt**. The
shared crowd body mesh's bone array resolves partly into one instance's skeleton
and partly into the other's, so its palette mixes positions ~2000u apart →
ratio 25x → guard drops it. (Both same-named `Character` dirs exist
simultaneously because the venue is force-loaded/warmed while the tv3 vignette
world is still resident — the W5 dwell-warm flow.)

This is precisely the band-character bug fixed in
docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md (`acd9c19a` + `0de768a1` /
`2580e128`): meshes bound to a shared/wrong skeleton instead of the owner's own.
Those fixes live in `BandCharacter::RebindOutfitBonesToOwnSkeleton` /
head-rest-pose rebind and are **scoped to band members only** — crowd-type
`Character`s never got the rebind.

### 2.3 What is NOT broken (verified negatives — don't re-investigate)

- **Placement/instancing**: `Draw3DChars` roots are spread and sane; crowd
  fullness ramps 0.6→1.0 with the crowd meter; the engine writes per-draw
  object AND bone uniforms into per-(mesh,instance) slots
  (`Rnd_Wgpu_RB3.cpp` `RB3MeshEntry::UniformSlot`, :3543-3604 + :4203-4225).
  No multimesh transform collapse.
- **Bones track the per-instance root**: CHAR-DRAW probe logs
  `bone_pelvis` world pos tracking rootPos per instance across dozens of
  positions (8626 log), incl. ±1.3u idle bob ⇒ the archetype skeleton both
  re-poses per instance and animates.
- **Polling**: de-aliased census (prime-modulus sampling, 8627 log) shows ALL
  8 archetypes polled uniformly (12-15 samples each). An earlier "never
  polled" readout was an aliasing artifact of mod-240/600 sampling against the
  fixed per-frame call order — noted here so nobody re-derives it.
  (`WorldDir::Poll`'s `kProcessPost` throttle, Dir.cpp:135, is real but is NOT
  starving the crowd in current builds.)
- **WorldCrowd lifecycle**: factory registered (`WorldInit()`, src/App.cpp:376),
  11 WorldCrowds construct+Enter per run (6 venue + 5 vignette).
- **2D imposter path inert here**: every small_club crowd is authored
  `force_3D_crowd=TRUE` ⇒ `Set3DCharAll()` clears `mInstances` ⇒
  `numInstances==0` ⇒ the broken imposter code (§2.4) never executes in this
  venue. Zero IMPOSTER-DRAW probe hits all runs.

### 2.4 LATENT: the Wii 2D-imposter crowd pipeline is structurally dead on native

For venues with non-force3D bowl crowds (arena/big_club/festival — `strings`
shows 800+ WorldCrowd refs in big_club milos), `WorldCrowd::DrawShowing`'s 2D
path will run and is broken three ways:

- `WiiRnd::GetSharedTex` → **no-op stub returning null**
  (`native/src/band3_link_stubs.s:667`) ⇒ `SetMatAndCameraLod()`
  (Crowd.cpp:82-86) leaves `gImpostorCamera` with no TargetTex and
  `gImpostorMat` with no diffuse.
- `WiiRnd::PrepareRenderAlley`/`RestoreRenderAlley` → no-op stubs (:669-672).
- Consequence: `gImpostorCamera->Select()` + archetype `DrawShowing()`
  (Crowd.cpp:532-537) renders each archetype **into the MAIN framebuffer at
  world origin** (charXfm.v=(0,0,0), :506-509) through a full-frame imposter
  projection — a literal "all archetypes merged at one location" splat — and the
  billboard rows draw untextured (white fallback) with kBlendSrc+alphaCut.

The engine already has the machinery to make this work: lazy mid-frame RTT off
`RndCam::sCurrent->TargetTex()` (`BeginDrawTarget` :1731, hook in DrawMesh :3140
/ DrawRect :2961; END via `RndCam::Select()`/`SetTargetTex` →
`RndTex::FinishDrawTarget` :4834; rndobj/Cam.cpp:51-71). Fix design in §3.

### 2.5 BLOCKER for testing other venues: native always loads `small_club_01`

`BandDirector::EnterVenue`'s HX_NATIVE bridge (src/system/bandobj/
BandDirector.cpp:610-654) reads the world's authored `venue` property (fallback
`small_club_01`) and never consults `MetaPerformer`. Verified live:
`{meta_performer set_venue_override arena_06}` sticks (handler at
MetaPerformer.cpp:1703) but the loaded venue stays small_club (zero arena asset
touches). Until bridged, arena/festival crowd (and §2.4) cannot be exercised.

Aside (unrelated, for whoever owns endgame): one run SIGABRT'd at song end in
`ui/endgame/endgame_helpers.dta(64):meta_performer` (backtrace in
`/tmp/rb3-rpcrowd-8625.log`).

## 3. FIX DESIGN

### Fix A (primary, fixes the user-visible symptom): crowd skeleton rebind

Port the band-character rebind to crowd archetypes — rb3 repo only, HX_NATIVE
additive, engine untouched:

- Where: mirror `BandCharacter::RebindOutfitBonesToOwnSkeleton()` (see
  `src/band3/bandobj/BandCharacter.*` + CHAR_SKINNING_DEFORM_INVESTIGATION.md).
  For each crowd archetype `Character` (Type()==crowd; reachable from
  `WorldCrowd::mCharacters[i].mDef.mChar`, or generically for any crowd-type
  char at Enter), walk its skinned meshes' bone lists and re-`SetBone()` any
  bone whose trans does NOT resolve inside the archetype's own dir to the
  same-named trans found in the archetype (`CharUtlFindBoneTrans(name, char)`),
  computeOffset=false — exactly the band fix's semantics.
- When: after venue load / `WorldCrowd::Enter` (Crowd.cpp:858) — the band fix
  runs from Poll() after Character::Poll for timing reasons; for crowd, Enter +
  first-Poll are both viable; copy the band fix's timing (first Poll after
  enter) to avoid the magnet-no-op trap documented in the investigation doc.
- Why this is sufficient: bones already track the per-instance root and animate
  (§2.3); only the cross-instance bindings poison the palette. Once palettes are
  self-consistent, ratio returns to ~1.0, the shard guard passes the bodies, and
  the crowd renders posed + animated at the spread positions.
- Alternative considered and rejected: relaxing the shard guard — wrong layer;
  the palette genuinely spans 2000u, the guard is correctly refusing to draw a
  torn mesh.
- Root-cause alternative worth 30 minutes during impl: make the vignette and
  venue NOT share/alias the crowd char objects (the mixing only exists because
  two same-named char dirs coexist during the dwell-warm overlap). If the
  bone-name resolution that mixes them is in a shared-subdir lookup, scoping it
  to the owning dir may be a cleaner fix than rebinding — but the rebind is the
  proven pattern.

Verify clip-driving afterwards: crowd excitement anims are DTA-driven
(`BandDirector::SetCrowd` BandDirector.cpp:820 ← song PropAnim `crowd` keys
:1264 → venue scripts → `{<crowd> iterate_frac ...}` →
`{main.drv play_group_flags bad|ok|great ...}`, see extracted
`world/crowd.dta`). If post-fix the crowd idles but never reacts, instrument
`WorldCrowd::OnIterateFrac` (Crowd.cpp:990) — separate, smaller issue.

### Fix B (latent 2D imposter pipeline; do with/after the §2.5 venue bridge)

1. Strong `WiiRnd::GetSharedTex` def in `native/src/` returning one persistent
   square render-target `RndTex` (~256×256; set Width/Height so
   `BandRnd::BeginDrawTarget` accepts it — it lazily creates the GPU RT and
   registers the view in `sTexGpu` so the billboard material samples it,
   `Rnd_Wgpu_RB3.cpp:1731-1760`). `PrepareRenderAlley`/`RestoreRenderAlley`
   stay no-ops (the lazy TargetTex redirect supersedes them).
2. Port per-instance camera-facing billboarding from `WiiMultiMesh::DrawShowing`
   (src/system/rndwii/MultiMesh.cpp:111-191 — instance TRANSLATION + current
   cam world basis) into an HX_NATIVE branch of `RndMultiMesh::DrawShowing`
   (src/system/rndobj/MultiMesh.cpp:162), else quads keep authored orientation
   and show edge-on.
3. Possible engine change (the only one): `BandRnd::WriteSceneUniforms` uses
   window aspect (`Rnd_Wgpu_RB3.cpp:1106`); the imposter cam is authored square
   (kAspect=1.0, Crowd.cpp:419/453). When `cam->TargetTex()` is set, use the
   target's aspect — also benefits the existing clouds RTT. If done: commit in
   `../milo-native-engine`, bump `MILO_ENGINE_PIN`.
4. Check the RT-compatible pipeline variant selection (:4602) covers the
   skinned layout (the imposter render draws a skinned char into the RT).

### Fix C (enabler): venue bridge honors MetaPerformer

In `BandDirector::EnterVenue`'s HX_NATIVE block (BandDirector.cpp:629), prefer
`MetaPerformer::Current()->GetVenue()` when non-null over the world-property/
small_club_01 fallback. One line + null guards; unlocks venue variety AND the
only way to verify Fix B.

Match-safety: everything above is HX_NATIVE-gated or lives in native/src /
engine; Wii asm untouched.

## 4. VERIFICATION

```bash
# Build (worktree pattern; main-repo build also fine for the impl agent):
cmake --build native/build-native --target rb3-native
# Run to gameplay with the engine's existing diagnostics:
CROWD_DBG=1 SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 scripts/native/keyboard-to-gameplay.py \
  --port 862X --diff hard --out /tmp/rp-crowd/verify --game-burst 12
# (CROWD_DBG only exists if the scout worktree probes are kept; SHARD_* are engine-side and enough.)
```

Pass criteria for Fix A:

1. `grep -a SHARD_GUARD /tmp/rb3-kbd2game-862X.log | grep crowd_body | wc -l`
   drops from ~100k+ to ~0 (transients tolerated).
2. `grep -a 'SHARD_RATIO' ... | grep crowd_body` shows ratio ≈ 1.0-1.9, never ~25.
3. Visual: `{band_director force_shot "coop_dir_crowd.shot"}` then two
   `/api/screenshot`s 2-3 s apart — full bodies (compare `crowdshot_06/07.png`
   baselines: no floating heads, no green statues), pose differs between the
   two frames (animation).
4. Band chars unregressed (the rebind must NOT touch band members —
   `scripts/native/song-select-capture.py` + a gameplay burst as usual).

Pass criteria for Fix B (requires Fix C): in arena/big_club,
`IMPOSTER-DRAW ... targetTex=0x...` non-null (scout probe) or simply: no
full-screen character splat, textured billboard rows visible in wide shots,
far-crowd texture changes frame to frame.

## 5. REFERENCE SCREENSHOTS NEEDED

- Retail small_club gameplay wide / crowd-cam shot (crowd density + idle motion
  baseline) — any RB3 small-club YouTube run.
- Retail arena/festival wide shot showing the 2D bowl crowd (Fix B look target).

## Appendix — probe inventory

| artifact | what |
|---|---|
| `/tmp/rb3-kbd2game-8624.log` | lifecycle: 11 WorldCrowds ctor/Enter; force3D=1 + instances=0 everywhere; fullness ramp |
| `/tmp/rb3-kbd2game-8626.log` | CHAR-DRAW rootPos vs bone_pelvis pos: bones track roots + idle bob; two world clusters ~2040u apart |
| `/tmp/rb3-kbd2game-8627.log` | de-aliased CHAR-POLL census (all 8 archetypes uniform); SHARD_GUARD mass-drops of crowd bodies; SHARD_RATIO 25x with worldExt≈2100-2240 |
| `/tmp/rp-crowd/crowdshot_{06,07,13}.png` | the symptom: heads/statues/remnants |
| worktree `.claude/worktrees/scout-crowd` | CROWD_DBG probes (Crowd.cpp, Character.cpp); keep for impl verification, do not land |

Sampling gotcha for future probes: throttling with `% 240/300/600` aliases
against fixed per-frame call orders and can make "X is never called" look true
when X is called every frame — use prime moduli or per-name counters.
