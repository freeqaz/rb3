# R4 — Loader determinism + per-axis ledger — STATUS

**Lane L (KEY=R4-DETERMINISM), Wave 17.** Plan: `RETROSPECTIVE/plans/PLAN-R4-loader-determinism.md`.
Worktree `wave17-R4-loaddet`, build dir `native/build-agent-R4`. Engine pin `51640ff`
(UNCHANGED — no engine edits this lane; classjson flip is coordinator close-out).

All changes stay behind the existing opt-in `RB3_FIXED_CLOCK && RB3_LOAD_DETERMINISM` gates
inside `#ifdef HX_NATIVE`. NO new default-ON behavior. Flag-OFF byte-identical.

## M1 — Attribution (name the real divergent consumers): **DONE — GO**

- Instrument: `native/src/rb3_loaddet_probe.cpp` (attrib aggregation + per-frame flush,
  `RB3_LOADDET_ATTRIB=1`, implies `RB3_LOADDET_PROBE`) + four `src/system/math/Rand.cpp`
  free-function wrappers tapped via `RB3_LOADDET_ATTRIB_TAP()`
  (`__builtin_return_address(0)`, captured in-wrapper). Flag-OFF = one predicted-not-taken
  branch, no call → normal boots and the M2/M3 gate arms are untouched.
- Wrapper completeness RE-VERIFIED (plan §3.3/§4): `grep 'gRand\.' src/` shows **no**
  member-call bypasses outside `Rand.cpp`; `Gaussian` is Wind-local only. The wrapper tap is a
  complete tap on the shared stream.
- Harness: `scripts/native/loaddet_gate.py` (promoted from `execution/W0.3d-b/`, which is now a
  `runpy` pointer stub) with `--attrib` (M1 table, boot + gate windows, addr2line offline) and
  `--ledger` (M3 axes).
- Run: `--attrib --arm off --n 4 --k 300 --boot-window 120 --jitter 200`. OFF-arm
  postAnchorDelta = [33847, 34010, 32482, 37747], spread **5265** (fail-red reproduced).
- **Result (grouped by consumer function):** 4 divergent gate-window consumers —
  `RndParticleSys::InitParticle` (spread 5801, dominant), `CamShot::Shake` (63),
  `RndParticleSys::CreateParticles` (16), `CharEyes::NextLook` (6); + `RandomGroupSeq::PickNextIndex`
  in the boot window. ≤8 named sites → **GO**. Evidence: `evidence/M1-divergent-consumers.md`
  + `evidence/attrib-off-*.json`.

## M2 — Isolate the M1-named consumers + ledger harness: (in progress / see below)

Route each named consumer's gRand draws onto a per-tag isolated `RB3LoadDetStream` (seam-ON
only; seam-OFF → `nullptr` → normal gRand path, byte-identical). Tags: `part` (InitParticle +
CreateParticles), `camshot`, `chareyes`, `randgroupseq`. Per-site `--n 4` spread-shrink is
committed evidence; a site that moves nothing is backed out (G6, no dead flags).

## M3 — PRIMARY gate at scale + inertness + flag disposition: (pending M2)

`--n 10 --k 300 --jitter 200 --ledger`. PRIMARY = ON-arm postAnchorDelta spread == 0, 10/10;
fail-red = OFF-arm spread > 0 under identical jitter. G2 (flag-OFF 0 `[LOADDET]` lines) + G3
(`batch_objdiff` on touched match units byte-unchanged). Coordinator (not lane) flips classjson.

## Notes / carried constraints

- ELEVEN defaults stay ON; refuted flags UNSET; pgid-only cleanup; frame-count settling.
- NEVER stage the engine's uncommitted `FxSendNative.cpp`. No engine edits, no pin bump.
- `Wind::` refuted empirically (Wind.cpp:30 = 1024 fixed) and absent from the M1 table.
