# task-pose-footwear-shard — band-aware V24 shard guard (footwear/gloves false-drop on normal curl) — FIXED

Render-polish wrap-up item. Ports 9825-9828.
Engine worktree `wt-task-pose-footwear-shard`
(`/home/free/code/milohax/milo-native-engine-worktrees/task-pose-footwear-shard`).
rb3 worktree `wt-task-pose-footwear-shard`
(`/home/free/code/milohax/rb3/.claude/worktrees/task-pose-footwear-shard`).

**STATUS: done. verified: true. needsEngine: true. Wii byte-identical: rb3 `git diff src/` is EMPTY.**

---

## TL;DR

The wave-5 WorldXfm recompose (`15ce606`) repaired the band POSE. The residual
band-garment guard-drops (footwear/gloves/legwear at ratio 2.2-3.5, bone0 at sane
body height) are a **FALSE POSITIVE in the fixed 2.0x V24 ratio guard** at
`Rnd_Wgpu_RB3.cpp:4915` — a small-bind garment legitimately exceeds 2.0x its tiny
bind AABB on a normal limb curl without the world extent ever becoming
geometrically impossible. Fixed the GUARD, not the pose: gate it on band membership
(bone owning-dir contains `skeleton_unshared.milo`, the wave-6 rebake's detector)
and, for band garments only, relax to a wider ratio cap + an absolute world cap +
a small-world floor; crowd/extras/instrument/UI keep the proven 2.0x EXACTLY.

**Result: band-class guard drops 12,597 → 0; crowd/UI/instrument drops unchanged;
genuine band flings (negative control) still drop 150K-190K; no NaN/crash; song→endgame stable.**

---

## The fix (engine-only, native-only file)

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, the V24
`degenerate` test (was a single line at L4915; now a ~50-line block). Engine commit
`5962059` on branch `wt-task-pose-footwear-shard` (one commit on top of pin `15ce606`).

1. **Detect band membership** at the guard site by walking the mesh's bones
   (`owner->BoneTransAt(b)->Dir()->mStoredFile`) for `"skeleton_unshared.milo"` —
   the SAME detector the wave-6 rebake uses (L4042). Early-out on first match;
   band garments match at a low bone index.
2. **For `bandMember` only**, replace `wext > 2.0f*lext` with three caps:
   - ratio cap **4.0x** (`RB3_BAND_SHARD_RATIOCAP`): deep curl tops out ~3.5x; true tears jump >4.4x.
   - absolute world cap **110u** (`RB3_BAND_SHARD_WORLDCAP`): every legit band garment ≤85u world; a real tear is 85-400u.
   - world **floor 40u** (`RB3_BAND_SHARD_WORLDFLOOR`) below which the ratio test is SKIPPED — so a tiny-bind
     SUBMESH (a glove fingertip submesh binds ~3.8u; a finger curl moves it to ~19u world = ratio 5x but
     geometrically sane) is not false-tripped. The negative control proves NO real band tear is <44.9u world.
   - `!bandMember` (crowd/extras/instrument/UI) keeps `wext > 2.0f*lext` verbatim.
3. **SHARD_RATIO_DBG** log line now prints `band`/`other` classification.

### Why three caps, not the planner's two

The planner's 2-cap design (4.0x ratio + 110u world) closed the boots/legwear/
fingernails residual but exposed a NEW residual the planner explicitly anticipated
in §2c: `gloves_resource.1` is a glove-FINGERTIP submesh with a 3.84u bind extent.
A normal hand pose moves it to ~19.6u world = **ratio 5.0**, tripping the 4.0x cap —
but 19.6u world is geometrically impossible to be a shard (a hand is ~20u across).
The 4.0x ratio is meaningless for a sub-10u bind. The **40u world floor** (derived
from the negative control: min real band tear = 44.86u world) is the principled fix —
exactly the item's "verts within N units of bind" sanity check, expressed on the
already-computed world extent. This is a measurement-backed deviation, not a guess.

---

## Verification (my own A/B, ports 9825/9826)

Harness: `scripts/native/keyboard-to-gameplay.py --diff hard --game-burst N`, engine
stderr in `/tmp/rb3-kbd2game-<port>.log`. Evidence: `/tmp/rp8-pose-footwear-shard/`.

### Symptom 1 — named garments render (drops → 0)

| metric | pre-fix-emulated (band cap=2.0/floor=0) | fix ON (default) |
|---|---|---|
| band-class SHARD_GUARD drops | **12,597** | **0** |
| top band drops | kidgloves 11932, eightholedocs 400, talldocs 265 | (none) |

The verify-pose-fling.md named residual (`lowtopsneaks`/`kidgloves`/`eightholedocs`)
is eliminated. SHARD_RATIO classification confirmed: boots/legwear/larger gloves now
pass (eightholedocs 1.05, kidgloves 1.07, talldocs 1.13, all `band`), and the
gloves-fingertip submesh (3.84u bind, 19.6u world) passes via the 40u floor.

### Symptom 2 — genuinely-broken meshes STILL dropped (guard not neutered)

**(a) Crowd / extras / UI unchanged** (fix on): `clap.mesh` 112 (dir=`crowd_male03`,
ratio 2.0), `male_extras_hair02`/`eyebrows11` 62/62, `scrollbar_bg` 490. ALL
classified `other` → unchanged 2.0x path. Zero out-of-scope meshes wrongly classed `band`.

**(b) In-class negative control** (`RB3_NO_SKEL_WORLDFIX=1`, real band fling
re-introduced): band-class tears STILL drop **192,639** — nailboots 156u, talldocs
224u, escapeartist 244u, vestandtank 312u, kidgloves 284u — all caught by the world
cap and/or ratio cap. Zero high-ratio band tears escaped. Min real tear = 44.86u world.

**(c) No NaN/inf/assert/segv/abort** across all runs (0 hits, ports 9825+9826).

### Symptom 3 — threshold is a stable plateau, not a knife-edge

`RB3_BAND_SHARD_RATIOCAP` sweep: band drops **0 at 4.0 AND 5.0** (stable plateau
above the 3.5x legit-curl envelope), jump to **925 at 3.0** (3.0 clips legit boots).
At the loose edge (ratiocap=5.0, floor=50) the fling control STILL drops 151,105 band
tears with zero large-world (>110u) escapes — the world cap holds. The chosen
defaults (4.0 / 110 / 40) sit in the safe interior.

### Symptom 4 — no scene regression

`song-end-test.py --require-endgame` (port 9825): full song → game-over →
`coop_endgame_screen` STABLE 25s / 2308 frames, no abort. Menu/song-select have no
band skinned meshes → branch is a no-op there.

### Visual

`/tmp/rp8-pose-footwear-shard/after2/burst_04.png` (pink venue closeup): foreground
guitarist solid-bodied, holding guitar, no shards, no flung limbs, no missing-garment
gaps. `burst_19.png` (club venue): band on stage coherent, no screen-crossing teal
shards. (Roster/camera randomize per run so a frame-matched A/B isn't available; the
per-mesh drop counts are the authoritative evidence, per the campaign's
"measurement, not a hero closeup" principle. The pink wash is the separate known
venue-lighting residual.)

---

## What I changed

- **rb3 src: NOTHING** — `git diff src/` is EMPTY → Wii build byte-identical by construction.
- **rb3 `native/CMakeLists.txt`**: pin bump `15ce606` → `5962059` (the only rb3 change), commit `3c89d897`.
- **engine** (`wt-task-pose-footwear-shard` @ `5962059`): `src/platform/Rnd_Wgpu_RB3.cpp`
  — the band-aware guard block + SHARD_RATIO_DBG classification. One self-contained commit on top of pin `15ce606`.

---

## Landing notes (for the orchestrator) — EXACT files + regions

- **Engine cherry-pick:** `5962059` (single commit) onto engine main on top of `15ce606`.
  - File: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, function `BandRnd::DrawMesh`.
  - Region: the V24 `degenerate` predicate — was the single line
    `bool degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f * lext);`
    at L4915, now a ~50-line block (the `bandMember` bone-scan + the 3-cap
    band branch + the unchanged `else` 2.0x), PLUS a 1-line tweak to the
    `[SHARD_RATIO]` fprintf format string immediately below it (adds `band`/`other`).
  - **Re-anchor** on `bool degenerate = (wext > 15.f) ...` if line numbers moved.
- **rb3 pin bump:** after landing the engine commit on engine-main, set
  `MILO_ENGINE_PIN` in rb3 `native/CMakeLists.txt:74` to the FINAL landed engine-main
  SHA (my rb3 commit `3c89d897` pins the wt SHA `5962059` as a placeholder — rewrite it).
- **Conflict risk with sibling engine tasks: LOW.** The hunk is confined to the V24
  ratio-guard region (~L4912-4946). It does NOT touch:
  - the wave-5 WorldXfm recompose (L4104-4156),
  - the rebake (L4015-4100, though it READS the same `skeleton_unshared.milo` detector at L4042 — no edit there),
  - the per-bone composed-skin finite guard (L4203-4236),
  - the IK_SHARD_VERT / C8_PROBE diagnostics (L4849-4898 / L4516+),
  - the bloom/halo composite or `standard_wgsl.inc` (different file).
  If a sibling also edits `Rnd_Wgpu_RB3.cpp`, the regions are disjoint; land in any order.

## Risk / match-neutrality

- **Wii byte-identical** by construction: only `Rnd_Wgpu_RB3.cpp` (native-only,
  `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, not in the Wii image, not DC3-compiled)
  + the rb3 pin line changed; rb3 `src/` diff empty.
- **DC3-inert:** the band branch fires only when a bone resolves to
  `skeleton_unshared.milo`, which DC3 does not load.
- **Guard not neutered:** relaxation gated on a positive `bandMember` skeleton-file
  match; crowd/extras/instrument/UI keep 2.0x. The 110u world cap + 40u floor are
  both below the negative control's measured real-fling envelope (min 44.9u world),
  so a real band tear can never slip through. Proven by the (b) negative control.
