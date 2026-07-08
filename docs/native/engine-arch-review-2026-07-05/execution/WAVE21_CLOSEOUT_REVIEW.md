# WAVE21_CLOSEOUT_REVIEW — Fable close-out (hands flagship FIX wave, L2-b / R5-wall)

**Reviewer:** Fable. **Date:** 2026-07-08. **Inputs re-verified from committed evidence**, not
trusted from the STATUS files: `parsed_summary_tables.txt`, the four rhand crops, both FIX burst
PNG pairs, `hand_mesh_slots_sharedroot.log`, `loadbind_subdir_and_counters_bindfix.log`,
`bindfix_compose_dump.log`, `e11_texture_{OFF,ON}.txt`, commits `24c2ac1c`/`c774657d`/`f1e81060`/
`ed6b84a9`, and `BandCharacter.cpp` at HEAD.

**VERDICT: ACCEPT-WITH-ERRATA.** The L2-b / R5-wall outcome is SOUND for both genders, honestly
reported, and not a premature give-up. The one record-level casualty is Wave-20's Layer-1 causal
story — Wave 21 CONTRADICTS (not merely refines) "restore per-member binding via the remap" — and
that correction (ERR-1/2) is the load-bearing erratum of this closeout.

## Q1 — Males L2-b: SOUND — ACCEPT

- The 8th cell reaches coherent basis **at draw, every block**: REPOINT male n=996 TIER1
  min=med=max=3.1°, count(>5°)=0 (`W21-DISCRIM/evidence/parsed_summary_tables.txt:70-71`) — gameplay
  burst frames at score 575, not rest. Female identically (:80-82). 3.1° equals the arm-S rest
  baseline, i.e. ≈floor.
- The tear is real and committed: `crop_armREPOINT_burst03_rhand.png` shows the finger-spike fan
  (reviewer-inspected); `crop_armBINDFIX_burst03_rhand.png` shows the same class.
- A7 operationalized correctly: Tier-1-coherent-at-draw AND visual-tear-at-matched-frames, with the
  Tier-2 EXACT-joint used comparatively vs matched flag-OFF (330 male / 114 female frames,
  :100-103) and the torso fail-red separating cleanly (jacket 0.1u / jeans 0.0u vs shim-off
  50-150u, :89-95). The "invisible-to-the-joint-metric" reading is corroborated in the strongest
  possible way: males 15% of frames ≤8.4u (small, distal, intermittent) and **females 0% >2u while
  equally torn** — a shell/weight-blend divergence between coherent joints, the R5 VERDICT §0-item-4
  signature class. One wording nit → ERR-3 ("exactly" overstates: §0 item 4 was in-principle
  bone-world blindness; the EXACT-joint metric is offset-aware and under-reads in practice).

## Q2 — Both genders L2-b, no partial ship: ACCEPT (adversarially probed — it holds)

The load-bearing "no female-only ship" claim survives three attacks:
1. **Dissolution is numeric, not narrative**: female REPOINT TIER1 3.1° count=0, n=511 (:80-82) vs
   flag-OFF 42.6° (:14). The gap is a seed-R artifact; under the coherent regime the female equals
   the male and still tears.
2. **The modus tollens is valid**: a Part-2 gender-rest-pose produces the same composition family
   (`inv(B)·L_own(t)` with basis ≈B) that REPOINT empirically realizes at every frame — measured
   torn. No female L2-a lever survives as the operative defect. (Note the 28.9°→42.6° capture-time
   discrepancy, freshness vs draw → ERR-4; both figures dissolve, so nothing turns on it.)
3. **There is no mechanism to ship**: BINDFIX == flag-OFF byte-for-byte on the female too (TIER1
   42.6° identical, :14 vs :36), and hands are not even per-member-bound under Part 1 — a female
   scope-flag would ship a no-op. Declining review-A8's third outcome is not a choice against a
   partial ship; the partial ship does not exist.

Residual weakness (non-blocking): per-gender VISUAL attribution is thinner than A7's letter — one
unlabeled rhand crop per arm; the gender split is carried by the nb=38/40 numerics plus full-frame
bursts. Noted, not verdict-affecting (ERR-5 covers the adjacent evidence-completeness gap).

## Q3 — Wave-20 Layer-1: CONTRADICTED in part — errata REQUIRED

- W20 claimed the remap controls hand binding: "the merge's sBoneMergeDir remap that WOULD re-point
  them" (`W20-NATIVETRACE/STATUS.md:36-37`) and charter item 1 "Restore per-member binding — scope
  the FilterSubdir shim" (`W20-SYNTHESIS/SYNTHESIS.md:113-116`).
