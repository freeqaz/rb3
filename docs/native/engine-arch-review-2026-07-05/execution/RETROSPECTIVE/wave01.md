# Wave 1 retrospective (hindsight review)

Run `wf_386f9206-c04`, 35 agents, 2026-07-05. Engine `a8089c3` → `9561a19`.
Items: W0.1 skin golden · W0.2 loud stubs · W0.3 draw-log golden · W0.4 bone live-pose ·
W0.5 non-blind lineup gate · W0.6 flag registry · W1.1 WGSL externalization.
Reviewed with knowledge of Waves 2–16. (Waves 1–2 predate the pre-dispatch review gate, so there
is no `WAVE1_KICKOFF/REVIEW` — a fact that itself matters below.)

## What this wave was for

Wave 1 is the "hang the safety nets before anyone walks the trapeze" wave: build the goldens,
gates, flag registry and file-hygiene infra that later structural refactors (DrawContext, placement
contract, hands rebind) would land against. Judged as setup work, it largely succeeded — 6/7 items
complete with fail-red proven. The interesting part with hindsight is not the six greens; it is the
**two conclusions Wave 1 shipped that later waves spent eleven waves refuting**, both of which trace
to a single missing tool.

## Shipped (held up)

- **W0.5 non-blind lineup gate** — the genuine durable win. It *proved the old image-only gate was
  blind* to patch-shard corruption (broken-skin run PASSes the old image layer + FAILs the new
  ratioB layer). This gate ran "lineup PASS" as a hard wave gate through all 16 subsequent waves.
  This is the model of what the whole wave was trying to be: a gate that measurably catches what
  "looks fine" misses.
- **W0.6 flag registry** — became the census every later wave regen'd (321→360 flags by Wave 15).
  Held, though it needed a Wave-4/5 coverage extension (it didn't scan `rb3/src/system/`, so
  `RB3_HANDS_BIND_FIX` went undetected — a real coverage gap surfaced two waves later).
- **W1.1 WGSL externalization, W0.2 loud stubs** — clean, held, uncontroversial.
- **W0.1 / W0.4 goldens** — the skinning + effector faithfulness oracles. They *work* for what they
  measure (fail-red proven: 25.5u hand fling, effector perturb). The problem is what they measure —
  see below.

## What Wave 1 got wrong (the two conclusions later waves refuted)

### 1. Draw-log nondeterminism mis-scoped as "wall-clock splash animation, fix with a frozen clock"

W0.3 landed partial with an exhaustively-documented conclusion (`W0.3/STATUS.md` S3 + VERIFY): the
`splash_screen` draw count jitters 885–890, this is wall-clock/real-dt-paced animation internal to
the scene, and the fix is a trace-free frozen-sim-clock seam → re-golden. Wave 1 correctly ruled out
gamewarm / tex-prewarm / async-open and settle-window length. Then it stopped.

Hindsight: the frozen clock (Wave 2 W0.3b) made the *count* exactly deterministic (888×) and
immediately exposed a **deeper layer Wave 1 never saw**: a 15-run sweep found ~33% draw-**submission-
ORDER** nondeterminism (mesh-identity swaps, up to 354-draw divergence, byte-identical binaries
disagreeing run-to-run). That order axis then became the campaign's longest structural thread:
Wave 3 root-caused it to async-loader completion order (Exit-A/B reshape), Wave 5 landed a SortDraws
tie-break, Wave 11 finally *named* it (global `gRand` stream **position**, not seed), and Wave 12
was still refuting H-TIMING and re-naming H-ORDER. It is the **BOOTRNG noise floor** that made every
later per-boot visual gate (WHITE, wash matrix, arm-mean gates) non-resolving.

Why Wave 1 couldn't see it: its `--determinism-check N` mode reported a count spread plus a
name-hash **multiset** drift. A multiset is order-insensitive *by construction* — the one tool Wave 1
had for this question was structurally incapable of surfacing the order axis. So Wave 1's honest
"counts jitter, needs a clock" was the correct read of a metric that hid the real problem.

### 2. The skin/effector goldens are blind to the actual finger-shard bug they were built to gate

`W0.1/STATUS.md`: the golden poses the character with a fixed ~34° `SetLocalRot`, checks 10 verts
(6 hand/finger), proves fail-red at 25.5u, and was framed (README Wave 3) as "the exact
bind-pose-identity oracle whose absence caused the two BandPatchMesh reverts." W2.2 (Wave 3) then
declared the hands fix "one flag-flip from shipping," gated on exactly these goldens.

Hindsight: Wave 6 (W2.8) measured the real finger shard — R·sin(θ) far-vertex smear, 79–107u — and
found it reads **CLEAN** on whole-mesh ratio + origin skinpos. W2.2's oracle (built on the W0.1/W0.4
pattern) is **blind** to it. The golden validates `RefSkinVertex ≡ compiled SkinVertex` given a
palette — a real faithfulness property — but the hands bug is that the *palette's bind basis* is
wrong (offsets baked against the shared magnet / male bind while drawing per-member gender-posed
bones). The golden feeds a correct-by-construction palette to both sides, so it can never see a
wrong-basis palette. The far-vertex rotation oracle (BL-A2) wasn't built until Wave 7; the
trustworthy joint-attachment instrument until Wave 11; the saga was only adjudicated at Wave 15.

