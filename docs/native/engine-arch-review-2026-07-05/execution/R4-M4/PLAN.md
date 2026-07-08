# R4-M4 — WHITE re-grade + wash per-FX co-sampling (Wave-18 Lane W) — PLAN

The R4-M4 cash-in: the R4 seam now delivers stream-matched boots (`R4-DETERMINISM`
ledger PROVES 10/10). Two deliverables, both MEASUREMENT-only — **the lane flips
nothing**; the coordinator E1-gates any flip.

Engine pin `49ca0d6` (READ-ONLY; the `RB3_VENUE_WHITE_GUARD` flag is already landed
engine-side, `Rnd_Wgpu_RB3.cpp:1243/1525`). Build `native/build-agent-R4-M4/rb3-native`
(clang Debug, master + engine 49ca0d6 — carries the R4 seam merged to master
`65c5092e`, the guard, and the BOOTRNG/loaddet probes).

## Item 1 — WHITE guard re-grade (`white_regrade.py`)

Re-grade the held `RB3_VENUE_WHITE_GUARD` (Wave-10 HOLD ×2: the arm-mean gate
sign-flipped between blind runs under W0.3d boot-noise) on the now-resolving gate.

- **Vehicle:** `eng_hot` (forced-hot ENGAGED, real env) — the guard fires ONLY in the
  world.cam engaged branch (`:1525`); the flood is provably inert (Wave-10). Both arms
  differ ONLY by `RB3_VENUE_WHITE_GUARD`.
- **A2 (F6 verbatim):** every boot runs `RB3_FIXED_CLOCK=1 RB3_LOAD_DETERMINISM=1
  RB3_LOADDET_ATTRIB=1 RB3_LOADDET_JITTER=200`. The per-axis ledger is graded from
  THOSE boots' OWN `.engine.log`s via `loaddet_gate.grade_external_logs` (new
  grade-external-logs mode). Precondition = stream axis N/N AND nParsed==N on the
  exact boots, else **VOID** — the harness REFUSES to emit a verdict (never
  discard-and-rerun a ledger-failing boot, never average one in). A VOID reads as a
  seam-regression finding (report axis + counts).
- **A3 (reproduce-first, single trajectory):** the seam pins ONE trajectory
  (`Seed(0x5EED)`, no seed knob). NO comparison to Wave-10/11 absolute numbers. FIRST
  gate = `mean(hi_frac | guard-OFF) >= 15` under the seam; on failure the item is
  **HELD substrate-blocked** and the follow-up is a coordinator `RB3_LOADDET_SEED`
  knob (outside Lane W's writable set). Cross-arm check: guard-ON vs guard-OFF
  `postAnchorDelta` identical (guard is render-side, draws no gRand) -> the A/B is a
  same-trajectory paired comparison.
- **R-B / G1a (unchanged):** flip iff G1a (`mean(hi_frac|ON) <= OFF-3.0` AND
  directional) AND G1b (`mean(mid_sat|ON) >= OFF+0.02`). N=10/arm; early-stop N=5 only
  if within-arm hi_frac sd<0.5 in BOTH arms AND ledger 5/5.
- **lint 8:** guard branch-entry hit-count (`[WASHPROBE] SCENE engaged=1`) reported
  per arm.

**Capture confounds removed (`r4m4_capture.py`).** The Wave-10 gate was confounded two
ways that this driver fixes, both MEASURED here:
  1. `wash-measure.capture_pinned` autohits POST-anchor -> hit-FX/scoring draws the
     seam does NOT isolate reach gRand -> ledger stream FAILS (measured 2/3). Fix:
     drive INPUT-FREE post-anchor (the M3 discipline; `nofail` holds the song).
  2. the +/-tol songMs window admitted different LIGHTING PHASES (Wave-11 bimodal
     NEARBLACK-vs-WHITE confound). Fix: screenshot the FIRST frame with
     `songMs >= target` (one-sided, tol=0) — deterministic frame under the fixed clock.

**Verdict space:** READY_FOR_FLIP | HELD-with-numbers | HELD substrate-blocked | VOID.

## Item 2 — wash per-FX co-sampling instrument (`wash_cosample.py`)

BOOTRNG backlog item 3 (Wave-11 FX/swept-light PHASE axis). Co-samples PER FRAME, in
ONE seam-pinned boot over a songMs sweep:
  - particle-emission phase = addr2line-attributed InitParticle/CreateParticles/
    PartLauncher draws (RB3_LOADDET_ATTRIB) over a trailing window;
  - swept-light phase = distinct `[BOOTRNG] LIGHTVAL valhash` count over the window;
  - hi_frac per screenshot (ground truth).

**lint 3 (BINDING):** the instrument's numbers mean nothing until it demonstrates
KNOWN-GOOD/KNOWN-BAD separation — BAD = WHITE-class shot, GOOD = low-hi_frac shot; a
phase signal must SEPARATE them (AUC>=0.75 or <=0.25 AND MWU p<0.10) or the instrument
reports "FX-emission not the driver" (a finding, not a silent pass).
**lint 8:** optional guard-ON boot reports the guard branch-entry hit-count.

## Deliverable
Measurement package under `evidence/` + this lane's verdict per item; STATUS.md;
coordinator E1-gates any flip. NO default flips, NO pin bumps, NO engine edits.
