# W2.8f — THE TRUSTWORTHY HANDS INSTRUMENT — VERDICT: **INSTRUMENT BUILT + RECONCILED**

## B.S1 — done (Opus, Lane B) — the corrected primary metric reads GREEN; axis re-pointed

**Charter:** WAVE11_KICKOFF Lane B + COORDINATOR ACCEPTANCE A5/A6/A7/A8. Build the rest-capture-free
palette-internal hands instrument; reconcile the Wave-10 contradiction (dual-skin metric refuted as
stale-bone-confounded vs A.S3's confirmed-real 61→106u visible smear). **No fix this wave.**

Engine HEAD at start `6834744`; build dir `native/build-agent-W2.8f` (clang). Commit `4c93608`.

### Verdict in one line
The **corrected PRIMARY metric — Tier-2 parent/child joint-attachment with the EXACT bind joint
`inverse(off_b).v` — reads ~0 (≤0.33u over 2,214 hand samples, IDENTICAL to the clean body control)
while the visible smear reproduces (`hands_naked` wext 35→106u) and does NOT co-vary with it (A7
FAIL, corr +0.34).** Per the kickoff's pre-registered S2 branch this is decisive: **the defect is
NOT in the inter-bone / palette-consistency axis.** The hands skeleton is inter-bone COHERENT (child
joints stay attached to parents at every pose — matching A.S3's "wrist bone stays attached"); the
106u smear is a **mesh-shell / offset-conjugation** phenomenon (Tier-1: offsets baked ~87° off each
per-member bone's own rest, == A.S1) that rotates the skinned shell about each bone by R·sin(θ)
**without dislocating any joint**, so joint-attachment is structurally BLIND to it. **Joint-attachment
is the WRONG Wave-12 gate; the axis is mesh-vertex-vs-offset (the skinned shell), not the skeleton.**

---

### The instrument (engine `Rnd_Wgpu_RB3.cpp`, DrawMesh dualskin cluster tail, `RB3_HANDS_ATTACH_PROBE`)
NOT gated on `wext` (co-samples the full 61→106u trajectory for A7). Two palette-INTERNAL invariants
on the UPLOADED palette (`bones.bones[]`); **NO rest reference for Tier 2** — the confound class that
produced the W2.8e artifact is dead by construction.

- **Tier 1 (secondary, amplitude predictor):** full-palette per-bone rest-coherence sweep with
  **pointer-identity freshness validation** (the repoint bound→own IS a `BoneTransAt(b)` pointer
  change → recapture, killing the W2.8e stale-bone confound). Reports `angle(off_b·restW_b, I)` (A5
  literal) + the pose-independent cross-check `angle(inv(off_b), restW_b)`.
- **Tier 2 (PRIMARY, pose-tracking, rest-FREE):** for each palette bone `b` with parent `p`
  (`TransParent()`, pointer-matched into the owner bone list), the authored child joint must map to
  the SAME world point under BOTH uploaded palette matrices: `‖j·P[p] − j·P[b]‖`. Computed with BOTH
  `j = −off_b.v` (A5 spec) AND the exact `j = inverse(off_b).v` cross-check.
- **Fail-red (`RB3_HANDS_ATTACH_PERTURB=<boneIdx>`, A5-mandated):** rotates one palette entry's basis
  ~0.15 rad in a probe-local copy (render untouched). Proves the metric CAN read RED.

### Readings — animated gameplay window, songMs 0→70,000, N=2,214 hand blocks (`evidence/readings.txt`)

| mesh | nb | Tier1 rest-coh (deg) | Tier1 xcheck (deg) | **Tier2 approx −off.v (u)** | **Tier2 EXACT inv(off).v (u)** |
|---|---|---|---|---|---|
| **hands_naked.mesh** | 38 | 42.6 / **87.3** (bimodal) | 42.6 / **87.3** | 11.0 / med 43.7 / **110.5** | 0.00 / med 0.00 / **0.33** |
| fingernails_resource.mesh | 10 | 168.8–**176.9** | 168.7–176.7 | 0.00 | 0.00 |
| gloves_resource.mesh | 40 | 68.8 | 68.8 | 41.2 / **117.6** | 0.40 / **6.43** |
| gloves_resource.1.mesh | 4 | 60.2 | 60.2 | 7.3 / 43.2 | 0.16 / 0.37 |
| gloves_skin.2.mesh | 9 | 38.9 | 38.9 | 15.8 | 6.73 |
| **greaserjacket_resource.mesh** (clean body control) | 29 | **27.3** | 27.3 | 18.5 / **89.7** | 0.00 / **0.33** |
| greaserjacket_resource.1.mesh | 4 | 0.6 | 0.6 | 0.2 / 18.0 | 0.07 / 0.18 |

**Fail-red (`evidence/failred.txt`):** baseline `hands_naked` exact-joint max **0.33u** → perturb
bone 0 → **5.53u (17×)**. Tier-2 CAN read RED ⇒ the ~0 baseline is a genuine GREEN, not a dead metric.

### What the numbers establish

1. **Tier-2 EXACT joint-attachment ~0 on the hands (max 0.33u) == the clean body control (0.33u)** →
   the hands palette is inter-bone COHERENT. Every child joint tracks its parent to sub-pixel at
   every pose. The fail-red proves a wrong-basis bone that dislocated joints WOULD show here.
2. **A5's approximate joint `j = −off_b.v` is CONFOUNDED** — it reads **90u false-positive on the
   clean greaserjacket control** (A.S3-confirmed coherent) and 110u on hands; the approximation folds
   the offset's own rotation into the "attachment error." **A5's Tier-2 spec as written is refuted;
   the exact `inverse(off).v` joint is required** (and reads clean).
3. **Tier-1 DOES discriminate** the shard family — hands 87.3° / fingernails 177° vs the clean
   control 27.3° (and reproduces A.S1's exact 87.3° magnet-vs-own conjugation) — but its value **flips
   42.6↔87.3° across freshness-recapture events**, exposing the rest-capture-timing sensitivity A5
   flagged. Tier-1 is a directional signal, NOT a trustworthy gate (correctly SECONDARY).
4. **A7 co-variation FAILS for the trustworthy metric:** neither Tier-2 form tracks `wext`
   frame-to-frame on `hands_naked` (corr +0.34 exact / +0.03 approx). Per A7 the joint-attachment
   metric is therefore **NOT** the Wave-12 fix gate.

### Reconciliation (the W2.8f job) — why the instruments disagreed
A.S3's confirmed-real smear (wext 106u) and the "characterize=MARGINAL / stale-bone dual-skin"
tension resolve cleanly: the smear is a **mesh-shell rotational conjugation** — the inverse-bind
offsets are baked against the shared magnet basis (~87° off each per-member bone's own rest, Tier-1 /
A.S1) — which rotates the skinned mesh shell about each bone by R·sin(87°) **while preserving the
articulation chain's joint origins** (the conjugation is per-bone-uniform, so parent/child joints
stay coincident). Inter-bone joint-attachment (Tier-2) is structurally blind to a per-bone-uniform
shell conjugation; that is why it reads GREEN while the shell visibly explodes. A.S3's "wrist attached,
fingertips fling" is the exact signature: the wrist JOINT is attached (Tier-2 ~0) but the fingertip
VERTS (far from bone origin) fling (mesh shell).

### The axis for Wave 12 (named, per A7 / kickoff S2 branch)
The genuine residual is **real** (wext 106u, A.S3 H1 confirmed) but lives **one level below the
skeleton**: the **mesh-vertex-vs-offset relationship (the skinned shell)**, not the inter-bone chain.
- The right Wave-12 gate is a **per-vertex mesh-shell invariant** — e.g. does the GPU-skinned shell
  match the authored shell rotated ONLY by the coherent bone motion — NOT joint-attachment and NOT any
  rest-capture reference.
- A6's GPU-vs-CPU-same-inputs branch is **moot** (both sides share the same coherent palette; it
  cannot see a shell-vs-offset fault).
- Tier-1's 87° signal is the correct DIRECTION (offset-vs-own-rest conjugation) but needs a
  rest-capture-free formulation to become a gate; Tier-2-exact provides the trustworthy "skeleton is
  fine" companion so a Wave-12 fix that touches the skeleton can be caught if it dislocates joints.

**No fix attempted (correct for this diagnosis wave).** All bind-side bake classes remain dead; this
wave reframes the target from "fix the palette/skeleton" to "the skeleton is coherent — the defect is
the mesh shell's relationship to the offsets."

### Gates
- **drawlog-golden `--fixed-clock --canonical-order` flag-OFF = PASS (792)** — run AFTER the probe
  commit `4c93608` on the exact committed-probe binary (A8). Probe render-inert (getenv-gated).
- **Fail-red proven** (perturb 0.33u→5.53u) — the metric reads RED on demand.
- **rb3-native build** = clean (clang, `build-agent-W2.8f`).
- **DC3 zero-blast:** all probe code lives in `Rnd_Wgpu_RB3.cpp` (rb3-backend TU); classification.json
  append-only (2 rows); NO gen.inc regen (coordinator).

### Concurrency / process (A8, rule 7/8)
- Lane A (BOOTRNG) was **concurrently editing `Rnd_Wgpu_RB3.cpp`** (its probe hunks at ~:1091/:1651,
  out of my declared :4389–4672 range — no textual overlap). I staged **ONLY my DrawMesh hunk** via a
  filtered `git apply --cached` (verified 0 BOOTRNG lines staged) and committed under
  `flock /tmp/milo-engine-git.lock`; **Lane A's uncommitted edits were left intact in the working
  tree** (verified present + untouched post-commit). No `reset --hard`/`checkout --`/`stash` on shared
  trees. classification.json appended + staged + committed atomically under
  `flock /tmp/milo-engine-classjson.lock` + git lock.

### Commits
- engine `4c93608` (flock git): `Rnd_Wgpu_RB3.cpp` (RB3_HANDS_ATTACH_PROBE Tier-1/Tier-2 + PERTURB
  fail-red) + `NativeCompatFlags.classification.json` (append-only 2 rows; NO gen.inc regen).
- rb3 `<this>` (flock git): `W2.8f/PLAN.md` + `W2.8f/STATUS.md` + `W2.8f/evidence/`.

Checkpoint: `/tmp/wave11-checkpoints/B-S1.json` (`verdict: INSTRUMENT_BUILT_RECONCILED`).

---

## B.S2 — done (Opus, Lane B) — RECONCILE WITH THE VISIBLE SMEAR — VERDICT: **GREEN BRANCH — axis is the mesh SHELL, not skeleton, not GPU path**

**Charter:** WAVE11_KICKOFF Lane B S2 + ACCEPTANCE A6/A7/A8. Reproduce the A.S3 forearm sighting WITH
the B.S1 instrument LIVE; walk the decision tree. **No fix (diagnosis wave).** Engine binary
`native/build-agent-W2.8f/rb3-native` (B.S1 probe, engine `4c93608`); **no engine change this stage.**

### Verdict in one line
Both A.S3 sightings **reproduce with the instrument live** (visible finger/hand shard shells +
wext 95-106u), while the corrected **Tier-2 EXACT joint-attachment reads GREEN on the exact
sighting frames** (≤0.33u) and **A7 co-variation FAILS** — so per the pre-registered S2 branch the
defect is **NOT in the palette-coherence / skeleton axis**. The decisive new datum: **`wext` (the
106u smear) is a pure CPU 4-bone blend of the same authored verts × weights × uploaded palette the
GPU consumes** → the GPU faithfully reproduces the CPU, so the A6 **GPU-vs-CPU-same-palette readback
is predicted GREEN** (confirmatory-only). **The named Wave-12 axis is the authored-vertex-to-offset
composition — the skinned mesh SHELL — localized to the far-radius finger bones by R·sin θ.**

### Reproduction WITH the instrument live (`evidence/bs2_sighting_harness.py`, `bs2_readings.txt`)
Same two A.S3 nav sequences as `W2.8e/evidence/s3_triage_harness.py`, now with `RB3_HANDS_ATTACH_PROBE=1`.

| sighting | hands_naked N | wext range | HI-wext bin (visible smear) Tier-2 EXACT | A7 corr(wext,t2exact) |
|---|---|---|---|---|
| GAMEPLAY (ceiling-forearm) | 1677 | 35.8–105.8u | ≥91.8u, n=206: min0.0 **mean0.029 max0.33u** | **+0.261 → FAILS** |
| PARTDIFF (wipe-transition) | 228 | 53.3–105.8u | ≥95.3u, n=44: min0.0 mean0.27 **max0.33u** | +0.620 → **SPURIOUS** |

- **Visible smear CONFIRMED on-screen** (`evidence/bs2_gameplay_smear_ms2000.png`,
  `bs2_partdiff_smear_060.png`): center singer's raised hand = finger starburst of thin shards;
  partdiff female hand = detached floating shard shell by the window — the exact A.S3 §(b) sightings.
- **Decisive verbatim gameplay record (frame 2439, wext=95.2u):** `TIER2 … exactJoint=0.00u
  (palette-wide exactWorst=0.33u)` while `TIER2approx worst=89.58u bone[9]='bone_R-thumb01' R=48.5`
  and `TIER1 worst=87.3deg bone[33]='bone_L-middlefinger03'`. The far-radius FINGERS fling; the
  wrist/joint stays attached — A.S3's exact signature.
- **The partdiff +0.620 is rejected as spurious:** Tier-2 EXACT spans only 0.0–0.33u across the
  *entire* wext 53→106u range — three orders below both the 5.53u fail-red and the 106u symptom. A
  fix that eliminated the 106u smear would move this metric by ≤0.33u (below noise). Correlation on a
  near-constant signal is the A7 "tracking for its own reasons" trap; the metric does **not** track
  the symptom's magnitude in either run.

### Why the GPU-vs-CPU readback is predicted GREEN (the axis, with data)
`wext` (`Rnd_Wgpu_RB3.cpp:4360-4394`) is the CPU 4-bone blend mirroring the shader — same
`skinnedView` authored verts, same per-vertex `boneWeights`, same uploaded `bones.bones[]`. It
reproduces the full 106u smear with **zero GPU involvement** ⇒ the GPU is faithful to the CPU ⇒ a
`|GPU − CPU_skinnedView|` readback (which shares all three inputs) cannot see the fault (A6 caveat).
- **Ruled OUT:** GPU-only V24 decode / WGSL indexing / shader weight-norm as the smear cause (CPU
  alone smears). **Weight normalization ruled out** — clean body control `greaserjacket_resource`
  (same wsum-normalized blend) reads normal 41-54u; only the 87°-conjugated appendage meshes smear.
- **Ruled OUT:** skeleton / inter-bone incoherence (Tier-2 EXACT = 0, joints coherent every pose).
- **Ruled IN:** authored-vertex-to-offset composition (the SHELL). `off_b` baked in the shared-magnet
  basis (87° off each per-member bone's own rest, Tier-1 / A.S1); off_b AND liveBone_b both in the
  magnet frame ⇒ joint origins (R=0) fixed ⇒ Tier-2 EXACT ≈ 0; authored VERTS at R≠0 swept by
  R·sin θ ⇒ wext smear. Arithmetic closes: `48.5·sin(87.3°) ≈ 48.4u`/bone × finger chain → 95-106u.

### Wave-12 instruments designed (GREEN branch, `S2_WAVE12_INSTRUMENT_DESIGN.md`)
- **Instrument A (scoped, Wave-12 build):** GPU-vs-CPU-same-palette readback (wgpu MAP_READ buffer +
  compute skin ~150-250 LOC + async map), gated `RB3_HANDS_GPU_READBACK`. **Pre-registered
  prediction: GREEN (≤1e-3u).** A RED result would OVERTURN this verdict and move the axis to the GPU
  vertex path — logged either way.
- **Instrument B (the actual fix gate):** per-vertex **shell invariant** `‖s(v) − ŝ(v)‖` (CPU blend
  vs authored shell transported by ONLY coherent bone motion, using Tier-1's freshness-validated
  rest). Reads the 106u magnitude Tier-2 EXACT cannot; **A7 bar: must co-vary with wext.** This is
  the gate a Wave-12 shell-level fix must satisfy.

**Fix-side honesty:** the 87° own-rest signal is the SAME one Wave-9/10 refuted as a *bind-side*
target (5 dead classes; rebaking off_b freezes the hands). B.S2 does NOT resurrect a bind-side fix —
it REDIRECTS Wave 12 to the **per-vertex shell level** (Instrument B), away from the skeleton (proven
coherent) and the GPU path (predicted GREEN by Instrument A). No fix attempted.

### Gates / process (A8, rule 7/8)
- **No engine change this stage** ⇒ no new engine probe commit ⇒ per A8 no new golden run mandated;
  the drawlog-golden 792 flag-OFF was validated by B.S1 on this exact binary lineage (`4c93608`).
  All B.S2 work is rb3-side docs + a read-only reproduction harness reusing the committed B.S1 probe.
- Lane A's uncommitted `FxSendNative.cpp` (unrelated audio) left untouched; only my own W2.8f files
  staged under `flock /tmp/rb3-git.lock`. No reset/rebase/checkout/stash on shared trees.

### Commits
- rb3 `<this>` (flock git): `W2.8f/STATUS.md` + `W2.8f/S2_WAVE12_INSTRUMENT_DESIGN.md` +
  `W2.8f/evidence/{bs2_sighting_harness.py,bs2_readings.txt,bs2_gameplay_smear_ms2000.png,bs2_partdiff_smear_060.png}`.

Checkpoint: `/tmp/wave11-checkpoints/B-S2.json` (`verdict: GREEN_BRANCH_SHELL_AXIS`).