The campaign's own Wave-3 review gate (instituted *because* Waves 1–2 had none) caught the adjacent
mechanical fact — "skin/effector goldens run in the DC3-context suite and never exercise the rb3
band-rebind path" — but nobody connected that to the far-vertex blindness for three more waves.

## Premise failures

- **Created:** "draw-log jitter = wall-clock splash animation, clock-fixable." Rode into Wave 2,
  caught by W0.3b's 15-run order sweep, fully un-wound only by Wave 11's `gRand` naming (10 waves).
- **Created (more expensive):** "skin + effector goldens are sufficient anti-revert gates for the
  hands fix." Rode UNCHALLENGED through Waves 3–5 (W2.2 declared ship-ready on their strength).
  Caught by Wave 6's far-vertex reading + the user-reported visible shard.
- **Process:** Wave 1 lost commits to a concurrent `git reset` on the shared tree (recovered by the
  W0.6 verifier, rb3 `6c8a3bbf`) — which is what added hard rule 7. A within-wave failure, correctly
  turned into a standing rule.

## Recurring-bug-family state after Wave 1

- **BOOTRNG / draw-order nondeterminism:** SURFACED (as count jitter) but mis-classified (multiset)
  and mis-scoped (clock-fixable). Left open and mis-framed.
- **Hands/fingers shard:** oracle built, but blind — left *falsely believed covered*.
- **Wash / UI-text / layout:** not touched (later waves).

## Tooling gaps (concrete)

1. **Order-aware determinism boot-sweep.** Missing: a harness that boots N times at a pinned frame
   under a fixed clock and classifies each divergence into orthogonal axes {count · submission-order
   · mesh-identity · world-xfm-value}. Wave 1's `--determinism-check` collapsed everything into a
   count-spread + name-hash *multiset* (order-blind). **One run** of an order-aware sweep at W0.3
   would have answered "count-only or order-too?" — the exact question Waves 2 (discover), 3
   (root-cause), 5 (tie-break), 11 (name gRand), 12 (H-TIMING/H-ORDER) each re-approached across
   ≥5 waves of lane-stages. The frozen clock (a prerequisite for this sweep) was also the thing
   whose absence made W0.3's own "inert when off" screenshot proof unverifiable — same root gap.
2. **Far-vertex / rotation-basis skinning oracle.** Missing: a metric on appendage vertices FAR from
   their dominant bone, under real per-member animation, vs an independent posed reference. Wave 1
   built a bind-relative faithful-to-compiled golden (correct but orthogonal). **One run** of a
   far-vertex oracle on the shipping build shows the 79–107u shard RED at Wave 1 instead of Wave 7 —
   and would have framed the hands problem at its true (much harder, ultimately asset/skeleton-side)
   difficulty from the start, instead of letting W2.2 be called "one flip from shipping." This is the
   single largest downstream effort sink in the campaign (Waves 6–16, 7 measured-dead fix classes).
3. **External ground-truth capture (Dolphin / milo-trace single-bone).** Every hands golden proves
   "faithful-to-reference," never "the changed bind is *right*." Wave 15 finally names
   Dolphin+milo-trace single-bone capture as the fallback truth. Had it existed at Wave 1, hands
   fixes could have been adjudicated against real hardware rather than a self-referential oracle.

## Wasted-effort estimate

- **Determinism thread: MODERATE.** Wave 1's own W0.3 investigation was thorough and not wasted (it
  legitimately eliminated several mechanisms). The waste is that the multiset-based determinism-check
  forced the order axis to be re-discovered in Wave 2 and re-approached in Waves 3/5/11/12 — an
  order-aware sweep at Wave 1 would have front-loaded that into one diagnosis.
- **Hands oracle: LARGE, but realized downstream, not in Wave 1.** The goldens themselves are real
  faithfulness gates and caught the coarse fling class; that work isn't wasted. What was expensive is
  the *false confidence* they created (W2.2 "ship-ready"), which the Wave-6 far-vertex reading and
  the user-visible shard had to unwind, opening an 11-wave saga. Not Wave 1's fault to *solve* the
  hands bug, but a far-vertex metric in W0.1 would have set the correct problem framing 5–6 waves
  earlier.

## One-line lesson

Both misses are the same shape: **Wave 1 built gates that were fail-red-proven against the failure
mode they imagined, not against the failure mode that existed** — a count-jitter clock problem
instead of an order-permutation RNG problem; a bind-relative fling instead of a far-vertex rotation
shard. The fix is not more gates but *adversarial coverage checks on each new gate* — the exact
discipline the Wave-3 review gate later institutionalized, and which W0.5 (the one Wave-1 item that
explicitly proved its predecessor blind) already embodied.
