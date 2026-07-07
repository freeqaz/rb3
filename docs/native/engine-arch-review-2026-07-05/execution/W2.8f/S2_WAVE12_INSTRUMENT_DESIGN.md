# W2.8f B.S2 — Wave-12 instrument design (GREEN branch)

B.S2 confirmed the decision-tree **GREEN branch**: the corrected Tier-2 EXACT joint-attachment reads
GREEN (≤0.33u) on the *actual A.S3 visible sighting frames* while the smear reproduces (wext 95-106u).
Per the kickoff/A6 this mandates scoping the **GPU-vs-CPU-with-SAME-uploaded-palette readback**
instrument. It is too big to build in a diagnosis wave (needs new wgpu readback machinery), so it is
**designed here for Wave 12** — together with the instrument the evidence actually points to.

## The load-bearing measurement B.S2 added

`wext` (the 95-106u smear metric, `Rnd_Wgpu_RB3.cpp:4360-4394`) is a **pure CPU 4-bone blend that
mirrors the shader**: it skins the same authored `skinnedView[i]` verts by the same per-vertex
`boneWeights[k]` and the same uploaded palette `bones.bones[bi]` the GPU consumes, with **zero GPU
involvement**. The 106u smear is therefore fully reproducible on the CPU. Consequences:

1. **The GPU faithfully reproduces the CPU blend** — the visible smear is NOT a GPU-only divergence.
2. Any `|GPU_skinned − CPU_skinnedView|` comparison **shares** the authored verts, weights, and
   palette on both sides → it is **predicted to read GREEN** (A6's own caveat: "cannot see
   palette/authored-input faults both sides share; does not track the smear if the fault is where all
   evidence points"). The readback is therefore **confirmatory-only**, not the Wave-12 fix gate.

## Instrument A (designed for Wave 12) — GPU-vs-CPU-same-palette readback [predicted GREEN]

**Purpose:** falsify the residual GPU-path hypotheses (V24 vertex decode, WGSL bone indexing,
shader-side weight normalization). A GREEN result confirms the axis is CPU-side (as predicted); a RED
result would OVERTURN B.S2 and move the axis to the GPU vertex path.

**Scope (why it's a Wave-12 build, not a diagnosis-wave probe):**
- Add a wgpu `Buffer` (MAP_READ | COPY_DST) sized to the skinned-vertex count of the target draw.
- Either (a) a tiny compute pass that runs the same skin as the vertex shader and writes positions to
  that buffer, or (b) capture post-VS positions via a transform-feedback-equivalent (wgpu has none →
  compute is the realistic path). Estimated ~150-250 LOC + a device queue submit + async map poll.
- On readback, compare each GPU position against the CPU `skinnedView` blend already computed at
  `:4370-4392`. Report worst `|Δ|` per mesh, gated behind a new `RB3_HANDS_GPU_READBACK` flag,
  getenv-gated, render-inert (dispatch only when the flag is set).
- **Pre-registered prediction:** worst `|Δ|` ≤ ~1e-3u (float noise) on `hands_naked` — GREEN — because
  wext already proves the CPU blend alone smears 106u. Log it anyway to close the axis with data.

## Instrument B (the ACTUAL Wave-12 fix gate the evidence points to) — per-vertex shell invariant

Tier-2 EXACT joint-attachment is the correct "skeleton is coherent" companion but is **structurally
blind** to the symptom (a per-bone-uniform rotational conjugation leaves the bone-ORIGIN joint fixed
at R=0 while sweeping authored VERTS at R≠0 by R·sin θ — full range only 0-0.33u, three orders below
the 106u symptom; it can never gate a shell fix). The gate that reads the symptom's magnitude:

- For each sampled authored vertex `v`, compute the CPU-blended world position `s(v)` (already done in
  the wext loop) AND a reference `ŝ(v)` = the authored shell transported by ONLY the coherent bone
  motion (i.e. bind-relative: `Σ_k w_k · v · [restW_bk]⁻¹ · liveW_bk`, using the freshness-validated
  per-bone rest B.S1's Tier-1 already captures). `‖s(v) − ŝ(v)‖` is ~0 for a shell that only follows
  coherent bone motion and grows to R·sin θ exactly where the offset carries the 87° own-rest
  conjugation. This is rest-capture-based but rest-capture-SAFE (Tier-1's pointer-identity freshness
  validation already kills the W2.8e stale-bone confound; the rest is the coherent bind, not an
  already-smeared frame).
- **A7 requirement:** this metric's worst `‖Δ‖` must rise/fall WITH wext across the 61→106u
  trajectory (unlike Tier-2 exact) — that co-variation, not co-existence, is the gate-validity bar.

## Axis named, with data (deliverable of the GREEN branch)

The evidence points to the **authored-vertex-to-offset composition (the skinned mesh SHELL)** — NOT
the skeleton (Tier-2 EXACT = 0, joints coherent) and NOT the GPU vertex path (CPU `wext` already
smears 106u with no GPU). Localization + arithmetic closure:

- Worst factor is the **far-radius finger bones**: Tier-2-approx worst `bone_R-thumb01` R=48.5u @
  89.58u; Tier-1 worst `bone_L-middlefinger03` @ 87.3° off own-rest.
- `R·sin θ = 48.5·sin(87.3°) ≈ 48.4u` per bone; the finger chain accumulates to the whole-mesh
  95-106u wext — the R·sin θ shell-conjugation signature.
- **Weight normalization ruled out:** the clean body control (`greaserjacket_resource`, same
  `wsum`-normalized blend) reads wext 41-54u normal; only the appendage meshes carrying the 87°
  offset conjugation smear. Not a global weight bug.

**Reconciliation (why the instruments disagreed):** off_b is baked in the shared-magnet basis (~87°
off each per-member bone's own rest); off_b AND liveBone_b are both in the magnet frame, so
parent/child joint ORIGINS stay coincident (Tier-2 EXACT ≈ 0) — but the authored VERTS live at R≠0,
so the rotation sweeps the shell by R·sin θ. Joint-attachment is blind to a per-bone-uniform shell
conjugation by construction; that is why it is GREEN while the shell visibly explodes.

**Fix-side honesty:** this is the SAME 87° own-rest signal Wave-9/10 refuted as a fixable *bind-side*
target (5 dead classes; rebaking off_b freezes the hands). B.S2 does NOT resurrect a bind-side fix.
Its contribution is to REDIRECT Wave 12: the fix must operate at the **per-vertex shell level** (gated
by Instrument B, which can read the 106u magnitude), not the palette/skeleton level (proven coherent)
and not the GPU path (predicted GREEN by Instrument A). No fix attempted this wave.
