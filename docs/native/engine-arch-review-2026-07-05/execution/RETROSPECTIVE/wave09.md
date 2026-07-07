# Wave 9 — Retrospective (hindsight through Waves 10–13)

**Reviewer:** retrospective subagent. **Read-only.** Sources: `README.md` (Wave 8/9/10/11/12
sections + Wave 9/10 menus), `WAVE9_KICKOFF.md`, `WAVE9_REVIEW.md`, `W2.8d/STATUS.md`,
`WHITE-fix/STATUS.md`, `REBASELINE/STATUS.md`, `WAVE10_KICKOFF.md`, `WAVE10_REVIEW.md`.

## What Wave 9 was

A deliberate **diagnosis-first** wave (run `wf_0a159b21-2f4`, 5 agents), no fourth blind hands
fix. Three lanes on engine pin `a320f9d`:

- **Lane A (W2.8d, Opus):** bone-level factor attribution for the finger shard — name the wrong
  factor with numbers before designing fix #4. Engine probes allowed (backend unowned this wave).
- **Lane B (WHITE-fix, Opus):** force-reproduce + localize the engaged-venue WHITE over-exposure
  residual disclosed by Wave 8; stage (not land) a fix.
- **Lane C (REBASELINE, Sonnet, capture-only):** fresh screenshot sweep vs the archived Wave-6
  baseline; surface what the user sees now.

## What shipped (no user-visible flip — correct for a diagnosis wave)

- **Dual-skin engine probe** `RB3_DUALSKIN_PROBE` (engine `30d4f00`, additive/gated), and the
  BL-A2 `RealPathFixture` golden populated (`goldens/w2.8-farvert/live_pose.txt`) so the oracle
  went SKIP → live RED (32.8u). This turned the anti-revert fixture into a hard instrument — a
  genuinely durable deliverable.
- **`RB3_APPENDAGE_REST_ROT`** landed default-OFF as a documented-refuted world-space rebake.
- **WHITE scene-side localization** proven three ways + staged patch `RB3_VENUE_WHITE_GUARD`
  (`staged-patch-scene-side.md`; landed Wave 10 as engine `2998e78`, default-OFF).
- **REBASELINE:** confirmed 4 fixes on-screen (hub grey quad gone, menu text crisp, black-head
  absent 6/6 frames, part_difficulty "missing widgets" re-refuted) + surfaced a **NEW floating
  forearm/hand anomaly** (deliberately not diagnosed; routed to Wave 10).
- Coordinator: pin bump `a320f9d` → `10a9ca6` (probes + regen, 339 clean); 792 + lineup PASS.

## What Wave 9 got WRONG (hindsight)

### 1. The headline hands verdict was a false-precise readout of a confounded instrument

W2.8d.S1 reported **"candidate (b) — the skin offset's rotation basis is conjugated ~42–87° off
the per-member rest basis, constant/pose-independent — CONFIRMED three independent ways."** This
became the Wave-10 menu's *"only unrefuted path."* All three "independent" confirmations shared
the **same confounded reference frame**: the probe compared a **char-space** `inverse(off)` against
a **world-space** `bw`, with a **first-capture-anchored** coherent reference (`Rnd_Wgpu_RB3.cpp`
:4400–4434). That carries an uncontrolled rigid **placement-yaw** term (member placements 27–60u,
same order as the 32.8u "shard").

The sharpest tell was hiding in plain sight: Wave 9 cited **"constant across the whole animation /
pose-independent"** as the *proof* the error is baked into the offset. But a placement/basis-
mismatch term is constant precisely *because* it does not depend on pose — constancy is the
**fingerprint of the confound**, not evidence against it. The wave read its own confound signature
as its strongest confirmation.

The wave's OWN S2 partly caught this ("**probe-metric caveat (A1 review risk realized)** … the
constant ~87°/43° is substantially a placement-rotation artifact") — but only *after* S1 had
already published the over-confident headline, and S2 still framed `worstSep` as "real" without
re-checking the same placement term there. The false precision propagated to Wave 10 as fact.

