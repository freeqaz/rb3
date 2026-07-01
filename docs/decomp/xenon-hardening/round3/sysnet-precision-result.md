# System/network ACCEPT precision — measured (2026-06-23)

Closes round-2 **caveat #4** (SYNTHESIS.md): the 654 system+network ACCEPT
identities ingested into rb3-xenon's `fn_resolver` (T4b) were only *extrapolated*
from band3's measured 0.905 — never judged at quality. Now measured.

## Result

**30/30 SAME — precision 1.000, ≥0.90 at 95% CI** (rule-of-three: 0 wrong in 30 →
failure rate ≤ 3/30). Consistent with band3 (27/30 = 0.900) and the cross-stratum
holdout (0.933); if anything system/network ACCEPT runs slightly cleaner.

| category / stratum | same / n |
|---|---|
| system HIGH | 3/3 |
| system BSIM≥30 | 4/4 |
| system BSIM 20-30 | 4/4 |
| system BSIM 15-20 | 4/4 |
| network BSIM≥30 | 4/4 |
| network BSIM 20-30 | 6/6 |
| network BSIM 15-20 | 5/5 |

Even the weakest stratum (BSIM 15-20, where round-2 band3 found 1 wrong) is clean here.

## Method + judging-quality verification

30 independent Opus judges, one per self-contained evidence pack
(`round3/evidence/pair-NN.md`, built by the parallel sysnet effort), each writing its
verdict to `round3/evidence/verdicts/verdict-NN.json` before returning. Judges were
primed with the validated cross-compiler artifact list
([sibling-check-validation.md](sibling-check-validation.md)) so MWCC↔MSVC rendering
differences (DataNode tag 6-vs-0, packing offset shifts, dropped trailing call-args,
regalloc/scheduling) would not be miscounted as real divergence.

A unanimous high-confidence pass is exactly the result to distrust, so it was
adversarially checked rather than taken at face value:
- **Reasoning is substantive, evidence-positive** — verdicts rest on resolved-callee
  agreement (Xenon callees mapping via matches.json to the same Wii callees) and
  referenced strings, not mere absence of differences. Judges correctly *attributed*
  artifacts (e.g. pair-12 dismissed a 0x19C↔0x1F0 offset shift as packing yet still
  required genuine callee/constant agreement).
- **Cited evidence is real, not hallucinated** (spot-checked against the packs):
  pair-30's pack genuinely references `TransportSignatureGenerator.cpp` (the TU naming
  the claimed Wii symbol — strongest possible signal); pair-26's three `FormatString`
  callees are present; pair-15's `Init__11TrackWidgetFv == 0x827bb4f0` claim is in the
  header.
- **No pack was judged on weak evidence** — all 30 carry substantial resolved-callee data.

## Honest caveats

- n=30 ⇒ true precision is [0.90, 1.0] at 95%; 1.000 is the point estimate, not a guarantee.
- This is the **ACCEPT tier** (already gate-filtered); high precision is expected and
  the measurement validates the *gate*, not all 7,555 vetted entries.
- The artifact-awareness priming could bias marginally toward "same"; mitigated by the
  evidence-positive reasoning requirement and the spot-check, but a future
  blind/adversarial re-judge (no priming, instructed to refute) would harden it further.

## Implications

- The **654 system/network ingested `fn_resolver` T4b identities** (system 438 +
  network 216) are validated at precision comparable to / better than band3 — safe to
  trust as the existing ~0.93 probabilistic tier. Round-2 caveat #4 is closed.
- **T-D partial:** pair-15 confirms `Init__11TrackWidgetFv == Xenon 0x827bb4f0` (same,
  high) — so of the two xenon addresses that claimed `TrackWidget::Init`, this is the
  correct one; the other is the wrong/sibling member (feeds the T-D dup resolution).

## Artifacts
- `round3/evidence/judge_verdicts_sysnet.json` — aggregate (this commit).
- `round3/evidence/verdicts/verdict-NN.json` — per-pair reasoning + key_evidence (this commit).
- `round3/evidence/pair-NN.md` + `round2/forensics/*_sysnet*` — the evidence substrate
  built by the parallel effort; **present but untracked** (left uncommitted when that run
  ended) — commit separately to make the measurement fully reproducible.
