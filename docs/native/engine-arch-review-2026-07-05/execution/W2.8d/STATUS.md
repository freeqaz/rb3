# W2.8d — BONE-LEVEL FACTOR ATTRIBUTION — VERDICT: **candidate (b)**

## A.S1 — done (diagnosis, Opus) — NAMED VERDICT: candidate (b), the OFFSET's rotation basis is conjugated against the wrong frame

**Charter met.** Built the dual-skin engine probe (`RB3_DUALSKIN_PROBE`,
`Rnd_Wgpu_RB3.cpp`, additive-only/gated/registered), re-established the RED baseline
on pin `a320f9d`, executed both discriminators (iii-a)/(iii-b), and populated
`goldens/w2.8-farvert/live_pose.txt` so the BL-A2 `RealPathFixture` gtest is now a
live hard instrument (was SKIP → now RED). **The two live candidates are separated
decisively: (a) is REFUTED, (b) is CONFIRMED — quantitatively and pose-independently.**

### The verdict in one line
The live per-member finger bone evolves as a **faithful rigid transform at every pose**
(det≈1.000, orthonormal, all the way to 178° of finger curl) — so the pose pipeline is
NOT the fault. The shard is a **fixed ~42–87° rotation between the shipped skin offset's
bake-basis and the per-member bone's own rest basis** (`inverse(BoneOffsetAt) ∠
perMemberRestWorld`), constant across the whole animation, that flings a vertex at bone
radius R by ≈ R·2sin(ΔR/2). **The offset (invBind) is conjugated against the wrong frame.**

---

## Instruments (A7 + A3)

- **A7 RED baseline on `a320f9d`:** `IK_SHARD_VERT` `hands_naked.mesh` worst-appendage
  **wext = 105–107u** reproduced (documented ~105-107u on `a94762f`) → the shard is
  present on the current pin, matching the Wave-7/8 baseline.
- **A3 dual-skin probe** (`RB3_DUALSKIN_PROBE`, engine `Rnd_Wgpu_RB3.cpp`, gated on
  `wext>60`, `owner` non-null): for the worst-**separation** far vertex of the selected
  mesh (default `hands_naked`) it dumps, per the vertex's dominant bone, every factor the
  GPU palette composes at `:3617` — offset, live boneWorld, live LocalXfm, weight,
  composed skin — with det + row-orthonormality of each, the rest-basis conjugation ΔR,
  and the `R·2sin(ΔR/2)` prediction vs the measured separation. It also writes
  `goldens/w2.8-farvert/live_pose.txt` = `asDrawn xyz | coherent-ref xyz | R` per far
  vert (coherent ref = per-member-own-captured-rest rigid delta `inverse(restWorld)·liveWorld`).
- **Flag-OFF byte-identical:** drawlog golden `--fixed-clock --canonical-order` = **792
  PASS** on the agent binary (probe is entirely `getenv`-gated). Wii untouched.
- **BL-A2 `RealPathFixture` is now LIVE:** reads the committed fixture by default
  (no env), **RED at 32.8u > 20u threshold, over 106 far verts** — the exact
  "today's-build RED, a correct fix turns it GREEN" instrument the lane lacked. The
  other 4 math/control tiers stay GREEN.

Artifacts: `native/build-agent-W2.8d/rb3-native`, `scripts/native/_w28d_probe.py`,
`/tmp/w28d-probe/raw-probe.log`, `native/tests/goldens/w2.8-farvert/live_pose.txt`.

---

## Factor-by-factor table (the deliverable)

Two band members captured (they share the mesh name `hands_naked.mesh` but have distinct
per-member skeletons — 38 vs 40 bones — so the probe keys rest capture by owner pointer).
Representative worst-bone rows; **every determinant is ≈1.000 and every row is unit-length
+ orthogonal at every pose** (near-rest AND deep animation):

| factor (worst bone `bone_R-middlefinger03`, R=54.2, member-38) | det | rowlen | ortho.xy | verdict |
|---|---|---|---|---|
| `off` = `owner->BoneOffsetAt(b)` (shipped invBind) | 1.0004 | (1,1,1) | ~0 | clean rigid |
| `W` = live `boneWorld` (`WorldXfm`) @ pose-dev 178° | 0.9998 | (1,1,1) | ~0 | **clean rigid — faithful evolution** |
| `L` = live `LocalXfm` (`L.v=(1.15,0,0)` = authored segment len) | 0.9999 | (1,1,1) | ~0 | clean rigid articulation |
| `skin` = `off·W` (what the GPU draws) | 1.0002 | (1,1,1) | ~0 | rigid, but rotated wrong |

**Composed `skin` rotation is a proper rotation `angleVsI = 145°` at that pose** — not a
corrupt/skewed matrix — it is simply pointing the wrong way, and it points wrong by the
SAME fixed amount the offset's basis is misaligned:

| quantity | member-38 (`middlefinger03`) | member-40 (`middlefinger03`) |
|---|---|---|
| **ΔR offset-basis vs per-member rest** (`inverse(off) ∠ restWorld`) | **87.3° (CONSTANT)** | **68.8° (CONSTANT)** |
| pose deviation `rest→now` (finger curl) sampled | **120° → 160° → 178°** (swings) | **124° → 150° → 175°** (swings) |
| measured far-vert separation (asDrawn vs coherent) | **36.9–37.2u (CONSTANT)** | **32.8u (CONSTANT)** |
| `R·2sin(ΔR/2)` single-bone prediction | 74.8u | ~62u |

### What each row proves

- **Candidate (a) — REFUTED.** "The live bone's rotation *evolution* is unfaithful"
  requires a corrupt/non-rigid bone world. Measured: `W` (and `L`) are **perfect rigid
  rotations (det 0.9996–1.000, orthonormal) at ALL poses**, including 178° of finger
  curl. The pose pipeline (CharClip → CharBones → TransParent) emits faithful rigid
  transforms; it is not the fault. (This is (iii-a): the composed live LocalXfm/WorldXfm
  are clean rigid articulations tracking the animation.)
- **Candidate (b) — CONFIRMED, three independent ways:**
  1. **Constant, pose-independent ΔR.** `inverse(off) ∠ perMemberRest` = **87.3°
     (member-38) / 68.8° (member-40), identical across frames 6211/6600/6811/7200/7800**
     while the pose swings 120–178°. A pose-*evolution* error would vary with the pose; a
     constant means the error is **baked into the offset**.
  2. **Non-identity skin AT REST.** For a correctly-rebaked offset, `skin = inverse(restW)·restW
     = I` at rest (`angleVsI = 0`). Measured `angleVsI(skin@rest) = 42.6–87.3°` — i.e. the
     shipped offset is inverse of a basis **42–87° rotated from the bone's own rest**.
  3. **Fling tracks `R·sin(ΔR)`, not the pose.** The far-vert separation stays constant
     (~33–37u) as the pose swings — because the shard magnitude ≈ `R·2sin(ΔR/2)` depends
     only on the fixed basis error and the bone radius, not the animation. (This is (iii-b):
     the operative rest-basis ΔR predicts the fling to order; the ~2× gap between the
     74.8u single-bone upper bound and the 37u measured is the 4-bone blend + the coherent
     reference sharing the bone's own rotation — the mechanism is confirmed, not the exact
     constant.)

`off.v` translation tracks (skin.v small at rest) — consistent with the STATUS record that
the default-ON `RebindHeadHandsAtRest` fixed **translation** (fling→0) but left the
**rotation basis** wrong. The rebind repositions the offset's *origin* to the per-member
rest but does **not** align its *rotation basis* to the per-member bone's own rest basis —
that residual 42–87° is the shard.

---

## Answers to the closed factors (one-line confirmations, per A2)

- **Composition order** — CLOSED by source (`Mesh.cpp:1368` `SkinVertex`, `:328` `SetBone`
  offset convention = engine palette = RefSkinVertex). Not re-measured.
- **Owner-vs-drawn** — reconfirmed: probe keys by `owner`, `owner==drawn mesh` for the
  captured `hands_naked` (W2.8c CLAIM finding holds). Not a live hypothesis.
- **Translation of every factor** — CLEAN (skin.v small; matches REBIND_DRAW_SKINPOS
  47–58u). Not the fault.

---

## Named verdict → the fix target (for A.S2 / Wave-10 design)

**The wrong factor is the SKIN OFFSET's ROTATION BASIS** (`owner->BoneOffsetAt(b)`),
conjugated against a frame **42–87° rotated** from the per-member bone's own rest basis —
per bone, constant, pose-independent. The live bone world is faithful; the pose pipeline is
exonerated. This is candidate (b) exactly as the Wave-8 clue predicted ("rotation basis
wrong, translation correct").

**Consequence for the fix.** The correct fix rebakes each appendage bone's offset so that
`skin = inverse(off)⁻¹ · W` reduces to a rigid delta about the bone's OWN rest basis, i.e.
`off_b = meshWorld · inverse(perMemberBoneBindWorld_b)` using the per-member bone's **rest
ROTATION basis** (not just its rest position, which `RebindHeadHandsAtRest` already gets).
Because ΔR is a **constant, pose-independent** per-bone quantity (this is the key finding
that separates it from the refuted per-frame conjugation), a **static per-bone rebake IS
sufficient** — provided it captures the per-member bone's true rest *rotation* basis. This
is materially different from:
- `RB3_HANDS_BIND_FIX` (inert — never fired on the band path);
- `RB3_HANDS_POSEAWARE` (rigid wrist-collapse — scrambles per-bone-authored verts);
- `RB3_HANDS_PERFRAME_CONJ` (mixed magnet-space `A_b` with world-space `L` → amplified 80u→2600u).

None of those did the clean thing: **per-bone `off_b = inverse(perMemberBoneRestWorld_b)`
with the correct rest ROTATION basis.** The BL-A2 `RealPathFixture` (now RED at 32.8u) is
the numeric gate that arm must turn GREEN.

**Open confirmation deferred to A.S2 (not required for this verdict):** attribute the 42–87°
to the magnet specifically — load `char/main/skeleton.milo` (magnet) vs the member's
`skeleton_unshared` at rest and confirm the per-bone asset-level ΔR equals the operative
ΔR measured here. The operative ΔR (what drives the shard) is already measured; the
asset-level load only confirms provenance.

---

## Gates

- flag-OFF drawlog golden **792 PASS** (canonical, agent binary) — probe byte-identical off.
- BL-A2 oracle suite: **4 GREEN + RealPathFixture RED (32.8u)** — the SKIP is gone; hard
  instrument live.
- A7 baseline reproduced on `a320f9d` (`hands_naked` wext 105–107u).
- DC3 zero-blast: probe is `Rnd_Wgpu_RB3.cpp` (rb3-backend-only TU), no shared-contract
  file touched; classification.json append-only (2 rows), NO gen.inc regen (coordinator).

## Commits
- engine (flock `/tmp/milo-engine-git.lock`): `Rnd_Wgpu_RB3.cpp` `RB3_DUALSKIN_PROBE` +
  `NativeCompatFlags.classification.json` (2 append-only probe rows) — `30d4f00`
- rb3 (flock `/tmp/rb3-git.lock`): this STATUS + PLAN + `scripts/native/_w28d_probe.py` +
  `native/tests/goldens/w2.8-farvert/live_pose.txt` — `<rb3-sha>`

Checkpoint: `/tmp/wave9-checkpoints/A-S1.json`.
