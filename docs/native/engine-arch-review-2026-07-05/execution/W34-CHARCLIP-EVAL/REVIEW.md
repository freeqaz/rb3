# W34-CHARCLIP-EVAL — pre-dispatch adversarial review

**Reviewer:** Fable subagent, 2026-08-01. Grounded in tree state at HEAD `d411aefa`.
Verdicts: CONFIRMED / REFUTED / CORRECTED. The coordinator adopts amendments into
KICKOFF.md; this file does not modify it.

**Bottom line:** the lane is dispatchable. The core theory (dynamic clip-eval
invariant violation, localizable by per-layer taps without Wii ground truth)
SURVIVES adversarial attack — the strongest attack (clip SELECTION vs clip EVAL)
fails because vignette-clip selection is retail decomp code and the 4.2× stretch
under an authored clip is a genuine invariant violation wherever it plays. But
three premises need correction before dispatch: the detonation PHASE (A3), the
"never audited" claim (A5), and the L4-tap/mitten fence collision (A4c).

---

## A1 — BAND_ANIM_ANAT probe: CONFIRMED (line range + env vars corrected)

The probe exists in `src/system/bandobj/BandCharacter.cpp`, inside `Poll()`,
`#ifdef HX_NATIVE`. Actual extent: **:711-919** (kickoff said 711-918; off by
one, immaterial). Structure:

- `:711-733` — `BAND_ANIM_PROBE` member-select + probed-bone pre-capture.
- `:747-800` — `evt=HI` event-triggered world-pos dump (fling detector).
- `:802-919` — `evt=ANAT` anatomical stretch probe. It does compute exactly what
  the kickoff claims (`:851-859`):

```cpp
float authoredLen = Length(cB->LocalXfm().v);
...
float liveDist = Length(d);            // |childWorld - parentWorld|
float ratio = (authoredLen < 0.01f) ? -1.0f : (liveDist / authoredLen);
```

**Env-var correction:** `BAND_ANIM_ANAT=1` alone does nothing. The gate is
`if (banim && getenv("BAND_ANIM_ANAT"))` (`:816`), where `banim` requires
`BAND_ANIM_PROBE=<member-substr or *>` (`:717-725`). Full set:
`BAND_ANIM_PROBE` (required, member select), `BAND_ANIM_ANAT=1` (required),
`BAND_ANIM_BONE` (HI-probe bone, default `bone_R-upperArm.mesh`),
`BAND_ANIM_YTHRESH` (default 50), `BAND_ANIM_CHAIN` (one-shot parentage dump,
`inParentKids`), `BAND_ANIM_CHAIN_HZ` (periodic chain dump).

**Coverage gap:** the ANAT chain (`:826-836`) is 9 pairs — pelvis→spine1→2→3 and
both arm chains (clavicle→upperArm→foreArm→hand). **No legs, no fingers, no
face.** The kickoff's own evidence list includes a "floating horizontal leg"
(gameplay_000) and face defects — neither is measurable by the probe as-is. If
legs are to be attributed/gated, the agent must extend the chain array (trivial,
in-fence).

## A2 — W33-V1-POSE evidence: CONFIRMED (with one framing note)

`W33-V1-POSE/STATUS.md` + `PLAN.md` say exactly what the kickoff claims:

- vignette clips `player2_f/player1_f/player3_m/player3_f` stretch upperArm to
  **4.202×** authored length (raw: `run5-anat_setplayON.engine.log.gz`,
  `ratio=4.202 authoredLen=3.4516 liveDist=14.5053`, member=player1,
  clip=player2_f, `clipType='vignette'`), world-Y flung to ~182-194;
- set_play REFUTED as cause: CA1 worktree re-swap, performance-clip dispatch
  ON=64 / OFF=0, V1 identical (4.2 == 4.2, 194.1 == 194.1) — STATUS.md:44;
- parentage intact: CHAIN dump `inParentKids=1` on every bone — STATUS.md:45.

