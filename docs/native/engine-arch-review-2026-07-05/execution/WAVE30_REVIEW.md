# WAVE 30 PRE-DISPATCH REVIEW (adversarial)

**Reviewer:** Fable pre-dispatch gate · **Date:** 2026-07-10 · **Kickoff audited:**
`WAVE30_KICKOFF.md` @ `988b6de7` · **Cross-checked against:** README §Wave-29/Wave-30
menu, `WAVE29_CLOSEOUT_REVIEW.md` §6-§8, `W29-GAMEPLAY/STATUS.md`, and the code at HEAD.

## VERDICT: **DISPATCH-WITH-AMENDMENTS** (A1-A5 BLOCKING, A6-A8 ADVISORY)

Menu fidelity is clean: kickoff items 1-2 are byte-verbatim vs the README Wave-30 menu
(diff-verified; items 3-5 correctly summarized as not-dispatched, riders carried).
Q(e) retire list quoted exactly. Engine pin `17807afd…` matches
`native/CMakeLists.txt:74`. `boot-to-song.py` flags cited (`--fixed-clock`) exist;
`PARTS = ["guitar"]` (`boot-to-song.py:47`) confirms the guitar-only constraint;
`health.jsonl` carries `songMs` per shot (`boot-to-song.py:107-114`) so the pairing
clause is executable. `CHARDRV_PLAY`/`CHARDRV_PLAY_BT`/`CHARDRV_CLIPSWAP` all exist in
`CharDriver.cpp` (grep 1/1/3; `backtrace_symbols_fd` pattern at :390-392, :807).
`RB3_BAND_PERF*` collides with nothing (repo grep 0). Analyzer FOCUS set verified:
`FOCUS = ("strum.ikhand", "fret.ikhand", "right_hand.ikhand")` (analyzer:21).
But five charter errors below would each stall or waste a lane.

---

## A1 (BLOCKING, Lane 2) — analyzer path is wrong; the chartered file does not exist

**Wrong:** Kickoff Path B and the ownership matrix cite
`scripts/native/analyze_prop_ab.py`. That path does not exist (`ls` fails). The
committed analyzer lives at
`docs/native/engine-arch-review-2026-07-05/execution/W28-PROP-FIX/analyze_prop_ab.py`.
**Consequence:** Lane 2 either errors out or forks a new script — violating its own
"extend, don't fork" instruction.
**Fix:** Replace the path in Lane 2 Path B and in the ownership-matrix row with
`docs/native/engine-arch-review-2026-07-05/execution/W28-PROP-FIX/analyze_prop_ab.py`.

## A2 (BLOCKING, Lane 2) — Path B's mechanical decider is unattainable as written

**Wrong:** Path B's decider is "the committed analyzer … mechanical decider = its
`ACCEPTANCE (ON): PASS` exit-0". The analyzer FAILs whenever ANY FOCUS ikhand has a
nonzero `[PROP_DST]` count (analyze_prop_ab.py:78-84), and `right_hand.ikhand` is in
FOCUS. The W30 menu itself rules the right_hand 8×31u residual SHIPS with ON
("visually acceptable per W28/W29 closeups") — and re-scoping piece 1 to prop chains
cannot remove it (right_hand IS a prop chain; the residual is blend geometry over an
idle pose per W29 Part A). W29 measured exactly `ACCEPTANCE (ON): FAIL` +
`  - right_hand.ikhand: 8 dst>30u entries (want 0)` (exit 1) as the ACCEPTED state.
**Consequence:** Path B mechanically guarantees FAIL → the lane can never report
FLIP-RESCOPED; the decider contradicts the menu's own shipping ruling.
**Fix:** Decider = **no regression vs the W29-accepted baseline**: strum/fret rows
`skip=0 AND dst_n=0`; right_hand `skip=0 AND dst_n ≤ 8 AND dst_med ≤ 33.0`. Lane 2
owns the analyzer: extend it with a `--w30-residual-baseline` mode that encodes
exactly that and prints `ACCEPTANCE (W30-ON): PASS` / exit 0 (quote that string in
STATUS). The unmodified `ACCEPTANCE (ON):` line must still be printed and committed
raw for continuity with W28/W29 numbers.

## A3 (BLOCKING, rider) — probe-retirement rider is unexecutable under the ownership matrix

**Wrong:** The rider assigns retirement to Lane 1, but every retire-listed probe lives
in files Lane 1 cannot write. Exact grep counts at HEAD:
- `src/system/char/CharDriver.cpp` (**READ-ONLY both lanes** per the matrix):
  `CHARDRV_ENTER` 1, `CHARDRV_REPLACE` 1, `CHARDRV_REPLACE_BT` 1, `CHARDRV_DEFCLIP` 1,
  `CHARDRV_STARVE` 1, `CHARDRV_LIFE` 1.
