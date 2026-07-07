# Wave 6 — Retrospective (hindsight review)

**Run:** `wf_9bfc818f-c39`, 16 agents + 3 side agents (2026-07-06). Engine `8e7eddd` → `0f3d7ef`.
Kickoff+review commit `caf28a2b` (Fable review, 8 amendments adopted). Reviewed with the hindsight
of Waves 7–8, which refuted two of this wave's diagnoses.

## What the wave set out to do

Wave 5 had the crowd/drum **placement contract** mechanically ready but the coordinator HELD the
flip at the E1 gate because one flag-ON capture blew out near-white (1/2 ON vs 0/2 OFF). Wave 6's
job was to **characterize that wash and ship the flip if the asymmetry was false**, plus land the
W3.1b lighting tail, prototype BoxMap, open Phase-4 UI parity, and characterize three
user-reported visuals (black singer head, grayscale venue, missing hands/transparent torsos).

## What actually landed (the wins)

- **The flip SHIPPED (`fced18b`).** This is the wave's headline and it was correct. The Wave-5
  hold rested on a *premise* — "wash is flag-ON-specific." Wave 6 refuted it under time control:
  `wash_score.py` (luma both-tails + pink-hue), songMs-pinned to 21000±250, interleaved OFF/ON,
  n=7/state → wash flag-OFF **2/7** vs flag-ON **4/7** (Fisher p≈0.59), luma Mann-Whitney **p=1.0**,
  and the wash was a full-frame magenta env cast a crowd-only transform cannot physically produce.
  A/A-variable → the hold reason evaporated → flip. This is the single best piece of work in the
  wave, and it only happened because the Fable review (A1/A2) forced *continuous* scoring +
  *time-pinning* over the coordinator's original underpowered N≥8 binary protocol. The original
  protocol would most likely have returned "indistinguishable" regardless of ground truth and
  converted low power into a ship decision — the review caught that before dispatch.
- **Bloom-halo suspect demoted before it burned a stage.** The kickoff's prime suspect ("crowd
  emissive geometry feeds the bloom pass into a wash") was mechanically impossible — the halo
  capture is `game.cam`-gated (`Rnd_Wgpu_RB3.cpp:4403`) and the A/B pins a *venue* cam. Fable's A3
  killed it in review from source, so S3 spent its budget ranking real alternatives instead of
  chasing a fixed-before-commit bug. Good example of pre-dispatch source review paying off.
- **Black singer head FIXED default-ON (`837808e1`).** Clean root cause (head-detail textures in a
  nested milo subdir unreachable by non-recursive `dir1->Find`; no `dummy_*` fallback → null
  diffuse → flat black/pink), game-side, Wii byte-identical. Held up.
- **W3.1b lighting tail complete** (projLight `RB3_ENV_PROJLIGHT`, fog verify closed). Held up.
- **BoxMap prototype-only discipline (Fable A6) prevented a bad land.** Declared
  worktree-prototype-only up front; the prototype's own data then refuted its design (boot-reachable
  venues have 27 point + 1 fakespot + **zero** directional approx lights → the directional cube is a
  near-no-op). Wave 7 formally DROPPED it. Not landing a near-no-op into the 656-byte cross-backend
  `SceneUniforms` contract is a real avoided cost.

## What the wave got WRONG (refuted by later waves)

### 1. W3.3 grayscale-venue mechanism — false, cost a landed no-op flag

Wave 6 diagnosed the song-start grey wash as **postproc-composite over-exposure**: the reveal runs
hotter than Wii GX, and "the per-channel Reinhard **ceiling** guard desaturates hot pink → grey"
(`W3.3/STATUS.md:9-65`). It staged a **luminance-preserving-ceiling patch**
(`staged-luma-preserving-ceiling.patch`, backlog `e5166976`) as the fix.

**Wave 7 refuted this from its own STATUS table** (`W3.3/STATUS.md:206-216`): a per-tonal-band
saturation decomposition showed the grey is a **sub-knee MID/LOW-tone desaturation** (default
mid-tone sat 0.026 vs pp_off control 0.389), and **both** ceiling branches are identity below the
knee (L<0.82) — the highlight-ceiling fix **architecturally cannot touch it**. Wave 7 still *landed*
the ceiling (`RB3_PP_LUMA_CEILING`, engine `7943bfa`) before A.S2's determinism-controlled analysis
proved it a no-op-on-target, so it stays in the tree as a documented dead flag. Wave 8 finally
fixed the real thing with `RB3_PP_CHROMA_PRESERVE` (graded luminance × ungraded chroma) — and
Wave 8's `RB3_WASH_PROBE` instrumentation *also* overturned the intermediate "revealed pink base"
theory (0/8 boots miss engagement; it's a downstream screen-space magenta flood, not an unmasked
base).

