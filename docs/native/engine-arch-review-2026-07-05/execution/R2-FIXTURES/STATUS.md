# R2-FIXTURES — Skinning oracle + oracle-validation harness — STATUS

**Lane:** Wave-17 Lane S (KEY=R2-FIXTURES). **Plan:** RETROSPECTIVE/plans/PLAN-R2-skinning-fixtures.md.
**Engine probe:** `04651e1` (RB3_PALETTE_DUMP), landed on engine main; engine main now `753ed20`.
**rb3-tests built against engine main 753ed20** (the pin `51640ff` mismatch is a WARNING, not fatal;
the current rb3 tree needs Lane U's `RB3DrawProv`, so engine main is the coherent build — build-r2's
pinned worktree no longer compiles the current rb3 tree). No pin bump, no engine edit, no default flip.

## Verdict: M1 GO (M_BlendSpread) + M2 exit GREEN

### M1 — does a palette-level metric see the tear? **GO.**
Confirmed-provenance A/B recapture (2026-07-07), identical protocol, hands_naked M+F, the ONLY
difference being the arm env:

| arm | env | Tier-1 @bind (male/female) | M_BlendSpread @gameplay |
|---|---|---|---|
| default (=bad-ceiling) | `{}` | **87.3 / 42.6** (Wave-15 ceiling modes reproduced) | 8–21 |
| repoint (=bad-torn) | `RB3_HANDS_AUTHORED_REPOINT=1` | **3.1 / 3.1** (reads GREEN — the Wave-16 blindness) | **20–33** |
| good-body (body meshes) | `{}` | 0.4 (bind) | **0.58–2.96** (coherent even under articulation) |

**Key result:** `M_BlendSpread` VALID-separates good-body `[0.58..2.96]` from bad-torn `[20.6..33.2]`
— margin **6.95x**, zero overlap — on REAL confirmed-repoint torn-hands data. The metric is not
merely reading articulation: the coherent body mesh articulates (nb=20-27 bones) yet stays <3,
while the torn hands blow out to 33. Suite-C's `BlendSpreadDiscriminationIsInTheBlend` proves the
same in vitro (force single-bone weights → spread ~0).

**Reframing worth recording:** neither hands config is "good" — RB3 native hands are broken in BOTH
the default (ceiling) and repoint (torn) arms; the known-GOOD reference is the body meshes. The
Wave-15/16 story is fully reproduced on real data: Tier-1 SEES the ceiling (87/42°) but is BLIND to
the torn blend (3.1° GREEN), while M_BlendSpread sees the tear.

### M2 — consolidation. **EXIT GREEN.**
Full `rb3-tests` (whole-engine link, engine main) builds clean; oracle suites **16 PASS / 2 SKIP**:
`OracleValidation.* : SkinOracleSynthetic.* : SkinOracleLoader.*` all green;
`BlendSpreadSeparatesTornBlend` runs on the REAL committed goldens (VALID 6.95x, not the synthetic
fallback). Goldens under `native/tests/goldens/r2-skinning/{good-body,bad-torn}/`.

## The 2 remaining SKIPs — honest disposition

- **`SkinningLiveGate.CandidateWithinGoodEnvelope`** — env-pointed (`RB3_SKIN_ORACLE_GOOD_DIR` /
  `_CAND_DIR`), SKIPs when unset. **By design** (the `test_crowd_bone_oracle.cpp` precedent it
  copies): it is the one-command grader a future wave (W2.4 BandPatchMesh / R5 reskin) points at a
  fresh candidate capture. It is a tool, not a CI regression pin — flipping it to always-run would
  require a committed "good candidate" that is not the baseline itself. **Kept SKIP intentionally.**

- **`VerdictTable.ArmWArmSReproducedFromFixtures`** — SKIPs without `arm-w`/`arm-s` goldens.
  **Cannot be honestly flipped within Lane-S constraints.** The offline `M_Tier1RestCoherence` is a
  *coherence analog*, not a bit-exact replica of the engine's Tier-1 xcheck that produced the
  Wave-15 table (male 0.1°/3.1°, female 28.9°). On real dumps the offline metric reads 3.1° at the
  bind pose but ~180° at gameplay frames (a bone-WORLD-basis-flip artifact), and never 28.9° on any
  observed frame. Reproducing the engine's exact verdict table would require the engine to EMIT its
  own Tier-1 value into the dump (an engine edit — out of Lane-S scope) or recalibrating the pinned
  numbers to what the offline metric produces (which would misrepresent the Wave-15 record).
  **Kept SKIP with this note; the verdict-table's *purpose* — a known-bad arm reads bad — is served
  by the reproduced ceiling (87/42°) vs torn (3.1° GREEN + blendspread 33) separation above.**
  Followup for a future wave: add an engine-emitted-Tier-1 field to RB3_PALETTE_DUMP, then this test
  reads the engine number directly.

## Gates (PLAN §5)
- G1 fixture integrity / strict loader — `SkinOracleLoader.RejectsTruncatedFixture` GREEN.
- G3 permanent Wave-16 red test — `BlendSpreadSeparatesTornBlend` VALID on real torn hands;
  `BlendSpreadDiscriminationIsInTheBlend` proves discrimination lives in the blend.
- G4 blindness pins — `Tier1IsBlindToTornBlend`, `Tier2IsBlindToTornBlend`, `WextIsNotAnOracle` GREEN.
- G5 registry rule — `RegistryComplete` GREEN.
- G7 hygiene — full rb3-tests green; no default flips; no engine edit; no pin bump.

