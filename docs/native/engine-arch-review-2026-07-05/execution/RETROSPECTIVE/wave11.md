# Wave 11 retrospective (hindsight through Wave 15)

Run `wf_8a8111a5-b24`, 2026-07-07, diagnosis-only, 2 lanes (Opus). Engine pin `6834744` → `146fd19`.
Sources: `WAVE11_KICKOFF.md`, `WAVE11_REVIEW.md`, `BOOTRNG/STATUS.md` (A.S1/A.S2), `W2.8f/STATUS.md`
(B.S1/B.S2), README Wave 11–15 results, `HANDS-ADJUDICATION/VERDICT.md` (Wave 15).

## What Wave 11 was

A deliberately fix-free two-lane diagnosis wave, dispatched after Fable's `dispatch-with-amendments`
(9 amendments, all adopted). Both walls that ended Wave 10 were MEASUREMENT problems, so the exits
were named mechanisms + trustworthy instruments, not landings.

- **Lane A — BOOTRNG:** root-cause the per-boot lighting/grade nondeterminism at a pinned
  `coop_dir_crowd.shot`/songMs≈21000 under `RB3_FIXED_CLOCK` (mid_sat swings 0.063–0.451).
- **Lane B — W2.8f:** build the trustworthy hands instrument and reconcile the Wave-10 contradiction
  (dual-skin metric refuted as stale-bone-confounded vs A.S3's confirmed-real 61→106u visible smear).

## What it shipped (instruments + named mechanisms; no behavior change)

**BOOTRNG.** Refuted its own pre-registered prime suspect (H-A: preset-pick partitions the boots) —
every lighting category holds exactly ONE preset so `PickRandomPreset`'s `RandomInt(0,1)≡0`; 12/12
boots identical picks. Value-level exoneration of lighting selection AND grade (resolved ColorXfm
`ppres_tail` byte-identical 12/12, per-light VALUE digest identical across a WHITE and a NEUTRAL
boot) — closing the A3 color-blindness hole in Wave-8's count-only exoneration. Named the boot-varying
state = **global `gRand` STREAM POSITION** (~11k draw-count spread, 12 distinct values / 12 boots).
Free harness lever found: capture `--tol` 2000→100–150 ms (songMs Pearson 0.77 = dominant confound).
Probes committed rb3 `fe696d63` / engine `29fc0aa`.

**W2.8f hands.** Built the **Tier-2 EXACT joint-attachment** instrument (`inverse(off_b).v`,
rest-capture-free, fail-red proven 0.33u→5.53u), engine `4c93608`. Reads GREEN ≤0.33u over 2,214 hand
samples ON the exact 95–106u smear frames, and A7 co-variation FAILS → **palette/skeleton axis
EXONERATED**; `wext` reproduces the smear in a pure CPU 4-bone blend → GPU exonerated too. Refuted
A5's *approximate* joint spec (`−off.v` reads 90u false-positive on the clean body control). Named the
axis: **authored-vertex-to-offset composition (the mesh SHELL)**.

## What it got right, and held

- The **no-fix discipline + self-refutation** worked: both lanes killed their own prime suspects
  (H-A preset-pick; A5's approximate joint) rather than confirming them.
- **Tier-2 EXACT joint-attachment** is the one durable artifact — it stayed the trustworthy
  "skeleton is coherent" companion gate all the way through Wave 15's adjudication.
- The **value-level lighting/grade exoneration held** (Wave 12/15 never overturned it).
- Directionally right that hands is an **offset/anchor** problem, not skeleton or GPU — Wave 12
  confirmed `SPACE_AXIS` (decode refuted; sub-shells transported isometrically), Wave 15 confirmed it.

## What it got wrong (refuted by later waves)

### 1. BOOTRNG hand-off embedded a nonexistent patch + a wrong mechanism

Wave 11 handed BOOTRNG to "**W0.3d part-b (staged since Wave 4)**" as an owned, staged,
coordinator-sequenced fix for "**async-loader/worker COMPLETION-ORDER**." Both halves were false:

- **Wave 12 review A1 correction:** "no staged patch exists — the only staged part-b artifact was the
  SortDraws tie-break, landed Wave 5; this is NEW design work." Wave 11 (and its STATUS §"BACKLOG"
  item 1) treated a design note as a landed-adjacent patch.
- **Wave 12 A.S1 REFUTED completion timing:** all 511 loader completions byte-identical across boots
  and landed by frame 2, yet gdraw diverges from frame 4 with ZERO completions on diverging frames.
  The real mechanism = **consumer-ORDER × variable-count rejection samplers** (unsorted `mAnims` walk,
  `Rand::Gaussian` do-while, `CameraShot`/`Crowd` conditional draws) sourced from a main↔worker
  glibc-arena allocation race. Wave 11's STATUS said "async-loader/worker completion-order" six times.
- The optimism didn't survive contact: Wave 12 A.S2's reseed+serialize attempt cut the spread only
  ~62% and **PRIMARY-FAILED** (reseed fixes VALUES not ORDER). Wave 11's implicit "close part-b and
  the gate resolves" underestimated a still-open problem.

### 2. Hands: the "87° magnet conjugation / shell rebake" mechanism was inverted, then refuted

Wave 11 named the axis correctly (shell/offset) but attached a **causal story that was wrong twice**:
"off_b baked against the **shared-magnet basis**, 87° off each per-member bone's **own** rest" — a
scalar "**magnet-vs-own conjugation**" reproducing A.S1.

- **Wave 13 PREMISE INVERTED it:** runtime pointer identity shows `own=Find(name)` is PER-MEMBER and
  ANIMATES (4 distinct ptrs, gender-posed), while `bound=BoneTransAt` is the SHARED static bind. "The
  old 'shared magnet animates' reading = the dual-skin probe sampling PRE-rebind state with own/bound
  **labelled backwards**." Wave 11 re-affirmed the backwards labels.
- **Wave 15 correction #2:** "The 87.3° is **not** a mysterious 'magnet conjugation' — it is the
  **B-vs-seed-R** relative rotation" (the rebake anchors hand offsets to a TRANSIENT SetDeformation
  seed pose R, 87.2° off both the authored bind B and the play pose ≈B), computable OFFLINE from
  committed matrices.