**Root of the error:** Wave 6's W3.3 read leaned on a **3-way flag matrix at a single songMs
(ms3000)** — default=grey / PP_OFF=color / VENUE_LIGHT_OFF=color. That correctly *localized* the
artifact to the composite stage, but a "default vs PP_OFF" A/B cannot distinguish *which* stage of
the composite (highlight ceiling vs sub-knee grade) removes the color. Wave 6 filled that gap with
a plausible-but-wrong mechanism story ("over-exposure → ceiling clip") from a single-crop visual
read.

### 2. W2.8 hands fix-class prediction — steered two waves into dead ends

Wave 6 characterized the finger shard correctly (R·sin(θ) sheeting; animated bone basis ≠ static
magnet the invBind was baked against) and, crucially, proved **W2.2's oracle is BLIND** to it
(whole-mesh ratio + origin skinpos read CLEAN). But it prescribed the fix as **BL-A1
rotation-aware invBind rebake** — a *static* correction. Wave 7 killed the rigid-anchor static
class (`RB3_HANDS_POSEAWARE` distorts per-bone-authored hands 106→205u); Wave 8 killed the
game-side per-bone conjugation (amplified twist 80u→500-2600u). Three fix classes are now dead with
numbers, and only in Wave 8 did the campaign conclude the correct *first* step was **bone-level
attribution** (dump per-bone offset/boneWorld/weight on a sharding vertex, name the wrong factor
before attempt #4). Wave 6's "static rebake" prescription aimed the next wave at a static approach
that was already doomed by the very "animated basis ≠ static magnet" diagnosis it wrote down.

## Tooling gaps (name the missing capability)

- **Per-tonal-band (knee-split) saturation decomposition, with a composite-OFF control.** One run
  outputting `{mid_sat, low_sat, high_sat}` for `default` vs `pp_off` would have told Wave 6 "the
  desat is sub-knee; a highlight ceiling is identity here" and killed the wrong mechanism *before*
  staging a patch. The campaign instead discovered this in **Wave 7 across two agent stages** (A.S1
  land the ceiling → A.S2 refute) plus a landed dead flag. The tool arrived implicitly as Wave 7's
  per-tonal-band verify; it should have been the Wave-6 W3.3 diagnostic gate, not the Wave-7
  post-mortem.
- **A render-input probe that dumps per-boot lighting/composite state at capture** (what became
  `RB3_WASH_PROBE` in Wave 8, engine `71469af`). Wave 6's WASH backlog ranked *async asset
  residency* as the #1 prior; Wave 8's probe showed **0/8 boots miss env engagement** — residency
  was not the mechanism at all; it's a composite-grade magenta flood. Between them the campaign ran
  a **~40-boot flag matrix in Wave 7** (`wash_matrix.py`, partial/in-flight) chasing a
  black-box-flag-isolation answer that a single instrumented boot later gave directly. An
  in-engine capture-time state dump would have collapsed the wash *and* grayscale attribution into
  one run instead of a matrix + two waves.
- **A far-vertex rotation oracle for skinned appendages** (built in Wave 7 as BL-A2, RED at
  79-107u). Wave 6 *named* the gap ("W2.2's oracle is BLIND … needs a far-vertex rotation metric")
  but the characterization-only lane didn't build it, so the gate that would empirically kill each
  static fix class didn't exist during Wave 6's own fix-class recommendation. Building the oracle in
  the same wave that discovered the blindness would have let Wave 6 test its own BL-A1 prescription
  and reject it a wave earlier.

## Wasted effort (estimate)

**Moderate.** Two attributable sinks, both downstream-triggered by Wave 6 diagnoses:
1. The W3.3 staged luminance-ceiling patch (`e5166976`) → Wave 7 landing it (`7943bfa`) → Wave 7
   refuting it: ~2–3 agent-stages producing a flag that is a documented no-op on the target. A
   per-tonal-band decomposition at Wave 6 W3.3 (a few minutes of scripted analysis) would have
   avoided staging the wrong-mechanism patch entirely.
2. The hands "static rebake" prescription steered Wave 7's B-lane into the rigid-anchor class
   (`RB3_HANDS_POSEAWARE`, landed default-OFF but proven distorting) — one more dead static class
   before Wave 8 called for attribution-first. ~1 agent-stage of misdirected fix attempt.

Everything else (the flip characterization, black head, W3.1b, BoxMap prototype) was well-spent;
the flip work in particular *saved* effort by refuting a false hold premise rather than shipping a
compensating hack. Net: the wave's process (Fable pre-dispatch review, prototype-only fencing,
oracle-blindness disclosure) was strong; its two diagnostic misses came from **accepting the first
plausible mechanism from a coarse A/B instead of decomposing the signal**, and both were caught —
but a wave or two late — for lack of the tonal-band and render-state-probe tools.

## One-line lesson

A "default vs feature-OFF" A/B **localizes** an artifact to a subsystem but does not **identify the
mechanism inside it**; Wave 6 twice filled that gap with a plausible story (highlight-ceiling
over-exposure; static invBind rebake) that a cheap decomposition tool (per-tonal-band saturation;
per-bone factor dump) would have falsified in one run — instead the campaign falsified them across
Waves 7–8 with a landed no-op flag and two dead fix classes.