## Provenance gap fixed
`skinning-fixture-capture.py` now records `arm_env` + the full palette-dump env in each
`provenance.txt` (the prior captures did not, so the A3 carve-out flag state was unverifiable).

## Followups (do not expand scope here)
1. Engine-emitted Tier-1 field in RB3_PALETTE_DUMP → flips VerdictTable to a real reproduction.
   **DONE — Wave-18 R2-NET (below).**
2. (Cheap, from D4 ground truth) a middle/ring inner-segment curl assertion could be added to the
   fixture set — the R1-DOLPHIN/D4 delta table shows those chains surviving 14-41° while wrist/thumb
   are exonerated (<0.06°). Register as the regression net for whatever fix R5 chooses.
   **DONE — Wave-18 R2-NET (below).**

---

# R2-NET — Wave-18 Lane N (fixture net-tightening)

**Engine:** `e69a35f` (Tier-1 field; pin bump = coordinator at close-out). **rb3:** see commit.
**Build:** `native/build-r2` against engine main. Full `rb3-tests` = **116 PASS / 7 SKIP / 0 FAIL**
(the 7 SKIPs are all env-pointed live-gate tools; VerdictTable is NO LONGER among them).

## Deliverable 1 — engine-emitted Tier-1 field: **DONE**
NEW `tier1 <worstDeg> <count(>5)> <recap> <worstBone> <cold>` header-key line in the
RB3_PALETTE_DUMP sidecar (engine `Rnd_Wgpu_RB3.cpp`, in the dump block ~:5060). Value = the
`:4820-4841` xcheck quantity `max_b angle(off_b * FIRST-SEEN cached rest world, I)` (A4/A5 BINDING),
via a **local first-seen rest cache replicated in the dump block** (pointer-identity fresh,
populated every matched draw) — so it is pose-STABLE, NOT off×WorldXfm (which the offline analog
reads ~180° at gameplay). `cold=1` marks a fully-recaptured-this-frame dump (LOUD per A4). NEW
header key ONLY (loader ignores unknown keys; bone/v lines untouched → all committed goldens still
load). Default-OFF; no new flag/knob → no classjson change.

## Deliverable 2 — VerdictTable flip: **DONE — first real reading is a measured PASS**
Captured `arm-w` + `arm-s` hands_naked fixtures WITH the field and flipped
`VerdictTable.ArmWArmSReproducedFromFixtures` to read the engine number
(`test_skinning_oracle.cpp`, `f.tier1Worst`).

**First real reading (engine tier1, gender-split):**
| arm | male | female | Wave-15 verdict |
|---|---|---|---|
| arm-w | **0.06°** | **28.88°** | 0.1 / 28.9 |
| arm-s | **3.13°** | **28.92°** | 3.1 / 28.9 |

Reproduces the Wave-15 adjudication within tolerance → **PASS**. Diagnostic contrast: the offline
`M_Tier1RestCoherence` on the *same* arm-s male frames reads **61.95°** (off×current-world drifts
under articulation; the engine cached-rest field does not) — this is exactly why the field was
needed.

### FINDING (SHARD_GUARD_OFF gates the palette dump)
arm-w sets `SHARD_GUARD_OFF=1`, which (`Rnd_Wgpu_RB3.cpp:4321`) gates the ENTIRE skinned-mesh
diagnostic block — **including RB3_PALETTE_DUMP** — behind `!SHARD_GUARD_OFF || SHARD_RATIO_DBG`.
Without `SHARD_RATIO_DBG=1` the dump never fires (0 hands_naked dumps) — the reason Wave-15's arm-w
numbers were never palette-dump-reproducible. Fix: `skinning-fixture-capture.py` arm-w env now also
sets `SHARD_RATIO_DBG=1` (runs the diagnostic block while `guardActive=false` keeps the guard DROP
disabled — arm-w render behaviour preserved). Bisected: NO_HEAD_REBIND/NO_SKIN_CLAMP each alone →
dumps fine; SHARD_GUARD_OFF alone → 0 dumps.

## Deliverable 3 — good-body re-capture (F3) + curl assertion: **DONE (early, per A6)**
- **F3 good-body:** re-captured (escapeartist body meshes, coherent, M_BlendSpread [0.581..2.980],
  VALID vs bad-torn margin 6.9x). Fresh provenance (`arm_env` + `palette_dump_env`), **md5-DISTINCT
  gameplay screenshots** (6 distinct; the prior 4 were one repeated all-black md5 — root cause: heavy
  broad-selector dump I/O stalls the headless GPU readback → black; a NARROW `escapeartist` selector
  fixes it), engine `tier1` field present. **M1 Tier-1 table committed as JSON**
  (`evidence/M1_tier1_table.json`) — was prose-only.
  *(Lane V's GT-A palette-forensics follow-up is unblocked: good-body re-capture landed.)*
- **A9 curl envelope:** `scripts/native/r2_curl_envelope_check.py` recomputes the D4 FLOOR envelope
  from the committed `D4_delta_table.json` (middle/ring FLOOR mean 13.41°, max 41.22° SURVIVE;
  thumb 3.44° + anchor 0.31° exonerated) and `CurlEnvelope.D4MiddleRingFloorSurvivesFrameMatching`
  pins it in the real-fixture suite (NOT synthetic Suite C). Carries the D4 CORRECTION framing:
  asserts the MEASURED ENVELOPE, not the dissolved mechanism.
