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

## M2 — Isolate the M1-named consumers + ledger harness: **DONE — PRIMARY collapses at N=4**

### Mechanism (design note / deviation from plan §3.2, documented)
The plan's §3.2 per-call `RB3DetRandomInt(tag)` wrapper would need 26 fragile one-line edits in
`InitParticle` alone (missing one silently fails the gate) and would not cover the two *other*
callers of `InitParticle` (RunFastForward burst path, `RndParticleGen` via Gen.cpp). Instead I
implemented the SAME "isolate each consumer onto its own stream" intent with a **scoped redirect
guard** `RB3LoadDetRedirect _det("tag")` at each consumer's function entry: for the guard's
dynamic extent the shared-stream `RandomInt/RandomFloat` free functions draw from the per-tag
`RB3LoadDetStream` instead of gRand. One line per consumer, covers every caller and every nested
particle helper, nesting-safe. Seam-OFF → `RB3LoadDetStream` returns null → guard inert →
byte-identical (G3 confirms). All `#ifdef HX_NATIVE`.

### Sites (5 guards, all HX_NATIVE, tag → stream)
- `part`         → `RndParticleSys::InitParticle` (Part.cpp) + `RndParticleSys::CreateParticles`
- `camshot`      → `CamShot::Shake` (CameraShot.cpp)
- `chareyes`     → `CharEyes::NextLook` (CharEyes.cpp)
- `randgroupseq` → `RandomGroupSeq::PickNextIndex` (Sequence.cpp)

API in `src/system/math/Rand.{h,cpp}`: `RB3LoadDetStream(tag)` (lazy, seeded `0x5EED^fnv1a(tag)`,
reset at `RB3ReseedGRandAtAnchor`) + `RB3LoadDetRedirect` guard + `RB3_LOADDET_REDIR` wrapper tap.

### Result (`--attrib --arm both --n 4 --k 300 --jitter 200`, build-agent-R4)
- **ARM ON: postAnchorDelta spread == 0** (all 4 boots postDelta=0, reseedAll=True). **PRIMARY PASS.**
- ARM OFF: spread 4869 ([32358,32838,32902,37227]) — **FAIL-RED reproduced.**
- absSpread ON 3 vs OFF 4881 — even absolute gRand count is now near-invariant.
Evidence: `evidence/M2-verify-n4.json`, `M2-verify-summary.txt`, `M2-attrib-on-gate.json`.

### G3 — Wii-match inertness: **PASS** (`evidence/G3-match-inertness.md`)
`batch_objdiff` on all touched units == report.json baseline; directly-edited `RandomInt`/
`RandomFloat`/`CreateParticles` stay exactly 100%; consumers keep pre-existing sub-100 values.

## M3 — PRIMARY gate at scale + inertness + flag disposition: (in progress)

`--n 10 --k 300 --jitter 200 --ledger`. PRIMARY = ON-arm postAnchorDelta spread == 0, 10/10;
fail-red = OFF-arm spread > 0 under identical jitter. + G2 (flag-OFF 0 `[LOADDET]` lines) +
per-axis ledger. Coordinator (not lane) flips classjson at close-out.

## Notes / carried constraints

- ELEVEN defaults stay ON; refuted flags UNSET; pgid-only cleanup; frame-count settling.
- NEVER stage the engine's uncommitted `FxSendNative.cpp`. No engine edits, no pin bump.
- `Wind::` refuted empirically (Wind.cpp:30 = 1024 fixed) and absent from the M1 table.
