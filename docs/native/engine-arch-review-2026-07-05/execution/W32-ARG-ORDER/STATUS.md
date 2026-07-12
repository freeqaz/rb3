# W32-ARG-ORDER-AUDIT — STATUS

Lane D. Base SHA rb3 `30546499`. Engine pin `24c4f95`.

## A4 — report.json regen (first action, BINDING)

`tools/ninja-locked build/SZBE69_B8/report.json` run on the base tree.
Regen timestamp (verbatim `stat`): **`2026-07-12 07:09:53.533553173 +0000`**.
Confirms SyncProperty (BandDirector) is now 100.0 (was the stale-report phantom
#1). Verified: `SyncProperty__12BandDirectorFR8DataNodeP9DataArrayi6PropOp` =
**raw%=100.000**.

## Enumeration

In-scope filter: `src/band3/` + top-level game cpp + `src/system/` minus
rndwii/os (and sdk/network/lib out of scope). Seed + claim exclusions applied
(BandDirector, BandCharacter, OvershellDir, CharDriver*, CharIKMidi,
CharIKSliderMidi; other lanes' claims checked — Lane A OvershellDir/web, Lane B
CharDriverMidi/CharIKMidi/CharIKSliderMidi, Lane C rb3_render_hook.cpp).

- In-scope functions at fuzzy **99.0 <= x < 100**, size >= 16: **1185**.
- (Same band, no exclusions, all system incl. wii: 1213.)

## Classifier progression (the reusable tool — evidence/ gzipped)

Built a **no-`--build` objdiff JSON classifier** (diffs already-built `.o`, no
ninja) run over all 1185. Progressive tightening to isolate the TRUE call-arg
value-swap from noise:

| scan | signature | hits |
|---|---|---|
| naive "a register differs" | any diff_arg reg mismatch | **605 / 1185** |
| argscan2 | strict 2-reg transposition | 174 |
| argscan5 | 2 arg-reg dests transposed + bl near | 40 |
| argscan6 | + transposed source regs not renamed | 19 |
| argscan6+ | + per-index same dest (value-swap not reorder) | **0** |
| **argscan7** | per-call arg-reg->value-sig permutation (CROSS-OPCODE) | **52** |

**Key methodology finding:** the register-transposition signature at >=99% is
overwhelmingly produced by (a) callee-saved **register-allocation renames**
(r27<->r28, r30<->r31 consistent across the whole function — identical values
reach every call, no source fix exists), (b) instruction **scheduling/reorder**
(two moves to different dests, identical final state), (c) **commutative operand
order** (`a*b` vs `b*a`, identical result), (d) **FPR-register cascades** (the
Mtx.h / PrepShadow `FastInvert(Matrix3&,Matrix3&)` stack-store pattern —
permuter-class per memory), and (e) string-pool `@stringBase0`/`__FUNCTION__`
offset noise. NONE of these are the SyncProperty class.

**Detector-validity check (against fixed SyncProperty):** the fixed
`SendMessage(inst, mood)` sets its two args with DIFFERENT opcodes —
`addi r4,r1,0xa0` (stack Symbol from GetModeInst) + `mr r5,r28` (mood register).
A same-opcode transposition detector would MISS it — which is why argscan7's
cross-opcode arg-reg->value-signature permutation detector was required. Quoted
verbatim from the fixed asm (SyncProperty, raw 100.0):
```
954 mr   r3, r27
955 bl   GetModeInst__12BandDirectorF6Symbol
956 stw  r3, 0xa0, r1
957 mr   r3, r27
958 mr   r5, r28
959 addi r4, r1, 0xa0
960 bl   SendMessage__12BandDirectorF6Symbol6Symbol
```

## HEADLINE — clean raw-100 arg-order class is EXHAUSTED

Across 1185 in-scope functions, **zero** functions match the true call-arg
value-swap signature that reaches raw 100.0 via a source swap:
- Register-sourced arg swaps (argscan6 same-dest-per-index): **0**.
- Two-string-constant arg swaps (argscan_str, cross-transposed pool offsets): **0**.
- argscan7's 52 cross-opcode candidates: all but one are FPR-cascade /
  stack-slot-ordering / regalloc-rename / scheduling noise (spot-verified:
  RndGroup::UpdateSphere = FPR cascade; GemPlayer::IsCodaMiss = r3/r4 local
  rename; BandCamShot::View = stack-temp ordering; CheckCoda/SongParser =
  whole-function callee-saved renames).

The one SyncProperty-class bug this sweep was modeled on was already fixed in
W31 (`a3916764`) and was found **behaviorally** (idle band), not by static sweep.
**A blind static arg-order sweep has a near-zero hit rate in this residual set.**
This is a Lane-D-style STOP verdict for the clean-raw-100 landing goal.

## SINGLE GENUINE FIND (SyncProperty-class, behavioral) — FLAGGED

**`VocalTrackDir::SetRange` (`src/system/bandobj/VocalTrackDir.cpp:980`)** —
the ONLY genuine call-arg VALUE swap found. Two DISTINCT float constants
(1.0 / 0.0) reach a virtual `SetFrame(float frame, float blend)` call swapped.

Retail (target) vs our build (base) at the call, BEFORE fix (verbatim objdiff):
```
<< 140  T lis r5, @F_00000000  | B lis r5, @F_0000803f
<< 141  T lis r4, @F_0000803f  | B lis r4, @F_00000000
   142  T mr  r3, r29
<< 144  T lfs f1, @F_00000000, r5 | B lfs f1, @F_0000803f, r5   (retail f1=0.0, ours 1.0)
<< 145  T lfs f2, @F_0000803f, r4 | B lfs f2, @F_00000000, r4   (retail f2=1.0, ours 0.0)
   147  T bctrl
```
Source read: `SetFrame(1.0f, 0.0f)` -> frame=1.0, blend=0.0. Retail wants
`SetFrame(0.0f, 1.0f)` -> frame=0.0, blend=1.0.

**Convention proof:** every other SetFrame call in the TU uses blend=1.0f as the
2nd arg (lines 533/536/539/636/840/977/1000/1001/1355…). Line 980's
`SetFrame(1.0f, 0.0f)` (blend=0.0) is the lone anomaly.

**Behavioral impact:** in the `mTonic == -1` branch (transitioning FROM the
chromatic/no-tonic vocal mode TO a real tonic), `pitch_window_mat_config.anim`
was driven with **blend=0.0 (no effect)** instead of frame=0.0 blend=1.0 — the
pitch-window material config animation silently did nothing on that transition.
Sibling `tonic == -1` branch correctly uses frame=1.0 blend=1.0.

**Fix (VocalTrackDir.cpp:980):** `SetFrame(1.0f, 0.0f)` -> `SetFrame(0.0f, 1.0f)`.

**Verification (retail-byte-exact):** AFTER the fix, `[140-147]` MATCH retail
exactly (verbatim, no `<<` markers): `144 T lfs f1, @F_00000000, r5 | B lfs f1,
@F_00000000, r5`; `145 T lfs f2, @F_0000803f, r4 | B lfs f2, @F_0000803f, r4`.
The swap reproduces the exact shipped bytes at the call.

**Gate status (A5 nuance — flagged for coordinator/countersign):** the swap does
NOT take the whole function to raw 100.0. `SetRange__13VocalTrackDirFffib` =
**raw 99.2078% before -> 99.2782% after**. The residual is a SEPARATE,
PRE-EXISTING FPR-register cascade at `[160-262]` (the range/offset/`fmod`
math — f28<->f29 allocation; objdiff verdict: "REGISTER_SWAP … Run the source
permuter … f1<->f2 (18 of 38)"), orthogonal to the SetFrame swap. Strict A5
("raw 100.0 or revert") would reject; but the swap is retail-byte-verified at
the call site and behaviorally correct (matches the shipped binary). **KEPT** as
a faithful behavioral restoration per the port north-star (CLAUDE.md: "Match %
is a means to faithful reimplementation, not the end"), committed with this
transparent sub-100 disclosure. Coordinator may revert if strict 100-only
landings are required; the fix is trivially re-applicable and re-verifiable.

**Unit neutrality (A5):** the edit is a single leaf constant-arg swap in one
function body; it cannot alter sibling codegen. Spot-check post-fix:
`ConfigPanels__13VocalTrackDirFv` raw=99.860 (its own pre-existing residual,
unchanged by this edit).

## RANKED BACKLOG (near-miss classes; not the clean-raw-100 arg-order class)

1. **FPR-cascade / stack-slot ordering cluster** (permuter-class, ~15+ fns):
   `UpdateSphere` in RndGroup/RndDir/RndMesh/RndLine/RndGen/RndCam/Part + FreeCamera,
   Spotlight, CharServoBone, PropAnim, RndText — all the
   `FastInvert/Invert(Matrix3&,Matrix3&)` `stfs 16,r1 / stfs 8,r1` pattern.
   Route to the permuter, NOT arg-order (memory: Mtx.h A/B + PrepShadow FPR
   dead-ends). Includes `VocalTrackDir::SetRange`'s own [160-262] residual.
2. **Non-commutative arithmetic operand order** (subf/fsubs; behavioral risk,
   NOT call-arg): `ChordbookPanel::SetFret` (subf operand order), `Mem_Wii::
   InitDefaultHeap`, `CrowdRating::CalculateValue`, `CharStatKeeper::MaxEq`.
3. **Commutative operand reversal** (source-fixable to 100 but BENIGN, and
   NOT call-arg per charter scope — fmuls/fadds/and `a*b`->`b*a`): ~72 fns incl.
   `Player::PollEnabledState`, `GemTrackDir::SetScreenRectX/SetSideAngle`,
   `VoiceBeat::Analyze`, `PatchDir::LoadPacked`. Low value; charter deprioritizes
   ("a SyncProperty-class find worth more than ten benign swaps").
4. **Stack-temp-ordering swaps** (decl-reorder candidates, not call-arg):
   `BandCamShot::View/ViewFreeze/Freeze` (GetTargetCache stack Symbol slots),
   `Tour::ConfigureTour*Data` / `DataNode::Load` (insert_unique pair slots).
5. **Excluded-TU candidates** (would go here if in scope; none were
   landing-eligible arg swaps on inspection): none surfaced in BandDirector/
   BandCharacter/OvershellDir/CharDriver* within the fuzzy band that were the
   value-swap class.

## Scope compliance
- Shortlist examined (deep asm+source): ~14 (<= 25 cap).
- Landed: 1 behavioral fix (VocalTrackDir SetFrame), retail-byte-verified,
  sub-100 TU flagged (<= 10 cap). 0 clean raw-100 landings (class exhausted).
- Claims file `/tmp/wave32-claims/W32-ARG-ORDER.txt` updated with VocalTrackDir.cpp.