- The "**shell**, rebake-the-shell" framing **licensed the wrong fix family**: Wave 12's
  `RB3_HANDS_SHELL_FIX` cell (shared-B onto every appendage mesh) and Wave 14's `RB3_HANDS_RESKIN`
  were both implemented and **refuted by their own gates**. Wave 15 then showed SHELL_FIX's death
  certificate was **CONFOUNDED** (it is correct for the male `hands_naked` at 0.1°, wrong only because
  it was misapplied to female/gloves/nails), and that the ACTUAL fix — **keep each mesh's OWN authored
  offsets and repoint, `SetBone(b, own, false)`, NO rebake** — was the one option cell **NEVER
  MEASURED** until Wave 15. Wave 11's mechanism story pointed the campaign at rebaking the shell; the
  fix keeps the shell's authored offsets untouched — the opposite move.

## Premise failures — entering and created

- **ENTERING (inherited, not caught): own/bound labelled backwards.** Wave 11 had the uploaded
  palette AND the offset matrices at the probe site, but reported **scalar angle-vs-identity** values
  (87.3°), which structurally cannot reveal a label swap between two instances. So it re-affirmed
  "magnet-vs-own conjugation." Caught two waves later (Wave 13) by comparing runtime POINTERS, and
  fully explained (Wave 15) by comparing MATRICES. Wave 15 correction #3 states the general lesson:
  "Scalar angle-vs-identity comparisons mislead… Future instruments must compare matrices."
- **ENTERING (inherited, not caught): "W0.3d part-b is staged."** Carried into BOOTRNG's backlog as a
  ready-to-land item. Caught at Wave 12 review A1.
- **CREATED: "the 87° is shared-magnet-basis conjugation of the shell."** This is the load-bearing new
  false premise — it validated the shell-rebake fix family (SHELL_FIX, RESKIN) that consumed Waves
  12–14. Unwound only at Wave 15.

## Tooling gaps (named capability → the one run it would have replaced)

