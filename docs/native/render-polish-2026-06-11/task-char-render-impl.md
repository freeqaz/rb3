# task-char-render-impl — gameplay band chars: invisible bodies / floating eyes / shard fans

Wave-2 implementer, 2026-06-11. Ports 8731-8737. Follows
[scout-char-render.md](scout-char-render.md); read that first.

---

## TL;DR

Two distinct problems were untangled this wave:

1. **Reload-churn fragility (scout's mechanism) — FIXED, default-ON.** StartLoad
   re-fires 5-6x/member (the mid-song trigger = `BandWardrobe::StartClipLoads`
   at song start: Wii-faithful venue clip loading, NOT suppressible) and used to
   wipe the rest snapshot, so re-scans captured "rest" from mid-clip poses →
   garbage inverse-binds → the baseline's pale SHARD FANS and head/hair
   drop-outs. Snapshot now survives non-closet reloads; rest is seeded
   deterministically post-`SetDeformation`; the rebake is two-pass
   (no partially-mutated palettes); latches re-arm on StartLoad + SyncObjects.
2. **Never-rebound `own==bound` garments (the dominant "only teeth/eyes" mass) —
   ROOT-CAUSED, NOT FIXABLE BIND-SIDE; default behavior unchanged.** Most
   dropped band meshes were never rebound because they are ALREADY bound to the
   live per-member skeleton (`own == bound`); their authored offsets fling under
   the native pose-pipeline rotation-basis divergence (C8), the clamp produces a
   mixed palette, and the V24 guard hides the mesh every frame. A full
   rest-rebake experiment (this wave, measured) anchors translation
   (skin-to-bone ≤92u, zero flings, zero mixed anchors) **but cannot repair the
   rotation basis** — far-from-bone verts smear by R·sinθ to persistent
   200-460u world extents (vs ~70u character height) and draw as full-screen
   slabs when un-hidden. The guard was RIGHT about these poses. The experiment
   is kept behind `RB3_BOUND_REBAKE=1` + `RB3_GUARD_EXEMPT_REBOUND=1`
   (both default OFF); the real fix is the C8 pose-math root-cause
   (CharBones/quat-decode basis), a future task.

## What changed (rb3, branch `wt-task-char-render`, commits `7ad80268` + `77fdde8a`)

All HX_NATIVE-gated. Wii build verified byte-identical (see "Wii match safety").

`src/system/bandobj/BandCharacter.{h,cpp}`:

1. **StartLoad re-arm no longer wipes the rest snapshot** (scout step 2).
   `mNativeRestPose` survives non-closet reloads (StartLoad re-fires 5-6x/member
   through the flow incl. at song start); only a closet transition does the full
   reset (outfit/gender swap = genuinely new skeleton pose). Latches still
   un-latch so newly streamed meshes get scanned.
2. **Deterministic rest seeding** — `NativeCaptureRestPoseAfterDeform()`, called
   from `SyncObjects()` right after `SetDeformation()` (the deform clip's
   `PoseMeshes` leaves every deform-driven bone at the weighted gender-bind REST
   pose). Seeds rest for every resolvable skin-mesh bone (distinct AND
   own==bound), with provenance (`mNativeRestDistinct`): distinct captures are
   authoritative and overwrite own==bound seeds once. Poison guard: seeding is
   skipped while a clip is playing (mid-song SyncObjects would capture mid-clip
   poses for bones outside the deform clip).
3. **own==bound rest-rebake — OPT-IN experiment** (`RB3_BOUND_REBAKE=1`,
   default OFF after measurement; see TL;DR #2 and "Experiment outcome").
   `RebindHeadHandsAtRest` can rebake meshes already bound to the Find-resolved
   instance (`mOffset = meshWorld * inv(rest)`, no repoint) and flag
   `mNativeBonesRebound`.
4. **Two-pass apply** — a mesh is only mutated when EVERY bone slot has a
   resolvable bone + rest basis; partial bakes (which hand the clamp a mixed
   palette that the guard then drops) are never left behind.
5. **Torso-name meshes whose bones are all own==bound** fall through to the
   rest-rebake here (the torso rebind can only repoint distinct-resolving meshes;
   such meshes — e.g. one member's `vestdenim`/`trackjacket` copies — previously
   fell to the clamp/guard).
6. **GeomOwner flag propagation** — a geometry-sharing mesh whose owner is
   rebound inherits the flag (the GPU palette comes from the owner; the clamp
   would otherwise freeze its correctly-animating palette).
7. **Latch policy** (scout step 3) — head-rebind latches when no unrebound
   in-scope mesh remains AND 30 quiet Polls pass (covers intra-merge streaming
   gaps), with a 600-Poll give-up purely to bound rescan cost (deviation from the
   scout's "no give-up": members with permanently-unresolvable hair-sim bones
   never reach pending==0 and would re-walk the draw tree every Poll all song).
   StartLoad AND SyncObjects re-arm both rebind latches.
8. **Shared mesh collector** `NativeCollectSkinnedMeshes` factored out of the two
   rebinds (was duplicated; also used by the seeding).
9. **RELOAD_PROBE instrumentation** (scout step 1): `gNativeStartLoadTag` caller
   attribution set at all 8 `BandCharacter::StartLoad` call sites
   (`RecomposePatches`, `MiloReload`, `start_load` DTA, `in_closet` propsync,
   `CharCache::Request`, `BandWardrobe::{StartClipLoads,LoadPrefabPrefs,OnUnloadVenue}`),
   `[STARTLOAD]/[SETDEFORM]/[POSTMERGE]/[REST_SEED]/[CHAR_MESH]/[HEAD_REBIND_PENDING]/
   [HEAD_REBIND_ANCHOR]` probes, plus a global Poll serial for log correlation.

`src/system/bandobj/BandWardrobe.cpp`, `src/band3/meta_band/CharCache.cpp`:
caller-tag one-liners (HX_NATIVE) only.

## What changed (engine, branch `wt-task-char-render`, commits `26508a2`, `da8445c`, `4e9a604`)

`src/platform/Rnd_Wgpu_RB3.cpp`:

- `26508a2` — SHARD_DBG drop lines now print the owning dir + bone0 world
  position (attribution probe; no behavior change).
- `da8445c` + `4e9a604` — V24 ratio-guard exemption for `mNativeBonesRebound`
  meshes, introduced then **demoted to opt-in `RB3_GUARD_EXEMPT_REBOUND=1`
  (default OFF)** after the slab measurement. Default engine behavior is
  unchanged except the SHARD_DBG print. (`4e9a604` supersedes `da8445c`;
  land both or squash.)

## Experiment outcome (why own==bound rebake + guard exemption are default-OFF)

With both ON (runs `final-r1..r3`, ports 8731-8733): band steady-state guard
drops collapsed 27-42/frame → 0-0.56/frame, FLING=0, worst draw-time
skin-to-bone delta 92.5u, zero mixed anchors, zero unresolvable-rest meshes
except 2 hair-sim meshes/run — i.e. by every per-bone metric the palettes
looked sane. BUT the per-mesh extents told the truth (fix4 distribution,
`~/tmp/rp2-char-render/fix4-r1-engine.log`): gloves/fingernails/jackets sat at
**wAvg 200-280u / wMax 300-466u world extent EVERY frame** (a character is
~70u) — translation is anchored but the native **rotation-basis divergence
(C8)** smears far-from-bone verts by R·sinθ. Visually: full-screen pink/grey
slabs (`/tmp/rp2-char-render/final-r{1,2,3}-sheet{A,B}.png`). The V24 guard was
correctly hiding genuinely broken poses. Bind-side baking cannot fix this; the
root cause is the pose pipeline (CharBones/quat decode), out of scope this
wave.

## Root-cause addendum (evidence the scout doc didn't have)

- **Mid-song StartLoad trigger identified** (scout's open sub-question): it is
  `BandWardrobe::StartClipLoads` ×2 per member at song start — once via venue
  `LoadCharacters`, once via `BandDirector::HarvestDircuts()` (the shot-track
  dircut harvest at `BandDirector.cpp:953`). Both load the song's venue clip
  merges (`rigging`, `body_tempo_clips`, instruments...; `[POSTMERGE]` showed the
  cascade) — **Wii-faithful, must NOT be suppressed** (deviation from scout
  step 1: suppressing would strip the band's animation clips). The
  CharCache::Request reloads (6x/member) are all pre-gameplay (menus).
- **The deform "storm" is bounded**: `SETDEFORM` runs every 1-2 frames only
  across the ~200-poll merge-completion window after StartClipLoads, then stops.
  With the rest snapshot preserved + re-arm + two-pass rebake it is harmless.
- **Drop attribution** (engine probe): the per-frame dropped non-crowd meshes are
  the band's own merged outfit resources (empty dir name, bone0 at stage
  coordinates) + `outfit`-dir meshes — NOT venue extras. Names match the
  players' `[CHAR_MESH]` inventories (fingernails/boots/jeans/jackets/hair),
  randomized per run with the band.
- **own==bound ≠ magnet-bound** for these meshes: `[HEAD_REBIND_ANCHOR]` found
  ZERO foreign-anchored bones — every rebaked bone is a trans-descendant of the
  member. The meshes were live-bound all along; their AUTHORED offsets fling
  under the native skeleton basis (the documented C8 basis mismatch), which is
  why the clamp/guard hid them.
- **Wave-5 unpack cache stays exonerated** (scout already proved it; nothing in
  this task touched it).

## Wii match safety

All edits in shared matched source are `#ifdef HX_NATIVE` (or probe one-liners
inside such blocks). Rebuilt the three touched TUs with `tools/ninja-locked` in
the worktree and compared every ELF section against the main repo's objects:
`.text/.data/.rodata/...` byte-identical for `BandCharacter.o` and
`BandWardrobe.o` (only `.strtab` ordering differs — no linked-image effect);
`CharCache.o` bit-identical. Match% unchanged.

## VERIFICATION (shipping config = safe-r1..r3, ports 8731-8733, n=3)

```bash
cmake --build native/build-native --target rb3-native
for i in 1 2 3; do
  RELOAD_PROBE=1 HEAD_REBIND_PROBE=1 SHARD_DBG=1 \
  REBIND_DRAW_SKINPOS=1 REBIND_DRAW_FLING=1 \
  python3 scripts/native/keyboard-to-gameplay.py --port 873$i --diff hard \
      --out /tmp/rp2-char-render/safe-r$i --game-burst 24 --verbose
done
python3 scripts/native/song-select-capture.py --port 8738 --out /tmp/rp2-char-render/songsel
```

Scout pass-criteria scorecard (amendments justified inline):

1. ~~Zero STARTLOAD/SETDEFORM after game_screen~~ — **criterion amended with
   evidence**: the song-start `Wardrobe::StartClipLoads` ×2/member is
   Wii-faithful clip loading (HarvestDircuts + venue LoadCharacters) and must
   not be suppressed. Amended criterion — *reloads must not corrupt the rebind
   state*: **PASS 3/3** (`[STARTLOAD] ... restPose=107-113` preserved through
   every reload — baseline wiped it to 0; `[REST_SEED] SKIPPED (clip playing)`
   poison guard active; latches re-arm and re-latch).
2. Non-crowd SHARD_GUARD drops ≈ 0/frame — **NOT MET (32-35/frame, = the
   27-35/frame baseline)**: the residual is the own==bound garment class, which
   this wave root-caused as a pose-pipeline (C8) problem the guard is RIGHT to
   hide (see "Experiment outcome"). With the opt-in experiment ON it reaches
   0-0.56/frame, but draws slabs instead.
3. No pale shard fans / no body-less floating-eyes frames — **PASS on the fan +
   floating-eyes classes** across 72 burst frames
   (`/tmp/rp2-char-render/safe-r{1,2,3}-sheet{A,B}.png` vs
   `BEFORE-base-contact.png`, `BEFORE-burst_07/08/09/12.png`): heads, hair,
   faces, hands, torsos render and persist across the mid-song reload (the
   baseline's radiating white/pale fan frames and eyes-floating-alone frames do
   not occur). Residual: broad sub-2.0-ratio smears (same marginal class
   present at baseline; engine guard's documented residual) and garments hidden
   per criterion 2.
4. `REBIND_DRAW_FLING=0` — **PASS 3/3** (and this run set is the first where
   FLING was actually armed: it requires `REBIND_DRAW_SKINPOS=1`, which the
   scout's command set omitted — earlier FLING=0 readings were vacuous). Worst
   `REBIND_DRAW_SKINPOS` delta 92.5u (fingernails finger bones; 06-09 clean bar
   was 37-65u, fling = hundreds). Song-select capture: **PASS** (5/5 frames,
   no regression; `/tmp/rp2-char-render/songsel/`).
5. Walk-in legs track — **NOT EXPLICITLY VERIFIED**: the venue cameras in the
   captured bursts didn't isolate a clean walk-in leg shot; no lifted-leg frames
   observed in the early bursts. Bare-skin legs/feet render (body meshes are
   distinct-rebound); jeans/boots may be guard-hidden per criterion 2.

Net: the reload-fragility regression class (scout's primary mechanism) is fixed
and proven; the deeper own==bound garment invisibility is root-caused with an
A/B-able experiment but needs the C8 pose-math fix to close. **verified=false
for the full user symptom; partial, evidence-backed improvement shipped.**

Evidence inventory: `/tmp/rp2-char-render/` (BEFORE-*, safe-r*/, safe-r*-sheets,
final-r*-sheets = slab experiment, fix*-sheets = intermediate, songsel/);
engine logs preserved in `~/tmp/rp2-char-render/*-engine.log` (note: /tmp logs
get reaped — the `~/tmp` copies are the durable ones).

## LANDING NOTES (orchestrator)

- **rb3**: cherry-pick `7ad80268` + `77fdde8a` from `wt-task-char-render`
  (or squash). Only `BandCharacter.{h,cpp}`, `BandWardrobe.cpp`,
  `CharCache.cpp` touched — all HX_NATIVE-gated; Wii sections verified
  byte-identical. CharCache.cpp edit is 3 lines (probe tag) — low conflict
  risk with sibling tasks; BandCharacter.cpp is heavily edited — if another
  task touched it, take mine and replay theirs.
- **engine**: branch `wt-task-char-render` = `26508a2` + `da8445c` + `4e9a604`
  (the last reverts the middle one's default; squashing all three to one
  commit is cleanest). Default engine behavior change = SHARD_DBG print only.
  After landing engine, bump `MILO_ENGINE_PIN` in `native/CMakeLists.txt`
  (current pin `8fb669d...`); coordinate with sibling engine-touching tasks
  (crowd, gem-*, fret-held, menu-lighting all may have engine commits) — land
  engine commits in one pin bump.
- **Conflict watch**: `Rnd_Wgpu_RB3.cpp` is also the crowd/gem tasks' likely
  file; my edits are confined to the V24 guard block (~L4128) + the SHARD_DBG
  drop print (~L4190).
- **Worktrees left in place**: rb3 `.claude/worktrees/task-char-render`
  (branch `wt-task-char-render`), engine
  `milo-native-engine-worktrees/task-char-render` (branch
  `wt-task-char-render`).
- **Follow-up task to file (the real remaining fix)**: C8 pose-pipeline
  rotation-basis divergence — native CharBones/quat-decode produces a basis the
  authored inverse-binds don't expect; affects every own==bound garment
  (gloves/nails/boots/skirts/jackets) + likely the crowd/extras clamp class.
  The A/B levers (`RB3_BOUND_REBAKE`, `RB3_GUARD_EXEMPT_REBOUND`) + the
  attribution probes (`RELOAD_PROBE`, `[HEAD_REBIND_PENDING/_ANCHOR]`,
  SHARD_DBG dir+bone0) are in place for it. A good starting measurement:
  compare a single bone's WorldXfm rotation across Wii (Dolphin) vs native at
  the same clip frame.
- The scout's worktree `.claude/worktrees/scout-char-render` (+ its engine
  worktree) can be torn down; its probe edits are superseded by this branch.
