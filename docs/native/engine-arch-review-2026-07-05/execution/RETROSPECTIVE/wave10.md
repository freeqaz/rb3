# Wave 10 — Retrospective (written with Wave 11–13 hindsight)

**Run:** `wf_79fca7a3-5a2`, 6 agents. Engine pin `10a9ca6 → 6834744`.
**Two lanes:** A = W2.8e hands asset-rebake (Opus, S1→S2→S3); B = WHITE-fix land + gate
(Opus, STEP-0 → B.S1 → B.S2). **Flips: none** (correctly none earned).

---

## 1. What the wave set out to do

Entering Wave 10 the campaign believed exactly two hands/scene fixes remained, each with
one sanctioned path:

- **Hands (W2.8e):** Wave 9 had "named the factor" — the finger shard was attributed to the
  skin offset's ROTATION basis being baked against the shared magnet skeleton instead of the
  per-member authored bind. Four fix classes were dead; the Wave-9 menu declared the
  asset-derived char-space static rebake **"the only unrefuted path."**
- **WHITE:** a staged `RB3_VENUE_WHITE_GUARD` scene-side patch (luminance-preserving venue
  highlight compression) waiting for single-writer sequencing to land + gate.

Fable's pre-dispatch review (`WAVE10_REVIEW.md`) was unusually strong: it caught an
impossible hard-exit (A2: RealPathFixture reads a committed static capture — a code fix can't
turn it green), demanded a two-fixture re-capture protocol, flagged the placement-yaw confound
in the ΔR metric (A3), and forced the WHITE binary gate to become paired continuous deltas
with a null control (A7). Those amendments are why the wave produced clean negatives instead
of a bad flip.

## 2. What the wave actually produced

| Item | Wave-10 verdict | Held up? |
|---|---|---|
| STEP-0 WHITE land | ✅ landed default-OFF (`2998e78`), inert-proven | ✅ yes (still landed, default-OFF) |
| W2.8e S1 | ✅ **MATCH** (6/6 tolerances) | ❌ **overturned** — instrument confounded |
| W2.8e S2 | ❌ **REFUTED** (rebake FREEZES hands) + instrument unmasked | ✅ the refutation held; forced the pivot |
| W2.8e S3 forearm | ✅ **H1** (real shard smear) | ✅ yes |
| WHITE B.S1/B.S2 | ✅ **HOLD** — primary metric sign-flips; null control = boot-noise floor | ✅ yes; spun out BOOTRNG |

The wave's own honest summary — *"No flips, and that is the system working"* — is accurate as
far as it goes. But it undersells one thing and gets one thing wrong, both visible only with
Wave 11–13 hindsight.

## 3. What Wave 10 got WRONG

### 3a. S1's "MATCH" verdict was rendered on a confounded instrument

S1 ran six pre-registered tolerances, all PASSed, and wrote `verdict: MATCH` to its checkpoint.
The signal it trusted was real: `inv(off).angleVsI = 106.0°` is **identical** across two members
with distinct 38-/40-bone skeletons ⇒ the offset is baked against the shared magnet, while each
member's palette evolves its own bone (restW 129.9°/119.3°). S1 correctly refuted the
**placement-yaw** confound Fable warned about (A3) — bone-chain roots at identity, placement in
`obj.world`, identity-placement members still shard 31–37u.

But it did **not** catch a *second* confound sitting in the same reference. As S2's `RB3_APD_DIAG`
then proved: the dualskin probe captures its rest reference `bw` from `owner->BoneTransAt(b)` at
first draw = the **static embedded bind copy** (`bound`, 129°), while `asDrawn` uses the
**post-repoint animating** bone (`own`, 106°). The 37.4u "shard" was the probe comparing the drawn
vertex against a bone the draw does not use. The MATCH verdict was mechanically defensible against
its own tolerance set and still wrong — it authorized S2 to implement the 5th dead fix class.

### 3b. The entire skeleton/bind axis was a dead end

Wave 11's rest-capture-free joint-attachment instrument (Tier-2 parent/child attachment on the
uploaded palette, 2,214 samples, perturb-proven fail-red) read **GREEN ≤0.33u on the exact
frames showing the visible 95–106u smear** — **exonerating the palette/skeleton axis entirely.**
Wave 12 named the real axis (SPACE: authored-vertex-to-offset shell composition, iso≈0 rigid
sub-shell transport); Wave 13 re-laned it as SKEL (skeleton instancing / loader merge). The
defect is a mesh SHELL that rotates about *attached* joints by ΔR (R·sin θ → 48u/bone → 95–106u
finger chain) — **structurally invisible to every skeleton-side metric the campaign built**,
because the joints stay attached and the palette is coherent.

So the Wave-9 menu premise Wave 10 inherited — "the only unrefuted path is asset-derived bind
rebake" — was a false narrowing: it assumed the defect lived on the bind/skeleton axis *at all*.
It didn't. Wave 10 spent a full Opus lane confirming (S1) then refuting (S2) a fix on an axis
that Wave 11 retired in one measurement.

## 4. The tooling gap — what would have caught it in one run

