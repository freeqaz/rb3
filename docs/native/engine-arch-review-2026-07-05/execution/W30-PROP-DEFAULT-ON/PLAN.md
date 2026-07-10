# W30-PROP-DEFAULT-ON — PLAN

**Lane 2 (decision lane).** Discharge E6(b) so the coordinator can decide whether
to flip `RB3_PROP_POSE_FULL` as the 15th default. This lane produces the DECISION
with evidence; it does NOT flip anything.

**Base:** worked on master at/after `fdc4d628` (HEAD `85143cdf` at start — visual-pass
doc commit, no source; `CharIKHand.cpp` untouched since base). Engine pin unchanged.

## The E6(b) question
W28 piece (1) — breaking the `mFinger` finger-compensation re-projection — is applied
GLOBALLY under `RB3_PROP_POSE_FULL` (every `mFinger != NULL` ikhand), whereas piece (2)
(the redirect) is scoped to `bone_target_*` parents. E6(a) already retired the vocalist
mic chain (`mic.ikhand` has `mFinger == NULL`). E6(b): show the global mFinger break is
harmless to NON-PROP finger=1 chains (feet/mic-stand/vocalist free hands) OR re-scope
piece 1.

## Path A (census-first, preferred)
1. **STEP 0 (discriminator).** Add a threshold-unbiased one-shot `[PROP_CENSUS]` probe
   (CA5) in `CharIKHand::Poll` — env `RB3_PROP_CENSUS_DBG`, `#ifdef HX_NATIVE`,
   default-OFF, deduped per (owning-character-root | ikhand name). Enumerate EVERY
   CharIKHand: char, ikhand, `mFinger != NULL?`, fingerName, hand chain, reach.
   NOT derived from the 30u-gated `[PROP_DST]` rows. Gate: `Poll__10CharIKHandFv`
   batch_objdiff baseline-exact (Wii `.o` byte-identical).
2. **A/B.** `--fixed-clock` songMs-matched OFF vs FULL-ON, scoring ALL finger=1
   ikhands per-character + a foot/plant sanity metric. To separate the drummer
   `right_hand` (PROP, expected residual) from the vocalist `right_hand` (NON-PROP,
   must-not-regress) — which collapse under the bare `[PROP_DST]` ikhand name — append
   `char='<root>'` to the `[PROP_DST]` line (probe-only, appended at END so the legacy
   `DST_RE` still matches; re-verify baseline-exact).
3. If no NON-PROP chain regresses → **DECISION: FLIP-SAFE**.

## Path B (only if a NON-PROP chain regresses)
Re-scope piece 1 to prop-chain ikhands inside the SAME flag (default stays OFF);
mechanical decider = extended analyzer `--w30-residual-baseline` → `ACCEPTANCE
(W30-ON): PASS`.

## Analyzer (CA1 — extend in place, do not fork)
`W28-PROP-FIX/analyze_prop_ab.py` gains: optional `char=` capture in `DST_RE`;
`[PROP_CENSUS]` parser; `--w30-census OFF= ON=` mode (census table + per-char finger=1
A/B + foot sanity + `W30 DECISION:` line); `--w30-residual-baseline ON=` mode (CA2:
prints legacy `ACCEPTANCE (ON):` AND `ACCEPTANCE (W30-ON):`).

## Bounds / gates
≤4 boot runs (used 4). batch_objdiff CharIKHand baseline-exact; drawlog PASS
(flag default-OFF); rb3-tests clean. Probe caps via existing `RB3_PROP_DST_DBG`
value; ONE new getenv `RB3_PROP_CENSUS_DBG` (unavoidable per CA5 — a NEW unbiased
enumeration probe, `RB3_PROP_*DBG` named as sanctioned).
