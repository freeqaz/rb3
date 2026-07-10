# WAVE 29 CLOSE-OUT REVIEW (adversarial audit)

**Reviewer:** Fable close-out agent · **Date:** 2026-07-10 · **HEAD audited:** `147eb1b6`
**Scope:** Lane 1 W29-CROWD-TRIGGER (`c4d2d46b`, outcome RECHARTER) + Lane 2 W29-GAMEPLAY
(`5a430eea`/`0081bae7`/`147eb1b6`, outcome PARTIAL). All load-bearing claims recomputed
from the committed raw gz artifacts; nothing accepted from lane excerpts alone.

## VERDICTS

| Lane | Verdict |
|---|---|
| **W29-CROWD-TRIGGER** | **ACCEPT-WITH-ERRATA** (E1-E4, all minor). RECHARTER = full lane success; the SIXTH narrative is evidence-decisive and the crowd chain should CLOSE. |
| **W29-GAMEPLAY** | **ACCEPT-WITH-ERRATA** (E5-E8, all minor). Part C LANDED verified; Part B RETIRED(E6(a)) sound; Part A PARTIAL label correct per CA2; Part D compliant. |

---

## 1. Evidence recomputation — Lane 1 (all from `W29-CROWD-TRIGGER/evidence/raw/step0-trace-v2.log.gz`, sha256 `c5e88ddc…` matches STATUS)

Every probe-count row reproduces exactly (`zgrep -ac`): CHARDRV_PLAY 54, PLAY_BT 27,
ENTER 30, CLIPSWAP 64, REPLACE 16, REPLACE_BT 8, DEFCLIP 23, STARVE 16, LIFE 304,
C13_PROBE 4, FileMerger 4, CharCache 0. Key tallies:

- **4 playerN plays at 2.433** (raw lines 1887/1925/1963/2001): `dir='player0'
  clip='player3_m'`, `player1→player2_f`, `player3→player1_f`, `player2→player0_m` —
  matches RECHARTER.md:45's mapping exactly. **4 loop replays at beat=25.333** (same
  dir→clip pairs). **0 `dir='crowd_*'` plays after beat 2.433** (all 8 crowd plays are
  `crowd1-5` at beat=0.000).
