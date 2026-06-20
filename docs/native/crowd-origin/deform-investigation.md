# DEFORM INVESTIGATION — the SHARD_GUARD instrument drop is a REAL skin explosion

Date: 2026-06-20. Agent: A (deform fork + root-cause + fix plan, Opus). Engine pin
`MILO_ENGINE_PIN = 884ab17d…` (matches engine HEAD; the A/B + probes ran against it).
Direct continuation of `measure-results.md` + `verify-verdict.md` (band placement
WORKS; the pile is the V24 `[SHARD_GUARD]` dropping deformed `dir='instrument'`
skin). This doc settles the OPEN QUESTION (verify-verdict §4): **guard false-positive
vs real explosion.**

**VERDICT: REAL EXPLOSION (not a guard false-positive).** With `SHARD_GUARD_OFF=1`
the dropped instrument geometry renders as a smeared dark mass intruding from the
LEFT into the highway — a genuine skin-deform blowup the guard was correctly masking.
The root cause is the SAME family as the char-skinning-deform saga: a skinned mesh
bound to the WRONG skeleton. Specifically, the exploding instrument's `*_strings.mesh`
has its neck bones resolved to the CHARACTER skeleton `char/char/main/
skeleton_unshared.milo` instead of the instrument's OWN `<instrument>_resource.milo`
neck bones. The character-skeleton neck bones are spread ~24u and animate, so the
rigid-authored strings mesh (bind 27u) skins to ~136u world (ratio ~5.0) → the guard
drops it. **The guard is CORRECT; the fix is the binding, not the guard.**

NOTE — this does NOT match the verify-verdict §4 hypothesis. Verify guessed the
instrument was a guard false-positive caused by the `bandMember` detector failing to
recognize instrument-on-band-bones (so it got the strict 2.0× cap). FALSE: the
exploding strings mesh IS detected as `band` (its bones DO resolve to
`skeleton_unshared.milo`), gets the RELAXED 4.0×/110u band caps, and STILL trips them
(ratio 4.95 > 4.0 AND world 136 > 110). Relaxing/extending the band caps will NOT fix
this and would pass a genuinely-exploded mesh. Do NOT extend the band-garment
relaxation for instruments.

---

## STEP 1 — THE A/B (SHARD_GUARD_OFF=1)

Env var verified in engine: `SHARD_GUARD_OFF` (Rnd_Wgpu_RB3.cpp:4924-4925). The
guard `degenerate` drop fires only when `guardActive = !getenv("SHARD_GUARD_OFF")`.
`SHARD_RATIO_DBG` computes + logs the bind/world ratio even with the guard off.

Driver: `/tmp/shard_ab.py` (same boot+nav as `crowd-origin-posdump.py`; settles to a
fixed song clock ≈6.2s, captures `/api/screenshot`). Two runs at matched song time:

- (a) guard ON  — `docs/native/crowd-origin/shots/guard_on.png`  (songMs 6231, f3074)
- (b) guard OFF — `docs/native/crowd-origin/shots/guard_off.png` (songMs 6287, f3070)

**What the screenshots show:**
- **guard_on.png:** clean note highway; band members visible at the sides; NO
  screen-crossing shards. 1781 skinned meshes dropped this frame
  (`instrument` 1552, `scrollbar` 105, `male_extras11/02` 62+62).
- **guard_off.png:** a large DARK SMEARED MASS of skin geometry intrudes from the
  LEFT edge toward the highway centre, plus extra clutter on the right. This is the
  un-dropped, EXPLODED geometry — NOT the guitar/kit rendering intact at a correct
  stage spot. 0 drops (guard disabled).

So the guard-off geometry is **exploded/flung, not intact** → **REAL EXPLOSION**,
the guard was masking it.

**Does the deformed skin congregate TOWARD ORIGIN?** NO. The dropped instrument
meshes' bone0 world positions are |95.0–100.7u| from origin (n=1552, min 95.09, max
100.66) — co-located with player0's staged guitar (root (88.9,49.6,13.2), inst
(96.6,59.3,46.6)). The bone is correctly PLACED; the SKIN explodes ~110u IN PLACE
around it. The user's "congregating at origin… incl the drum kit" is the 2D
projection of a ~136u skin smear sweeping across the frame from a correctly-staged
guitar, NOT actual origin placement. CONFIRMS measure/verify: placement is fine; this
is purely a skin-deform blowup.

