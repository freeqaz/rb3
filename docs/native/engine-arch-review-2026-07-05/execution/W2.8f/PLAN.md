# W2.8f — THE TRUSTWORTHY HANDS INSTRUMENT (Lane B, Opus)

**Charter:** WAVE11_KICKOFF Lane B (S1) + COORDINATOR ACCEPTANCE A5/A6/A7/A8. Build the
rest-capture-free palette-internal hands instrument the Wave-10 refutation demanded. NO
fix this wave — the exit is a TRUSTWORTHY INSTRUMENT + Tier-1/Tier-2 readings.

## Why the old instrument is dead (from W2.8e/STATUS.md A.S2 + WAVE11_REVIEW A5)
- The dual-skin "shard" (37.4u RED) compared the drawn vertex against a **pre-repoint stale
  bone** the draw does not use — unsatisfiable without FREEZING the hands (S2 refuted, 5th class).
- The old probe's "rest" was captured at the first `wext > 60` frame — i.e. an **already-smeared**
  frame (the gate `:4411-4413` encloses the capture `:4459-4472`). Any rest-capture-based
  reference is untrustworthy.
- "Track the post-repoint live bone" (kickoff draft) = trivial-zero trap: the rebind bakes
  `off = inv(rest(own))` against that same bone → `v·inv(rest_own)·live_own` vs
  `asDrawn = v·off·live_own` is ~0 by construction. Measures bookkeeping, not the defect.

## The instrument (two palette-INTERNAL invariants on the UPLOADED palette; NO rest ref for Tier 2)
At the existing dualskin site region, gated by a NEW dedicated flag `RB3_HANDS_ATTACH_PROBE`
(NOT gated on `wext>60`, so it co-samples the full 61→106u trajectory for A7):

- **Tier 1 (amplitude predictor, secondary):** full-palette per-bone sweep with
  **pointer-identity freshness validation** — capture `restW_b = BoneTransAt(b)->WorldXfm()`
  once per pointer-identity; the repoint bound→own IS a pointer change → recapture (kills the
  W2.8e stale-bone confound). Report per bone `angle(off_b·restW_b, I)` (A5 literal form; ~0 for
  a coherent `off=inv(restW)` bake, magnet-vs-own residual for a mis-baked/mixed entry) AND the
  pose-independent cross-check `angle(inv(off_b), restW_b)` (= per-bone thetaOffRest). Catches
  stale/mixed palette entries the worst-bone-only DIAG could not see.
- **Tier 2 (PRIMARY, pose-tracking, rest-capture-FREE):** parent/child joint-attachment. For each
  palette bone `b` with parent `p` (via `TransParent()`, pointer-matched into the owner bone list),
  the authored child joint `j_b = −off_b.v` (the R-radius base, `:4545`) must map to the SAME world
  point under BOTH uploaded palette matrices: `attach_b = ‖j_b·P[p] − j_b·P[b]‖` using
  `bones.bones[]` directly. ~0 at EVERY pose for a coherent palette; grows as `R·sin(θ_pose)`
  (exact form + localization of the visible smear); ZERO at rest. Also computed with the EXACT
  bind joint `inverse(off_b).v` as a cross-check (same point both sides → invariant holds either way).

## Files / line ranges (A8 — declared BEFORE editing)
- **Engine `src/platform/Rnd_Wgpu_RB3.cpp`** — additive probe block inserted at the tail of the
  dualskin probe cluster, **:4671–4672 boundary** (just after the `RB3_DUALSKIN_PROBE` block
  closes, still inside `if (n>=3)`). Declared Lane-B range :4389–4672. Probe-only, getenv-gated,
  render-inert with the flag unset. No collision with Lane A (:1044–1660 + RB3PostProc.*).
- **`BandCharacter.cpp`** — read-mostly, NOT edited this stage.
- **`NativeCompatFlags.classification.json`** — append-only 1 row `RB3_HANDS_ATTACH_PROBE`
  (flock `/tmp/milo-engine-classjson.lock`; NO gen.inc regen — coordinator).

## Verify / gates
- Build dir `native/build-agent-W2.8f` (own dir; never touch build-native/build-web*).
- **drawlog-golden `--fixed-clock --canonical-order` flag-OFF = 792** re-run AFTER the engine
  probe commit (WAVE11_REVIEW A8): the probe must be byte-identical render-inert.
- Run the instrument on hands/finger palettes across an animated gameplay window; record Tier-1
  (worst deg, count>5°, freshRecap) and Tier-2 (worst u, exact-joint worst u, R, pairs) with the
  co-sampled `wext` per frame.
- **A7 co-sampling:** Tier-2 worst must rise/fall with the measured `hands_naked` worldExt
  (61→106u) on the same frames — else the metric is RED for its own reasons (report honestly).
- In-run negative control: a clean body mesh (`greaserjacket_resource`) in the same run.

## Exit
Committed instrument + STATUS.md with Tier-1/Tier-2 readings + the co-variation verdict. NO fix.
Checkpoint `/tmp/wave11-checkpoints/B-S1.json` written before returning.
