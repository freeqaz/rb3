# W2.8e — LIKE-SPACE INSTRUMENT + ASSET GROUND TRUTH — VERDICT: **MATCH**

## A.S1 — done (Opus, single writer of `Rnd_Wgpu_RB3.cpp`)

**Charter met (WAVE10_KICKOFF A.S1 + WAVE10_REVIEW A2/A3/A4).** Engine HEAD at start
`2998e78` (STEP-0 WHITE-fix). Build dir `native/build-agent-W2.8e`.

### Verdict in one line
The finger shard is a **real, placement-free, pose-independent ROTATION-basis conjugation
error**: the appendage offset is baked against the **shared magnet** skeleton basis
(`inv(off).angleVsI = 106.0°`, IDENTICAL across two members with distinct 38-/40-bone
skeletons) while the GPU palette evolves **each member's OWN per-member bone**
(`restW.angleVsI = 129.9° / 119.3°`). The magnet-vs-per-member rotation mismatch IS the
operative shard (ΔR 87.3° / 68.8°). A static, char-space, asset-derived rebake
`off := inv(restW_perMember)` collapses it (the coherent reference IS that rebake; its
ref-side is a coherent 19.2u hand). **The A3 placement-yaw confound is REFUTED by direct
measurement.** The only unrefuted path (Wave-9 menu) is alive → **MATCH**.

---

### (A2) Capture gate parameterized — `RB3_DUALSKIN_MINWEXT`
The probe's `wext > 60.f` entry gate suppressed the instrument exactly when a fix works (a
correct rebake DROPS wext below 60). Added `RB3_DUALSKIN_MINWEXT` (float, default 60u,
probe-scoped, inert otherwise) so a flag-ON GREEN capture is mechanically possible
(`Rnd_Wgpu_RB3.cpp:4405` region). **Modification, not additive** → drawlog-792 re-run
below. Registered append-only in `NativeCompatFlags.classification.json` (NO gen.inc regen —
coordinator).

### (A3) Like-space fixture reference — RE-PROVEN RED (37.43u); confound REFUTED
The Wave-9 review (A3) suspected the 32.8u fixture carried a placement-yaw lever arm
(asDrawn placement-anchored vs an origin-anchored coherent ref). **Direct measurement in the
probe refutes it:**

- **The owner bone-chain roots (`player0/1/2`) are at IDENTITY world** — placement is carried
  by `obj.world`/`meshWorld` under the placement contract (measured `angleVsI` up to 98–177°
  facing yaw, `.v` up to (−102,74,13)), **NOT the bone chain**.
- **Band members drawn with IDENTITY `obj.world` (zero placement) still shard 31–37u** — a
  real, placement-independent shard (e.g. asDrawn (2.7,15.1,40.5) vs coherent (25.0,1.7,14.3)
  = 37u with `obj.world==I`).
- **Frame-resolution control:** the char-space (unplaced-coherent) sep = 31.6u stays **< 2R
  bound (110u)**; applying `obj.world` to the coherent (placed control) gives **140u > 2R** =
  spurious double-placement. This proves the Wave-9 metric (asDrawn-placed vs char-space
  coherent) was **already placement-free** — the coherent `v·inv(bw)·live` is in the same
  frame as the drawn vertex.
- **char-space ΔR(off, bone-own-rest) = 87.3°/68.8°** is computed entirely from char-space
  owner bones (roots identity) → placement-free by construction, refuting S2's "bw is
  world-space ⇒ ~placement yaw."

Because the corrected like-space reference is mathematically the Wave-9 char-space coherent,
the re-committed `goldens/w2.8-farvert/live_pose.txt` holds the same ~32–37u shard (fresh
capture worst **37.43u**, header updated to the verified like-space semantics). The
`RealPathFixture` gtest reads it **RED (37.43u > 20u)**; the 4 math/control tiers stay GREEN.
**Not premise death** — the corrected fixture is still RED.

### (A4) Asset ground truth — provenance-by-invariance (no new milo parsing)
Operational target (WAVE10_REVIEW A4): compare the offset's bake-basis `inv(off)` against
each per-member palette bone's own settled clip-free rest `restW` (owner = `mesh->GeomOwner()`,
self-owned `hands_naked`, `meshRebound=1`). Route: the render-path dual-skin probe (sanctioned,
no milo parse). **Decisive finding:**

