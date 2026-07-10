# W29-GAMEPLAY — STATUS

**Headline.** Part **C** (E-C2) LANDED — `RB3_CROWD_CLIP_KEEP` five-site scaffolding
deleted from `CharDriver.cpp`, all three CA3 gates PASS (grep 0, `CHARDRV_PLAY_BT` 1,
`Play` 100.0% / `Poll` 93.54% == baseline). Part **B** verdict: the E6 default-ON
mFinger blocker is **RETIRED for the vocalist mic chain** — the actual mic hand has
no `mFinger` (piece 1 is a provable no-op), and the mic_stand grip shows no
piece-1-attributable divergence. Part **A** (PROP piece 3) = **PARTIAL** (CA2): the
chartered prop-tip clip-track binding is **inapplicable** — the on-stage band plays
only idle+expression clips, so **no clip animates the drum-tip bones to bind**; the
right_hand residual (8 dst>30u at ~31u) is drum-kit blend geometry over an idle-posed
arm, whose real driver is performance-clip *selection* (Lane-1-adjacent), not
CharClip clip-track binding. strum/fret PASS. Part **D**: both coordinator seeds
triaged with second angles; logs 0-anomaly.

---

## Part A — PROP piece 3 — PARTIAL (CA2), piece-3-inapplicable finding

**Pre-fix baseline** (`gameplay-prop3-baseline.json`, buildSha stamped, written BEFORE
any investigation): flag-ON reproduces the W28 residual — strum/fret PASS (skip=0,
0 dst>30u), right_hand skip=0 but **8 dst>30u at median 31.4u**. Analyzer exit 1.

**Why piece 3 as chartered has nothing to bind (new evidence this wave):**
- **The band plays no performance clip.** `CHARDRV_PLAY` census during gameplay
  (`evidence/raw/propA_chardrv_play.log.gz`): `player0`–`player3` play only
  `stand_realtime_idle_*` and `exp_rocker_*`/`exp_banger_*` (idle + facial
  expression). **Zero** drum/tom/snare/hihat/tip clips ever play.
- **The drum prop bones are static.** `[PROP_POSE]` dump
  (`evidence/raw/propA_geom_POSE_DBG.log.gz`): the drummer `right_hand.ikhand` targets
  a family of `bone_L-tip_<piece>.mesh` bones, each parented to
  `bone_target_<piece>.mesh`. The tip *and* parent frames are static across the window
  (floor_tom dPar≈45.3 constant, R-tom≈52, snare≈37, hihat≈29, R-cymbal≈66) — no clip
  drives them. Unlike the guitar (`bone_target_fret` dPar≈7.6, near the hand), the
  drummer's redirect parents are all far, so the multi-target blend sits ~31u out.

