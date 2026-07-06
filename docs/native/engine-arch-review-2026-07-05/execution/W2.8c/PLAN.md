# W2.8c — Per-frame pose-aware appendage (hands/fingers) basis correction — PLAN

**Stage:** B.S1 (planner, Opus). **KEY:** W2.8c. **Lane:** B (hands fix; rb3 game-side).
**Fence:** rb3 `src/system/bandobj/BandCharacter.{cpp,h}` + `native/tests/` ONLY. **No engine
edits.** (If the design needed one it would be staged as a Wave-9 patch — it does not; see §6.)
**Engine pin:** `a94762f` (unchanged; lanes never bump). **This document is design-only; no source
edited.**

Parent record: `execution/W2.8/STATUS.md` (B.S1–B.S4), `execution/README.md` Wave 6/7 W2.8 rows,
`docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md:104-183`, `WAVE8_KICKOFF.md` Lane B,
`WAVE8_REVIEW.md` A4/A5/A7.

---

## 0. TL;DR

Two whole classes of *static* fix are now empirically dead (measured, not argued):

- `RB3_HANDS_BIND_FIX` (smarter clip-free rest **seed**) — INERT on the real band path
  (`clipPlaying` fires 0×); IK_SHARD_VERT wext unchanged 105–107u (B.S2).
- `RB3_HANDS_POSEAWARE` (rigid **wrist-collapse**, `NativeRepinHandsRigid`, default-OFF, landed) —
  helps *uniformly-authored* glove shells (70–84 → 60–67u) but **DISTORTS per-bone-authored
  articulated hands** (`hands_naked` 38 bones / `fingernails`): 106 → 205–253u, into the 200–460u
  STOP band, +2 drops, +2 flings. Confirmed twice (B.S3 impl, B.S4 independent). MUST NOT flip.

The one remaining fix class is a **per-frame per-bone basis conjugation**: keep every hand/finger
bone bound to its *own* live per-member bone (preserving articulation — the anti-rigid-collapse
property), and each Poll rewrite that bone's palette offset so the live bone's rotation is applied
**about the authored (magnet) rest frame**, cancelling the growing R·sin(θ) twist per bone rather
than collapsing the hand to one rigid body. §1 gives the math; §2 the two A4 hazards; §3 the flag;
§4 the measurement protocol (IK_SHARD_VERT wext A/B, the B.S4 hard exit).

**Subtask:** one Opus impl (`model: opus`) — `W2.8c.S2` — landing the fix behind a new default-OFF
flag with the W2.2 four-layer gates; verify (`W2.8c.S3`) is the separate B.S4-protocol lane.
This planner returns this document unchanged on resume (it has a `## Subtasks` section, §7).

---

## 1. The per-frame math

### 1.1 The engine draw and the shard (the invariant we correct against)

The palette is composed **engine-side, per draw**, from **game-writable** state
(`Rnd_Wgpu_RB3.cpp:3529`, verified WAVE8_REVIEW A4):

```
skin_b(t) = owner->BoneOffsetAt(b)  ·  boneWorld_b(t)          // Milo row-vector order
v'(t)     = v · skin_b(t)                                       // vertex v (mesh space)
```

`BoneOffsetAt(b)` is the per-bone offset (invBind); `boneWorld_b(t) = bt->WorldXfm()` is the live
bone world (post-`WorldXfm_Force`, §2b). Both are game-writable: `BoneOffsetAt` returns a **mutable
`Transform&`** (`Mesh.h:257`); `SetBone(b, bone, calcOffset)` rebinds and writes the offset
(`Mesh.cpp:328`).

Define, per hand/finger bone `b`:

- **`A_b`** — the **authored magnet world** the mesh's invBind was baked against, i.e.
  `A_b = inverse(offset_authored_b)` (equivalently the world of the shared static magnet bone the
  mesh loaded bound to). Constant. Captured **before any rebind mutates the offset** (§2a note).
- **`L_b(t)`** — the live per-member bone world (`Find`-resolved animated bone, post-force).
- **`L_b(t0)`** — `L_b` snapshotted at the rebind/latch frame `t0` (the first Poll at which the
  live bone is distinct-resolved and its whole `TransParent` chain has a finite forced world).