Framing note: W33 STEP-0(iii) did NOT leave the layer unnamed — it **positively
named the seed-R rotation-basis class** (STATUS.md:46) and invoked the
FAMILY-STOP reopen clause (PLAN.md §FAMILY-STOP, justification documented). The
kickoff's reframe ("the un-audited dynamic clip-eval path") is a *refinement*
(the basis error, if real, must live somewhere in L1-L4), not a contradiction —
but the work agent should know the prior wave's verdict was "same closed family,
new surface," so a STEP-0 result of "L3 composition wrong in the family's
signature way" is a live possibility, and the FAMILY-STOP documentation burden
then applies.

## A3 — CORRECTED (most important): the detonation is NOT during gameplay; it is the shell/loading-vignette phase, and the gameplay-visible wreckage includes FROZEN remnants

The kickoff says "gameplay *vignette* body clips" detonate. The raw W33 evidence
says otherwise:

- The detonating clips load from **`world/vignette/shell/sv3/a/streetslomo/
  streetslomo_clips.milo`** and **`world/vignette/shell/sv4/d/subwayhangout/
  subwayhangout_clips.milo`** — shell vignette milos (loading vignettes), not
  venue/gameplay anim sets.
- ALL 2682 `clipType='vignette'` ANAT detonations sit at engine frames
  **efr 0-399** (~0-6.7 s of the venue clock — the pre-walkon loading-vignette
  window). Distribution: 1008/570/638/474 events in efr buckets 0/100/200/300;
  then NOTHING vignette-typed later.
- During actual gameplay (efr ≥ 600) the only ANAT events are
  `clipType='guitar_body'` at **ratio 1.500-1.515** — borderline threshold
  noise, two orders of magnitude milder.
- The gameplay-time world-Y +194 the kickoff cites is a **frozen remnant**, not
  live vignette playback: `evt=HI frame=7963 member='player3' grp='sit'
  clipType='drum_body' FirstPlaying=(nil) clip='(none)' ...
  pre=(10.9065,194.0830,...) post=(10.9065,194.0830,...) moved=0.000005` — the
  bone parked at the detonated vignette pose with NO clip playing. That is the
  known stale-vignette-freeze mechanism (`BandCharacter.cpp:603-632` — the
  native walk-on snap exists but demonstrably did not re-drive this bone).
- The fling is born on the FIRST vignette Poll: `frame=286 pre=(4.83,-1.55,58.4)
  post=(7.86,182.49,53.69) moved=184.14` under `clip='player1_f'`.

**Why the theory still survives (the selection-vs-eval attack fails):** band
chars playing shell-vignette clips during the vignette phase is *retail*
behavior — the vignette mask path is retail decomp code
(`BandCharacter::PlayMainClip`, `BandCharacter.cpp:467-494`: `InVignetteOrCloset`
→ mask 0x20 male / 0x40 female, female-fallback notify). Clip DATA is authored.
So this is not a native mis-trigger of clip SELECTION; the 4.2× stretch of a
rigid bone under an authored clip is an invariant violation in the EVAL/compose
chain regardless of phase. The lane's target layer is right.

**Consequences to adopt:**

1. **STEP-0 is cheaper than chartered:** the detonating vignette frame occurs in
   the first ~2-7 s after venue load — no need to reach gameplay for the tap;
   every boot gives ~2700 detonation events in the vignette window.
2. **Acceptance #2/#3 must cover the frozen-remnant path too.** Fixing eval so
   maxRatio ≤1.5 during the vignette will *also* fix the frozen gameplay pose
   only because the frozen pose is a snapshot of the detonated one. The A/B must
   check gameplay-time frames for BOTH: no live detonation AND no frozen
   `clip='(none)'` bone parked at a detonated position. If a residual frozen
   pose remains after the eval fix, that is the `:603-632` walk-on-snap gap — a
   SEPARATE (in-fence, BandCharacter.cpp) defect; attribute it as such, don't
   fold it into the eval fix.
3. The pre-existing gameplay `guitar_body` 1.50-1.52 flutter is at-threshold
   noise; do not chase it to satisfy "maxRatio ≤1.5" — gate on the vignette
   clips as the kickoff says, and report the performance-clip max separately.