1. **Per-call-site gRand consumer-sequence differ.** A tracer that logs each `RandomInt/RandomFloat`
   call site + running draw-count and diffs the consumer SEQUENCE between two boots, naming the FIRST
   diverging consumer and whether it is a rejection sampler. Wave 11 built only a **global
   draw-COUNTER** (`RB3GRandDrawCount`) — it proved the *position* diverged but not *where* or *why*,
   so it could only hand off a POSITION with a guessed ("completion-order") mechanism. Wave 12 then
   had to build `RB3_LOADDET_PROBE` and spend A.S1 **and** A.S2 to discover the mechanism is
   rejection-sampler order (and to refute completion timing). One run of a sequence-differ would have
   printed "first divergence at `CameraShot.cpp:265` shake sampler, +N draws" and killed the
   completion-order hypothesis before it was carried a full wave. **Cost of absence: ~2 agent-stages +
   one wave carrying a wrong mechanism.**

2. **Per-bone MATRIX relational comparator with pointer-identity instance labels.** Report the actual
   relative rotation (axis + angle) between {authored offset basis, seed-rest R, live `own`, shared
   `bound`} as MATRIX products, each runtime instance labelled by validated pointer. Wave 11 reported
   the 87.3° as a scalar angle-vs-identity and got the provenance backwards. Wave 15's
   `offset_basis_derivation.py` computed the truth (`B·inv(R)=87.2°`, `own≈B` at 3.1°) **offline from
   already-committed matrices** — i.e. the data existed at Wave 11; only the instrument SHAPE (scalar,
   not matrix) hid it. This single tool would have prevented the own/bound inversion AND the entire
   shell-rebake detour (Waves 12–14 fix attempts). **Cost of absence: the largest single line-item of
   the whole hands saga — ~3 waves of partially-misdirected fix work.**

3. **Bake option-table coverage tracker.** An enumerator of the (X-anchor, Y-bone) bake cells with
   measured/unmeasured status. Wave 15 laid out a 7-cell table and found the winning cell ("keep own
   authored offsets + repoint, no rebake") had **never been measured**. A coverage matrix maintained
   from Wave 11 would have surfaced the untried cell four waves earlier instead of discovering cells
   one-at-a-time. **Cost of absence: cells tried serially across Waves 12–15.**

4. **Ground-truth Wii bone-world capture (Dolphin + milo-trace).** Diff native `own` WorldXfm against
   real Wii execution at matched clip time — the external check the hands derivation ultimately rests
   on ("own animates Wii-faithfully"). Named as Wave-15 R-A rank-3; still not built. Its absence is
   why every hands verdict through Wave 15 is internally-consistent-but-not-externally-verified.

## Wasted effort

**Direct waste this wave: none-to-small.** Wave 11 itself was efficient — 2 agents, both hit their
exits, durable instruments, honest self-refutations, clean concurrency handling (filtered
`git apply --cached` to avoid clobbering the other lane's uncommitted probes).

**Seeded/downstream waste: MODERATE.** Wave 11 propagated (did not originate) the own/bound inversion
and originated the "shell-rebake" mechanism story; together these licensed ~3 waves of
partially-misdirected fix attempts (Wave 12 SHELL_FIX cell, Wave 13 SKEL, Wave 14 RESKIN) that Wave 15
had to unwind with a confounded-death-certificate finding. The decisive point: Wave 11 **had the
matrix data in hand** to catch the inversion but chose a scalar instrument shape. On BOOTRNG the
seeded waste is smaller — one Wave-12 stage to refute the "completion-order / staged patch" framing.
All of it traces to the two missing measurement SHAPES above (matrix-relational bone comparator;
per-site RNG sequence differ), not to any error of diligence in the wave.

## Recurring-bug families touched

- **Hands/finger shard family:** Wave 11 named the axis (shell/offset) with a durable gate (Tier-2
  EXACT) but a mechanism story ("shared-magnet conjugation") later inverted (Wave 13) and refuted
  (Wave 15). State after the wave: palette+GPU exonerated (held), axis named (held directionally),
  causal mechanism WRONG. Not resolved — the fix cell that works was still 4 waves away.
- **wash / BOOTRNG:** owner named (`gRand` stream position) with render exonerated (held) and the
  `--tol` lever found (landed Wave 12), but the OWNER-fix framing ("staged part-b / completion-order")
  wrong. Still open through Wave 12 (PRIMARY-FAIL).
- **WHITE:** reframed as per-FX / swept-light PHASE rendering fidelity (not a static venue-exposure
  constant); deferred to a co-sampling instrument (Wave-12 item).