- `C13_PROBE`: `src/band3/meta_band/CharCache.cpp` (1 site) — in NO lane's surfaces.
- `RB3_CROWD_PANEL_DBG`: **four** files, none owned — `src/system/ui/UIPanel.cpp`
  (:52,:57,:70), `src/system/ui/UIScreen.cpp` (:562,:579), `src/system/ui/PanelDir.cpp`
  (:244), `src/band3/meta_band/BandScreen.cpp` (:66,:73,:87).
Additionally the rider's re-run gate ("batch_objdiff Play/Poll baseline-exact") only
covers CharDriver — incomplete for a six-file deletion (same error class as the W29
pre-review's incomplete-deletion-list catch).
**Fix (pick one, state it in the kickoff):**
(a) *Recommended:* move retirement to the **coordinator at close-out** (it already
performs the single census/pin step; retirement is mechanical once the perf-clip
lane's keep-list is known). Lane 1's only rider duty = declare in STATUS which
retire-listed probes STEP 0 used; or
(b) grant Lane 1 a **scoped exception**: probe-block deletion ONLY, in exactly the six
files above, only AFTER the lever/recharter decision, with gates = repo-wide grep 0
per retired tag + batch_objdiff baseline-exact on ALL touched units (CharDriver
Play/Poll, CharCache.cpp, UIPanel.cpp, UIScreen.cpp, PanelDir.cpp, BandScreen.cpp).
Either way, ledger note: `scripts/native/_w28_crowd_step0_boot.py` /
`_w29_crowd_trigger_boot.py` still reference `RB3_CROWD_PANEL_DBG` (script-side, may
stay as historical evidence tooling).

## A4 (BLOCKING, Lane 1) — the W29-proven issuing mechanism's file is not in Lane 1's owned surfaces

**Wrong:** W29's symbolized backtraces already name `BandCamShot::StartAnim()`
(BandCamShot.cpp:357) as the mechanism issuing character group plays; the code
confirms it: `src/system/bandobj/BandCamShot.cpp:351-357` sends the
`play_group` message per shot target with `cur.mAnimGroup` — this IS the
performance-clip selection input (which anim group each shot carries). Yet
`BandCamShot.cpp` is absent from Lane 1's owned surfaces, and the matrix mandates
STOP+HANDOFF on discovering an unowned needed file. The trace is near-certain to land
there (it already did, last wave).
**Consequence:** a guaranteed mid-lane stall on the most likely load-bearing file.
**Fix:** add `src/system/bandobj/BandCamShot.cpp` (+ header, probe-only) to Lane 1's
owned surfaces and to the serialization matrix. CamShot base
(`src/system/world/CamShot.cpp`) stays READ-ONLY unless requested.

## A5 (BLOCKING, Lane 2) — the global finger=1 census cannot be built from the existing [PROP_DST] probe

**Wrong-by-omission:** Path A step 1 demands "enumerate EVERY CharIKHand … which have
`mFinger != NULL`", and the kickoff points the lane at "existing dbg envs". But the
existing `[PROP_DST]` probe only logs when `dd > 30.0f`
(`src/system/char/CharIKHand.cpp:415`) — i.e. it structurally MISSES every well-behaved
finger=1 ikhand, which are precisely the non-prop chains the census exists to protect
(the W28 "undisclosed foot-chain change" worry). W29 precedent (finger= read off
PROP_DST rows) points the lane at this biased instrument; a census built on it would
make FLIP-SAFE unsound while looking complete.
**Fix:** require a **one-shot enumeration probe** in CharIKHand.cpp (owned): on first
Poll (or bind) per ikhand, log `[PROP_CENSUS] ikhand=… finger=… owner=…` UNCONDITIONALLY
of distance when the env is set. Gate it via a sentinel value of the existing
`RB3_PROP_DST_DBG` if practical, else the kickoff's own escape clause applies (name it
`RB3_PROP_CENSUS_DBG`). Acceptance table must state the census instrument is
threshold-unbiased.

## A6 (ADVISORY, Lane 1) — BandPerformer.cpp is a name-lure; and `set_play` has zero C++ senders

`src/band3/game/BandPerformer.cpp` (222 lines) is scoring/crowd-meter only
(ComputePoints/SetCrowdMeter/stars) — grep 0 for SetState/PlayGroup/anim. Keep it as a
cheap negative check at most; don't spend probe budget. The productive receiving-side
choke points are all owned: `BandCharacter::PlayGroup` (:3845), `SetState` (:3870),
`OnPlayGroup` (:4389, handler :4245), and `OnSetPlay` (:4417 — sets play-flag bits into
SetState). Note for STEP 0(iii): `set_play` has **zero C++ senders** (repo grep: only
the Symbols.cpp registration) — it is DTA-script-driven, so the "song events →" leg may
dispatch from script land; receiving-side probes + `BANDPERF_*` backtraces are the
correct instrument (they name script/handler callers too).

## A7 (ADVISORY, both) — base SHA is stale relative to the kickoff itself

Kickoff says "pin all lane work to `a77608aa`", but the kickoff commit is `988b6de7`
(child of a77608aa), and CA adoption will move HEAD again. Dispatch prompts must name
the post-CA-adoption SHA as the lane base (engine pin `17807afd…` unchanged).

## A8 (ADVISORY, Lane 1) — STEP 0(ii) clips-enumeration does not need CharDriver write access

`CharDriver::ClipDir()` is a public accessor (`src/system/char/CharDriver.h:64`,
returns `mClips`). The clips-enumeration probe can live in owned `BandCharacter.cpp`
iterating `mDriver->ClipDir()` — say so in the dispatch prompt to pre-empt a
coordinator-request round-trip.

---

## PROPOSED COORDINATOR ACCEPTANCE BLOCK (adopt verbatim)

- **CA1 (analyzer path).** Everywhere the kickoff says
  `scripts/native/analyze_prop_ab.py`, read
  `docs/native/engine-arch-review-2026-07-05/execution/W28-PROP-FIX/analyze_prop_ab.py`
  (ownership-matrix row included). Lane 2 extends THAT file in place.
- **CA2 (Path B decider).** Path B's mechanical decider is replaced by: extended
  analyzer mode `--w30-residual-baseline` printing `ACCEPTANCE (W30-ON): PASS`
  (exit 0) iff strum/fret rows have `skip=0 AND dst_n=0` AND right_hand has
  `skip=0 AND dst_n <= 8 AND dst_med <= 33.0`. The legacy `ACCEPTANCE (ON):` line is
  still printed/committed (expected: `FAIL` + `right_hand.ikhand: 8 dst>30u entries
  (want 0)` — continuity with W29; any strum/fret regression or right_hand
  worse-than-baseline = DO-NOT-FLIP). Both lines quoted in STATUS.
- **CA3 (retirement executor).** The Q(e) probe retirement is **coordinator-executed
  at close-out**, not Lane 1. Lane 1's rider duty reduces to a STATUS section
  "retire-list probes used in STEP 0: <tags or none>". Coordinator retirement scope
  (exhaustive): CharDriver.cpp (`CHARDRV_ENTER`, `CHARDRV_REPLACE`,
  `CHARDRV_REPLACE_BT`, `CHARDRV_DEFCLIP`, `CHARDRV_STARVE`, `CHARDRV_LIFE`),
  CharCache.cpp (`C13_PROBE`), UIPanel.cpp/UIScreen.cpp/PanelDir.cpp/BandScreen.cpp
  (`RB3_CROWD_PANEL_DBG`). Gates: repo-wide `grep -rc <tag> src/` == 0 per retired
  tag; batch_objdiff baseline-exact on every touched unit (not just Play/Poll);
  KEEP-list (`CHARDRV_PLAY`, `CHARDRV_PLAY_BT`, `CHARDRV_CLIPSWAP`, live `BANDPERF_*`)
  byte-identical.
- **CA4 (Lane 1 surfaces).** Add `src/system/bandobj/BandCamShot.cpp` + `BandCamShot.h`
  to Lane 1's owned surfaces, **probe-only** (`BANDPERF_*`, `#ifdef HX_NATIVE`,
  byte-identical `#else`) unless STEP 0 names it as the lever layer — a lever there
  needs the standard flag gate + Wii `.o` byte-identical proof. `CamShot.cpp` (world)
  stays READ-ONLY. Matrix row updated accordingly.