| member | bones | `invOff.angleVsI` | `restW.angleVsI` | operative ΔR | worstSep |
|---|---|---|---|---|---|
| A (players 0,2) | 38 | **106.0°** | 129.9° | **87.3°** | 37.2u |
| B (player 1) | 40 | **106.0°** | 119.3° | **68.8°** | 32.8u |

`inv(off)` is **IDENTICAL (106.0°)** across two members that have **distinct per-member
skeletons** (38 vs 40 bones) ⟹ the offset is NOT per-member; it comes from a **shared source**
— the magnet `char/main/skeleton.milo` that every resource milo shares by name-resolution
(`BandCharacter.cpp:3735`). The palette, however, evolves each member's **own** bone (restW
differs). The magnet-vs-per-member rotation deltaR = the operative shard, per member. This is a
**stronger, cleaner asset-provenance signal than loading `skeleton_unshared.milo`** (which the
review warned may be vacuous — male-bind, same file).

### Pre-registered tolerances (all written in PLAN.md BEFORE measuring) — ALL PASS
- **T1a** fixture ≥ 20u — PASS (37.43u).
- **T1b** identity-placement member still shards + charSpace<2R + placed>2R — PASS (37u @ obj.world==I; 31.6<110; 140>110).
- **T2a** ΔR ≥ 20° and pose-INDEPENDENT — PASS (87.3°/68.8° constant across frames 18–6600 while pose swings to 178°).
- **T2b** factors rigid (det∈[0.99,1.01]) — PASS (off/W/L det≈1.000, rows unit, ortho≈0).
- **T3a** rebake target coherent ≤ 25u — PASS (ref=inv(restW) extent 19.2u).
- **T3b** asset/static per-member basis — PASS (invOff shared 106.0°/magnet, restW per-member; static, pose-independent, asset-derivable).

**VERDICT (mechanical rule): MATCH** (T1 ∧ T2 ∧ T3a ∧ T3b).

### S2 fix guidance (for the next stage)
Char-space rebake of appendage (hand/finger/nail/glove) offset against **each per-member
palette bone's OWN settled clip-free rest** (`owner->BoneTransAt` rest — NOT `own->WorldXfm()`
at the rebind timing, NOT world-space). This slots into the existing `RB3_APPENDAGE_REST_ROT`
site inside `RebindHeadHandsAtRest`. **Why S2's world-space attempt regressed:** it captured
world-space rest at the rebind timing / a different bone, reintroducing the placement lever
arm; the MATCH path is char-space against the bone the GPU actually evolves. **Capture-timing
risk:** the default rebind bakes against the shared magnet (invOff constant 106°); the fix
must capture per-member and clip-free (settled ~frame 18–20). Compose in ONE space end-to-end
(frame-mixing killed W2.8c).

### Gates
- **drawlog-792** `--fixed-clock --canonical-order` flag-OFF = **PASS (792 draws, byte-identical**;
  probe render-inert behind `RB3_DUALSKIN_PROBE` getenv; 275 known-residual eye-jitter within bound).
- **RealPathFixture gtest** = RED (37.43u) — intended hard instrument on today's build; 4 tiers GREEN.
- **rb3-native + rb3-tests build** = clean (clang).
- DC3 zero-blast: probe + provenance live entirely in `Rnd_Wgpu_RB3.cpp` (rb3-backend-only TU);
  classification.json append-only (1 row); NO gen.inc regen (coordinator).

### Process notes
- **Single writer:** `Rnd_Wgpu_RB3.cpp` is Lane A's sole writer this wave. The probe edits are
  a MODIFICATION (capture gate + like-space frame-resolution + provenance dump) — drawlog-792
  re-run confirmed byte-identical flag-OFF.
- **Rule-7 disclosure:** one accidental `git checkout -- src/system/bandobj/BandCharacter.cpp`
  was used to revert MY OWN uncommitted provenance-probe edits (the `RB3_APD_RESTSRC` log never
  fired — `hands_naked` is rebound via a path other than `RebindHeadHandsAtRest`). **No sibling
  edits existed** (verified file == HEAD `1bf61a13`); no work lost. Provenance was then obtained
  render-side instead (invOff/restW invariance). Flagging per rule 8; will avoid `checkout --`.

