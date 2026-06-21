# Scout — Residual Shard-Drop Root-Cause + Fix Classification

Analysis-only companion to `probe-data.md`. For each mesh the V24 `[SHARD_GUARD]`
silently drops (so the screen looks clean but geometry vs retail is missing), this
roots the cause to a file:line and decides **fix vs accept**. No build, no run, no
edits — all on-device numbers are quoted from `probe-data.md`.

Source anchors used throughout:
- Guard: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4907-5141`
  (classify `bandMember` 4073-5081; band-relaxed caps 5098-5106; `other` strict 2.0x 5108).
- "skinned" = `RndMesh::IsSkinned() = !mBones.empty()` (`src/system/rndobj/Mesh.h:251`)
  — **any** mesh with ≥1 bone is ratio-tested, including UI widgets.
- Rebinds (all are `BandCharacter` methods, called ONLY from `BandCharacter::Poll`
  `src/system/bandobj/BandCharacter.cpp:526,543`; confirmed grep — never reach
  extras/crowd/UI): `RebindOutfitBonesToOwnSkeleton` (953, torso-only), `RebindHeadHandsAtRest`
  (1105, head/hair rest-rebake), `RebindInstStringsToRestBasis` (1395, rigid-anchor strings).

The probe's drop table (guard ON, full run):

| mesh | drops | bind~ | world~ | ratio | owning dir | bone0 | guard class |
|---|---|---|---|---|---|---|---|
| `scrollbar_bg.mesh` | **607** | 80.8 | 324.1 | 4.0 | `scrollbar` | (0,-0,40) | other (UI) |
| `clap.mesh` | 128 | 51.3 | 109.4 | 2.1 | `crowd_male03` | (22,-3,57) | other (crowd prop) |
| `male_extras_hair02.mesh` | 62 | 14.6 | 36.6 | 2.5 | `male_extras02` | (-227,72,184) | other (vignette extra) |
| `male_extras_eyebrows11.mesh` | 62 | 4.9 | 23.2 | 4.7 | `male_extras11` | (-164,598,-43) | other (vignette extra) |

`lowtopsneaks_skin` (the prior-batch "band shoe" residual) did NOT manifest this
song — see §5; it is band-relaxed-cap-protected here (ratio 1.7, 0 drops, probe §5).

---

## (a) `scrollbar_bg.mesh` — UI STRETCH-MESH MISCLASSIFIED AS A CHARACTER SHARD

### What / where
Not a character mesh at all. It is the **background of the UI list scrollbar
widget**. Asset: `ui/resource/gen/scrollbar_display.milo_xbox` (and the
`scrollbar_accomplishments` variant). The milo holds, with `.mat`+`.mesh` for each:
`scrollbar`, `scrollbar_bg`, `scrollbar_bg_bone_top`, `scrollbar_bg_bone_bottom`,
`scrollbar_bone_top`, `scrollbar_bone_bottom` (`strings` dump).
Bound to the UI widget bones, NOT a `skeleton_unshared`/`CharServo` skeleton.
It is **globally preloaded**: `config/preload_subdirs.dta:80`
(`"ui/resource/scrollbar_display.milo"`) — resident in every screen incl. gameplay.

The widget DTA (`ui/ui_objects.dta:568-598`, `ScrollbarDisplay`) declares the four
endpoint meshes as "bones": `top_bone`/`bottom_bone` =
`scrollbar_bg_bone_top.mesh`/`..._bottom.mesh`; the C++ `ScrollbarDisplay::Update`
(`ScrollbarDisplay.cpp:191-198`) `Find<RndMesh>`s them and stores them.

### Root cause of the "explosion" — IT IS A DESIGNED 200u STRETCH, NOT A BUG
`ScrollbarDisplay::UpdateScrollbarHeightAndPosition` (`ScrollbarDisplay.cpp:130-144`)
explicitly drives the bottom endpoint bone `mScrollbarHeight` units below the top:
`m_pBottomBone->SetLocalPos(top.x, top.y, top.z - mScrollbarHeight)`. The ctor
default is `mScrollbarHeight = 200.0f` (`ScrollbarDisplay.cpp:21`). `scrollbar_bg.mesh`
is skinned between top and bottom, so at the default height it is **stretched to ~4×
its bind**: bind 80.8u → world 324.1u, **ratio 4.01** — the probe's exact number
(`80.8 → 324.1 = 4.01`). The guard classifies it `other` (no `skeleton_unshared`
bone → bandMember=false, `Rnd_Wgpu_RB3.cpp:5073-5081`) and applies the strict
`wext > 2.0*lext` cap (`:5108`) → 4.0 > 2.0 → DROP, every frame (607 drops).

This is **NOT** the char-skinning-deform family. There is no wrong-skeleton-basis
bind and no per-member-skeleton involved. The mesh is doing exactly what it was
authored to do (stretch a list-background ribbon). It also legitimately trips the
original-game old-format warning: `gAltRev < 3 && NumBones() > 1` →
`"--->Arvin/Diana: Skinned mesh needs to be re-exported: scrollbar_bg.mesh"`
(`src/system/rndobj/Mesh.cpp:902-906`) — seen verbatim in the guard-ON log. The
scrollbar bg is a low-rev multi-bone stretch mesh; the warning is dev-noise, not a
native fault.

### The ACTUAL bug = a preloaded UI scrollbar is drawn IN THE GAMEPLAY 3D WORLD
The visual (probe §6 + `shots/guardoff_coop_dir_d_lt00_B.png`, re-inspected here)
is an ornate teal filigree ribbon sprawled across the **note highway**. A song-list
scrollbar background must never appear in the gameplay venue at all — it should only
draw via `ScrollbarDisplay::DrawShowing` (`ScrollbarDisplay.cpp:173-183`), which
gates on `mAlwaysShow || m_fSavedScale < 1.0f` and draws the resource dir at the
**widget's** WorldXfm. Here the bg mesh is being walked by the gameplay world draw
tree directly (bone0=(0,-0,40), at the highway, not at a UI panel) at its preloaded
default 200u stretch — i.e. the preloaded `scrollbar_display.milo` resource dir is
reachable from the 3D scene render and drawn unconditionally, bypassing
`DrawShowing`. The guard is masking a **screen/draw-tree leak**, not a deform bug.

### Fix approach
This is a **UI-mesh-in-3D-scene leak**, not a skinning fix. Two tractable options;
prefer the first.
1. **Stop drawing the preloaded UI scrollbar resource in the gameplay world.** The
   real convergence fix: the gameplay render must not walk
   `ui/resource/scrollbar_display.milo`'s dir. Locate where the preloaded scrollbar
   resource dir gets parented/drawn into the venue/world tree at gameplay (it should
   only be drawn by an active `ScrollbarDisplay::DrawShowing` with an attached list).
   Likely a native screen-teardown / draw-tree-scoping gap (cf. MEMORY
   "MusicLibrary Text-class stale-slot text overlap — 360-ARK draws unused slots Wii
   hid"; same family of native-only over-draw). **Code:** native/src screen or world
   draw scoping under `HX_NATIVE`; possibly engine draw-tree walk. No `src/` deform
   change.
2. **Guard-classification fallback (band-aid, current state):** keep dropping it.
   Cheap and already invisible, but it is a *mask* — retail does not draw this at all,
   so dropping ≈ matches retail by accident. If option 1 is deferred, the drop is the
   correct stopgap (dropping a UI ribbon that should not be in-scene = zero visual
   regression).

**Do NOT** rebind/rebake it — there is no skeleton to rebind to; the stretch is
intentional. Risk of option 1: must not break the scrollbar when it IS legitimately
shown (song select, accomplishments) — scope the change to the gameplay/venue draw
path only.

### Convergence value: **MEDIUM-LOW (visible but the drop already converges).**
71% of all drops, and visually loud WITHOUT the guard — but WITH the guard the
result (no scrollbar on the highway) already matches retail. So the guard's drop is
*accidentally correct*; the clean fix (option 1) removes the band-aid and the
old-format warning but yields no further visible gain over today's guarded render.

---

## (b) `male_extras_*` (venue "vignette" extras: hair02, eyebrows11) — TRUE CHARACTER SHARD, but on a NON-band servo skeleton

### What / where
Venue vignette background people. Asset:
`char/extras/gen/male_extras02.milo_xbox`, `male_extras11.milo_xbox`, etc. Spawned
by the venue: `world/venue/arena/arena_01/gen/arena_01.milo_xbox` references
`male_extras01..04`, `female_extras01..04` by name. Driven by a full
**`CharServoBone` / `Character` servo skeleton** (the milo contains `Character`,
`CharServoBone`, and a complete bone set: `bone_L-brow1..3`, `bone_L-eye*`,
`bone_L-cheek*`, `bone_L-lipcorner`, plus body `bone_L-ankle/knee/hand/index*`).
The two dropped meshes are face/hair geometry: `male_extras_hair02.mesh` (hair) and
`male_extras_eyebrows11.mesh` (eyebrows). bone0 well away from the band
((-227,72,184) / (-164,598,-43)) = background vignette positions.

Crucially: the extras milo has **NO `skeleton_unshared.milo`** (grep empty) → the
guard correctly sets `bandMember=false` (`Rnd_Wgpu_RB3.cpp:5073-5081`) → strict
2.0x cap → hair02 2.5x DROP, eyebrows11 4.7x DROP. Classification is correct.

### Root cause of the explosion
This **IS** the char-skinning-deform family (wrong/divergent skeleton basis vs the
authored inverse-bind), but on the **servo skeleton of a non-band character**. The
guard's own header (`Rnd_Wgpu_RB3.cpp:4907-4919`) describes exactly this: "the
crowd / extras characters' servo skeletons can momentarily produce a FINITE-but-wrong
bone pose … a vertex weighted to that bone flings". Long-thin face geometry (hair
strands, eyebrows) is the most sensitive — a small rotation-basis error θ flings a
vert at radius R by R·sin θ (`Rnd_Wgpu_RB3.cpp:4977-4984` IK_SHARD_VERT note). This
is the same geometry class the BAND rebind DELIBERATELY skips: `RebindOutfit...` is
**torso-only** precisely because "head/hair/finger geometry shards under the
rotation-basis mismatch" (`BandCharacter.cpp:940-951, 1090-1104`).

### Fix approach
A **new servo-skeleton rest-rebake for extras/crowd**, modeled on
`RebindHeadHandsAtRest` (`BandCharacter.cpp:1105`), which already solves the
identical head/hair shard for band members: capture each servo bone's REST WorldXfm
at first pose, bake `mOffset = meshWorld · inv(restWorld)`, bind to the live bone →
coherent at rest, follows the servo animation correctly.

The blocker: extras/crowd are **NOT `BandCharacter`s** (`BandCharacter : public
Character` `BandCharacter.h:41`; extras are plain `Character`/servo), so none of the
existing rebinds run on them. The fix must live where extras/crowd are actually
polled — i.e. a rest-rebake on `Character`/`CharServo` itself, or a native-side
hook that walks the venue's extras/crowd dirs. **Code:** either a new method on
`Character`/servo in `src/system` under `HX_NATIVE` (Wii-neutral), or a native/src
venue-poll hook that collects extras/crowd skinned meshes and rest-rebakes them
(reusing the `NativeCollectSkinnedMeshes` + rest-snapshot pattern). Engine guard
unchanged (rebound meshes already set `mNativeBonesRebound` → guard skips,
`Rnd_Wgpu_RB3.cpp:4940-4942` exempt path / `:5526` latch).

**Risk/blast-radius: HIGHER than the band rebinds.** (1) It touches the
generic `Character` path that ALL crowd/extras share (hundreds of instances —
probe §7: crowd=292) — a basis mistake regresses the whole audience, not one band
member. (2) The "rest pose" capture timing is harder than for band (the band rebind
relies on `SetDeformation` gender-bind rest before `Character::Poll`;
extras/crowd's first-reachable frame may already be animating, exactly the failure
`RebindOutfitBonesToOwnSkeleton` hit with `calcOffset=true`, `BandCharacter.cpp:946-947`).
Needs the rest-snapshot-at-first-Poll discipline of `RebindHeadHandsAtRest`.

### Convergence value: **LOW (small, peripheral, far from camera).**
Visible-impact is small: hair02 world 36.6u, eyebrows11 23.2u (probe table) — tiny
face slivers on a background vignette person at (-227,72,184) / (-164,598,-43), far
from the band/highway focus. The drop removes 2 hair/brow submeshes from a
background extra for the frames the pose is bad — barely perceptible. Fix tractability
is MEDIUM (clear pattern from `RebindHeadHandsAtRest`) but blast-radius is large
(whole crowd path). Net: low convergence ROI.

---

## (c) `clap.mesh` — CROWD HAND-PROP SHARD (held prop on the servo skeleton)

### What / where
A crowd character's clapping-hands prop. Asset: `char/crowd/gen/crowd_male03.milo_xbox`
holds `clap.grp` + `clap.mesh`, weighted to `bone_R-hand` (strings dump shows
`clap.mesh` adjacent to repeated `bone_R-hand.mesh`). Owning dir `crowd_male03`,
bone0=(22,-3,57). Same `CharServoBone` family as the extras (b). No
`skeleton_unshared` → guard `other`, strict 2.0x. ratio 2.1 → DROP (128×).
(Same prop family as `handclap_bank.milo_xbox` in `sfx/gen/`.)

### Root cause
The guard header's canonical example: a "held prop … weighted to that bone flings"
when the servo hand bone hits a finite-but-wrong pose (`Rnd_Wgpu_RB3.cpp:4907-4919`).
It is a rigid-ish prop riding ONE animated hand bone, so when that bone's basis
diverges the whole prop AABB inflates. ratio 2.1 (51.3u→109.4u) is the **mildest**
of the four — right at the 2.0x cap edge.

### Fix approach
Cleanest pattern: **rigid-anchor**, exactly like `RebindInstStringsToRestBasis`
(`BandCharacter.cpp:1395`, which rigid-anchors the band guitar `_strings` mesh to its
least-moving bone and rebakes `offset = meshWorld · inv(anchorWorld)` → ratio ~1.0
through animation). A hand-prop has essentially ONE driving bone (`bone_R-hand`), so a
single-anchor rebake holds ratio ~1.0 trivially. Same non-BandCharacter blocker as
(b): must hook the crowd `Character`/servo path, not a BandCharacter method.
**Code:** the same new `Character`/servo native hook proposed in (b), with a
prop-specific rigid-anchor branch (anchor = the hand bone). Engine unchanged.

**Risk:** medium — narrower than (b)'s face rebake (one bone, one prop), but still
on the shared crowd path. Because ratio is only 2.1, an alternative low-risk option
is a **guard-cap nudge for crowd held-props** (raise the `other` cap for prop-class
meshes, or add a small absolute-world floor like the band path's 40u floor
`Rnd_Wgpu_RB3.cpp:5103`), but that risks re-admitting true shards — the rigid-anchor
is the correct convergence fix.

### Convergence value: **LOW (background crowd, brief flicker).**
A small clap prop on one crowd member at (22,-3,57); 109u world but it's an audience
member behind the band. Dropping it for a frame is near-invisible. Fix shares (b)'s
infrastructure cost.

---

## (d) `lowtopsneaks_skin` — NOT REPRODUCED THIS SONG; already cap-protected

Per probe §5: on the boot song `lowtopsneaks_skin.2.mesh` is **band-classified**
(binds `skeleton_unshared`), ratio 1.7 [0.9–3.5], **0 drops** — fully inside the
band-relaxed caps (4.0x / 110u / 40u-floor, `Rnd_Wgpu_RB3.cpp:5098-5106`). The
closest band edge case is `gloves_resource.1.mesh` (bind 3.8u, ratio 3.7) saved by
the 40u world-floor. So no residual exists here to fix; the prior-batch report was
song/outfit-specific. **Accept as already-converged on this song.** To even observe
a drop, a downstream batch would need a song whose outfit assigns lowtopsneaks AND a
high-motion animation pushing the bound limb past the 4.0x/110u band caps — at which
point it would fall to the SAME band rebind family as the strings/outfit fixes
(`RebindOutfitBonesToOwnSkeleton`/`RebindHeadHandsAtRest`), since it IS a band-outfit
mesh on `skeleton_unshared`. No action this batch.

---

## Ranking by convergence value (visible-impact × fix-tractability)

| rank | mesh | visible impact | tractability | verdict |
|---|---|---|---|---|
| **1** | `scrollbar_bg.mesh` | HIGH raw (607 drops, fills highway w/o guard) but the **guarded** render already ≈ retail | clear (screen/draw-tree scoping, no skinning math) | **Fix as a UI-in-3D-scene leak (option a-1); accept the guard drop as a correct stopgap meanwhile.** Highest ROI because it's a clean non-skinning fix and removes the most drops + the old-format warning. |
| **2** | `clap.mesh` | LOW (background crowd, ratio 2.1, brief) | MEDIUM (single-bone rigid-anchor, like InstStrings) but needs a new crowd/servo hook | **Fix-eligible** via a new `Character`/servo rigid-anchor rebake; defer until the crowd/servo rebind hook exists. |
| **3** | `male_extras_*` | LOWEST (tiny face slivers on far vignette extras) | MEDIUM pattern (RebindHeadHandsAtRest) but HIGH blast-radius (whole 292-crowd path) | **Accept-as-droppable for now**; fix only alongside (2) once a crowd/servo rest-rebake hook is built + adversarially gated. |
| — | `lowtopsneaks_skin` | none this song | n/a | **Accept** (already cap-protected; song-dependent, no repro). |

### Cross-cutting note
(b)+(c) share ONE root cause (non-band servo-skeleton basis divergence) and ONE
missing piece: **there is no rebind for crowd/extras** — all three rebinds are
`BandCharacter`-only (`BandCharacter.cpp:526,543` call sites; grep confirms no other
caller). A single new servo/`Character` rest-rebake hook (rigid-anchor for props,
rest-rebake for face/hair, mirroring `RebindInstStringsToRestBasis` +
`RebindHeadHandsAtRest`) would address both (b) and (c). It is the highest-blast-radius
change of the batch (shared crowd path) and the lowest visible payoff — implement it
last, behind an opt-out env and a strict crowd-wide visual gate. (a) is independent
and is NOT a skinning fix at all.
