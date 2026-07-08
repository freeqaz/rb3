# R4-M4 — WHITE re-grade + wash per-FX co-sampling (Wave-18 Lane W) — STATUS

Lane W (KEY=R4-M4). Engine pin `49ca0d6` (READ-ONLY; guard already landed). Build
`native/build-agent-R4-M4/rb3-native` (clang Debug, master@`dde99406` = R4 seam merged
`65c5092e` + engine guard `:1525` + BOOTRNG/loaddet probes). The lane FLIPS NOTHING;
coordinator E1-gates any flip.

Deliverables (all committed under `evidence/`):
`smoke_boot.py`, `r4m4_capture.py`, `white_regrade.py`, `wash_cosample.py`, and the A2
grade-external-logs extension to `scripts/native/loaddet_gate.py`.

---

## A2 integration — grade-external-logs mode (done)

`loaddet_gate.py` only graded boots it launched itself (input-free). Added
`parse_boot_log()` (refactored out of `boot_measure`) + `grade_external_logs()` +
`--grade-logs` CLI so the ledger grades the EXACT WHITE/wash measurement boots' OWN
`.engine.log`s. Every measurement boot runs
`RB3_FIXED_CLOCK=1 RB3_LOAD_DETERMINISM=1 RB3_LOADDET_ATTRIB=1 RB3_LOADDET_JITTER=200`;
the `[LOADDET]` markers (anchor/reseed/frame/attrib/complete) land in the boot log
(stderr→log), so the same-boot ledger is exact. **VOID semantics (F6 verbatim):** the
harness requires stream axis N/N AND nParsed==N on the exact boots or it REFUSES to
emit a verdict — never discard-and-rerun a ledger-failing boot, never average one in.
(Self-tested: `--grade-logs smoke.engine.log` → stream 1/1 PASS.)

## Capture-confound removal — `r4m4_capture.py` (done, MEASURED)

The Wave-10 HOLD had TWO confounds this driver removes, both measured here:

1. **`wash-measure.capture_pinned` autohits POST-anchor** → hit-FX/scoring draws the
   seam does not isolate reach gRand → ledger stream FAILS. **Measured 2/3** through
   `capture_pinned`. Fix: drive INPUT-FREE post-anchor (the proven M3 discipline;
   `nofail` holds the song).
2. **the ±tol songMs window admits different LIGHTING PHASES** (Wave-11 bimodal
   NEARBLACK-vs-WHITE confound). Fix: screenshot the FIRST frame with
   `songMs >= target` (one-sided, tol=0) — a deterministic frame under the fixed clock.

## CORE FINDING (the discrimination R4 was built to deliver)

Under the seam, the dominant post-anchor divergent consumer is
**`RndParticleSys::InitParticle`** with per-boot draws **[2505, 1755, 2509]** (~30%
swing) across three fixed-song-time boots (evidence: `evidence/*validate*`, and the
attribution snippet). Because InitParticle is R4-isolated onto its private `part`
stream, those 754 draws **do not reach gRand** — so the gRand `postAnchorDelta` stays
near-matched (16/0/16) — **but the particle-emission COUNT/timing itself still varies
boot-to-boot.** BOOTRNG named particle/pyro-FX phase (and swept-light position) as the
WHITE driver; therefore:

> **Stream-matching (the R4 ledger PRIMARY) does NOT collapse the WHITE render
> variance.** The WHITE phenomenon lives on the particle-emission-COUNT / async-
> completion-order axis — exactly the `callerOrder` axis R4 reports as 1/10 and
> explicitly leaves non-gating (LEDGER.md), owned by **W0.3d part-b** (loader/worker
> completion-order determinism), NOT by the gRand-stream seam.

Corroboration: same-arm boots render **NEARBLACK / NEUTRAL** (hi 0 / 12) at fixed
songMs 21005 despite near-matched gRand stream; `[WASHPROBE] SCENE engaged` frame
counts swing 19898–27938 across boots (the render timeline length itself varies). This
is a *sharper* result than Wave-10's "boot RNG": the residual is a NAMED, non-gRand
axis with a designated owner.

---

## Item 2 — wash per-FX co-sampling instrument (`wash_cosample.py`) — VALIDATED

