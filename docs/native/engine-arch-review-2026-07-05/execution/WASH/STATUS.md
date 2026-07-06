# WASH — Stage A.S3: the 5-config wash isolation matrix

Lane A (Wave 7). Owner: A.S3 (Opus). Goal: on the W3.3-fixed build, run the wash
isolation matrix (N≥6 boots/config, songMs-pinned 21000±250, placement contract at
DEFAULT in every config) to (1) answer whether the W3.3-fix (`RB3_PP_LUMA_CEILING`)
collapsed the stochastic venue-cam wash rate, and (2) name the residual mechanism.

Prior (carried in): W3.3-fix was **refuted as a colorization fix** by A.S2 (rb3
`770675b7`) — `RB3_PP_LUMA_CEILING` only engages highlight pixels (L>0.82); the wash
lives in sub-knee mid/low tones + a full-frame PINK env cast the ceiling cannot
touch. Landed default-OFF, safe (flag-OFF byte-identical). So config-1-vs-config-5
is expected to show **no collapse** — this matrix confirms that quantitatively and
isolates the real driver.

## Configs (all with placement contract DEFAULT-ON — no contract env pinned)

| # | key | env | role |
|---|---|---|---|
| 1 | `luma_on` | `RB3_PP_LUMA_CEILING=1` | W3.3-fix ON baseline |
| 2 | `highway_bloom_off` | `RB3_PP_LUMA_CEILING=1` + `RB3_HIGHWAY_BLOOM_OFF=1` | gem/now-bar halo (game.cam-gated) |
| 3 | `bloom_off` | `RB3_PP_LUMA_CEILING=1` + `RB3_BLOOM_OFF=1` | composite bloom term |
| 4 | `venue_light_off` | `RB3_PP_LUMA_CEILING=1` + `RB3_VENUE_LIGHT_OFF=1` | per-environ SceneUniforms venue lighting |
| 5 | `control_w33_off` | (none) | **CONTROL: W3.3-fix OFF, else default** |

`RB3_TRACK_LIGHT_OFF` was **not** substituted: config 2 (highway bloom, game.cam) and
config 3 (composite bloom) are not redundant (different passes), so all four kickoff
isolation flags are kept. Track-light is a game.cam highway relight and, like the
game.cam-gated halo, is a priori irrelevant to these venue-cam shots.

## Method / tooling

- **`scripts/native/wash_matrix.py`** (this stage) — 5-config driver. Reuses
  `wash-measure.py`'s proven `capture_pinned` primitive verbatim (songMs-pinned wide
  `coop_dir_crowd` venue shot; boots overshooting the window are discarded + re-run).
  **Round-robin interleaving** (round r = one boot of each config, repeat N rounds)
  controls for machine-state / thermal / cache drift over the ~35-min run — the same
  discipline the Wave-6 S2 A/B used, generalized to 5 arms. Each capture scored by
  `wash_score.score_image()` → `PINK|WHITE|NEARBLACK|NEUTRAL`; wash = PINK or WHITE.
  No early-stop (every config needs its full N to compare rates). Writes
  `measure/batch_log.json` (crash-safe, after every capture), `measure/matrix.json`
  (per-config `wash_rate` + class histogram + luma/pink lists), `measure/montage.png`.
- Build: `native/build-agent-WASH/rb3-native` (clang/Debug, engine HEAD `af4a22a`;
  all 5 flags live, placement contract default-ON verified). `RB3_FIXED_CLOCK=1`.
- Run: `python3 scripts/native/wash_matrix.py --bin native/build-agent-WASH/rb3-native
  --raws /tmp/wash-matrix-captures --out .../WASH/measure --n 8`

## STATUS: measurement IN PROGRESS (round 1 of 8 captured at handoff)

The matrix run is executing in the background (~40 boots × ~40-50s ≈ 30-40 min). At
the turn-budget handoff only round 1 (N=1/config) was scored. The detached run
(`nohup`, `start_new_session`) continues autonomously and will land the complete
`measure/matrix.json` + `montage.png` on disk when done. **Finalize by reading
`measure/matrix.json`** and filling the results table below — no re-run needed if
`matrix.json` exists.

### Round-1 preliminary (N=1/config — NOT a rate, directional only)

| config | round-1 class | mean_luma | pink% |
|---|---|---|---|
| luma_on | PINK | 0.257 | 41.1 |
| highway_bloom_off | PINK | ~0.51 | ~29 |
| bloom_off | NEARBLACK | ~0.09 | 0 |
| venue_light_off | PINK | ~0.68 | ~76 |
| control_w33_off | NEARBLACK | ~0.09 | 0 |

**Early directional signal (must be confirmed at full N):** the PINK wash reproduces
with `RB3_HIGHWAY_BLOOM_OFF=1` AND with `RB3_VENUE_LIGHT_OFF=1` already in round 1 —
i.e. neither the halo pass nor the per-environ venue-light rewrite is *necessary* for
the pink cast. That is consistent with the ranked prior (WAVE6_REVIEW A3 / W2.1-flip-
blocker backlog): **async asset/texture residency at capture** (a full-frame broken-env
placeholder class, boot-nondeterministic even under `RB3_FIXED_CLOCK`), for which **no
fix exists** and any fix is new loader/`ThreadCall` work **outside Lane A's fence**.
The whole-frame reach (note highway + venue backdrop, both cams) further points at the
final composite / a scene-wide uniform rather than a per-pass lighting term.

### To finalize (resume or coordinator)

1. `python3 -c "import json;d=json.load(open('.../WASH/measure/matrix.json'));..."` →
   fill the per-config table (n / wash_count / wash_rate / class histogram).
2. **ANSWER 1** (did W3.3-fix collapse the wash?): compare `luma_on` (cfg 1) rate vs
   `control_w33_off` (cfg 5) rate. Prior predicts ≈equal (no collapse).
3. **ANSWER 2** (which config → rate ~0?): if `venue_light_off` or `bloom_off` drives
   the rate to ~0, the mechanism is **in-lane** (root-cause + fix behind a registered
   default-OFF flag with fail-red). If **none** collapse it (all ≈ baseline), the
   mechanism is **async residency** → file the finding with attribution (per the task
   fence: do NOT improvise a loader patch; loader/ThreadCall is out of this lane).
4. If none of the 4 isolation flags collapse the rate, run the **`RB3_PP_OFF` probe**
   (composite fully OFF, same window/N) to disambiguate "postproc composite grade"
   (in-lane: saturation amp @ `rb3_postproc.wgsl.inc:107`, vignetteColor tint @ :121,
   or bloomColor — all from boot-varying `RndPostProc::Current()`) from "async
   residency" (out-of-lane). This is the one discriminator the 4-flag matrix cannot
   make on its own, because none of the 4 flags disable the grade itself.
