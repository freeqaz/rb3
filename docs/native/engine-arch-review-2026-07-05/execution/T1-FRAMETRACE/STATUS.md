# T1-FRAMETRACE — STATUS (Wave-19, Lane F, IMPLEMENTER)

**Tool:** T1 frame-timeline tracer (ATTRIBUTION mode) + wash co-sampler v2.
**Verdict:** DONE (all six milestones; gate-2 is an honest-negative attribution result — see M4).
**Base:** rb3 HEAD `14f96575` at start; engine pin `beb89e5` (NO pin bump, NO default flips).
**Build:** `native/build-T1` (reflink of build-native, path-rebased) — used the one stable
binary for every boot to isolate from sibling-lane engine churn.

> **NAMING BOX (review F9).** Everything here targets the **FRAME-ASSIGNMENT TIMING**
> axis — *which frame* async work lands on and *what each frame's consumers emit*. This is
> NOT R4's ledger `order` axis (completion SEQUENCE), which already PASSES 10/10 and was left
> byte-untouched (verified: `ledger_for_arm`/`order_sig` not in the diff; 710 `complete`
> lines still parse to 710 completes).

---

## Amendment adoption (PLAN_REVIEW R1–R6, all BINDING — how each was applied)

- **R1 (HIGH) — OFF-arm all-three-axes-RED (the binding fail-red 2).** Added the OFF-arm run
  (`--off-arm`, `RB3_FIXED_CLOCK=1`, `RB3_LOAD_DETERMINISM` unset, `JITTER=200`, N=3 eng_hot).
  All THREE axes DIVERGE (`frameAssign`/`songClock`/`emitTimeline`), confirmed over a COMMON
  frame window `[0,2800]` so it is genuine frame-assignment divergence, not capture length.
  `evidence/gate1-offarm-eng_hot.json` + `gate1-offarm-common-window.json`.
- **R2 (MED-HIGH) — gate-2 OFF-arm, carrier per arm.** Ran gate-2 on the OFF-arm (seam unset)
  and pre-registered the carrier (`frameAssign` named-move + `songClock` envelope). Added
  `songclock_envelope_test` (noise-robust). Result is an HONEST-NEGATIVE (M4).
- **R3 (MED) — refused superset + covariate/outcome sets.** wash v2 defines COVARIATES
  `{fx_emit_win, light_changes_win, songms, frame, light_pos_amp}` (distinct-gated) vs
  OUTCOMES `{hi_frac, mean_luma, class}` (exempt). Fail-red asserts `refused ⊇
  {fx_emit_win:2, light_changes_win:2}` (superset, not equality). Proven: refused was
  `{fx_emit_win:2, light_changes_win:2, songms:2, frame:2}`.
- **R4 (MED) — post-RunOneFrame A-2 verified, VOID discipline.** songMs sampled at the clean
  `RB3HttpServerPoll` site (A3 opt-b; never touches the dirty `rb3_session_trace.cpp`), keyed
  on `gRB3TraceFrame`. Docs say post-RunOneFrame (not the stale "pre-Draw"). Grader VOIDs a
  boot with all `frame=0` (mis-armed) or 0 positive songms; VOID never PASSes.
- **R5 (MED) — runnable exits.** M1(b) = grep-count 0 for `filearrive|songms` under
  PROBE-only + keyword-set equality (byte-diff reserved for the boot window). M6(c) = `off=`
  stable for a fixed set of syms across ASLR bases + grep proving no `pc=` in any sig/key
  (both verified below).
- **R6 (LOW) — disclosures.** `filearrive` covers ONE of three DoneLoading entries (open-fail
  `:924` and LoadStream `:1016` are known-uncovered); `FlushAttrib` emits `frame-1`
  (end-of-frame attribution); line drift noted. All folded into code comments.