- Wave 21 measured the refutation: BINDFIX fires br2/br3 31,488×/member == full shim-off
  (`loadbind_subdir_and_counters_bindfix.log`) yet every committed hand slot stays
  `owningDirClass=SHARED_ROOT distinct=0` (`hand_mesh_slots_sharedroot.log`, 30/30 rows). The
  mechanism is coherent: `ReplaceRefs(o1, found)` re-points refs to the incoming merged COPY; the
  hand mBones already resolved at parse to the resident shared instance (= `found`), so the remap is
  a structural no-op for them. Parse decides; the remap never did.
- CONSEQUENCE for the W20 record: Lane N's NOSHIM per-member flip (owningDir 4 distinct,
  `W20-NATIVETRACE/STATUS.md:47-48`) was therefore NOT the remap acting on hand meshes — it must be
  a parse/merge-topology side effect of FULL un-shim. The differential remains UNEXPLAINED on
  committed evidence (BINDFIX ≈ NOSHIM minus colorpalettes-only, same br2 count, opposite hand
  topology; also 205 W20 slots vs 199/30 W21). The wall verdict does NOT depend on resolving this —
  all four regimes were measured directly — but the record must stop saying the remap restores
  per-member hand binding. → **ERR-1, ERR-2** (exact wording below).

## Q4 — RB3_HANDS_BINDFIX disposition: KEEP default-OFF, document-as-partial (as FIX already does)

Not dead weight: (a) it is the faithful load-topology restore (retail kMerge everywhere but
colorpalettes) with the white-texture fix intact (e11: 0 dummy/skin cascade both arms) and zero
crashes — the substrate any future parse-time un-share charter would build on; (b) the
`RB3_BINDFIX_KEEPCS` A/B knob re-demonstrates the counted mechanism (br2 31,488↔0); (c) removal
loses the measurement harness. Flag-ON delivers zero hand benefit ("arguably worse" mitten-OFF), so
default-OFF is correct and a flip should stay behind extraordinary evidence. BUT the in-tree
comments contradict the shipped behavior in two places and a flag-name collision exists → **ERR-6**.

## Q5 — Manufacture / over-hedge audit: the wall is real; two gaps to record

- **Not a premature wall.** The kickoff's Part-1 FALLBACK ("targeted post-merge re-point
  replicating the remap's effect", KICKOFF:121-122) was never run — but it is implicitly refuted:
  its draw regime is the measured NOSHIM/REPOINT family (per-member own==bound; catastrophic at
  rebound=0, coherent-but-torn at rebound=1). Declining it was sound; the STATUS should say so
  explicitly so it is not read as an untried in-charter lever → **ERR-7**.
- **Parse-time un-share correctly deferred**: as a hands FIX it lands in the same measured-torn
  composition family; verifying any variant needs articulated Wii GT — the walled half. Deferring
  is not giving up; chartering it as a fix lane would have been dead-cell-by-the-back-door.
- **No overclaim in "faithful Layer-1"**: FIX states plainly it is "NOT landable as a hands fix"
  (STATUS:97-98) and reports G-FIX-E1 as FAIL-to-fix. Honest.
- **One process deviation to bless**: DISCRIM ran the BANNED 8th-cell flag as its fourth measurement
  arm (charter authorized BINDFIX / RB3_LOADBIND_NOSHIM substrates; process rules say "Refuted flags
  UNSET"). It was measurement-only, zero code, transparently reported — and it is what makes the
  L2-b verdict decidable. Blessed here as NOT a dead-cell re-attempt → **ERR-8**.

## Q6 — Banned-citation / GT-D consistency: CLEAN

- Routes to the wall WITHOUT claiming reopen: the reopen condition is an articulated **Wii** capture
  (`R5-HANDS-ENDGAME/CLOSURE.md:63-65`); native-side measurements do not qualify and neither lane
  claims they do. GT-D stands.
- §8.4 respected: reskin cited only as banned; HANDS-FIX §Dolphin-fallback cited "its 'confirms
  reskin' framing NOT inherited" (DISCRIM STATUS:26, per A10). §8.3 clamp-shorthand respected
  (seed-R rebake named as the shipped state). wext descriptive-only (A9). The code's "errata E10"
  cite is valid (`WAVE20_CLOSEOUT_REVIEW.md:156`).

## Q7 — WAVE-22 RECOMMENDATION

**Hands are terminal: doubly walled (R5 GT-D + this native-side gender-split decision). Mitten
(default-ON) is the answer.** No non-banned, non-walled fix lever remains: every reachable
coherent-basis regime is measured torn; a parse-time un-share lands in that same family and cannot
be verified without articulated GT; no distinct render-mitigation mechanism beyond the mitten has
been identified. The only falsifier path stays CLOSURE follow-up #2 (root-cause
`CharClipDrivers=0` → articulated capture) — OPTIONAL, separately-charterable, do not spend a lane
by default. **Wave 22 should apply ERR-1/2/6 (cheap, doc+comment-only) and move to the deferred
Wave-20 menu: WHITE re-grade / T3 / FOREARM-FLOAT** — FOREARM-FLOAT is the natural next visual
item and should absorb the mitten-OFF "arguably worse" observations if they prove forearm-level.