---

## STEP 2 — ROOT CAUSE (data layer, not just the screenshot)

The guard ratio = blended(world)-AABB / bind(local)-AABB, computed via the exact
4-bone shader blend over the mesh's skinned verts (Rnd_Wgpu_RB3.cpp:4946-5108). A
ratio ≈1.0 means the bones move rigidly (world AABB == bind AABB → no relative
deformation); a high ratio means the bones spread/rotate the verts apart.

### The decisive split (SHARD_RATIO_DBG, within ONE scene)

Instrument `*_strings.mesh` is BIMODAL across band members in the same frame:

| strings mesh | bind | world | ratio | classified | result |
|---|---|---|---|---|---|
| `c20bass_strings` (bass)   | 38.1 | 38.1 | **1.00** | other | FINE |
| `precision01_strings` (bass)| 42.3 | 42.3 | **1.00** | other | FINE |
| `dinky01_strings` (guitar) | 33.9 | 33.9 | **1.00** | own_resource | FINE |
| `telebass_strings` (bass)  | 43.4 | 43.4 | **1.00** | own_resource | FINE |
| `chainsaw_strings` (guitar)| 27.6 | **136.8** | **~4.95** | **band** | **DROP/EXPLODE** |
| `guitar_brain_strings`     | 27.6 | ~137 | ~5.0 | band | DROP/EXPLODE |

`chainsaw_resource.mesh` (1.00) and `chainsaw_teeth.mesh` (1.00) — other sub-meshes
of the SAME exploding instrument — are FINE. Only the `*_strings` sub-mesh blows up.

### WHY — bound to the WRONG skeleton (the saga's root-cause family)

A draw-time bone dump (temp probe `STRINGS_BONE_PROBE`, now reverted — using draw-time
bone WORLD positions + per-bone `|composedSkin.v − boneWorld.v|`, per the saga trap
that mesh-local skinPos is meaningless because a skinned mesh's WorldXfm == identity):

FINE (`precision01_strings`, 8 bones, ratio 1.00):
```
  every bone -> dir='char/main/bass/precision01/precision01_resource.milo'
  bone x-range = [-26.4, -24.9]  => 1.5u spread (RIGID neck), skinDelta 47-74u uniform
```
EXPLODING (`chainsaw_strings`, 10 bones, ratio 4.95):
```
  every bone -> dir='char/char/main/skeleton_unshared.milo'   <-- CHARACTER skeleton!
  bone x-range = [67.1, 90.8]    => 23.7u spread, skinDelta 37-64u (each bone sane)
```

The correlation is perfect and causal: **strings bound to their own
`<instrument>_resource.milo` neck bones skin rigidly (ratio 1.0); strings bound to the
character `skeleton_unshared.milo` bones explode (ratio ~5.0).** Each individual
chainsaw bone's skinDelta is sane (37-64u, same as the bass) — the bones are not
broken in isolation; they are the WRONG bones. The character-skeleton bones named
`bone_nut.mesh`/`bone_bridge.mesh`/`bone_bend_string*` (the guitar attaches to the
character's posing skeleton so the guitarist can flex/vibrate the neck) are spread
~24u apart and animate independently, while the strings mesh authored its verts for a
rigid compact neck (≈1.5u spread, 27u bind). Skinning a rigid-authored mesh onto
spread/animated bones whose rotation BASIS / relative spacing diverges from the
authored inverse-bind → R·sin(θ) + spread smear → 136u world. This is precisely the
char-skinning-deform basis/inverse-bind divergence, now manifesting on `mInstDir`.

### It is a LOAD/MERGE-time mis-binding, NOT a native rebind

`chainsaw_strings` draws with `reb=0 ownerReb=0` — neither the native
`RebindOutfitBonesToOwnSkeleton`/`RebindHeadHandsAtRest` (both scope
`{this, mOutfitDir}` and DELIBERATELY EXCLUDE `mInstDir`, BandCharacter.cpp:723,
719-721) NOR the engine SKEL_REBAKE (Rnd_Wgpu_RB3.cpp:4128) touched it. The strings
mesh was ALREADY bound to `skeleton_unshared.milo` bones at parse/merge time, via the
matched instrument-merge `Filter` path (BandCharacter.cpp:2738-2748 instrument bones;
2762-2770 `bone_`-prefixed bone keep/merge). On Wii the authored inverse-bind composes
correctly against the same-name character-skeleton bones; on native the per-member
skeleton's basis/spacing diverges (same divergence the outfit saga fixed for torso
meshes by repointing them to the live per-member bone with the gender-correct offset).

### bandMember caps are already in play and already insufficient

The engine `bandMember` detector (Rnd_Wgpu_RB3.cpp:5073-5081) DID fire for
`chainsaw_strings` (its owner bones resolve to `skeleton_unshared.milo`), so it got
the RELAXED band caps (4.0× ratio, 110u world, 40u floor; :5101-5106). It still
drops: ratio 4.95 > 4.0 AND world 136 > 110. The verify-verdict §4 fork (extend the
relaxation to recognize instrument-on-band-bones) is therefore MOOT — the mesh is
already treated as a band member and the relaxed caps still (correctly) reject it.

### Factual corrections to prior docs

- The "needs to be re-exported" NOTIFY DOES name instrument `*_strings.mesh` (e.g.
  `c20bass_strings.mesh` / `precision01_strings.mesh`) — contradicting verify §2's
  "never names the guitar." BUT those named ones are the FINE (ratio 1.0) basses, NOT
  the exploding chainsaw. So verify §2's CONCLUSION (the NOTIFY is a red herring for
  the drop) is still correct, even though its specific claim was wrong. Do not use the
  NOTIFY as the lead.