**How it was eventually caught (3 more waves):**
- **Wave 10 (W2.8e):** review A3 forced a like-space re-derivation; S2's `RB3_APD_DIAG` found the
  probe's coherent reference was **captured pre-repoint** — the 37.4u "shard" compared the drawn
  vertex against a bone the draw does not use. The asset rebake "fixed" the metric to 0.0u by
  **freezing the appendages**. Verdict: *"the metric is unsatisfiable without freezing … all five
  bind-side bake classes are dead … the next step is a NEW instrument."*
- **Wave 11 (W2.8f):** a **rest-capture-free joint-attachment** invariant on the uploaded palette
  read **GREEN ≤0.33u over 2,214 samples on the exact visible-smear frames** → the palette/skeleton
  axis was **EXONERATED**, retro-explaining why all five bind-side fix classes died (they patched a
  coherent palette). Axis re-named: **authored-vertex-to-offset SHELL composition** — structurally
  invisible to every skeleton-side metric Wave 9 built.
- **Wave 12 (W2.8g) → Wave 13 SKEL:** root re-confirmed as **skeleton instancing** — the animating
  `Find(name)` resolves the shared **magnet** (invOff identical 106° across members with distinct
  38/40-bone skeletons), not the per-member authored 129° rest. The renderer/bake charter cannot
  fix it.

Net: Wave 9's candidate-(b) "offset rotation basis" verdict was **directionally half-right and
operationally wrong**. The half that held: candidate (a) (pose pipeline) was correctly exonerated
— live bones ARE faithful rigid transforms, and that survived to Wave 11. The half that misled:
the "offset rotation basis is THE wrong factor, static rebake is sufficient" framing sent Wave 10
to chase an asset rebake that froze the hands, before the axis was honestly re-named.

### 2. The WHITE lane localized correctly but had no way to know its gate could not resolve a fix

Lane B correctly proved the WHITE is **scene-side** (raw PP_OFF scene ≥ composite peak; composite
*raises* mid-sat 0.045→0.283, i.e. chroma-preserve restores color, is not the gain) and staged the
guard. That localization held. But Wave 9 measured its natural reproducer at a **1/5 stochastic,
director-shot-dependent** rate and attributed the stochasticity to director-shot RNG — it had no
instrument to see that the *same* per-boot nondeterminism was an un-characterized **visual-gate
noise floor**. Downstream: the staged guard landed default-OFF (Wave 10) then was **HELD** — its
primary `d_hi_frac` metric **sign-flipped** (+17.96 vs −6.89) and a null control swung ±5–18, the
eng_hot delta being the same noise. Wave 11 BOOTRNG then named the root: **global `gRand` stream
POSITION varies per boot** (mid_sat 0.067–0.362 at identical params/shot/fixed-clock). Wave 9
staged a fix onto a gate that could not, in principle, grade it — and had no tool to discover that.

### 3. REBASELINE — held up

The floating-forearm find was correctly deferred (H1 detached-shard vs H2 wipe-illusion) and
confirmed **H1 (real shard-family smear)** in Wave 10. Good triage discipline; nothing to retract.

## Recurring bug families this wave touched

- **HANDS / finger-shard:** Wave 9 landed the **4th** dead fix class (`RB3_APPENDAGE_REST_ROT`,
  world-space rebake) and produced the confounded candidate-(b) verdict. State after wave:
  *named-but-confounded*; instrument unmasked in Wave 10, palette/skeleton axis exonerated Wave 11,
  root (skeleton instancing) reached Wave 12→13. Wave 9 did NOT advance the true fix; it advanced
  (and hardened) a false localization while correctly killing candidate (a).
- **WASH / WHITE / BOOTRNG:** Wave 9 localized WHITE scene-side + staged a guard. State after wave:
  *localized, staged, ungradeable* — blocked on the then-unnamed BOOTRNG noise floor (Wave 11).