## A4 — code-path map: CONFIRMED as distinct code sites; concrete anchors provided (one fence collision at L4)

The L1-L4 layers are real, distinct sites. RB3-side TUs (all
`src/system/char/`):

**L1 — raw key data + decode.**
- Storage/load: `CharBonesSamples::LoadData` (`CharBonesSamples.cpp:554`) — has
  a LARGE `HX_NATIVE` branch (`:558-650`): padded cached Xbox/PS3 read
  (per-element BinStream reads byte-swap on LE host; pad float consumed per
  uncompressed Vector3; per-sample 16-byte round-up). **Already audited** (V38
  CBS_DBG; stride-desync hypothesis REFUTED — comment block `:574-584`).
  `ReadCounts` (`:466`) computes the channel offsets that steer decode —
  **61.93% match**, see A6.
- Decode at timestamp: `CharBonesSamples::EvaluateChannel`
  (`CharBonesSamples.cpp:262`) — quantized decode: rotX `short * 1/1638.4`,
  quats `signed char * 1/127` or `short * 1/32767`, pos `short * 1/32767 *
  1300.0`, with lerp branch for frac≠0. Plain C++, no HX_NATIVE inside the
  decode itself. **90.76% match — the single most suspicious function on the
  path** (A6). Sample indexing: `CharClip::BeatToSample` (`CharClip.cpp:456`),
  `CharBonesSamples::FracToSample` (`:27`).

**L2 — clip evaluation → bone weight buffer.**
- Drive: `CharDriver::Poll` (`CharDriver.cpp`, 93.54%) → `CharClipDriver::
  PreEvaluate/Evaluate` (`CharClipDriver.cpp:226/:320`) → `CharClipDriver::
  ScaleAdd/RotateTo` (`:128/:142`) → `CharBones::ScaleAdd(CharClip*,...)`
  (`CharBones.cpp:1331`) → `CharBonesSamples::ScaleAddSample`
  (`CharBonesSamples.cpp:109`) + `CharClip::FacingSet::ScaleAddSample`
  (`CharClip.cpp:385`).
- Weight-buffer math (the quat/pos blend kernels): `CharBones::ScaleAdd`
  (`CharBones.cpp:361`, 92.37%), `Blend` (`:612`, 97.31%), `RotateBy` (`:735`,
  91.61%), `RotateTo` (`:967`, 95.59%), `ScaleAddIdentity` (`:1230`, 97.07%).
  No inline asm / paired-single intrinsics anywhere in the RB3-side chain
  (grep: none) — these are C++ decomp of what on Gekko were paired-single-heavy
  kernels, which is exactly why their sub-100% residuals deserve the Bank-8
  cross-check the kickoff prescribes.

**L3 — weights → bone LocalXfm → world.**
- `CharServoBone::Poll` (`CharServoBone.cpp:41`, 99.75%) → `CharBonesMeshes::
  PoseMeshes` (`CharBonesMeshes.cpp:98`, 99.51%): pos → `SetLocalPos`; quat →
  `Normalize` + `MakeRotMatrix`; rotX/Y/Z → `RotateAboutX/Y/Z`; scale →
  row-scale divide. Then facing/delta compose in `CharServoBone::Poll` itself
  (`MoveToFacing` 96.13%, `MoveToDeltaFacing` 95.99%, `Regulate` 90.58%).
- World compose is LAZY: `RndTransformable::WorldXfm_Force`
  (`src/system/rndobj/Trans.cpp:127`) up the `TransParent` chain. A "world wrong,
  locals right" L3 verdict points here / at dirty-propagation, not at
  PoseMeshes.

**L4 — skinning palette.** NOT in rb3: engine repo
`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` skinned palette compose
(`A_i * O_i(t)`), the same site where `RB3_HANDS_MITTEN` lives
(GameRenderHook.h:236; NativeCompatFlags classification entry).