- **CLIPSWAP PathName assertion (CA4 item 1):** 4 lines at beat=2.433 with
  `drvPath='main.drv (char/main/main.milo)'` and `clipsPath='clips
  (world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo)'` — plus the 8
  `crowd_*` rebinds to the same bank (reproduces W28's rebind observation).
- **Sustained animation:** `CHARDRV_LIFE dir='player0' frames=2280 … playing=2209
  starved=71` (main.drv); `crowd_male01 … playing=71 starved=2280` (frozen at the
  splash count) — both as claimed.
- **C13 timing:** C13_PROBE at raw lines 214-217, streetslomo_clips load NOTIFY at 433,
  first play at 1887; `grep -c CharCache` in both backtraces = 0. STEP-0(ii) verified.
- **nTriggers:** `[PANELDBG] PanelDir::Enter dir=streetslomo_ao nTriggers=0` and
  `dir=sv3_a nTriggers=0` — exactly 2, as claimed (W28's fact reproduces).
- **Symbolized backtraces** (`evidence/step0-play-backtraces-symbolized.txt`): both
  chains contain `BandCamShot::StartAnim()` BandCamShot.cpp:357 →
  `CameraManager::StartShot_` :252 → `CameraManager::PrePoll` :304 → `WorldDir::Poll`
  Dir.cpp:160; leaf A = `CharDriver::OnPlayGroup` CharDriver.cpp:962, leaf B =
  `BandCharacter::OnPlayGroup` BandCharacter.cpp:4407. Raw `CHARDRV_PLAY_BT depth=36`
  frame block sits immediately after the line-1887 play. Mechanism claim VERIFIED.
- **Screenshots:** `evidence/shots/mainhub_walkers_frame597.png` shows four textured,
  venue-lit, mid-stride walking figures on main_hub (IK spike shards visible on arms,
  as disclosed). Coordinator E1 confirmed inter-frame motion (597 vs 701).
- **Checkpoints:** all four `crowd-step0-*.json` carry
  `buildSha=5a430eea…`; `/tmp/wave29-checkpoints/*` byte-identical to the committed
  copies (diff clean). The v1→v2 re-run provenance story (STATUS §Provenance) is
  internally consistent with the commit clock (5a430eea @17:50:56; checkpoints 18:03;
  c4d2d46b @18:09:49).

## 2. The SIXTH-narrative risk — adjudicated

**(a) Is the filter-artifact explanation COMPLETE for W28's four observations?** Yes:

| W28 observation | W29 disposition | recomputed? |
|---|---|---|
| zero CHARDRV_PLAY after 2.433 | filter artifact — player0-3 unmatched by `CHARDRV_PROBE=crowd` | yes: W29 `'*'` run shows 4 plays + 4 loops |
| `nTriggers=0` (streetslomo_ao, sv3_a) | TRUE but red herring — PanelDir UITrigger list ≠ walk mechanism (camshot anim) | yes: ×2 reproduce in v2 raw |
| CLIPSWAP rebind of the 8 proxies | correct rebind, harmless (bank has no crowd clips) | yes: 8 crowd_* CLIPSWAP @2.433 in v2 raw |
| DEFCLIP serialized='' (mDefaultClip NULL) | faithful data, unchanged; explains crowd_* freeze (playing=71) rather than idle fallback | consistent with LIFE rows |

**(b) Could W28's build genuinely not have fired the walk?** No. Recomputed the delta
`c6ef7795..95df30f2`: rb3-side = `ab1e5461` (docs + pin bump), `6c803adb` (kickoff docs,
boot-to-song.py, CHARDRV_PLAY_BT probe), `95df30f2` (docs + harness fix) — nothing that
can start an animation. Engine pin delta `be401ec→80e4c0f` is ONE commit:
`chore(flags): classify Wave-28 RB3_PROP_POSE_FULL compat fix (411 rows)` —
flags-classification metadata, no behavior. **The W28 boot would also have shown the
walk under `CHARDRV_PROBE='*'`.**

**(c) Unfalsifiability from W28's own log — CONFIRMED and stated.** W28's
`step0-combined.log.gz` contains **zero `dir='player…'` lines of ANY probe kind**
(recomputed: 0) because `_w28_crowd_step0_boot.py:47` set `CHARDRV_PROBE=crowd` — the
player plays are absent BY CONSTRUCTION. The artifact claim therefore cannot be
confirmed or refuted from W28's log alone; it rests on (i) W29's reproduction at
`5a430eea` and (ii) the no-behavior-delta argument in (b). Both hold. This epistemic
basis should be recorded in the close-out (see E3).

**(d) Residual risk (non-blocking):** walker-COUNT parity vs retail was not formally
counted (native shows 4 `player0-3` walkers; `streetslomo_clips.milo` contains only 8
`playerN_{f,m}` clips = 4 walkers × 2 genders, so the data supports exactly 4 — the
asset itself precludes the crowd_* walking). The lighting-saturation delta and arm IK
shards are correctly carved out as separate threads.

## 3. Evidence recomputation — Lane 2

- **Part C gates at HEAD:** `grep -c "RB3_CROWD_CLIP_KEEP\|gCrowdKeep\|CrowdKeepState"`
  CharDriver.cpp = **0**; `grep -c CHARDRV_PLAY_BT` = **1**. `git show 5a430eea` = 118
  deletions, CharDriver.cpp only; the five A3 sites (struct/map/getenv, dtor prune,
  Play snapshot, Poll re-arm, W25/W26 comments) all gone; the pre-W26 `mBones` UAF
  dtor guard KEPT (CharDriver.cpp:51-60, unconditional). Coordinator E1's
  batch_objdiff baseline-exact accepted. **LANDED verified.**
- **Part A clip census** (`propA_chardrv_play.log.gz`): distinct clips played =
  idle/expression family only (`stand_realtime_idle_*`, `idle_b_*`, `still`,
  `stand_around_05`, `stand_idle_norm_c_*`, `exp_rocker_*`, `exp_banger_*`,
  `Neutral_hi/lo`, `cam_ms_scream_irhn`, plus hub-transit `playerN_{f,m}`).
  `grep -ciE "drum|tom|snare|hihat|groove|tip"` on plays = **0**. Load-bearing claim
  ("nothing to bind") VERIFIED; enumeration wording incomplete (E7).
- **Part A static bones** (`propA_geom_POSE_DBG.log.gz`): per-target dPar dominant
  values — L-tip_floor_tom 45.2-45.5, R-tom 52.8-53.0, snare 36.5-36.9, hihat
  29.4-29.7, R-cymbal 65.2-66.7, guitar `bone_tip_fret` **7.6** — all match STATUS.
  Every drum `bone_R-tip_*` target has EXACTLY 1 distinct `twpos` across the whole
  window (vs 10-11 for the moving vocalist mic targets) → **tip frames static,
  VERIFIED.**
- **Part A residual:** `propA_baseline_FULL_ON.log.gz` right_hand
  `dst_from_hand>30` count = **8** (median ~31u) — matches the analyzer FAIL line and
  coordinator E1's re-run. PARTIAL per CA2 is the mechanically correct label.
- **Part B numbers** (recomputed medians): OFF-cap30k mic 75.7 (n=888) / mic_stand
  80.9 (n=1381); FULL-ON mic 56.7 / mic_stand 60.4 (n=1190 each) — match STATUS within
  rounding. `finger=0` in EVERY mic.ikhand PROP_DST row across all logs carrying mic
  rows; mic_stand `finger=1`; `propB_OFF_capped` has no mic rows (E5 starvation, as
  disclosed). **The lockstep-control argument is logically sound:** the piece-1-immune
  mic.ikhand (mFinger==NULL ⇒ the `if (mFinger && !sFingerBypass && !sPropPoseFull())`
  block is skipped identically OFF/ON) shifted −25% in lockstep with mic_stand ⇒ the
  shift is a shared cross-run confound, not a piece-1 effect. A fixed-clock
  frame-matched rerun is NOT required for the verdict — the code-level no-op proof is
  primary; the numeric control is corroboration.
- **CA6 compliance:** the prop-probe harness (`W26-PROP/run_prop_probe.py:74`) sets
  `RB3_FIXED_CLOCK=1` — A/B probe runs compliant. The observed cross-run dst shift is
  a log-cap WINDOW mismatch (different sim-time windows captured OFF vs ON), not clock
  jitter. The committed closeup "pair" is NOT songMs-matched (E5 erratum below).
- **CharIKHand cap parameterization** (`0081bae7`): reuses the existing
  `RB3_PROP_DST_DBG`/`RB3_IK_CLAMP_DBG` values (numeric >1 raises cap), no new getenv
  name, `#ifdef HX_NATIVE` — exactly as CA6 sanctioned.
- **Part D / CA7:** 0 new boot runs (≤3), 2 reused closeup runs (≤2), 2 full findings
  + one-liners (≤6), no fix code. Anomaly greps recomputed 0 on sampled logs
  (`propA_baseline_FULL_ON`, `sweep_seedrun_engine`). Compliant.
- **Checkpoints:** `/tmp` copies byte-identical to committed; micAB/sweep carry
  buildShas `5a430eea`/`c4d2d46b`; prop3-baseline carries `95df30f2` (see E6).

## 4. Process compliance

- **Ownership (CA8), verified via `git show --stat`:** Lane 1 `c4d2d46b` = own docs/
  evidence + new `scripts/native/_w29_crowd_trigger_boot.py` only. Lane 2 `5a430eea` =
  CharDriver.cpp only; `0081bae7` = CharIKHand.cpp only; `147eb1b6` = own docs/evidence
  only. No cross-lane touches; `rb3_session_trace.cpp` / engine `FxSendNative.cpp` /
  pin / census untouched. **CLEAN.**
- **pkill violation (Lane 1, self-reported):** blast radius assessed **LOW** — every
  Lane 2 evidentiary run reports rc=0, all committed logs are complete (mic medians
  computed over 888-1381 rows; no truncation signature), and Lane 1's own re-run was
  motivated by the unrelated address-shift provenance issue. Corrective action (pgid-
  only self-contained capture script) already adopted. No erratum beyond recording it.
- **Serialization rule:** every `CHARDRV_*` probe block byte-identical post-Part-C
  (grep gates PASS); Lane 1 stamped buildSha in every checkpoint as required.
- **rb3-tests:** Lane 1's 113/123 (10 GPU SEGFAULTs) = environmental/concurrent-run;
  coordinator's clean re-run at HEAD = 116/0 governs. Correctly flagged, not hidden.
- **Flag hygiene:** `RB3_VIGNETTE_TRIG_REPLAY` grep-0 in src/native/scripts (verified)
  — reserved, never used. Ruling Q(e′): RELEASE the name at close-out (chain closed,
  no W30 crowd lane will use it); record as released like `RB3_HUB_CROWD_CLIPBIND`.

## 5. ERRATA (append-only texts)

- **E1 (minor, Lane 1 — leaf-chain cosmetics).** STATUS.md:53 and
  crowd-step0-i.json name the streetslomo leaf as "PlayMainClip →
  `CharDriver::PlayGroup`", but the committed symbolized backtrace
  (step0-play-backtraces-symbolized.txt:47-48) shows `BandCharacter::PlayMainClip` →
  `CharDriver::Play` directly — no PlayGroup frame (inlined or not on the path). The
  issuing-mechanism finding (BandCamShot::StartAnim) is unaffected.
- **E2 (minor, Lane 1 — committed table artifact).** `evidence/probe-count-table.txt`
  row `PANELDBG PanelDir::Enter 0` is a grep-pattern artifact (raw has
  `[PANELDBG] PanelDir::Enter …` ×2; the literal pattern misses the bracket).
  STATUS.md:101,109-110 footnotes the correct count (2) but the committed table file
  itself carries the misleading 0.
- **E3 (note, Lane 1 — epistemic basis).** The W28 filter-artifact claim is
  UNFALSIFIABLE from W28's own raw log: under `CHARDRV_PROBE=crowd` the player0-3
  lines are absent BY CONSTRUCTION (recomputed: 0 `dir='player…'` lines of any kind in
  W28 step0-combined.log.gz). The claim rests on W29's `'*'` reproduction at
  `5a430eea` plus the verified no-behavior delta c6ef7795→95df30f2 (engine
  be401ec→80e4c0f = flags metadata only). Sound — but the close-out should state this
  basis explicitly.
- **E4 (trivial, Lane 1).** crowd-step0-flag.json says "NativeCompatFlags.gen.inc
  (415 rows)"; the census is 411 classified rows (415 = raw file line count).
- **E5 (minor, Lane 2 — closeup pair not matched).** The committed
  `vocalist_closeups/micAB_{OFF,ON}_coop_v_n01.png` are NOT a songMs-matched pair
  (different camera moment/cut; score 347 vs 400; framing differs), so they do not
  satisfy CA6's pairing clause as an A/B visual. The Part B verdict is unaffected
  because it rests on the mFinger==NULL code-level no-op proof + the mic.ikhand
  lockstep numeric control; the ON shot supports only "coherent pose, no spike-fan"
  (ON-only sufficiency), and should be cited as such.
- **E6 (minor, Lane 2 — baseline provenance).** gameplay-prop3-baseline.json carries
  `buildSha=95df30f2` while the binary included the then-uncommitted Part C deletion
  (== `5a430eea` content for CharDriver.cpp; the checkpoint's own note says so).
  buildSha records HEAD, not tree state — acceptable this time (deletion is flag-OFF
  inert), but future checkpoints should stamp `git diff --stat` non-empty markers too.
- **E7 (minor, Lane 2 — census enumeration).** STATUS.md:25-27 says the band plays
  "only `stand_realtime_idle_*` and `exp_rocker_*`/`exp_banger_*`"; the raw census
  also contains `still`(14), `idle_b_01/02`, `stand_around_05`,
  `stand_idle_norm_c_*`, `Neutral_hi/lo`, `cam_ms_scream_irhn`, and hub-transit
  `playerN_{f,m}` plays. The load-bearing claim — ZERO drum/tom/snare/hihat/groove/tip
  clips (grep 0) — is verified; the enumeration is incomplete, not wrong.
- **E8 (process, Lane 1 — recorded).** The self-reported `pkill -f` violation; blast
  radius LOW (see §4). Keep the "pgid-only" rule verbatim in every W30 dispatch.

## 6. Q RULINGS

- **Q(a) — Lane 1 RECHARTER = full success; CLOSE the crowd chain: YES.** Six-wave
  chain ends: the walk works, renders, and matches retail; the measurement trap
  (probe filter) is named and the no-behavior-delta check rules out a lucky fix. No
  lever exists to land. Do not reopen without NEW user-visible evidence that names the
  `player0-3` drivers explicitly.
- **Q(b) — corrected acceptance target set = new census ground truth: YES.** The 4
  `player0-3` (`char/main/main.milo`) `main.drv` drivers with
  `mClips==streetslomo_clips.milo` (PathName-asserted) + `playerN_{f,m}` play after
  2.433 + sustained FirstPlaying(). Any future crowd census MUST target these drivers;
  measuring the `crowd_*` family re-enters the trap (RECHARTER.md §census guidance
  adopted verbatim).
- **Q(c) — Part A label: PARTIAL stands** (CA2 is mechanical: analyzer exit 1 ⇒
  PARTIAL with numbers). The recharter-to-performance-clip-selection is the Wave-30
  DISPOSITION, not a relabel. Declining to game the 30u floor was correct
  (no-fake-match discipline) and is explicitly endorsed.
- **Q(d) — RB3_PROP_POSE_FULL default-ON: NOT YET.** Part B retires E6(a) only. The
  E6(b) gate stands: piece 1 skips the mFinger re-projection GLOBALLY when ON, and the
  mic hand was proven safe only because its mFinger==NULL. W30 must show ONE of:
  (i) a full-song census of every ikhand with `finger=1` (PROP_DST) and a fixed-clock,
  songMs-matched OFF/ON A/B on each showing no regression; or (ii) piece 1 re-scoped
  to prop-chain (strum/fret/right-hand) ikhands only, with the analyzer + closeups
  green. Note the right_hand 8×31u residual ships with ON either way — visually
  acceptable per W28/W29 closeups, but say so in the flip commit.
- **Q(e) — probe retirement.** KEEP: `CHARDRV_PLAY`, `CHARDRV_PLAY_BT`,
  `CHARDRV_CLIPSWAP` (the W30 performance-clip-selection lane's primary instruments:
  play census + clip-bank PathName assertion). RETIRE in a single W30 rider (after
  the perf-clip lane confirms it doesn't need them): `CHARDRV_ENTER`,
  `CHARDRV_REPLACE`, `CHARDRV_REPLACE_BT`, `CHARDRV_DEFCLIP`, `CHARDRV_STARVE`,
  `CHARDRV_LIFE`, `C13_PROBE`, `RB3_CROWD_PANEL_DBG`. All are `#ifdef HX_NATIVE`
  probe-only, so the Wii gate is trivially green; the payoff is cross-attribution
  hygiene (the E-C2 lesson applied to read-only scaffolding).
- **Q(e′) — flag name:** release `RB3_VIGNETTE_TRIG_REPLAY` (reserved-unused, chain
  closed).

## 7. WAVE-30 MENU (ranked)

1. **W30-BAND-PERF-CLIP (primary):** why do the on-stage band members play only
   idle+expression clips (zero instrument-performance clips — raw-proven)? Trace the
   performance-clip selection layer (song events → BandDirector/BandPerformer →
   BandCharacter::SetState/PlayGroup) with CHARDRV_PLAY/_BT + CLIPSWAP. This is Part
   A's recharter AND the likely root of SEED1 (dormant hands exist because hand
   placement falls entirely to IK against static props). Success = a named mechanism
   (working-reference style) and either a landed default-OFF lever or an honest
   recharter.
2. **W30-PROP-DEFAULT-ON (decision lane):** discharge E6(b) per Q(d) (global finger=1
   census + matched A/B, or scoped piece 1) → coordinator flips the 15th default.
   Independent of lane 1; fixes visible strum/fret hands now.
3. **Green-faces lane (blocked on reference):** acquire a retail band-member closeup
   lighting reference FIRST (no pair exists in `images/retail-screenshots/` —
   CA7-honest), then A/B char-env face lighting vs stage key-light on the green-faced
   models (C8-faces family). Do not dispatch without the reference in hand.
4. **Exit-trap teardown SIGSEGV:** pre-existing exit-time crash (W0.3 S1; also seen
   post-rb3-tests). Bounded, well-reproduced, hygiene payoff for every future gate.
5. **`part:` verb tooling:** extend `rb3_game_input.cpp:1080-1085` beyond guitar
   (coordinator-owned file) so future sweeps can charter vocals/drums/keys runs.
   Riders: probe retirement (Q(e)), flag-name release (Q(e′)).

## 8. COORDINATOR ACTION LIST

1. **Engine:** remove the `RB3_CROWD_CLIP_KEEP` classjson row (E-C2), regen
   `NativeCompatFlags.gen.inc` (census 411→410); mark `RB3_VIGNETTE_TRIG_REPLAY`
   released (never classified — no row to remove, note in ledger). Commit engine
   FIRST, then bump `MILO_ENGINE_PIN` in `native/CMakeLists.txt` ONCE.
2. **README (§Wave 29):** record — crowd chain CLOSED (sixth narrative, RECHARTER,
   filter-artifact + E3 epistemic basis); corrected census ground truth (Q(b));
   Part C landed; Part B E6(a) retired / E6(b) still gates default-ON; Part A PARTIAL
   → W30-BAND-PERF-CLIP; Wave-30 menu per §7. Append errata E1-E8 to the lane docs'
   errata blocks (append-only; do not edit lane text).
3. **Memory:** update `project_engine_arch_review_2026_07_05.md` — W29 close: crowd
   chain CLOSED-DO-NOT-REOPEN with the census trap warning; defaults still 14 ON;
   W30 = perf-clip lane + PROP default-ON decision.
4. **Probe retirement rider** scheduled per Q(e) (W30, after perf-clip lane needs are
   known).
5. **rb3-tests:** the clean 116/0 at HEAD is the governing result (coordinator E1);
   Lane 1's 10 GPU SEGFAULTs recorded as environmental/concurrent-run.