Co-samples PER FRAME, in one seam-pinned boot: particle-emission draws (addr2line-
attributed InitParticle/CreateParticles/CheckBursts, `RB3_LOADDET_ATTRIB`) + swept-
light phase (`[BOOTRNG] LIGHTVAL valhash` distinct-count) + hi_frac.

**lint 3 (known-good/known-bad separation) — PASS.** Natural-venue frame-burst (89
frames, hi 0.9–40.9, both classes): the FX-emission signal SEPARATES BAD (WHITE,
hi≥15, n=7) from GOOD (hi≤5, n=82): `fx_emit_win` mean_bad **1290** vs mean_good
**23264**, **AUC=0.000, p=0.058** — a clean *inverse* separation (WHITE frames have
LOW particle emission). `light_changes_win` did NOT separate (AUC 0.423). **Finding:**
the co-sampler works, and its result points the WHITE spike at the **swept-light /
non-particle** sub-axis (WHITE frames are particle-LULLS, not bursts) — consistent with
BOOTRNG's "swept point-light position hitting the camera" candidate over the particle-
burst candidate. Caveat: n_bad=7, p marginal; the eng_hot boot (below) whites the whole
song so cannot supply GOOD frames. Evidence: `evidence/wash_natural.json`.

**lint 8 (guard branch-entry hit-count) — recorded.** eng_hot guard OFF vs ON boot:
guard branch-entry hits **OFF=30895, ON=31007** (guard code provably ran); co-sampled
mean hi_frac OFF **48.69** → ON **44.04**. Evidence: `evidence/wash_enghot_lint8.json`.

---

## Item 1 — WHITE guard re-grade (`white_regrade.py`) — N=10/arm, RUN

`eng_hot`, target songMs 20900 (the sweep-located WHITE spike), input-free post-anchor,
first-frame-crossing capture, seam + attrib on every boot, ledger graded from the exact
boots' own logs. Evidence: `evidence/wr_n10.json` (+ per-arm ledgers).

| arm | ledger stream | postAnchorDeltas | hi_frac mean (sd) | mid_sat | WHITE | guard hits |
|---|---|---|---|---|---|---|
| OFF | **1/10 FAIL** | [16,0,0,0,0,0,0,0,0,0] | 12.43 (7.76) | 0.306 | 1/10 | 19.9k–26.9k |
| ON  | **10/10 PASS** | all 0 | 27.20 (20.46) | 0.238 | 4/10 | 25.7k–29.7k |

### Formal verdict: **VOID** (per A2/F6 — the harness refused)

The OFF arm fails the ledger precondition on the exact measurement boots (stream 1/10:
one boot drew +16 gRand draws post-anchor). Per A2 verbatim the measurement is VOID —
no WHITE verdict is emitted; the failing boot was NOT discarded-and-rerun and NOT
averaged in. The +16 is fully attributed (`evidence/venue-path-divergent-consumers.md`):
`CharClipDriver::CharClipDriver`(8) + `WorldCrowd::OnIterateFrac`(7, the Crowd
Fisher-Yates, liveness now PROVEN) + `CharInterest::ComputeScore`(1) — venue-path
consumers R4's M1 (default-path attribution) never saw, hence never isolated. This is
the A2-mandated seam-regression-style finding with axis (stream) + counts.

### What the run nonetheless proves (the R4 discrimination, PLAN-R4 risk 6 verbatim)

The ON arm is a clean, ledger-PASS 10/10 stream-matched sample — and on those PROVEN
stream-matched boots hi_frac still spans **9.5 → 65.4** (sd 20.5, WHITE 4/10, mid_sat
on the WHITE boots 0.049/0.097/0.100 = the zero-chroma white-lit wash). A persisting
spread on stream-matched boots **cleanly indicts a non-RNG axis**: the particle-
emission COUNT/timing (InitParticle [2505,1755,2509] under the seam) = the callerOrder
axis (1/10 info, non-gating) owned by **W0.3d part-b**, NOT gRand position. The
Wave-10 "boot RNG" story is now decomposed: gRand-stream noise (closed by R4) vs
render-timeline/emission-count noise (open, named owner) — the discrimination the
campaign lacked.

Secondary observations (reported, not verdict-bearing under VOID):
- reproduce-first would ALSO fail on this trajectory: mean(hi_frac|OFF) = 12.43 < 15
  (WHITE fires 1-2/10 as narrow per-frame spikes; the frame-burst sweep shows hi up to
  81 at this same song-time). Expression is stochastic ACROSS stream-matched boots, so
  an `RB3_LOADDET_SEED` trajectory knob alone cannot pin it — the emission-count axis
  dominates expression.