- **CA5 (census instrument).** Lane 2's Path A census MUST come from a
  threshold-unbiased one-shot enumeration probe (`[PROP_CENSUS]` in CharIKHand.cpp,
  logged independent of `dst_from_hand`; env-gated; reuse `RB3_PROP_DST_DBG` sentinel
  or name `RB3_PROP_CENSUS_DBG`), NOT from `[PROP_DST]` rows (30u-gated at
  CharIKHand.cpp:415). STATUS's census table cites the probe and the boot+song window;
  a census derived from `[PROP_DST]` alone = lane rejection.
- **CA6 (BandPerformer + script senders).** Lane 1 STEP 0(i) treats BandPerformer.cpp
  as a negative check only (scoring-only, grep-proven); primary instrumentation =
  BandCharacter receiving side (`OnPlayGroup`/`OnSetPlay`/`SetState`/`PlayGroup`) +
  BandCamShot::StartAnim send side, with `BANDPERF_*` backtraces expected to surface
  DTA-script dispatch (`set_play` has no C++ sender).
- **CA7 (base SHA).** Lane dispatch prompts pin to the post-CA-adoption commit SHA
  (named at dispatch), not `a77608aa`. Engine pin `17807afd…` unchanged; no pin bump
  by lanes.
- **CA8 (clips enumeration placement).** STEP 0(ii)'s clips-enumeration probe is
  pre-authorized in BandCharacter.cpp via public `CharDriver::ClipDir()`
  (CharDriver.h:64) — no CharDriver.cpp edit, no coordinator round-trip.

All other kickoff text (menu fidelity, STEP-0 discriminators, A7 raw-log mechanics,
checkpoint paths, pgid-only rule, git rules, bounds) verified consistent with the W29
close-out and the code — dispatch once CA1-CA8 are adopted.
