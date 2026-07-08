# W-ISO — STATUS (implementer)

**Lane:** Wave-19 Lane I (W-ISO) — venue-path consumer isolation + capture-lint hardening.
**Role:** IMPLEMENTER (Opus). **Charter:** OPTIONS.md §6.5 (`c4395043`), WAVE19_KICKOFF Lane I
(`d93fa894`). **Plan:** `W-ISO/PLAN.md`; **Review:** `W-ISO/PLAN_REVIEW.md`
(APPROVE-WITH-AMENDMENTS). Engine pin `beb89e5` — NOT bumped. HEAD at start `5942915f`.

**NAMING BOX (review F9, binding):** this lane isolates per-consumer gRand **stream position**
(frame-assignment count variance). It does NOT touch R4's ledger `order` axis (already 10/10).
No doc/code comment conflates the two.

---

## Amendment adoption (PLAN_REVIEW AM-1..AM-6 — all BINDING, all folded)

- **AM-1 (HIGH, M1 F7 threshold):** ADOPTED. `partition_black_frames` threshold `T = 0.05`,
  derived from the NEARBLACK row `wr_n10_OFF_01` (mean_luma **0.0**, the 12.43→13.81 restatement
  precedent) — NOT the cited `wr_n10_ON02_WHITE_zerochroma.png` (measured mean_luma **0.78**, a
  bright zero-CHROMA WHITE frame; a threshold above it would wipe 19/20 committed frames). Lowest
  legitimate committed frame = `wr_n10_OFF_02` at 0.4798; `T=0.05` sits in (0.0, 0.1]. Both bounds
  documented in `capture_lints.py`. Verified against committed `wr_n10.json` this session.
- **AM-2 (MED-HIGH, M4 EXIT):** ADOPTED. (a) EXIT gates on `summary.stream == "10/10"` AND
  `nParsed == 10` (the binding contract = boot-invariance vs reference boot, loaddet_gate.py:367),
  NOT the plan's stricter added `postAnchorDelta == 0`. Per-boot `axes.stream.value` recorded in
  evidence; a nonzero boot-invariant constant is informational, NOT a gate failure / R-A trigger.
  (b) The seam-OFF alternate fail-red is STRUCK (seam-OFF boots emit no anchor markers → broken-
  harness red, not a stream-axis fail). The PREGUARD binary at N=10 is the ONLY valid fail-red.
- **AM-3 (MED, R-A iter 2):** ADOPTED. Attribution subtraction rule: the attrib tap fires BEFORE
  the redirect (Rand.cpp:186-212), so the four isolated sites + R4-M2's five stay per-PC-counted.
  R-A iteration-2 residual set = attributed post-anchor PCs MINUS the union of all isolated sites
  (4 new + 5 R4-M2). Encoded in `iso_ledger_gate.py`'s R-A helper.
- **AM-4 (MED, M2 exit):** ADOPTED. M2 exit is MECHANICAL: validate run completes, disclosure
  fields present, JSON round-trips through strict `allow_nan=False`, and both refusals fire (NaN
  injection + `--refinish`-without-`--validate`). Ledger-cleanliness is M4's business.
- **AM-5 (MED, sequencing/lint 10):** ADOPTED. Order = M1 → M2 → build `iso_ledger_gate.py` +
  capture PREGUARD RED (N=10, current pre-guard binary) → M3 → M4 GREEN → M5/M6. Instrument built
  and run on the diagnosis before the fix.
- **AM-6 (LOW cluster):** ADOPTED. (1) guard comment says "eng_hot venue-path divergent (R4-M4
  attribution), spread <N>". (2) graded boots take the :286 probe branch (seam sets
  RB3_BOOTRNG_PROBE=1); function-scope covers both :286 and :294. (3) M6 evidence artifact
  `m6_default_boot.txt`; M6 retitled "flag-OFF inert" (native binary is NOT byte-identical — the
  guard ctor is compiled in; byte-identity is M5's Wii property). (4) import-drift hazard pinned
  to coordinator if F changes grade_external_logs schema. (5) refusal message states what remains
  allowed. (6) line-cite nits noted.

ASSUMPTION-A resolved to FACT by review: all four TUs `#include "math/Rand.h"` directly
(CharClipDriver.cpp:3, CharInterest.cpp:3, LightPresetManager.cpp:3, Crowd.cpp:14) — no fallback
include needed.

---

## Progress

- [ ] M1 — capture_lints.py + selftest
- [ ] M2 — wire white_regrade.py
- [ ] PREGUARD RED (N=10, pre-guard binary)
- [ ] M3 — four isolation guards
- [ ] M4 — EXIT gate GREEN (stream 10/10)
- [ ] M5 — G3 batch_objdiff
- [ ] M6 — flag-OFF inert + rb3-tests + drawlog

## Gate-change requests to Lane F/T1 files (coordinator folds)
_(none yet)_
