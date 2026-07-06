# WASH-fix — Stage A.S1 (Wave 8): two-hypothesis venue-wash instrumentation

KEY=WASH-fix. Checkpoint id `A-S1`. Opus root-causer. Engine pin `a94762f`.
Lane A owns `Rnd_Wgpu_RB3.cpp` + `RB3PostProc.*` + WGSL — the probe lives entirely
in-fence.

## Goal

Root-cause the venue "wash" with per-boot instrumentation, proving/refuting the two
hypotheses from WAVE8_REVIEW A1 (the "one env-state bug, two directions" framing was
half-wrong):

- **H1 (PINK @ ms21000):** per-boot P4 venue-light **engagement miss / staleness**
  (`Rnd_Wgpu_RB3.cpp:1435` incl. the silent `mAmbientFogOwner==null` clause; DrawMesh
  pointer-equality staleness `:2453`) → the flat-default else path = the PINK base.
  Test: engagement-miss correlates **1:1** with the PINK class at the pinned ms21000
  shot.
- **H2 (GREY @ ms3000):** the grey needs the composite to **desaturate hot venue
  input** (NOT the grey-key fallback — refuted Wave 6). The composite intermediate is
  a UNORM target (`RB3PostProc.cpp:155`) so hot >1.0 lighting is clamped at write,
  then the grade pulls the flattened channels into the mid-tones as grey. Test: with
  the venue ENGAGED at ms3000, default (composite ON) mid-band saturation collapses
  vs the `RB3_PP_OFF` control (composite OFF).

## Subtasks

- **S1a (A7 reproduction):** confirm the matrix baseline signal on pin `a94762f`
  (reduced arms: pure-default + `venue_light_off`, `RB3_PP_LUMA_CEILING` UNSET,
  N≥4 each, ms21000±250). Expect venue_light_off = wash 8/8-class, default = low.
- **S1b (H1 instrumentation):** ≥8 default boots @ ms21000, parse the SCENE digest
  tail per boot → engaged/miss + class. Build the 1:1 correlation table.
- **S1c (H2 instrumentation):** default + `pp_off` sweeps @ ms3000 (W3.3
  grayscale-sweep harness, since capture_pinned cannot reach the 0-20s window),
  SCENE engaged state + PP unorm fact + per-tonal-band saturation (`tonal_band_sat.py`).
- **S1d:** decision-path file:line writeup → `STATUS.md`; commit probe + docs.

## Instrumentation (committed, engine `71469af`)

`RB3_WASH_PROBE=1` (registered probe flag, default-OFF, additive stderr):
- `Rnd_Wgpu_RB3.cpp` WriteSceneUniforms → `[WASHPROBE] SCENE env=.. engaged=.. miss=.. dl=.. pl=.. greykey=..`
- `Rnd_Wgpu_RB3.cpp` DrawMesh → `[WASHPROBE] STALE rewrite=env env=.. ptr=.. prevptr=..`
- `RB3PostProc.cpp` EnsureIntermediate → `[WASHPROBE] PP intermediate WxH fmt=.. (unorm=..)`

## Harnesses

- `wash_probe_run.py` — H1/A7 driver (reuses `wash-measure.capture_pinned`; ms21000).
- `W3.3/grayscale-sweep.py` — H2 sweep (ms200..25000; reaches ms3000).
- `tonal_band_sat.py` — per-tonal-band HSV saturation (H2 composite-desat test).

## Exit criteria

Per-boot evidence tables for H1 (engaged/miss vs class, ≥8 boots) and H2 (engaged +
composite input-vs-emit per tonal band); a file:line decision-path writeup that
proves or refutes each hypothesis. No fix this stage (S2 owns the fix).
