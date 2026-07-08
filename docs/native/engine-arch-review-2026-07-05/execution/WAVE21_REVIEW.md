# WAVE21_REVIEW — Fable pre-dispatch review (hands flagship fix)

**Reviewer:** Fable. **Date:** 2026-07-08. **Target:** `WAVE21_KICKOFF.md` (rb3 `c4ed750e`).
All code anchors re-derived from `src/system/bandobj/BandCharacter.cpp` at HEAD, the Wave-20
committed evidence, and the char_shared.milo asset itself — not trusted from the kickoff.

**Verdict: DISPATCH-WITH-AMENDMENTS (A1–A10 binding).** The wave is dispatchable and Part 1
is NOT structurally blocked — but the kickoff's stated "central hope" (R-B, a
differently-resolved instance) is CONTRADICTED by Wave-20's own committed measurement and
must be reframed before dispatch (A1), and the Part-1 arm is uninterpretable without an
explicit RebindHeadHandsAtRest/clamp-regime charter (A2).

## Q1 — The central hope (R-B): the instance distinction is REFUTED; a real distinction survives

The kickoff claims the merge-time `sBoneMergeDir` remap "may land on a correctly-posed
instance where FULL-rebind does not" (KICKOFF:37-42). Re-derived:

- The remap (`Filter`, :4337-4359) does `FindObject(o1->Name(), false)` → `ReplaceRefs` at
  merge time. FULL-rebind (`RebindOutfitBonesToOwnSkeleton`, :1261-1267) does
  `Find(bound->Name())` → `SetBone(b, own, false)` at Poll (sCalc defaults 0 — kickoff
  fact 1 verified).
- **Both resolve BY NAME in the same character dir, and Wave-20 measured the convergence
  directly**: under shim-off (= the faithful merge remap firing), rebind-entry
  `distinctFromOwnFind` = **205× False — bound == own**
  (`W20-NATIVETRACE/STATUS.md:47-48,54`). The instance the merge remap lands on IS the
  instance Poll-time `Find()` returns. The "differently-resolved instance" hope is dead on
  committed data; as written, the wave's make-or-break premise is FALSE.

**What genuinely differs** (and keeps the wave from being moot):
1. **Rebake/clamp regime.** Under Part 1, hand meshes enter `RebindHeadHandsAtRest`
   (pre-`Character::Poll`, call :636) with own==bound → the default path MISSES with
   `missWhy="boundRebakeOff"` (:1831-1852; `sNoBoundRebake` defaults ON :1600-1602) → the
   mesh is never flagged `mNativeBonesRebound` → **no seed-R rebake runs, authored offsets
   survive, and the engine fling-clamp/V24 guard stays ACTIVE**. That is a different draw
   regime from both dead cell #1 (seed-R rebake) and the 8th cell (repoint, clamp-exempt) —
   and from FULL-rebind. This, not instance identity, is the honest residual hope.
2. **Ref-graph scope**: `ReplaceRefs` rewrites every reference (TransParent hierarchy,
   drivers, weight-setters) at load; FULL-rebind touches only mesh bone slots.

Not moot, but the FIX brief must be corrected (A1) and the composition instrumented (A2) —
E4's gap (draw-time offsets never dumped) otherwise recurs verbatim.

## Q2 — Part-1 feasibility (R-A): feasible; NOT structurally blocked; two named hazards

- **Distinguishable at the shim site: YES.** The override keys on
  `o1->mStoredFile` (:4433-4441); the committed control log prints
  `storedFile='char/char/main/shared/char_shared.milo'` plainly
  (`W20-NATIVETRACE/evidence/loadbind_control_shimON.log:9`). Match by stored-file suffix
  (or pointer-compare vs `sCharSharedDir`, set from `feet_skin.mat`'s dir at :4516-4519).
- **Does char_shared.milo carry palette content? YES — verified from the asset**
  (`orig-assets/wii-extracted/char/main/shared/gen/char_shared.milo_wii`): it contains the
  skin materials + dummy textures (`feet_skin.mat`, `torso_naked.mat`, `dummy_torso.tex` —
  the exact texture in the white-fallback cascade, :4397-4398) AND two subdir refs:
  `colorpalettes.milo` and `../skeleton.milo`.
- **Why kMerge on it is still viable**: (i) `colorpalettes.milo` is its own stored file —
  it gets its own FilterSubdir decision and KEEPS kReplace under the scoped shim, so the
  main drain path stays fixed; (ii) char_shared's own objects hit the `sCharSharedDir`
  branch in `Filter` (:4311-4319) → `ReplaceRefs` + **kIgnore** — they are ref-swapped,
  never MOVED, so the drain mechanism doesn't apply to them on the retail path.
- **Hazards to gate** (A3): (i) that branch carries `MILO_ASSERT(mine->Dir() == this,
  0xAB8)` (:4316) — `Find(name, true)` is fail-fast; if a member lacks its own copy of a
  char_shared object, flag-ON CRASHES; (ii) the retail branches compare against file-scope
  STATICS reassigned per `OnInstallFilter` — native's interleaved loader (the very thing
  the shim was written for, :4393-4396) can race installs, so texture integrity must be
  checked on ALL members. E11's texture gate is in the kickoff — good — but the crash gate
  is missing.
