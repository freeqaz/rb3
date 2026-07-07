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