### Commits
- engine (flock `/tmp/milo-engine-git.lock`): `Rnd_Wgpu_RB3.cpp` (A2 gate + A3 like-space +
  provenance dump) + `NativeCompatFlags.classification.json` (append-only `RB3_DUALSKIN_MINWEXT`) — `<eng-sha>`
- rb3 (flock `/tmp/rb3-git.lock`): `native/tests/goldens/w2.8-farvert/live_pose.txt` (re-committed
  like-space fixture) + this STATUS + PLAN — `<rb3-sha>`

Checkpoint: `/tmp/wave10-checkpoints/A-S1.json` (`verdict: MATCH`).

---

## A.S2 — done (Opus, single writer of `Rnd_Wgpu_RB3.cpp`) — VERDICT: **REFUTED (honest-negative)**

**Charter:** WAVE10_KICKOFF A.S2 + WAVE10_REVIEW A2/A3/A4. S1 checkpoint verdict = MATCH ⇒
authorized to implement (not the NO_MATCH no-code stop). Build dir `native/build-agent-W2.8e`
(engine HEAD `e0bf593`). The fix was implemented faithfully at the sanctioned site and then
**empirically REFUTED at verification: it FREEZES the appendages.** The 5th and final hands fix
class (asset-derived char-space static rebake) is dead. Flag + diagnostic kept **default-OFF** as a
documented dead-end (matching `RB3_APPENDAGE_REST_ROT` / `RB3_HANDS_POSEAWARE` /
`RB3_HANDS_PERFRAME_CONJ`).

### What was implemented (the S1-guided MATCH path)
New default-OFF flag **`RB3_APPENDAGE_ASSET_REBAKE`** at the existing `RB3_APPENDAGE_REST_ROT` site
inside `RebindHeadHandsAtRest` (`BandCharacter.cpp`). For appendage (hand/finger/nail/glove) meshes:
keep the mesh bound to its OWN per-member bone (`mesh->BoneTransAt(b)` — the char-space rest ~129°
that equals the dual-skin `restW`), **NO repoint**, and rebake `off = meshWorld·inv(that bone's
settled clip-free char-space rest)`. Coherent by construction (at rest `skin = inv(restW)·restW = I`),
one char-space end-to-end (a single captured transform supplies rotation AND translation — no
W2.8c frame-mixing). New member `mNativeApdAssetRest`; new helper `NativeRotAngleVsIdentityDeg`.
Plus **`RB3_APD_DIAG`** — a per-bone rest-basis provenance probe (render-inert) that produced the
decisive data below. `apdMesh` scoping generalized from `sApdRestRot`-only to `sApdAny` (any
appendage path) so the new flag + diag can see the scope; the refuted `RB3_APPENDAGE_REST_ROT`
path is preserved unchanged when it (and only it) is set.

### The decisive measurement — the fix FREEZES the hands (A/B, this wave)
| metric | flag-OFF (default) | flag-ON (`RB3_APPENDAGE_ASSET_REBAKE=1`) |
|---|---|---|
| dual-skin `worstSep` (S1 target) | 37.4u (RED) | **0.0u** |
| `hands_naked` worldExt over time | **CONTINUOUS 91.8–106.6u** (animating) | **PINNED to 56.1/80.3u** (2 discrete values = frozen) |
| IK_SHARD wext / worst vert | varies (86–107u, worst vert jumps 108…1057) | **constant 80u, same vert 686 every frame** |
| hands_bind_characterize verdict | BRANCH-RESIDUAL / **MARGINAL-GRAZE** (no hard hand shard) | BRANCH-RESIDUAL / **HARD-SHARD** |

The dual-skin worstSep collapses to 0.0u exactly as S1 predicted — but the appendage is now STATIC.