- The exploding instrument is SONG-dependent (`chainsaw`/`guitar_brain` = the lead
  guitar of specific songs). Most instruments (bass + many guitars) are ratio 1.0 and
  FINE. The blowup is NOT universal across instrument strings — it is specific to the
  instrument models whose `*_strings` bones resolve to the character skeleton.

---

## STEP 3 — FIX PLAN (REAL-EXPLOSION fork; do NOT implement here)

The fix is the inverse-bind/rebind path for `mInstDir`'s `*_strings` mesh — the
char-skinning-deform family extended to the instrument the existing rebind excludes.
The guard MUST stay (PLAN.md 0.4); the goal is to make the strings skin compose back
to ~bind so the world AABB collapses and the guard stops firing on its own.

### Primary fix — extend the band rebind to cover the instrument `*_strings` mesh

The clean, lowest-risk fix mirrors the proven torso rebind
(`RebindOutfitBonesToOwnSkeleton`, BandCharacter.cpp:942-1009) but for `mInstDir`'s
strings mesh: repoint each strings bone from the spread/animated character-skeleton
bone to a rigid, instrument-local rest basis, baking a gender/pose-neutral offset so
the rigid-authored strings re-compose to identity (ratio → ~1.0).

Two viable mechanisms, in preference order:

1. **Rebake the strings offsets at REST against the bones they are bound to**
   (`mesh->SetBone(b, bone, /*calcOffset=*/true)` at the deterministic rest pose, à la
   the engine SKEL_REBAKE at Rnd_Wgpu_RB3.cpp:4157-4203 and the head-rebind
   `mOffset = meshWorld * inv(restWorld)` at BandCharacter.cpp:1262-1270). This anchors
   the strings to the bones' REST relative spacing. CAVEAT: the C8 finding (the band
   neck bones may animate via CharServo when the guitarist plays) means a one-time
   static rebake may not stick if these specific neck bones move; verify the chainsaw
   neck bones are STATIC during gameplay (the bass ones are, ratio stays 1.0) before
   relying on this. If they animate, fall to (2).