**(A4c) Fence collision:** the kickoff bans "Rnd_Wgpu_RB3.cpp mitten/clamp
code" while STEP-0 L4 requires reading the palette entry — which is composed in
exactly that file. Amendment: the fence should read *"no behavioral edits to
mitten/clamp logic; a read-only, env-gated palette-entry probe adjacent to the
compose is permitted"* — otherwise L4 is untappable. Also note the fence path
should name the ENGINE repo explicitly (the file is not in rb3), and the engine
commit-first / no-pin-bump rule already covers it.

**Existing probe inventory (reuse, don't re-derive):** `SERVO_PROBE`
(CharServoBone.cpp:45 — L3 local delta), `CBM_DBG`/`CBM_DBG2`
(CharBonesMeshes.cpp:117/143/172 — L3 quat magnitude + matrix det per bone),
`RB3_NO_POSEMESHES` kill-switch (CharBonesMeshes.cpp:103), `CBS_DBG`
(CharBonesSamples LoadData layout), `CHARDRV_PROBE` (CharDriver.cpp:183ff),
plus the BAND_ANIM_* family (A1). Missing for STEP-0: an L1/L2 VALUE tap
(decoded floats at the eval timestamp for a named bone) — that is the genuinely
new instrumentation.

## A5 — "the un-audited layer" claim: CORRECTED (overstated, but the narrow claim holds)

The dynamic path is NOT virgin territory:

- V38 audited the L1 storage layout (`CBS_DBG`, padded-read stride hypothesis
  **refuted**, comment at CharBonesSamples.cpp:574-584) and put quat-magnitude
  and det probes into L3 PoseMeshes (`CBM_DBG2`).
- wave-07 tapped L3 input (`SERVO_PROBE`) and the driver chain
  (`CHARDRV_PROBE`).

What is true and remains the lane's justification: **no one has ever traced a
detonating vignette frame's VALUES through L1→L4** (decoded key floats → clip
eval output → composed world → palette) for one bone. The kickoff's literal
sentence ("Nobody has ever put per-layer taps on a detonating vignette frame")
is CONFIRMED; the broader "un-audited layer" framing should be softened so the
agent doesn't waste budget re-deriving the V38 refutations (esp. do NOT re-open
the padded-read/stride hypothesis — it is closed with in-code documentation).

## A6 — report.json suspects: CONFIRMED direction; here are the concrete names the kickoff omitted

`build/SZBE69_B8/report.json` (dated **2026-07-13** — 19 days stale; the 3
decomp commits since touch UI/meta only, none in char/. Agent should regenerate
via `tools/ninja-locked build/SZBE69_B8/report.json` or use batch_objdiff for
current numbers). Sub-100% functions on the eval path, worst-first:

| Function | Match % | Size | Layer |
|---|---|---|---|
| `ReadCounts__16CharBonesSamplesFR9BinStreami` | 61.93 | 452 | L1 (offsets that steer decode) |
| `FrameToBeat__8CharClipCFf` | 83.06 | 320 | L2 timing |
| `Relativize__16CharBonesSamplesFP8CharClip` | 84.58 | 4188 | L1 (edit-path; likely inert at runtime) |
| `EvaluateChannel__16CharBonesSamplesFPviif` | **90.76** | 1884 | **L1 decode — prime suspect** |
| `Regulate__13CharServoBoneFv` | 90.58 | 240 | L3 |
| `RotateBy__9CharBonesCFR9CharBones` | 91.61 | 1976 | L2 kernel |
| `ScaleAdd__9CharBonesCFR9CharBonesf` | 92.37 | 2280 | L2 kernel |
| `Poll__10CharDriverFv` | 93.54 | 1556 | L2 drive |
| `Poll__9CharacterFv` | 93.96 | 364 | L2/L3 orchestration |
| `RotateTo__9CharBonesCFR9CharBonesf` | 95.59 | 2192 | L2 kernel |
| `MoveToDeltaFacing/MoveToFacing__13CharServoBone...` | 95.99/96.13 | 568/620 | L3 |
| `BeatToFrame__8CharClipCFf` | 96.80 | 300 | L2 timing |
| `Blend__9CharBonesCFR9CharBones` | 97.31 | 1088 | L2 kernel |
| `PreEvaluate__14CharClipDriverFfff` | 97.52 | 1184 | L2 |
| `FracToSample__16CharBonesSamplesCFPf` | 98.51 | 752 | L1 |
| `ScaleAddSample__Q28CharClip9FacingSet...` | 98.45 | 476 | L2 |
| `PoseMeshes__15CharBonesMeshesFv` | 99.51 | 1380 | L3 |
| `Poll__13CharServoBoneFv` | 99.75 | 1832 | L3 |
| `Evaluate__14CharClipDriverFfff` | 99.80 | 1376 | L2 |