- **UI text / layout:** not edited; REBASELINE confirmed the earlier text-floor + hub-quad fixes
  hold on-screen.

## Premise failures (entering + created)

- **Entering:** Wave 8's clue *"rotation basis wrong, translation correct"* was taken as ground
  truth and Wave 9 built a probe to CONFIRM the rotation-basis hypothesis rather than to
  DISCRIMINATE it against a null. `WAVE9_REVIEW` A1 explicitly warned the CPU reference is **not an
  independent oracle** (same inputs) and that stale-cache/placement could "masquerade as the wrong
  factor" — the review predicted the exact failure mode.
- **Created/hardened:** *"the offset's rotation basis is THE wrong factor, a static rebake is
  sufficient"* — published as "CONFIRMED three independent ways," it survived as the Wave-10 menu's
  "only unrefuted path." Caught only by Wave 10's forced like-space re-derivation + `RB3_APD_DIAG`
  and Wave 11's rest-free instrument. Cost: ~2 waves chasing a confounded target.

## Tooling gaps (the load-bearing lesson)

1. **A space-consistent, rest-free skinning-fidelity oracle — the single missing tool.** Wave 9's
   dual-skin probe mixed spaces (char-space offset vs world-space bone), anchored the reference at
   first-capture, and — per Wave 10 review — only *printed* at `wext>60` (`Rnd_Wgpu_RB3.cpp:4325`),
   so **a working fix drops wext below 60 and the instrument never fires: success suppresses its own
   evidence.** The tool that would have answered "is the skeleton/palette axis faithful?" in ONE
   run is exactly what Wave 11 finally built (Tier-2 joint-attachment invariant on the uploaded
   palette, GREEN ≤0.33u, fail-red proven by perturb). Had it existed at Wave 9, it would have
   exonerated the palette/skeleton axis immediately and pointed at the SHELL/skeleton-instancing
   axis — collapsing **~4 waves / ~8 Opus agent-stages** (W9 S1+S2, W10 S1+S2+S3, W11 W2.8f, W12
   W2.8g) of hands work down to a diagnosis + a lane hand-off. Instead the campaign built,
   confounded, unmasked, and rebuilt the instrument across four waves.

2. **A "does this metric divide out placement?" self-check.** The review named the risk (char-space
   vs world-space, placement not divided out); S2 confirmed it *post hoc*. A cheap invariant —
   "recompute the metric with member placement zeroed; if the number moves, it's confounded" —
   would have flagged the 42–87° as placement-laden **in S1's own run**, before it was published as
   a verdict. The campaign instead spent a full Wave-10 review cycle (A3) prescribing the like-space
   re-derivation.

3. **A per-boot determinism audit ("what varies at a pinned shot under fixed clock?").** Wave 9's
   WHITE gate had an un-characterized noise floor. The tool that answers this in one run — Wave 11's
   BOOTRNG instrument (per-light VALUE digest + gRand draw-count spread across N boots) — would have
   told Lane B up front "your visual gate cannot resolve a fix until gRand stream position is
   pinned." Wave 9 staged a fix onto an ungradeable gate for want of it; it took until Wave 11 to
   name the floor and Wave 12 to attribute it.

## Wasted effort

**Moderate.** Not total waste — the candidate-(a) exoneration held, the RealPathFixture became a
durable instrument, REBASELINE and the WHITE scene-side localization held, and the process caught
S2's world-space rebake at its tripwire (nothing bad shipped). The waste is the **false-precision
cascade**: S1's over-confident candidate-(b) headline (a confounded placement term read as a fix
target) hardened into the Wave-10 "only unrefuted path," costing Wave 10's asset-rebake stage
(froze the hands) and part of Wave 11 before the axis was honestly re-named. Attributable cost ≈
one wasted Opus fix-stage (W10 S2) + the confound-unmasking overhead, all of which a
space-consistent rest-free oracle at Wave 9 would have avoided. The WHITE lane's staging effort was
sound but its fix could not be graded until BOOTRNG (Wave 11), so ~1 stage of gate work was
premature.