The default (shipped, non-rebound-hands) skin is `skin_b = inverse(A_b) · L_b(t)` (`meshWorld ≡ I`
for skinned meshes, so `offset_authored_b = inverse(A_b)`). At the magnet rest this is identity; but
`A_b` (magnet basis) and `L_b` (per-member animated basis) differ by a **rotation** (measured
sign-flip, e.g. `bone_R-upperArm` magnet `(0.73,-0.07,-0.68)` vs animated `(-0.73,0.09,-0.68)`), so
the residual `inverse(A_b)·L_b(t)` carries a rotation θ and a vertex at radius `R` from the bone
flings by **`R·sin(θ)`** (`IK_SHARD_VERT`, `Rnd_Wgpu_RB3.cpp:~4062`). θ grows as the pose moves →
distal finger joints (largest R, largest swing) shard worst (79–107u), origins stay clean (69.5u).
This is the exact signature `CHAR_SKINNING_DEFORM_INVESTIGATION.md:104-156` documents.

### 1.2 The correction — per-bone conjugation of live motion into the authored frame

Keep bone `b` bound to its **own** live bone `L_b` (NOT the wrist). Set the per-Poll offset so that
the drawn skin applies the live bone's **relative motion since t0**, conjugated into the authored
magnet frame `A_b`:

```
target  skin_b(t) = inverse(A_b) · [ inverse(L_b(t0)) · L_b(t) ] · A_b        (similarity of live Δ)
```

`inverse(L_b(t0))·L_b(t)` is the live bone's world-space motion since `t0` (basis-free — a physical
displacement). Conjugating by `A_b` re-expresses that rotation/translation **about the authored bone
position and orientation**, so the vertices rotate about the magnet-authored joint (correct
articulation, no twist) instead of about the mismatched live basis (the shard).

Solve for the offset the engine multiplies (`skin_b = offset_b(t) · L_b(t)`):

```
offset_b(t) = inverse(A_b) · inverse(L_b(t0)) · L_b(t) · A_b · inverse(L_b(t))
```

Per bone, per frame; all four factors are `Transform`s available game-side; compose with the
engine's `Multiply`/`Invert`. Sanity checks:

- **At t0:** `L_b(t)=L_b(t0)` ⇒ `offset_b = inverse(A_b)·A_b·inverse(L_b(t0)) = inverse(L_b(t0))`,
  and `skin_b(t0) = inverse(A_b)·A_b = I` ⇒ `v'=v` (the authored magnet-rest hand). No pop at latch.
- **Under animation:** the *rotation* applied to `v` is `inverse(A_b)·[Δrot]·A_b` — the live joint's
  rotation **rebased onto the authored joint axis**, so the R·sin(θ) basis twist is cancelled to
  first order (the only residual is second-order, from any t0≠true-rest offset — measured, §4).
- **Per-bone, not rigid:** each `b` uses its own `L_b(t)`, so fingers articulate independently. This
  is precisely what the wrist-collapse (`RB3_HANDS_POSEAWARE`) destroys — and why it distorts
  per-bone-authored verts to 205–253u.

### 1.3 Why it cannot regress the uniformly-authored meshes the rigid anchor helped

For a **uniformly-authored** shell (drivinggloves: all verts effectively share one near-rigid frame,
i.e. `A_b ≈ A` and `L_b ≈ L` for the shell's bones), the per-bone conjugation degenerates to a
**single** similarity `inverse(A)·inverse(L(t0))·L(t)·A` applied to all verts — the same rigid
transport that already drove gloves to 60–67u (B.S3/B.S4), just derived per-bone. There is no
cross-bone scrambling term (each vert still rides its own bone), so uniform shells stay at their
improved value and articulated hands additionally stop sharding. The rigid collapse's failure mode
(forcing 38 finger bones through one wrist mapping) is structurally absent because no bone is
repointed to another bone. **Net: strictly a superset of the rigid win, without its distortion.**

### 1.4 Relationship to B.S3's backlogged "faithful" formula

B.S3 sketched `offset_b = wristDelta · restSkin_b · inverse(wristLiveWorld)` and flagged it as
needing `restSkin_b` (the char-root world / an uncertain placement convention) and a dual-skin
engine probe. §1.2 avoids both: it references each bone's **own** `A_b` and `L_b(t0)` (no char-root
reconstruction, no cross-bone `restSkin_b`), so it is expressible entirely game-side and gated by the
**existing** `IK_SHARD_VERT` probe. The finger-**articulating** ideal (per-member skeleton carries
the authored basis — BL-A0 CharBones pose-pipeline) stays the deepest engine backlog item; §1.2 is
the highest-fidelity correction reachable inside Lane B's fence.

### 1.5 The one falsifiable assumption (what §4 must decide)