- **Stale in-tree record**: the shim's NOTE (:4406-4428) still claims "full shim-off …
  same shared root" — superseded by Wave-20 (errata E10) — and its 2026-06-06 "scoping"
  dead-end was the OPPOSITE scoping (palettes-only shim; char_shared stayed kReplace), so
  it does NOT kill this wave's arm. Lane FIX must update that comment (A4) or the next
  reader re-inherits the dead record.

## Q3 — Part-2 banned-cell proximity: genuinely distinct, IF the boundary is pinned

The 8 dead cells all vary `(X anchor, Y bone)` of the OFFSET bake `off = meshWorld·inv(X)`
(VERDICT §3 table); the 8th cell = authored-off + own repoint (:1682-1723, pass-A). Part 2
writes a THIRD variable — the per-member skeleton's rest `L_own(rest)` — touching neither
offsets nor verts/weights. That is outside the table and not the reskin. But it slides into
a dead cell the moment the lane "helps" by rebaking offsets to the new pose. Discriminator
(A5): **BoneOffsetAt/verts/weights byte-identical before/after Part 2** (dump them), and the
authored-provenance signature must move for the RIGHT reason (off·own_rest 87.2°-class → ≈0
because own_rest moved, off unchanged). Two spec gaps the brief must close: (i) animation
SETS absolute local poses (VERDICT §1: `RotateAbout*` SETS, `PoseMeshes`) — a rest write is
overwritten by any driven channel, so Part 2 must state WHEN it applies and which windows it
can affect; (ii) per-asset authored binds differ (gloves 60–69°, nails ~170° — VERDICT §4),
so "pose to B" must define B operationally with a per-asset Tier-1 ≈0 post-condition, else
one rest pose cannot serve all appendage meshes.

## Q4 — Lane DISCRIM soundness: measurable; verdict logic needs one correction

- Draw-frame own-vs-B IS reachable with existing instruments: the Wave-18 engine-emitted
  Tier-1 field (engine `e69a35f`, palette-build side = draw-adjacent) — the arm-S/Lane-N
  timing gap the kickoff names is real (arm S read freshness captures at Poll cadence;
  LOADBIND reads rebind entry). Inter-bone machinery exists natively
  (`d4-bonedump-sweep.py`, `interbone_framematch.py`). No new fundamental hook needed;
  small extensions at most. R2's per-mesh palette dumps cover the E4 offset-composition
  dump (A2).
- **Verdict-logic gap**: "males divergent inter-bone at draw ⇒ L2-b" is unsound as
  written — fingers legitimately articulate, so raw inter-bone divergence is not a tear
  proof. Operationalize L2-b as **per-bone basis coherent at draw (Tier-1 ≈0) AND visual
  tear present at matched frames**, with the two-adjacent-bone relative-pose metric used
  comparatively vs the matched flag-OFF arm (A7). The torso/hand fail-red in the kickoff is
  right and stays. The arm-S reconciliation framing (rest≈B yet tears animated → L2-b) is
  correct — that IS the 8th cell's empirical shape (HANDS-FIX: Tier-1 3.1° both genders,
  visual torn).

## Q5 — The male-coherent framing (R-D): right mechanism decider, partially a ship red herring

Shipped male state = seed-R ceiling-hand **masked by mitten default-ON**: fingers degrade
toward rigid at extreme poses, hand attached (CLOSURE §Accepted-residual + POST-FLIP
addendum). So males are already borderline-acceptable SHIPPED; male coherence is
load-bearing for the MECHANISM verdict (L2-a vs L2-b) but NOT for shipping. The kickoff's
binary (completable-now vs walled) misses the middle outcome: **females fixed, males
retained on shipped default** — a legitimate per-gender-scoped partial ship. Bless the
framing with that third outcome added (A8).

## Q6 — Gate completeness

Present and correct: G-FIX-E1 (both genders), E11 texture-integrity, batch_objdiff,
drawlog-792, guard-DROP, crowd oracle, remap-fire counts (lint 8). Missing (binding):
- **Mitten-state control** (A8): both Wave-20 arms were mitten-ON (E8) — the mitten MASKS
  finger-level tear/coherence. E1 evaluation needs mitten-OFF pairs plus a mitten-ON pair
  for the shipped comparison, else "spike-fan GONE" can be mitten-mediated.
- **Crash gate** (A3): no `MILO_ASSERT` 0xAB8 / full boot-to-gameplay, all 4 members,
  flag-ON.