**Conclusion.** Piece (3) presupposes a drum-groove clip with tip tracks that natively
isn't playing. There is no unbound clip track to bind in `CharClip`/`CharDriver`; the
~31u right_hand residual is drum-kit blend geometry over an idle-posed arm. The real
driver is **which performance clip the drummer plays** (idle vs groove) — a
scene/performance-clip-selection layer (same family as Lane 1's crowd-trigger gap),
NOT prop-tip clip-track binding. Per CA2 this is **PARTIAL with the numbers**; no
piece-3 fix code was written (a redirect/weight refinement to game the 30u floor
would be a mislabel and is declined — no-fake-match discipline). **Wave-30 recharter:
performance-clip selection for on-stage band members.** strum/fret (played guitar)
remain fully fixed by W28 pieces 1+2.

### Part A focus-ikhand numbers (committed `analyze_prop_ab.py`)
| ikhand | OFF skip | ON skip | OFF dst_n | ON dst_n | ON dst_med | ON verdict |
|---|---|---|---|---|---|---|
| *strum.ikhand      | 46 | **0** | (capped) | **0** | — | PASS |
| *fret.ikhand       | 45 | **0** | (capped) | **0** | — | PASS |
| *right_hand.ikhand | 46 | **0** | (capped) | **8** | 31.4 | FAIL (dst bar) |

`ACCEPTANCE (ON): FAIL — right_hand.ikhand: 8 dst>30u entries (want 0)` (exit 1).

---

## Part B — E6 vocalist-mic A/B — blocker RETIRED for the mic chain

Verdict (`gameplay-micAB.json`): **piece (1) does NOT damage the vocalist mic-hand
pose.**
- **Code-level proof:** `mic.ikhand` has `mFinger == NULL` (`finger=0` in every
  `[PROP_DST]` row across OFF/BYPASS/ON) → the
  `if (mFinger && !sFingerBypass && !sPropPoseFull())` re-projection block is skipped
  identically OFF and ON. Piece (1) is a provable no-op for the actual mic hand.
- **mic_stand.ikhand** (`finger=1`, reach=0.00 → clamp dormant): cross-run
  dst_from_hand OFF≈81u vs FULL-ON≈60u — BUT the piece-1-IMMUNE `mic.ikhand` shifted
  by the SAME proportion (OFF≈75 → ON≈56, ~−25%), so the shift is shared vocalist
  animation-phase variance across runs, not a piece-1 effect.
- **Visual:** `band-closeup-capture.py --member vocals --anchor-ms 26000` OFF vs ON —
  both `verdict=PASS`, pinned 6/6, 0 band drops; ON `coop_v_n01` shows a coherent
  leaning singing pose (mic stand visible), no spike-fan/explosion
  (`evidence/vocalist_closeups/micAB_{OFF,ON}_coop_v_n01.png`).

**Caveat:** clears the vocalist mic-chain specifically (E6(a)). E6(b) — scope the
global mFinger break OR show it globally harmless — and the default-ON flip remain
coordinator-owned.

**Tooling (committed, CA6/E5):** `[PROP_DST]`(120) and `[IK_CLAMP]`(300) probe caps
parameterized through the *existing* `RB3_PROP_DST_DBG`/`RB3_IK_CLAMP_DBG` values
(numeric >1 raises the cap; no new getenv name). The mic rows are starved even at
cap 6000 by the always-over-reaching guitar/drum hands (E5 confound); cap 30000 was
needed to capture OFF `mic_stand`. Probe-only, `#ifdef HX_NATIVE` → Wii `.o`
byte-identical (`Poll__10CharIKHandFv` 96.13% == W28 baseline).

---

## Part C — remove `RB3_CROWD_CLIP_KEEP` — LANDED

Five-site deletion per WAVE29_REVIEW.md A3 (commit `5a430eea`): (1) struct
`CrowdKeepState` + `gCrowdKeep()` + `gCrowdClipKeepEnabled()` + W25/W26 comments; (2)
dtor E-C3 prune block (kept the pre-W26 `mBones` UAF guard, unconditional); (3) Play()
snapshot (probe blocks byte-identical); (4) Poll() re-arm block + comment.
CA3 mechanical gates (all in the table below): **PASS**.

---

## GATES

| gate | requirement | result |
|---|---|---|
| grep `RB3_CROWD_CLIP_KEEP\|gCrowdKeep\|CrowdKeepState` (CharDriver.cpp) | 0 | **0 PASS** |
| grep `CHARDRV_PLAY_BT` (CharDriver.cpp) | 1 | **1 PASS** |
| batch_objdiff `Play__10CharDriverFP8CharClipifff` | 100.0% | **100.0% PASS** |
| batch_objdiff `Poll__10CharDriverFv` | 93.54% | **93.54% PASS** |
| batch_objdiff `Poll__10CharIKHandFv` (Part B) | 96.13% (W28 base) | **96.13% PASS** |
| `drawlog-golden --fixed-clock --canonical-order` (flag-OFF) | 792 PASS | **792 PASS** (307 known-residual within bound) |
| `rb3-tests` | 116 / 0 | **116 pass / 0 fail** (7 skipped = baseline; exit-time teardown SIGSEGV is the known pre-existing exit-trap, post-completion) |
| boot A/B flag-ON reaches gameplay crash-free | reachable | **PASS** — all prop-probe ON runs rc=0 reached gameplay; band-closeup ON PASS 6/6, 0 drops |

Wii byte-identical: every code edit is `#ifdef HX_NATIVE` (mwcc doesn't define it),
so Wii `.o` is unchanged (objdiff == baseline confirms).

---

## Probe / anomaly counts (A7)

**PROP focus ikhands** (`grep -ac`; logs carry NUL → `-a`). Full analyzer output per
run in `evidence/raw/propB_*.log.gz`, `propA_baseline_FULL_ON.log.gz`.

| ikhand | OFF (cap120) | BYPASS (cap120) | FULL-ON (cap120) | OFF (cap30k) |
|---|---|---|---|---|
| strum.ikhand skip | 46 | 0 | 0 | — |
| fret.ikhand skip | 45 | 0 | 0 | — |
| right_hand.ikhand dst_n | 24 | 3 | 8 | — |
| mic.ikhand finger | — (starved) | 0 | 0 | 0 |
| mic.ikhand dst_med | — | 56.2 | 56.5 | 75.4 |
| mic_stand.ikhand finger | — (starved) | 1 | 1 | 1 |
| mic_stand.ikhand dst_med | — | 60.4 | 60.4 | 81.0 |

**Engine-log anomaly classes** (`grep -ci`, every lane run log — full table
reproduced from the run set; zeros visible):

| class | seed engine.log | prop ON | prop OFF | prop BYPASS | prop OFF-30k | closeup ×4 |
|---|---|---|---|---|---|---|
| FAIL-MSG | 0 | 0 | 0 | 0 | 0 | 0 |
| abort | 0 | 0 | 0 | 0 | 0 | 0 |
| MILO_NOTIFY | 0 | 0 | 0 | 0 | 0 | 0 |
| OOB | 0 | 0 | 0 | 0 | 0 | 0 |
| MILO_WARN | 0 | 0 | 0 | 0 | 0 | 0 |

---

## Honest false-starts / confounds
- **Part A initial framing** (from W28 PLAN) held that an unbound prop-tip clip track
  just needed binding. The CHARDRV census refuted the premise: there is no drum-groove
  clip playing at all — the band idles — so there is nothing to bind. Corrected to a
  performance-clip-selection finding.
- **Part B cap starvation:** two OFF runs (cap 120, then cap 6000) showed **no mic
  rows** — first misread as "mic never over-reaches", actually the shared per-process
  `[PROP_DST]` cap is consumed by the always-broken guitar/drum hands in OFF (E5). Cap
  30000 exposed the mic rows. Motivated the CA6-sanctioned cap parameterization.
- **Cross-run dst comparison is variance-prone** without frame-matching; the
  piece-1-immune `mic.ikhand` moving in lockstep with `mic_stand` is what proves the
  shift is animation variance, not piece 1.

## Checkpoints (buildSha-stamped; check-first)
`/tmp/wave29-checkpoints/gameplay-{prop3-baseline,micAB,sweep}.json`, copied durable
into `checkpoints/`.

## Disposition / no-touch compliance
No default flips, no pin bump, no census/classjson/sidecar/golden edits (all
coordinator-only). Touched only `CharDriver.cpp` (Part C, HX_NATIVE-only deletions)
and `CharIKHand.cpp` (Part B probe-cap, HX_NATIVE-only). Did NOT touch world/vignette/
PanelDir/ui (Lane 1), the `Crowd.cpp` oracle, RndMesh loader, `rb3_session_trace.cpp`,
or engine `FxSendNative.cpp`. Every `CHARDRV_*` probe block left byte-identical (CA3).
The `RB3_CROWD_CLIP_KEEP` census-row removal + regen + any pin bump are
coordinator-only at close-out.

---

## ERRATA (appended at close-out from WAVE29_CLOSEOUT_REVIEW.md — append-only; lane text above unedited)

- **E5 (minor — closeup pair not matched).** The committed
  `vocalist_closeups/micAB_{OFF,ON}_coop_v_n01.png` are NOT a songMs-matched pair
  (different camera moment/cut; score 347 vs 400; framing differs), so they do not
  satisfy CA6's pairing clause as an A/B visual. The Part B verdict is unaffected
  because it rests on the mFinger==NULL code-level no-op proof + the mic.ikhand
  lockstep numeric control; the ON shot supports only "coherent pose, no spike-fan"
  (ON-only sufficiency), and is to be cited as such.
- **E6 (minor — baseline provenance).** gameplay-prop3-baseline.json carries
  `buildSha=95df30f2` while the binary included the then-uncommitted Part C deletion
  (== `5a430eea` content for CharDriver.cpp; the checkpoint's own note says so).
  buildSha records HEAD, not tree state — acceptable this time (deletion is flag-OFF
  inert); future checkpoints stamp a `git diff --stat` non-empty marker too.
- **E7 (minor — census enumeration incomplete, not wrong).** STATUS.md:25-27 says the
  band plays "only `stand_realtime_idle_*` and `exp_rocker_*`/`exp_banger_*`"; the raw
  census also contains `still`(14), `idle_b_01/02`, `stand_around_05`,
  `stand_idle_norm_c_*`, `Neutral_hi/lo`, `cam_ms_scream_irhn`, and hub-transit
  `playerN_{f,m}` plays. The load-bearing claim — ZERO drum/tom/snare/hihat/groove/tip
  clips (grep 0) — is verified.