The conjugation is exact iff conjugating the world-space live-Δ by `A_b` faithfully reproduces the
authored articulation — valid when the magnet and per-member skeletons share bone-**local** rest
conventions up to the constant bridge `A_b·inverse(L_b(t0))` captured at `t0`. If `t0` is not at true
rest, the bridge is imperfect and a second-order residual survives; the conjugation still removes the
dominant **growing** R·sin(θ) term (that is the whole point), but whether the residual lands under
the 20u exit bar is an **empirical** question. §4 is the arbiter — the plan does not assert success,
it wires the gate that proves or refutes it (as B.S2/B.S3/B.S4 did for the dead classes).

---

## 2. The two A4 hazards, handled explicitly

### 2a. Palette source is the OWNER mesh — write the owner's offsets

The draw reads `owner->BoneOffsetAt(b)` where `owner = drawn->GeomOwner()` (`:3529`, `:3183-3200`).
Writing a shared/aliased mesh's *own* bone array has no GPU effect. The impl MUST operate on
`RndMesh* owner = drawn->GeomOwner() ? drawn->GeomOwner() : drawn` and dedupe owners across the pass
— **exactly the pattern `NativeRepinHandsRigid` already uses** (`BandCharacter.cpp:1739-1745`,
`ownersDone` list, propagating `mNativeBonesRebound` to the shared drawn mesh). Reuse it verbatim.
Prior hands rebinds via the owner reached the palette (B.S3's 106→205u proves writes land), so the
seam is confirmed; the impl must not regress to writing `drawn` when `drawn != owner`.

**`A_b` capture ordering (a corollary of 2a + the Poll seam).** Our pass runs at `:581`, AFTER
`RebindHeadHandsAtRest` (`:524`) and `RebindOutfitBonesToOwnSkeleton` (`:572`), both of which may
already have rewritten the hand mesh's offsets — so `owner->BoneOffsetAt(b)` at `:581` is NOT
necessarily the pristine authored magnet invBind. Two mitigations, impl picks one and states it:
(i) **mutual exclusion** — when the new flag is ON, `RebindHeadHandsAtRest` skips hand/finger/glove
meshes (they become this pass's property), so the offset it reads is pristine `= inverse(A_b)`; OR
(ii) **snapshot A_b at first-encounter** into a per-member `std::map<mesh*, std::vector<Transform>>`
before any mutation, captured on the mesh's very first appearance. (i) is cleaner and is also the A5
requirement that the two flags be mutually exclusive — **prefer (i).**

### 2b. Draw-time world recompute — force the chain before sampling

The default-ON `WorldXfm_Force` pass (`Rnd_Wgpu_RB3.cpp:3421-3446`, opt-out `RB3_NO_SKEL_WORLDFIX`)
force-recomputes every referenced bone's world **inside DrawMesh, AFTER Poll**, precisely because
Poll-time `WorldXfm()` reads can be a stale cache (`:3400-3411`). Our `L_b(t)` is sampled in Poll; the
palette uses the post-force `L_b(t)`. If they differ, the conjugation cancels against the wrong world
and re-introduces a residual — this is the exact defect that poisoned prior rest captures. **Mitigation
(mandatory):** before sampling `L_b(t)` (and `L_b(t0)` at latch), force each live bone's
`TransParent`-chain root→leaf with `WorldXfm_Force()` (calling `DirtyLocalXfm()` first if needed),
mirroring the engine pass's root→leaf order (`Trans.h:77` `WorldXfm_Force`, `:150` `DirtyLocalXfm`,
both game-callable). Deduplicate forced bones per Poll (a `std::set`, as the engine pass does with
`sForced`) so cost stays bounded. After this, Poll-time `L_b(t)` == the palette's `L_b(t)` by
construction. **Finite-world guard:** reuse the existing `|v.{x,y,z}| < 1e5f` NaN/inf guards
(`NativeRepinHandsRigid:1660`+) — a bone with a non-finite forced world defers that mesh (the engine
clamp is disabled for rebound meshes, so there is no backstop).

---

## 3. Flag shape

- **New flag:** `RB3_HANDS_PERFRAME_CONJ` (default-**OFF**; opt-in presence, getenv-cached early
  return so flag-OFF is a byte-identical no-op — the W0.6 registry pattern, matching
  `NativeRepinHandsRigid:1698`). Register append-only in engine
  `NativeCompatFlags.classification.json` under `flock /tmp/milo-engine-classjson.lock`
  (class `feature`, owner `skinning`, default `off`, read `presence`) — **no `gen.inc` regen**
  (coordinator does the single regen). *This is the one engine-repo write, and it is the sanctioned
  append-only classification sidecar, not an engine code edit — the fence holds.*
- **`RB3_HANDS_POSEAWARE` stays landed, default-OFF, UNTOUCHED** (A5) — it writes the same meshes at
  the same seam. The two are **mutually exclusive in code**: if `RB3_HANDS_PERFRAME_CONJ` is set, the
  new pass runs and `NativeRepinHandsRigid` early-returns for the hand meshes it would claim (guard
  at the top of `NativeRepinHandsRigid`, or the new pass runs first and marks `mNativeBonesRebound`
  so the rigid pass skips). In **all measurement arms** (§4) `RB3_HANDS_POSEAWARE` is **unset**
  (A5), as is `RB3_PP_LUMA_CEILING` (A7).
- **Probe flag:** `HANDS_CONJ_PROBE` (default-OFF, print-only) — per-mesh/per-bone log
  (`member`, `mesh`, `nb`, per-bone `A_b`/`L_b(t0)`/wext), gating only its own `fprintf` block, like
  `HANDS_RIGID_PROBE`.

---

## 4. Measurement protocol (the hard exit — A5 / B.S4)

**Primary gate = in-engine `IK_SHARD_VERT` wext A/B, same-binary / same-member** (B.S4's protocol,
`scripts/native/_w28_poseaware_ab.py` machinery, both arms players 0–3, contract default-ON):

| metric (worst appendage) | flag-OFF (baseline) | flag-ON (exit bar) |
|---|---|---|
| `IK_SHARD_VERT` wext | ~106u (`hands_naked`, distal finger bone) | **< 20u** |
| appendage FLING (>120u) | 0 | **0** |
| worst appendage in 200–460u STOP band | none | **none** |
| added appendage guard-DROPs | 0 | **0** |
| crowd/scrollbar clamp drops | 0 | **0 (byte-identical — flag-scoped)** |
| uniform glove (`drivinggloves`) wext | 70–84u | **≤ 67u (no regression; ideally improved)** |

- **Both arms:** `RB3_HANDS_POSEAWARE` **unset**, `RB3_PP_LUMA_CEILING` **unset** (A5/A7);
  `RB3_PLACEMENT_CONTRACT` default-ON. Same binary, same nav, same 4 members ⇒ any wext delta is
  attributable to the flag (B.S4's cleaner design; avoids B.S3's cross-member confound).
- **Baseline re-establishment (A7):** first reproduce flag-OFF ~106u on the **current pin `a94762f`**
  build before attributing any change (the B.S2/B.S3 tables were on `af4a22a`/`585816f5`).
- **flag-OFF byte-identical (W2.2 layer 1):** splash **drawlog golden = 792** (`--fixed-clock
  --canonical-order`); **lineup gate PASS** all layers (img/segA/ratioB/countC/pin); **0
  `HANDS_CONJ` probe lines** in the flag-OFF arm; Wii untouched (`#ifdef HX_NATIVE`).
- **fail-red (W2.2 layer 4):** BL-A2 oracle stays RED under `RB3_FARVERT_PERTURB` (the metric fires
  on basis mismatch, not animation) — unchanged from B.S4 axis 3.
- **Visual (W2.2 layer, supporting not gating):** finger close-up before/after on a **fixed** member
  (avoid B.S4's random-member confound — force `coop_g_n03` + `coop_g_b` and pin the same member both
  arms) against `images/retail-screenshots/`.
- **RealPathFixture gtest** (`native/tests/`, BL-A2 `live_pose.txt` arm): remains a **SKIP** unless
  the dual-skin engine probe is added — that probe is an engine edit (`Rnd_Wgpu_RB3.cpp`) and is
  therefore **optional / staged Lane-A-tail** (coordinator-sequenced), per A5. The operative hard
  gate is the `IK_SHARD_VERT` wext A/B above, which needs **no new engine code**.

**Bonus check (WAVE8_KICKOFF Lane B):** measure `saddleshoe_skin.2` guard-DROP under the fix — same
rotation-basis family? If the conjugation is generic over `NativeCollectSkinnedMeshes` scope and the
shoe improves, W2.6 closes for free. **Measure, don't assume** — the shoe is *not* in the
hand/finger/glove name scope, so a positive here is a bonus, a negative is expected and fine.

---

## 5. Files touched (fence-compliant)

- `src/system/bandobj/BandCharacter.cpp` — new method `BandCharacter::NativeConjHandsPerFrame()`
  (or fold into a mode branch of `NativeRepinHandsRigid`; a distinct method is cleaner for the mutual
  exclusion and the census), wired at the `:581` seam (replacing/guarding the `NativeRepinHandsRigid`
  call under the flag); reuses `NativeCollectSkinnedMeshes`, the owner/`ownersDone` dedupe, the
  L/R-side + wrist-anchor resolution (for scope + the t0-latch), and the finite-world guards. Adds a
  per-member `A_b` snapshot map (if mitigation 2a-ii is chosen) and a `t0`/`L_b(t0)` snapshot + latch
  (`mNativeHandsConjOnce` / re-armed like the siblings). **No unlatched per-frame drift**: `L_b(t0)`
  and `A_b` are snapshotted once at latch; only `L_b(t)` is re-sampled per frame — the offset is
  recomputed per frame but from fixed anchors (§1.2), so the hand animates without re-baking its bind.
- `src/system/bandobj/BandCharacter.h` — method decl + the new member(s) (snapshot maps, latch int).
- `native/tests/` — extend/parametrize the BL-A2 oracle harness only if pursuing the SKIP arm
  (optional; the wext A/B script lives in `scripts/native/`).
- engine `NativeCompatFlags.classification.json` — one append-only row (§3), under its flock.

**Explicitly NOT touched:** any `milo-native-engine/src/` **code** (`Rnd_Wgpu_RB3.cpp`,
`RB3PostProc.*`, WGSL) — Lane A owns the render backend this wave. The correction needs none:
`WorldXfm_Force`/`DirtyLocalXfm` (`Trans.h:77,150`), `BoneOffsetAt`/`SetBone`/`GeomOwner`
(`Mesh.h:226,241,257`), `Multiply`/`Invert` are all game-callable.

## 6. Does the design need an engine edit? — NO (with one staged optional)

The **fix** is 100% game-side (§5). The only engine-repo write is the append-only classification row
(sanctioned sidecar, not code). The **dual-skin `RealPathFixture` numeric arm** would need an engine
probe in `Rnd_Wgpu_RB3.cpp` (compute drawn-skin and conjugated-skin at the same vert) — that is
**staged as an optional Lane-A-tail patch**, coordinator-sequenced (A5), and is NOT required for the
hard gate. If S2/S3 find the game-side conjugation cannot reach <20u without the char-root world
(the §1.5 assumption fails), the honest outcome is a **recorded negative + a staged engine patch for
the per-member-skeleton basis fix (BL-A0)** — the same discipline that closed the two dead static
classes, not a papered-over "fixed" claim.

## 7. Subtasks

- **W2.8c.S2 — impl** (`model: opus`): land `NativeConjHandsPerFrame` per §1–§3 behind
  `RB3_HANDS_PERFRAME_CONJ` (default-OFF), W2.2 four-layer gates. Commit rb3 `BandCharacter.{cpp,h}`
  under `flock /tmp/rb3-git.lock`; append the classification row under
  `flock /tmp/milo-engine-classjson.lock`. Exit: build green, flag-OFF byte-identical (drawlog 792 +
  lineup PASS + 0 probe lines), flag-ON reaches gameplay, `HANDS_CONJ_PROBE` shows per-bone
  (not wrist-collapsed) offsets on `hands_naked`/`fingernails`.
- **W2.8c.S3 — verify** (`model: opus`): fresh from-scratch build; the §4 `IK_SHARD_VERT` wext A/B
  hard exit (flag-ON worst appendage **<20u** vs ~106u OFF; FLING=0; nothing in 200–460u; 0 added
  drops; gloves not regressed; both arms `RB3_HANDS_POSEAWARE`+`RB3_PP_LUMA_CEILING` unset, contract
  default-ON, pin `a94762f`); flag-OFF byte-identical re-confirmed; fail-red intact; fixed-member
  finger close-up; saddleshoe bonus measurement. Recommend flip / no-flip honestly (partial/blocked
  beats inflated complete). **Do NOT flip** (coordinator-only).

## 8. Exit criteria (lane)

Fix landed default-OFF, registered, flag-OFF byte-identical, four-layer gates wired; S3's
`IK_SHARD_VERT` wext A/B either **passes <20u** (→ coordinator flip candidate) or returns a **measured
negative + staged BL-A0 engine patch** (→ Wave 9). Either way the lane advances the campaign with
numbers, and the dead-end classes are not re-derived.
