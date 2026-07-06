# W0.6b — STATUS

Item: classify the 91 game-root `Unknown` NativeCompat flags into
`probe|workaround|feature|perf`. Lane B (parallel to Lane A). Engine pin `609efb7`.

Planner (Opus) seeded this file + `PLAN.md` + `census-snapshot.json`. Implementers append one
`## <subtask-id> — done|partial|blocked` section per subtask under `flock /tmp/rb3-docs.lock`,
with the staging-file path / commit SHA. S4 appends the final 91-row flag→class→reason table +
verify log.

Fixed facts (planner-verified 2026-07-06):
- 91 game-root flags, all `FlagClass::Unknown`, none in `classification.json` (join-verified).
- Partition: S1 = 23 char/skinning behavior; S2 = 24 ui/render/audio/loader/synth/world behavior +
  perf/value + `MILO_HEADLESS`; S3 = 44 probes. 23+24+44 = 91, zero overlap (see PLAN.md).
- Scanner already covers `rb3/src/system` (`a537c2a3`); `check` already exits 0 at 318 flags — the
  gap is classification, not scanning.
- D2: NO `gen` regen by this lane; coordinator regens `gen.inc`/ledger once at wave end.

## (pending implementers)

## W0.6b.S1 — done
Staging: `execution/W0.6b/classified-S1.json` (23 rows). Commit `92a53bcb`.
All 23 flags re-scanned (fresh `native_compat_census.py scan`), confirmed present + `Unknown`,
classified by reading each call site (READ-ONLY on src/). Verify: parses; count==23; keys==S1 set;
classes all valid. Histogram: 15 workaround / 6 probe / 1 perf / 1 feature.

DEVIATION (site-verified, per PLAN "verify at call site, do NOT classify from the name"): the
name-guidance lumped the whole `RB3_NO_*` family as opt-out workarounds-default-ON, but the call
sites split into TWO kinds:
- **probe** (default-OFF debug/bisection *disable* of a faithful subsystem that runs by default,
  `g=getenv?1:0; if(g)return`): `RB3_NO_CLIP` (CharDriver::Poll), `RB3_NO_DEFORM`
  (SetDeformation), `RB3_NO_FACE` (CharFaceServo+CharHair::Poll), `RB3_NO_IK` (all 10 IK Poll
  sites), `RB3_NO_POSEMESHES` (CharBonesMeshes::PoseMeshes). Absent = faithful; setting = study.
- **workaround** (opt-out of a native hack that IS default-ON): `RB3_NO_SKEL_REBIND`,
  `RB3_NO_CROWD_REBIND`, `RB3_NO_HEAD_REBIND`, `RB3_NO_INST_REBIND`.
- **workaround / faithfulStatus:live** (native now does the FAITHFUL load by default; flag is an
  escape-hatch opt-out that re-enables a legacy hack): `RB3_NO_DEFORM_LOAD` (deforms load),
  `RB3_NO_HEAD_SHAPER` (head shapes load, CharLoad5b byte-correct).

Other calls: `RB3_SKIN_RTT`=feature (gates broken engine RTT composite; default-OFF direct-bind
bypass ships, C8 memory); `RB3_SKIN_NOCACHE`=perf (cache disable, measurement); `RB3_SKIN_TIMING`=
probe (timing print); `RB3_SKEL_REBIND_FULL`/`_CALCOFF`/`SET_SKEL_REBIND`=workaround default-OFF
study/variant knobs (each selects a rebind behavior when set); `RB3_MESH_FREE`=workaround default-ON
(native keeps mesh CPU data); `RB3_WALKON_SNAP_OFF`=workaround default-ON; `RB3_BOUND_REBAKE`/
`RB3_HANDS_BIND_FIX`=workaround default-OFF experimental; `RB3_INST_STRINGS_MODE`=workaround
read=value (rigid/rebake mode); `RB3_SKIN_FIX_OFF`=workaround default-ON (coupled to RB3_SKIN_RTT).

No src/ edits. classification.json NOT touched (S4 merges). No gen regen.