Clean TUs (0 below 100): CharBone, CharBoneDir, CharBoneOffset,
CharBonesBlender. Per the W31 SyncProperty lesson the 99.x% entries stay on the
table until STEP-0 clears their layer.

## A7 — banned classes and fences: CONFIRMED (two path corrections)

- `execution/R5-HANDS-ENDGAME/CLOSURE.md` EXISTS; records GT-D closure, the
  dead-cell flags (`RB3_HANDS_AUTHORED_REPOINT`, `RB3_HANDS_RESKIN`), the seed-R
  87.2° rebake mechanism, and the pre-registered reopen falsifier. The closure's
  founding premise ("Bank-8 substrate never animates band CharBones, 0/992")
  is indeed the premise W33 declared void — kickoff's reopen story checks out.
- Mitten code: **engine repo**, `../milo-native-engine/src/platform/
  Rnd_Wgpu_RB3.cpp` (+ `GameRenderHook.h:236`), NOT an rb3 path — kickoff fence
  should say so (and see A4c for the L4-tap carve-out).
- No fence collisions at HEAD: `git status` shows NO modified files under
  `src/system/char/` or `src/system/bandobj/` (only untracked scripts/ and
  docs/ from other sessions). The dirty `BandCharDesc.cpp` /
  `rb3_platform_native.cpp` that W33 noted are no longer dirty.
- `native/build-agent-W34` does not exist yet (agent creates it);
  `native/build-native` exists to copy config from. Engine pin `2ea8e34`
  unchanged since W33.

## A8 — gates: CONFIRMED runnable (one re-baseline caveat, one env caveat)

- `scripts/native/drawlog-golden.py` supports `--fixed-clock` and
  `--canonical-order` (script header, W0.3b.S3 / W0.3c.S3). 792 PASS last
  verified in Wave 33 (`W33-RESULTS-SCREEN/STATUS.md`: "--fixed-clock
  --canonical-order, PASS 792"; also W31-EXIT-TRAP). Commits since W33 base
  (`6186706e..HEAD`) are 3 UI/meta decomp matches + macOS cmake + the
  results-screen sentinel — none render-path; expected still-792, but the agent
  should run it ONCE at HEAD before touching anything to own the baseline.
- rb3-tests **123 ran / 116 pass / 7 skip / 0 fail** is the current baseline,
  independently stated by W30, W31, and W33 STATUS docs. CONFIRMED.
- `boot-to-song.py`: default = first song via 1 down (deterministic list
  order), guitar/expert; logs `songMs` per shot (`:114`, `:180`) → the
  songMs±150ms matched A/B in acceptance #3 is supported by the harness as-is.
- Env caveat: W33's environment rendered gameplay 3D BLACK (V4 blocker,
  W33-V1-POSE/PLAN.md). The W34 evidence PNGs (`gameplay_000/003/005.png`)
  show live renders at HEAD, so the coordinator's environment renders; if the
  work agent lands in a V4-black environment anyway, the charter's
  render-independent bone-anatomy A/B (per A3) is the fallback — same
  substitution W33 used.

---

## Blocking issues

None hard-blocking. Two amendments need adoption BEFORE dispatch to avoid a
mis-aimed lane:

1. **A3** — detonation phase + frozen-remnant split (changes where STEP-0 taps
   and what acceptance #2/#3 must show).
2. **A4c/A7** — L4 palette tap vs mitten fence collision (engine-repo path +
   read-only-probe carve-out), else STEP-0's L4 row is unexecutable in-fence.

Adopt A5's softening and A6's named-suspect table as working notes (cost/aim,
not correctness).