## ERRATA (append-only; targets + wording)

- **ERR-1** `W20-SYNTHESIS/SYNTHESIS.md` ERRATA block, append **E13**: "§Layer-1 'binding flips to
  per-member — exactly matching Wii's topology' (:38-40) and charter item 1 'Restore per-member
  binding [via the scoped shim/remap]' (:113-116) are CONTRADICTED by Wave 21: hand-mesh mBones bind
  at PARSE to the resident shared instance; `ReplaceRefs(o1→found)` is a no-op for them (BINDFIX
  br2=31,488 == shim-off, hands stay SHARED_ROOT). The NOSHIM per-member flip is a FULL-un-shim
  parse/merge-topology side effect, mechanism unpinned. Restoring the remap does NOT restore
  per-member hand binding."
- **ERR-2** `W20-NATIVETRACE/STATUS.md:36-37`, append: "SUPERSEDED (Wave 21): the remap fires under
  the scoped BINDFIX and does NOT re-point hand mBones — see W21-FIX/STATUS + WAVE21_CLOSEOUT ERR-1."
- **ERR-3** `W21-DISCRIM/STATUS.md:77` "reproduces R5 VERDICT §0 item 4 exactly" → "reproduces the
  §0-item-4 signature CLASS (joint-level metrics cannot gate the shell tear); the EXACT-joint metric
  is offset-aware and under-reads in practice, unlike item 4's in-principle bone-world blindness."
- **ERR-4** `W21-DISCRIM/STATUS.md:94`: note the committed 28.9° is the freshness-capture figure;
  the same arm reads 42.6° at draw (parsed_summary_tables.txt:14). Both dissolve under repoint.
- **ERR-5** `W21-FIX/STATUS.md:30-31`: the committed `hand_mesh_slots_sharedroot.log` holds 30
  player0 rows; the 199/199 census lives in the uncommitted `/tmp` run log. Commit the full census
  (or restate as 30/30 committed + 199/199 observed) and reconcile vs W20's 205-slot population.
- **ERR-6** `BandCharacter.cpp` (next touch, comment-only): :101-105 ("outfit *_resource.milo
  subdirs KEEP the shim's kReplace") and :4550-4552 ("kReplace ONLY for the two shared
  texture-PALETTE subdirs (colorpalettes.milo + char_shared.milo)") contradict shipped behavior —
  authoritative :125-131 + the committed log: ONLY colorpalettes stays kReplace; char_shared only
  under `RB3_BINDFIX_KEEPCS`. Also disambiguate the pre-existing DISTINCT flag `RB3_HANDS_BIND_FIX`
  (:1991-1993) from `RB3_HANDS_BINDFIX` (name-collision hazard).
- **ERR-7** `W21-FIX/STATUS.md` §Banned-cell hygiene, append: "The kickoff's Part-1 fallback
  (targeted post-merge re-point) was intentionally NOT run: its draw regime is the measured
  NOSHIM/REPOINT family (both torn) — refuted by measurement, not skipped."
- **ERR-8** Process blessing (this file): DISCRIM's `RB3_HANDS_AUTHORED_REPOINT=1` measurement arm
  was a justified charter deviation — measurement-only, no code, load-bearing for the L2-b verdict;
  it is NOT a re-attempt of the dead cell and sets no precedent for fix lanes.

**FINAL: ACCEPT-WITH-ERRATA.** Wall verdict sound for both genders; no partial ship exists; keep
`RB3_HANDS_BINDFIX` default-OFF as a documented partial; correct the Wave-20 Layer-1 record (ERR-1/2)
so no future wave re-buys "restore the remap → per-member hands."