### Root cause — the S1 dual-skin metric is confounded by a STALE-BONE reference (`RB3_APD_DIAG`)
`RB3_APD_DIAG` logged, per resolved appendage bone: `bound` = `mesh->BoneTransAt(b)` (the mesh's
authored bone) has char-space rest **~129°**; `own` = `Find(bone-name)` resolves a **different**
instance at **~106°** (the shared magnet — `invOff` is IDENTICAL 106° across members with distinct
38/40-bone skeletons, S1's own provenance). The DEFAULT distinct rebind **repoints the mesh
`bound`→`own`** and bakes `off = inv(own rest ~106°)` — and that is **coherent w.r.t. the DRAWN
(animating) bone**: `own` is the live per-member instance that animates, `bound` is a STATIC
embedded bind-pose copy. The dual-skin probe's rest reference `bw` is captured from
`owner->BoneTransAt(b)` at the FIRST draw, **before** the repoint completes = the static `bound`
(129°), while `asDrawn` uses the post-repoint animating `own` (106°). The 37.4u "shard" is the probe
comparing the drawn vertex against a bone the draw does not use — a **bound-vs-drawn-bone confound
A.S1 ruled out for PLACEMENT but not here**. The metric is **unsatisfiable without freezing**:
own=106° (animating) and bound=129° (static) are mutually exclusive, and matching `bw`=129° forces
the mesh onto the static bone. The real hands are already coherently animating (characterize
MARGINAL, no hard hand shard); the fix trades animation for a metric artifact. **5th fix class dead.**

### Gates
- **drawlog-792** `--fixed-clock --canonical-order` flag-OFF = **PASS** (792 draws, byte-identical;
  172 known-residual eye-jitter within bound). All new code is `getenv`-cached; with every flag
  unset `sApdAny=0` ⇒ `apdMesh=false` ⇒ the whole block is inert.
- **Two-fixture protocol, leg 1** — committed flag-OFF `live_pose.txt` `RealPathFixture` gtest =
  **RED (36.9u > 20u)** on today's build (anti-revert instrument intact; not overwritten — restored
  to S1's HEAD via `git show` redirect after a baseline probe run touched it, NOT `checkout --`).
- **Two-fixture protocol, leg 2** — a flag-ON re-capture DOES read <20u (0.0u), but ONLY by
  freezing the hand ⇒ this is the REFUTATION, not a pass. Placement-independent wext + characterize
  A/B (the mandatory S2 exits) FAIL flag-ON (freeze / MARGINAL→HARD).
- **rb3-native + rb3-tests build** = clean (clang).
- DC3 zero-blast: all code is rb3 `BandCharacter.cpp/.h` (game code, HX_NATIVE) + append-only
  classification rows; no engine `Rnd_Wgpu_RB3.cpp` edit this stage (Lane A single-writer respected;
  no dual-skin probe modification needed — the A.S1 `RB3_DUALSKIN_MINWEXT` gate sufficed).

### Floating-forearm triage (S3) — deferred
Not separately captured this stage: the refutation supersedes it (the fix does not collapse the
finger shard — it freezes it — so the "does the forearm anomaly collapse with the S2 flag" check is
moot). Filed for a future wave with the H1/H2/H3 draw-log at the sighting.

### Disposition
`RB3_APPENDAGE_ASSET_REBAKE` + `RB3_APD_DIAG` kept **default-OFF** (flag-OFF byte-identical),
registered append-only in `NativeCompatFlags.classification.json` (class workaround / probe;
not-live; do-NOT-flip). No pin bump, no default flip (coordinator-only). The three static +
per-frame + world-space classes AND now the asset-derived static class are all dead with numbers;
the genuine open item, if any real residual survives the stale-bone confound, is a DIFFERENT
instrument (a coherent-vs-drawn-bone metric that tracks the post-repoint bone), not a 6th bind-side
bake attempt.

### Commits
- rb3 (flock `/tmp/rb3-git.lock`): `BandCharacter.cpp` + `BandCharacter.h` + this STATUS — `<rb3-sha>`
- engine (flock `/tmp/milo-engine-classjson.lock` + `/tmp/milo-engine-git.lock`):
  `NativeCompatFlags.classification.json` (append-only `RB3_APPENDAGE_ASSET_REBAKE` + `RB3_APD_DIAG`;
  NO gen.inc regen — coordinator) — `<eng-sha>`

Checkpoint: `/tmp/wave10-checkpoints/A-S2.json` (`verdict: REFUTED`).