**Missing capability: a rest-capture-free joint-attachment invariant.** For each pair of verts
sharing a bone (or each parent/child bone pair on the *uploaded* palette), does the vertex move
rigidly with its bone? This needs **no captured rest reference and no "coherent" bone** — it is
a property of the palette + weights + verts as drawn.

- **Question it answers in one run:** "Is the skeleton/palette coherent, or is the basis wrong?"
  GREEN ⇒ skeleton axis exonerated ⇒ there is nothing on the bind side to bake against.
- **What the campaign did instead:** built and iterated **three waves of rest-ANCHORED metrics**
  — Wave 9 dualskin ΔR + `RealPathFixture` golden (world-space), Wave 9 S2 world-space rest
  capture (refuted, +7u regression), Wave 10 S1 like-space re-derivation + S2 char-space rebake
  (freeze). Every one compares `asDrawn` against a *captured* reference bone, and every one is
  structurally incapable of distinguishing "bone basis wrong" from "shell rotates about an
  attached joint." Each iteration changed the reference's **anchor** (world → char → like-space)
  when the defect was in the metric's **class** (rest-anchored point-comparison).

**Cost of the gap (this wave alone):** Lane A's S1+S2 = 2 Opus stages produced zero forward
progress on the fix. Across Waves 9–10 it is ~4–5 Opus stages plus 2 Fable reviews. Wave 11
built the rest-free invariant and collapsed the whole question to a single GREEN reading. Had
that instrument existed before Wave 10, S1 would have read GREEN (no MATCH), S2's freeze-rebake
would never have been dispatched, and the campaign jumps straight from Wave 9 to the SPACE_AXIS
diagnosis (Wave 12) / SKEL relane (Wave 13).

**The review process missed it too.** Fable's A2/A3 correctly attacked the metric — but only for
the *placement-yaw* confound and the impossible committed-fixture exit. Neither the coordinator
nor the reviewer identified that even a perfectly like-space-corrected reference retained the
**stale-bone (bound-vs-drawn) confound**. The generalizable lesson: when a diagnosis has survived
N metric-anchor revisions, the next move is not another anchor — it is a metric of a **different
class** (here: reference-free / invariant-based), *or* an explicit adversarial check that the
metric can distinguish the two live hypotheses.

**Second, smaller gap — a per-boot noise-floor tool run BEFORE any arm-mean visual gate.** The
WHITE lane was designed (Wave 9), staged, landed (STEP-0), and gated (B.S1/B.S2) while the
arm-mean gate was confounded by per-boot lighting nondeterminism (W0.3d) that had been *staged
since Wave 4 and never characterized*. The gate could not resolve: the ±15 hi_frac boot-noise
floor swamped the effect, and G1b (chroma-up) failed on both blind runs because washing regions
are zero-chroma anyway. The campaign discovered the floor only via the **null control inside the
WHITE gate** (a genuinely good instrument choice by B.S1) — but by then the STEP-0 + B.S1 + B.S2
budget was spent producing a HOLD that a "boot N times at a pinned shot, digest lighting/postproc
state, report variance" tool would have predicted before any WHITE work began. That tool is
exactly what Wave 11's BOOTRNG lane then built — after the fact.

## 5. What Wave 10 got RIGHT (credit where due)

- **STEP-0 serialization worked.** Fable's A1 (pre-land the tiny inert engine patch, then
  single-writer holds) executed cleanly; audit confirmed exactly one Lane-B engine commit and
  drawlog-792 byte-identical.
- **The fail-conditions fired.** S2's freeze tripwire caught the 5th dead class before any flip;
  the WHITE null control quantified the noise floor rather than rationalizing a marginal delta.
  Neither lane shipped a false positive.
- **Wave 10 unmasked its own false premise within the wave.** S1's MATCH became a new false
  premise, but S2's `RB3_APD_DIAG` refuted it in the same wave — the wave did not propagate its
  own error downstream; it named "the next step is a NEW instrument (coherent-vs-drawn-bone,
  post-repoint)," which is precisely the Wave-11 instrument that closed the axis. That
  self-correction is the wave's real product.
- **S3 (H1) held** across later waves.
- **The WHITE HOLD → BOOTRNG spin-out** was the single most valuable byproduct: it converted a
  stalled fix into the campaign's highest-leverage blocker (Wave 11 named gRand stream position;
  Wave 12 named the consumer-order rejection samplers; a partial reducer landed under
  `RB3_LOAD_DETERMINISM`).

## 6. Bottom line

Wave 10 earned no flips and that discipline was correct, but it was **one full Opus lane too
late to a metric-class problem it could not see with the metric it had.** The forward progress
of the wave was almost entirely *negative* knowledge — the 5th bind-side class is dead, and the
instrument that "proved" the hands defect is confounded. Both negatives were necessary and both
were produced honestly; both would have been produced in a single Wave-11-style reading had a
rest-capture-free joint-attachment invariant existed one wave earlier. The recurring pattern to
carry forward: **re-anchoring a confounded metric is not the same as replacing it; after two
anchor revisions, switch metric class.**