Also honored the pre-dispatch WAVE19_REVIEW A2/A3/A5/A6 (seam rename, songMs source, Rand.cpp
= F-only and left unmodified, wash_cosample → F). `Rand.{h,cpp}` was NOT edited (emitTimeline
derives from the existing per-PC attrib tap).

---

## Milestones + evidence

**M1 — markers + flag (DONE).** `RB3_LOADDET_TIMELINE` (default-OFF) gates two new rb3-side
markers in `rb3_loaddet_probe.cpp`: `[LOADDET] filearrive frame=N gdraw=M name=F` (FileLoader
ReadDone→DoneLoading flip, `Loader.cpp` one HX_NATIVE call) and `[LOADDET] songms frame=N
ms=X.X` (`RB3HttpServerPoll`). Verified: eng_hot boot emits filearrive=12, songms=4393 (2023
distinct ms, -1..29152); PROBE-only + plain boots emit ZERO new keyword lines (keyword set ==
baseline). classjson row appended (engine).

**M2 — three axes + `--timeline` grader (DONE).** `loaddet_gate.py --timeline`. `frameAssign`
(`kind:name@frame` + `file:name@frame`), `songClock` (first-frame ladder over a fixed 2000ms
grid), `emitTimeline` (per-frame per-`off` draw sequence, module-offset keyed). Each sig
DISJOINT from `order_sig`. Grader is a pure function (re-grading a log twice = byte-identical),
`allow_nan=False`. Naming-box proof: `ledger_for_arm`/`order_sig` byte-untouched, 710==710
completes.

**M3 — gate-1 (DONE, GREEN).** N=6 seam-ON eng_hot `JITTER=200`: `emitTimeline` DIVERGE (6
distinct sigs, 34099–36801 events), `songClock` DIVERGE (6 distinct, 4711–4967 positive
samples), `blind=False`. The frame-assignment timing axis is UNPINNED by the R4 stream-
position seam — the T1 residual is real and the instrument detects it. Plus the R1 OFF-arm
(binding fail-red 2): all three axes RED. `evidence/gate1-eng_hot-seam-on.json`.

**M4 — gate-2 injection (DONE, HONEST-NEGATIVE).** OFF-arm differential, controls `JITTER=0`
vs injected `JITTER=2000` and `20000`. **DOSE-INDEPENDENT HONEST-NEGATIVE:** neither dose
moved frame-assignment beyond the ambient control envelope (injected `songClock` crossings sit
at the CENTER of controls, 0/4 checkpoints outside; controls disagree on 177/457 names at
`JITTER=0`). **Attribution for T3:** the frame-assignment timing residual is driven by
**ambient thread scheduling, NOT the `RB3_LOADDET_JITTER` knob** — T3 must not rely on jitter
to reproduce/pin it. The instrument correctly REFUSES to manufacture a mechanism-response
where none is distinguishable (lint 3); its real responsiveness is established by gate-1.
`evidence/gate2-injection.json`.

**M5 — wash v2 + fail-red (DONE).** Rewrite of `wash_cosample.py`: per-frame join, ≥5-distinct
covariate refusal, midrank AUC (full-tie → 0.5 not the v1 0.0), light-position amplitude last,
capture_lints wired (real Lane I module). **Fail-red (3) PROVEN:** `--regrade wash_natural.json`
→ verdict `DEGENERATE`, `instrument_validated=False`, `refused` superset (exit 0).
**Live natural-venue re-run:** honest `DEGENERATE` disclosure with counts (NOT a silent pass) —
the upstream `r4m4_capture.multi_capture` sweep captured 37 shots at only 2 distinct song
positions (same collapse as the committed red baseline; an upstream Lane-W capture-loop
residual, not the join). The v2 correctly catches the degeneracy v1 falsely validated.
`evidence/wash_v2_regrade_refusal.json` + `evidence/wash_v2_live.json`.

