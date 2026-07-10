# W29-GAMEPLAY — PLAN

**Lane 2 of Wave 29.** Owner: Opus. Base `95df30f2` (probe/harness present).
Owned files (CA8): `CharIKHand.cpp`, `CharDriver.cpp`/`CharClip*.cpp` (Parts A/C),
`scripts/native/` additions. NOT world/vignette/PanelDir/ui (Lane 1). Protected list
per CA8. Priority order **A > C > B > D**; Part D stops while A-C gates unmet.

## Part A — PROP piece 3 (prop-tip clip-track binding)
Flag: existing `RB3_PROP_POSE_FULL` (HX_NATIVE, default-OFF). Acceptance (CA2,
verbatim): committed `analyze_prop_ab.py` prints `ACCEPTANCE (ON): PASS` (exit 0) —
strum/fret/right_hand **skip=0 and 0 dst>30u** — same harness/song/window
(`run_prop_probe.py beastandtheharlot 18`, reach clamp default-ON). Pre-fix baseline
(reproducing W28 right_hand residual) → `gameplay-prop3-baseline.json` BEFORE any edit.

## Part C — remove `RB3_CROWD_CLIP_KEEP` (E-C2 rider, MUST NOT survive un-ruled)
Five-site deletion (WAVE29_REVIEW.md A3, binding). Gates: grep==0, `CHARDRV_PLAY_BT`
grep==1, batch_objdiff `Play`==100.0 / `Poll`==93.54. `CHARDRV_*` probe blocks
byte-identical. Census/regen/pin coordinator-only.

## Part B — E6 vocalist-mic A/B (rider)
`--part guitar` runs (vocalist on stage regardless; CA1) with
`RB3_PROP_FINGER_BYPASS` / `RB3_PROP_POSE_FULL` A/B + `--member vocals` closeups,
frame-matched by songMs (CA6). MAY parameterize the E5 probe caps through existing
dbg envs (CA6, no new getenv). Verdict → `gameplay-micAB.json`: does piece (1) damage
the vocalist mic-hand pose (blocker CONFIRMED) or not (RETIRED)?

## Part D — user-directed bug sweep (findings only, NO fix code)
Bounded (CA7): ≤3 boot runs + ≤2 closeup runs; seeds FIRST (retail pair + 2nd angle
before any bug label); ≤6 full entries. Deliverable `SWEEP_FINDINGS.md` + log
anomaly grep table. Checkpoint `gameplay-sweep.json`.

## Gates (every code-touching part)
batch_objdiff on touched Wii fns == baseline; `drawlog-golden --fixed-clock
--canonical-order` 792 PASS (flag-OFF); `rb3-tests` 116/0; boot A/B flag-ON.
Checkpoints under `/tmp/wave29-checkpoints/gameplay-*.json` (check-first, buildSha
stamped). Raw stderr gzipped into `evidence/raw/`, committed.