- **Draw-time composition dump** (A2): name which cell/regime the flag-ON state occupies.
- **wext** is nowhere cited as a gate — kickoff avoids the VERDICT §6.4 trap; make the ban
  explicit in both briefs anyway (A9).
- Female-specific numeric gate: covered by E1 "both genders" + HANDS-FIX A6 precedent;
  require the female Tier-1 table be reported gender-split (already lint 2).

## Q7 — Honest-wall discipline: adequate

The Torn branch is pre-authorized in the decisive-question section ("cannot ship alone …
blocked on the R5 wall or the banned reskin. A sharp characterization, not a failure"),
DISCRIM carries an explicit do-not-manufacture-L2-a clause naming E5/E6, and lint 10
restates it. Accept-no-ship is correctly framed. One tightening: Part 2's dispatch trigger
("if Part 1's male result still shards OR the female still flings") can fire Part 2 in an
L2-b world where it is the wrong shape (E12) — condition Part 2 on DISCRIM's reading (A6).

## Q8 — Lint / banned citations / scope

- No §8-banned citation in the kickoff: reskin appears only as banned; the 6th-cell death
  cert is not cited; the HANDS-FIX "Dolphin fallback" § is cited for its INSTRUMENT — do
  not inherit that paragraph's closing "will confirm the reskin conclusion" framing (A10).
- Kickoff anchors verified: :4433 shim ✓, :1211/:1239 torso rebind + sTorsoOnly ✓,
  "shards … for study only" :1209/:1236-1238 ✓, 8th cell :1465-1723 ✓,
  RebindHeadHandsAtRest :1370 pre-Poll (:632-636) ✓.
- Scope: both lanes rb3-side ✓; Part 2 is implementable in BandCharacter.cpp (bone local
  writes) — any drift into engine TUs is out of charter (engine writers NONE). No default
  flips ✓. Hazard-note files respected.

## AMENDMENTS (binding)

- **A1** Reframe the central hope: strike "differently-resolved instance"
  (KICKOFF:37-42). Committed Wave-20 data (`distinctFromOwnFind` 205×False) proves the
  merge remap lands on the SAME instance Poll-`Find()` returns. The real distinctions:
  rebake/clamp regime + whole-graph ReplaceRefs. Lane FIX pointer-verifies its landing
  instance vs Poll-Find (lint 1).
- **A2** Lane FIX must charter the `RebindHeadHandsAtRest` interaction under the fix flag:
  own==bound hand meshes default to `boundRebakeOff` miss → unflagged → clamp/V24 guard
  active with authored offsets. Specify the intended regime (unflagged-on-clamp vs
  flagged-authored vs rebake) and DUMP the draw-time composition (offsets + flags per
  slot, R2 palette-dump machinery) — this discharges E4 for the new arm.
- **A3** Add gates: zero `MILO_ASSERT` 0xAB8 crashes, full boot-to-gameplay all 4 members
  flag-ON; texture-integrity across ALL members (char_shared itself carries
  `dummy_*.tex`/`*_skin.mat` — strings-verified).
- **A4** Same commit: update the stale :4406-4428 shim NOTE per errata E10, and state that
  its 2026-06-06 "scoped shim" dead-end was the opposite scoping (char_shared stayed
  kReplace) — not this wave's arm.
- **A5** Part-2 discriminator: BoneOffsetAt/verts/weights byte-identical before/after
  (dumped); pose writes only bone local/rest transforms; provenance signature moves via
  own_rest, not off. Spec must address absolute-SET clip semantics (when does the pose
  survive?) and define B with a per-asset (hands/gloves/nails) Tier-1 ≈0 post-condition.
- **A6** Part 2 dispatches only on a DISCRIM reading consistent with L2-a for its target
  population (E12) — not on Part-1 visuals alone.
- **A7** DISCRIM L2-b call = per-bone Tier-1 coherent at draw (engine `e69a35f` field) AND
  visual tear at matched frames; inter-bone metric is comparative vs matched flag-OFF, not
  a standalone threshold. Per-member gender/visual attribution required (crop or
  single-member lineup).
- **A8** E1 arms run mitten-OFF pairs (`RB3_HANDS_MITTEN_OFF`) alongside mitten-ON; add the
  third pre-authorized outcome: female-only fix ships per-gender-scoped, males stay on the
  shipped default (coordinator decision).
- **A9** State explicitly in both briefs: wext is descriptive only, never a gate
  (VERDICT §6.4).
- **A10** Do not inherit HANDS-FIX §Dolphin-fallback's "will confirm the reskin
  conclusion" framing; the citation covers the instrument spec only.

**FINAL: DISPATCH-WITH-AMENDMENTS.** Part 1 is feasible (not structurally blocked; E11
risk is real but gated and mechanically mitigated by the :4311 kIgnore branch + separate
colorpalettes stored file). The wave's written hope needed surgery, not the wave itself:
the decisive gender-split, draw-frame, mitten-controlled measurement has never been made,
and either branch outcome — fix or named wall — is a real conversion of the flagship.