**M6 — inertness + G3 + PIE + closeout (DONE).**
- **G3 Wii-match:** `batch_objdiff` on the touched symbol `LoadFile__10FileLoaderFv` = 100%/100%
  == baseline (HX_NATIVE inert; DoneLoading also 100/100). No regression.
- **G2/G4 flag-OFF inertness:** plain boot → 0 timeline lines; `drawlog-golden.py --scene
  splash_screen --fixed-clock --canonical-order` → **PASS, 792 draws** match the golden
  flag-OFF (my surfaces don't touch draws).
- **G6 PIE-stable keys:** (i) `off=` keys shared 61/61 across two boots while all 61 raw `pc=`
  are DISJOINT (ASLR moved the base); `off=` resolves offline via addr2line to real functions
  (DataRandomElem, HxGuid::Generate). (ii) grep proves no axis sig references raw `pc`
  (`timeline_sigs`/`grade_timeline`/`injection_differential`/`songclock_envelope_test` all
  pc-free).
- **rb3-tests:** 112/122 PASS. The 10 failures are GPU-device tests (TexSharpen×7,
  DrawLogGolden.PopulatesFromRealDrawMesh, WgslValidation×2) — the pre-existing host-GPU
  bounded-boot **teardown SIGSEGV** (documented W0.3.STATUS S1; the same rc=-11 the
  drawlog-golden capture tolerates). PROVEN flag-inert: the failing set is IDENTICAL with
  `RB3_LOADDET_TIMELINE=1` vs unset (10/122 both ways). My lane introduced ZERO new failures.

---

## Collision handled

`native/src/rb3_http_handlers.cpp` was co-edited by T2-WORLDROI (HandleDrawLog boneRects hunk)
— a different function from my songMs hunk. Staged ONLY my hunk via `git apply --cached`;
never staged T2's work. Engine classjson committed alone (never staged the dirty
`FxSendNative.cpp`). `rb3_session_trace.cpp` never touched.

## Files owned / changed

- `native/src/rb3_loaddet_probe.cpp` — new flag + markers
- `src/system/utl/Loader.cpp` — one HX_NATIVE `filearrive` call (G3-neutral, LoadFile 100%)
- `native/src/rb3_http_handlers.cpp` — one `RB3LoadDetSongMs` call (clean file)
- `scripts/native/loaddet_gate.py` — `--timeline` (3 axes) + `--injection` + envelope test
- `execution/R4-M4/wash_cosample.py` — v2 rewrite
- engine `src/platform/NativeCompatFlags.classification.json` — `RB3_LOADDET_TIMELINE` row
- `Rand.{h,cpp}` — owned per A5, **left unmodified** (emitTimeline from existing tap)

---

## ERRATA (Wave-19 close-out review `WAVE19_CLOSEOUT_REVIEW.md` F1/F3 — supersede the wording above)

- **Gate-2 "DOSE-INDEPENDENT" is overstated.** The evidence is n=1 injected boot per dose vs 3
  controls (5 boots total), and shows a monotone +~100-frame shift 2000→20000 at all 4
  checkpoints (inside the ambient envelope). The defensible claim: **jitter is proven NOT
  NECESSARY for the frame-assignment residual** (control-arm divergence at JITTER=0: 177/457
  name disagreement + gate-1 6/6 seam-ON divergence) — it is NOT proven to have no effect.
  The T3 consequence is UNCHANGED (the residual exists without injection; T3 must pin ambient
  actors, and jitter-reproduction is a NO-GO route). The jitter knob itself is live on the
  OFF-arm (`ThreadCall_Native.cpp:38-52` fires at `WorkerMain:99`; the worker carries
  DataLoader parses, `DataFile.cpp:786`).
- **Test-count scope note:** the "10 pre-existing GPU teardown failures" are
  ENVIRONMENT-CONDITIONAL (same suite reads 116/0 on the final pin in the other Wave-19 lanes
  and the coordinator's close-out run). The flag-inert A/B (identical failure set ON vs OFF)
  remains valid.