2. **Repoint the strings bones to a RIGID anchor** — bind ALL strings bones of a given
   instrument to a single rigid neck root (e.g. `bone_neck`/`bone_bridge`'s rigid
   parent on the instrument, or the mInstDir root) so the strings ride the instrument
   rigidly (matching the FINE instruments' ratio-1.0 rigid-neck behavior). This drops
   the in-mesh string-bend animation but the FINE instruments already render with
   effectively-rigid strings (ratio 1.0), so visually this is the correct target state.

### Where it goes (file:line)

- `src/system/bandobj/BandCharacter.cpp` — NEW HX_NATIVE method
  `RebindInstStringsToRestBasis()` (or extend the rebind scope to include `mInstDir`'s
  `*_strings` mesh), called from `BandCharacter::Poll()` AFTER `Character::Poll()`
  alongside the existing `RebindOutfitBonesToOwnSkeleton()` call at
  BandCharacter.cpp:524. Scope it to `mInstDir` only and gate on the mesh name
  ending `_strings.mesh` AND its bones resolving to `skeleton_unshared.milo` (so it
  never touches the already-FINE instruments bound to their own resource, which would
  regress them). Set `mesh->mNativeBonesRebound = true` so the engine guard/clamp/
  rebake skip it (same contract as the torso rebind).
- `src/system/bandobj/BandCharacter.cpp:719-723` — update the
  `NativeCollectSkinnedMeshes` comment + (if using the existing collector) add a
  narrowly-scoped `mInstDir` strings pass; do NOT broadly include `mInstDir` in the
  general collector (the rest of the instrument — `_resource`/`_teeth` — is ratio 1.0
  and must stay untouched).

### Repo + gating

- **Repo = rb3 `src/system/bandobj/BandCharacter.cpp`** (shared TU compiled into
  rb3-native from rb3's own src). MUST be `#ifdef HX_NATIVE` with a byte-identical
  `#else` (Wii byte-identical — verify the matched `Poll()`/`SyncObjects` bytes are
  unchanged). DC3-inert (DC3 lacks this rebind machinery).
- **Opt-out env var:** `RB3_NO_INST_REBIND=1` (mirror `RB3_NO_SKEL_REBIND`), default
  ON, so an A/B reverts to the guard-drop status quo.
- The engine guard (`Rnd_Wgpu_RB3.cpp:4924-5141`) is NOT modified — it stays as the
  backstop and will simply stop firing on the strings once they compose to ratio ~1.0.
  Do NOT relax/disable the guard or extend the band-cap relaxation (it is already
  applied and correctly rejects ratio 4.95).

### Expected measurable effect

- `chainsaw_strings`/`guitar_brain_strings` world AABB collapses from ~136u back to
  ~bind (≈27-42u, ratio → ~1.0, matching the FINE instruments) once the strings ride
  a rigid/rest neck basis instead of the spread/animated character bones.
- Ratio drops below BOTH the 2.0× and 4.0× caps → `degenerate` is false →
  `[SHARD_GUARD]` `dir='instrument'` drop count goes from ~1552/frame to ~0.
- Guard-off and guard-on screenshots converge: the left-side smeared mass disappears;
  the guitar strings render intact at the staged guitar (|~95-100u|, NOT origin).
- Verify with the SAME A/B harness: `SHARD_RATIO_DBG=1` shows the exploding
  `*_strings` at ratio ~1.0 (was ~5.0), and a guard-on screenshot with no left-edge
  smear.

### Out of scope here (do NOT pursue)

- The `scrollbar_bg.mesh` (105 drops, `dir='scrollbar'`) and `male_extras*` (62+62,
  venue vignette extras) drops are a SEPARATE smaller residual (crowd/UI skin), not
  the instrument blowup; address after the instrument fix if still visible.
- The audience `WorldCrowd` (verify §4 fork 2) is never SHARD-dropped and is NOT the
  reported symptom — unrelated to this deform fix.

---

## REPRO / ARTIFACTS

- Screenshots: `docs/native/crowd-origin/shots/guard_on.png`,
  `docs/native/crowd-origin/shots/guard_off.png`
- A/B driver: `/tmp/shard_ab.py` (`python3 /tmp/shard_ab.py both`)
- Bone-structure probe (temp, REVERTED from engine after use): `STRINGS_BONE_PROBE`
  env in `Rnd_Wgpu_RB3.cpp` — the engine tree is clean (no uncommitted Rnd_Wgpu_RB3
  diff). Bone-dump snapshots preserved at `/tmp/strings_explode.snapshot.log`.
- Key env vars: `SHARD_GUARD_OFF=1` (disable guard), `SHARD_RATIO_DBG=1` (log every
  ratio), `SHARD_DBG=1` (log drops + dir + bone0). Build: `cmake --build
  native/build-native --target rb3-native -j16` (engine: `--target milo-engine`).
