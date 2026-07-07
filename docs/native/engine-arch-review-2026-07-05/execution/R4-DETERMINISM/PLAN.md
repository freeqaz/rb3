# R4-DETERMINISM (Wave-17 Lane L) — execution PLAN

**Executes:** `RETROSPECTIVE/plans/PLAN-R4-loader-determinism.md` (Fable-authored), with
the WAVE17_KICKOFF acceptance (A1-A6) and `plans/INDEX.md` COORDINATOR RESOLUTIONS applied,
and the OPTIONS.md §4 ten process lints binding.

**Engine pin:** `51640ff`. **Flag:** `RB3_LOAD_DETERMINISM` (already landed PARTIAL/opt-in,
DO-NOT-flip). **No default flips, no pin bumps** (coordinator close-out only).

## Objective (unchanged from PLAN-R4 §1)

Make the landed `RB3_LOAD_DETERMINISM` seam *sufficient*: 10/10 boots with identical
post-anchor gRand stream position under contention + jitter (the PRIMARY that failed in
Wave 12 A-S2), by isolating the *measured*-divergent variable-count gRand consumers onto
per-tag isolated `Rand` streams. Ship a standing per-axis (count/order/stream/clock)
PASS/FAIL ledger. Every change `RB3FixedClockActive() && RB3LoadDeterminism()`-gated inside
`#ifdef HX_NATIVE`, default-OFF, flag-OFF byte-identical.

## Isolation / build note (concurrency)

- rb3 source edits in worktree `.claude/worktrees/wave17-R4-loaddet`
  (branch `wt-wave17-R4-loaddet`, from master HEAD — all probe infra committed at HEAD).
- **The shared engine tree carries Lane U's uncommitted provenance-sidecar WIP
  (`ProvNotePassOpen`/`mDrawProv`/…) which does NOT compile.** R4 touches NO engine source
  (the plan's only engine touch — the classjson flag text — is coordinator close-out work),
  so R4 builds against an isolated CLEAN engine worktree `milo-engine-r4-clean` (detached at
  pin `51640ff`). Never stage any engine file.
- Native build dir: `native/build-agent-R4` (own dir; no shared-build-lock contention).

## Milestones (PLAN-R4 §4)

- **M1 — Attribution.** Build the `RB3_LOADDET_ATTRIB` tap (caller-PC per gRand draw,
  per-frame flush). Run OFF-arm N=4, jitter 200µs, input-free, in BOTH windows (boot 0..120,
  gate anchor..anchor+300). Produce the ranked divergent-caller table.
  GO if ≤ ~8 named divergent variable-count call sites; NO-GO (>8 diffuse) → re-price
  (walk-granularity isolation or ledger-only landing).
- **M2 — Isolate + ledger harness.** `RB3LoadDetStream(tag)` + Det wrappers; one-line
  flag-gated reroutes at the M1 sites (Dir.cpp-template); promote `loaddet_gate.py` to
  `scripts/native/` with `--attrib`/`--ledger`. Per-site monotonic spread shrink; back out
  dead sites (G6). Exit: N=4 ON-arm `deltaSpread == 0`.
- **M3 — PRIMARY at scale + inertness + ledger doc.** `loaddet_gate.py --n 10 --k 300
  --jitter 200 --ledger`: ON spread==0 10/10 all four axes PASS; OFF fail-red spread>0;
  G2/G3 inertness. Commit the per-axis ledger doc + regenerable script + evidence.
  Coordinator flips classjson disposition.
- **M4 — cash-in (coordinator-gated on PRIMARY).** WHITE-guard re-grade validity substrate +
  wash per-FX co-sampling charter. NOT started unless M3 PRIMARY passes.

## Gates (PLAN-R4 §5): G1 PRIMARY, G2 flag-OFF inertness, G3 Wii-match inertness (trivial —
all edits HX_NATIVE), G4 ledger axes real, G5 re-grade validity, G6 no dead flags.

## Instrument design (M1)

`RB3_LOADDET_ATTRIB=1` (implies probe). Static-init flag `gRB3LoadDetAttribOn`; the four
`Rand.cpp` free-function wrappers do a single `if (gRB3LoadDetAttribOn)
RB3LoadDetAttribRecord(__builtin_return_address(0))` — flag-OFF is one predicted-not-taken
branch, no call. `rb3_loaddet_probe.cpp` accumulates a per-frame `(caller-pc)->draws` map and
flushes one line per caller at each frame boundary: `pc / off (pc - dli_fbase) / dladdr sym /
mod / draws`. Caller identity keyed by `(mod, off)` — invariant under PIE ASLR. Offsets
resolved to exact `file:line` OFFLINE via `addr2line -e rb3-native` (robust to static/inlined
functions dladdr can't name; validated: `0x68a479 -> RndWind::Init Wind.cpp:30`).
