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

## Progress — ALL MILESTONES COMPLETE

- [x] M1 — capture_lints.py + selftest 4/4 (commit `14f96575`)
- [x] M2 — wire white_regrade.py (commit `8ee9f6bd`; mechanical exit met — F2+F10 refusals fire)
- [x] PREGUARD RED — N=30, stream **29/30 RED** (evidence/iso_ledger_n10_PREGUARD.json)
- [x] M3 — four isolation guards (commit `801faf3c`; worktree GREEN + main repo identical)
- [x] M4 — EXIT GREEN: N=30 stream **30/30**, all deltas 0 (evidence/iso_ledger_n10.json)
- [x] M5 — G3 batch_objdiff: delta 0.0 vs baseline on all four (Wii-inert). evidence/g3_batch_objdiff.json
- [x] M6 — flag-OFF inert: 0 [LOADDET] seam-OFF; drawlog-golden 792 match; rb3-tests 116 PASS/0 FAIL.
      evidence/m6_default_boot.txt

## EXIT RESULT (BINDING) — A/B decisive

Clean-engine worktree A/B, N=30/arm, same eng_hot seam regime, binaries differ ONLY by the four guards:

| arm | guards | ledger stream axis | per-boot postAnchorDelta |
|---|---|---|---|
| PREGUARD | absent | **29/30 (RED)** | 29×0, 1×1 (distinct=[0,1], NOT invariant) |
| GREEN | present | **30/30 (PASS)** | 30×0 (boot-invariant) |

The PREGUARD divergent boot's in-window gRand-reaching draws attribute to
`CharInterest::ComputeScore:172` (4) + `LightPresetManager::PickRandomPreset:286` (1) — two of
the four guarded consumers. **R-A residual CLEAN** (attributed post-anchor PCs MINUS the union of
9 isolated sites = my 4 + R4-M2's 5 = empty; no fifth consumer, no R-A round needed, AM-3
subtraction applied). The fail-red is the PREGUARD binary reproducing stream < 30/30; the seam-OFF
alternate fail-red was struck per AM-2b. The Wave-18 WHITE re-grade VOID precondition (eng_hot
OFF-arm ledger stream 10/10) is now SATISFIABLE.

Note on divergence rarity (ASSUMPTION-D borne out): the stream divergence is rare/timing-dependent
(~1/30 boots land venue draws in [anchor,anchor+300]); N=10 under-samples it (a fresh N=10 sample
caught 0 divergent boots → misleading 10/10), so both arms were run at N=30 for a reliable RED. The
guard MECHANISM is deterministic (sDetRedirect!=NULL → draw bypasses gRand); the statistical A/B is
the load-bearing proof.

## Commits
- rb3 `14f96575` — M1 capture_lints.py
- rb3 `8ee9f6bd` — M2 white_regrade.py wiring
- rb3 `801faf3c` — M3 four isolation guards
- rb3 `<this commit>` — iso_ledger_gate.py + all evidence + STATUS
- engine: NONE (rb3-side only; pin `beb89e5` NOT bumped)

## KEY FINDING — the stream divergence is RARE (regime detail, not a lane blocker)

The eng_hot OFF-arm stream-axis divergence is timing-dependent and RARE: only ~1/10 boots
land the four venue consumers' draws inside the fixed post-anchor window `[anchor, anchor+300]`
(committed baseline `wr_n10-ledger-off.json`: stream **1/10**, per-boot deltas
`[16,0,0,0,0,0,0,0,0,0]`; reference boot = boots[0], so the axis reds only when the sample
contains MIXED deltas). A fresh N=10 PREGUARD sample happened to catch **zero** divergent boots
(all deltas 0 → a misleading 10/10). This is exactly ASSUMPTION-D / the rare-divergence trap the
plan flagged. Mitigation: capture **N=30** for both arms so the ~10% event reliably appears in
PREGUARD (RED) while the guarded GREEN arm forces every delta to 0 (redirect is deterministic:
`sDetRedirect != NULL` → draw bypasses gRand). The guard mechanism is deterministic; the
STATISTICAL A/B (PREGUARD@30 mixed-delta RED vs GREEN@30 all-zero) is the load-bearing proof.

Guard-tag string check confirms the A/B binaries differ exactly by the four guards
(GREEN has charclip/crowditer/charinterest 0→1; PREGUARD backup lacks them).

## Gate-change requests to Lane F/T1 files (coordinator folds)
_(none yet)_