- ON>OFF hi_frac (+14.8) cannot be guard-caused (`compressHighlightsLuma` is monotone
  luminance-reducing, source-proven Wave-10) — it is the same render-axis draw-luck,
  now visible at PROVEN-matched stream. Guard branch-entry hits 25.7k–29.7k per ON
  boot (lint 8: the guard code ran).

---

## Verdicts (summary)

- **WHITE re-grade: VOID → HELD substrate-blocked, with numbers.** The A2 ledger
  precondition fails on the OFF arm (stream 1/10, fully attributed to four named
  un-isolated venue-path consumers); and even ledger-PASS stream-matched boots express
  WHITE stochastically on the emission-count axis. The guard stays landed default-OFF.
  **Named follow-ups (all outside Lane W's writable set, coordinator items):**
  1. extend R4 consumer isolation to the venue path — `CharClipDriver.cpp:62`,
     `Crowd.cpp:1234`, `CharInterest.cpp:172`, `LightPresetManager.cpp:286` (makes the
     ledger precondition satisfiable on eng_hot);
  2. **W0.3d part-b** (async completion-order determinism) — the axis that actually
     drives WHITE expression at fixed stream;
  3. `RB3_LOADDET_SEED` (the A3-named knob) is recorded but subordinated to (2):
     expression varies at fixed trajectory, so a seed-search cannot pin it alone.
- **wash instrument: BUILT + VALIDATED (lint 3) + lint-8 recorded.** FX-emission
  signal separates known-BAD from known-GOOD (AUC 0.000, p 0.058, inverse) — WHITE
  frames are particle-LULLS, pointing the spike at the swept-light/non-particle
  sub-axis; swept-light valhash-change count did not separate (AUC 0.42) — next
  instrument iteration should co-sample light POSITION amplitude, not value-hash
  change count.

## Notes / constraints honored
ELEVEN defaults ON; refuted flags UNSET; NO default flips / pin bumps / engine edits;
never staged `FxSendNative.cpp` or `rb3_session_trace.cpp`; pgid-only cleanup; own build
dir; evidence committed.

---

## CORRECTIONS (Wave-18 close-out review `WAVE18_CLOSEOUT_REVIEW.md`, F1-F3 — SUPERSEDE the claims above)

- **F1 (HIGH): the wash co-sampler "VALIDATED, AUC 0.000, WHITE=particle-lulls" claim is
  WITHDRAWN.** The review refuted it from `evidence/wash_natural.json` itself: the 89
  "per-frame" samples collapse to **2 covariate clusters** (the join is per-BURST-stale, not
  per-frame); all 7 BAD frames share `fx_emit_win=1290` with 53/82 GOOD frames; the AUC 0.000
  is a tie-handling artifact in `wash_cosample.py:127` (argsort ranks without midranks —
  midrank-corrected AUC ≈ 0.32; the identical p=0.058 for both signals betrays the
  degeneracy). Status downgraded to: **instrument BUILT, validation FAILED** — v2 must fix
  the per-frame join AND use midrank AUC before any separation claim. The particle-lull
  hypothesis is unproven, not disproven.
- **F2: the graded N=10 came through `--refinish` crash recovery** — all 20 rows have
  `frame/songms: null`, so the first-frame-crossing property is unverifiable for this run;
  all-black frames (luma 0.0) were counted as hi_frac=0 lighting data. Reproduce-first
  12.43 → 13.81 excluding them; the VOID/HELD verdict is unchanged either way.
- **F3: "+16 fully attributed" overstates by ±1** — the per-site deltas sum to +15, and this
  STATUS vs the evidence md disagree on the +1's owner (CharInterest vs LightPresetManager).
  The four named venue-path sites stand; the exact split of the residual +1 does not.
- (Unchanged by the review: the VOID→HELD verdict, the four venue-consumer attributions as a
  set, and the ON-arm 10/10-stream-matched-yet-spreading discrimination — the wave's core
  finding. Naming note per F9: "W0.3d part-b" here means the FRAME-ASSIGNMENT TIMING axis —
  R4's ledger `order` axis is already 10/10.)
