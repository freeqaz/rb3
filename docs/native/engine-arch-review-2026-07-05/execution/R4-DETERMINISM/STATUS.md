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

## M3 — PRIMARY gate at scale + inertness + flag disposition: **DONE — PRIMARY PASS 10/10**

`--arm both --n 10 --k 300 --jitter 200 --ledger` (build-agent-R4, input-free):
- **ARM ON: postAnchorDelta spread == 0, 10/10 boots** (distinctDeltas=[0], reseedAll). **PRIMARY PASS.**
- ARM OFF: spread **7179** (10/10 distinct, 32679…39858) — FAIL-RED reproduced (larger at N=10).
- **Per-axis ledger (`LEDGER.md`, `evidence/M3-ledger.json`): stream 10/10 (PRIMARY), count 10/10,
  order 10/10, clock 10/10; callerOrder 1/10 (informational, non-gating — particle-spawn timing
  still jitters on the now-PRIVATE streams, no longer reaching gRand).**
  - NOTE: the ledger's count/order/clock axes were corrected to anchor-relative / completion-only /
    span==k semantics (the pre-anchor absolute-gdraw, attrib-caller-timing, and exact-anchor-frame
    residues are non-gating by the seam's re-base-at-anchor design). Harness updated; this run's
    ledger recomputed offline from the saved ON-boot logs.

### G2 — flag-OFF inertness: **PASS** (`evidence/G2a-normal-boot.txt`, `G2b-drawlog.txt`)
Normal boot (no `RB3_LOADDET_*`) emits 0 `[LOADDET]` lines; flag-OFF splash_screen drawlog
matches the committed golden (792 draws).

## Disposition / hand-off to coordinator
- All lane milestones M1→M3 done; PRIMARY (the gate that failed 3× in Wave 12) now resolves.
- `RB3_LOAD_DETERMINISM` stays **opt-in**. The classjson PARTIAL/DO-NOT-flip → PASS-PRIMARY flip
  is **coordinator close-out** (plan §M3), gated on the flag-ON gameplay-scene drawlog re-golden.
  NOT done by this lane (no engine edits, no default flips, no pin bump).
- M4 (WHITE re-grade + wash per-FX co-sampling dispatch) is unblocked: the seam now delivers
  stream-matched boots (ledger PROVES 10/10), so any residual WHITE spread cleanly indicts a
  non-RNG axis. M4 itself is a follow-on lane, not R4's deliverable.

## Commits
- `937e4194` M1 — attribution instrument + promoted harness (GO)
- `63df7f93` M2 — isolate consumers, PRIMARY N=4 spread 0 + G3
- (M3 commit below) — N=10 PRIMARY + ledger + G2

## Notes / carried constraints

- ELEVEN defaults stay ON; refuted flags UNSET; pgid-only cleanup; frame-count settling.
- NEVER stage the engine's uncommitted `FxSendNative.cpp`. No engine edits, no pin bump.
- `Wind::` refuted empirically (Wind.cpp:30 = 1024 fixed) and absent from the M1 table.
