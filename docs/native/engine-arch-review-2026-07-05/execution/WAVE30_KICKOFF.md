# WAVE 30 KICKOFF — BAND-PERF-CLIP (primary) + PROP-DEFAULT-ON (decision lane)

**Base SHA (pin all lane work to this):** `a77608aa` (rb3), engine pin `17807afd649e551b5d776180e4435a034e448edd`.
**Coordinator:** Fable. **Lanes:** Opus via ultracode Workflow.
**User directive (standing):** "let's continue work and running waves to fix bugs"; gameplay/in-song is the required validation surface (W29 directive carried).

**Concurrent, NOT part of this workflow:** a coordinator-dispatched W30-VISUAL-PASS
agent is running `scripts/native/boot-to-song.py` sweeps and will commit
`execution/W30-VISUAL-PASS/FINDINGS.md` (no source edits). Lanes ignore it; its
candidates feed the Wave-31 menu. It shares the build flock — expect occasional
lock waits, never kill its processes.

---

## Committed Wave-30 menu (README / WAVE29_CLOSEOUT_REVIEW.md §7 — verbatim)

> 1. **W30-BAND-PERF-CLIP (primary):** why do on-stage band members play only
>    idle+expression clips (zero instrument-performance clips — raw-proven)? Trace the
>    performance-clip selection layer (song events → BandDirector/BandPerformer →
>    BandCharacter::SetState/PlayGroup) with CHARDRV_PLAY/_BT + CLIPSWAP. This is Part
>    A's recharter AND the likely root of SEED1 (dormant hands exist because hand
>    placement falls entirely to IK against static props). Success = a named mechanism
>    (working-reference style) and either a landed default-OFF lever or an honest
>    recharter.
> 2. **W30-PROP-DEFAULT-ON (decision lane):** discharge E6(b) per Q(d) (global finger=1
>    census + songMs-matched A/B, or piece 1 re-scoped to prop-chain ikhands) →
>    coordinator flips the 15th default. Note: the right_hand 8×31u residual ships with
>    ON either way — visually acceptable per W28/W29 closeups; say so in the flip commit.

Items 3 (green-faces, BLOCKED on retail reference), 4 (exit-trap), 5 (`part:` verb
tooling, coordinator-owned) are NOT dispatched this wave. Riders: probe retirement
per Q(e), released-flag ledger per Q(e′) — see Lane 1 rider below.

---

## Lane 1 — W30-BAND-PERF-CLIP (primary)

**Question:** the W29 raw play census (W29-GAMEPLAY STATUS §Part A, raw gz committed)
proves the on-stage band plays ONLY idle + expression clips in-song
(`stand_realtime_idle_*`, `exp_rocker_*`/`exp_banger_*`, `still`, `idle_b_*`; grep 0
for drum/tom/snare/hihat/groove/tip; drum prop-tip AND parent bones static — 1
distinct twpos across the window). Retail plays instrument-performance clips. Name
the missing mechanism.

**STEP 0 — three discriminators, checkpointed BEFORE any lever (blocking):**

(i) **Call census.** Do the selection-layer entry points get CALLED in-song
natively? Instrument (default-OFF, getenv-gated, `#ifdef HX_NATIVE`, byte-identical
`#else`) the candidates: `BandDirector` song-event/state handlers
(`src/system/bandobj/BandDirector.cpp`), `BandPerformer`
(`src/band3/game/BandPerformer.cpp`), `BandCharacter::SetState` / `PlayGroup` /
play-path (`src/system/bandobj/BandCharacter.cpp`). New probe family name:
`BANDPERF_*` (do NOT overload `CHARDRV_*`). On any performance-clip-adjacent call,
symbolized backtrace (same `backtrace_symbols_fd` pattern as `CHARDRV_PLAY_BT`).
W29 lesson: probes must be UNFILTERED by character name on the first run — the
six-wave crowd chain died on a name filter.

(ii) **Asset census.** Are instrument-performance clips RESIDENT in each band
member's bound CharClipSet during gameplay? Use `CHARDRV_CLIPSWAP` (exists) plus, if
needed, a clips-enumeration probe (dump every clip name in each member's `mClips`
set once bound, with the mClips PathName printed — W29 binding census rule). If the
clips are not loaded, the gap is loading/merging, NOT selection, and the lever
belongs there.

(iii) **Event-stream census.** Does the song's character-anim event stream (song
mid anim events → whatever dispatches to BandDirector/BandCharacter) parse and
dispatch natively? Count dispatched events in the in-song window; 0 events vs
events-arriving-but-ignored is the load-bearing split.

Checkpoint each discriminator to `/tmp/wave30-checkpoints/perfclip-step0-<i|ii|iii>.json`
(buildSha + verdict + evidence paths) BEFORE moving on; check-first on re-entry.

**THEN:** ONE lever at the layer STEP 0 names, flag-gated default-OFF
(`RB3_BAND_PERF*` naming; NOT `CROWD_`/`CLIP_KEEP`/`CLIPBIND`/`REBIND`), OR an
honest recharter if the mechanism spans more than one lever.

**Acceptance (mechanical):**
- Named mechanism with symbolized-backtrace evidence; raw stderr gzipped into
  `execution/W30-BAND-PERF-CLIP/evidence/raw/` as committed deliverables; STATUS
  carries a per-log `grep -c` count table for EVERY probe tag (A7).
- If lever landed: songMs-matched `--fixed-clock` A/B (pair by nearest songMs from
  health.jsonl, never shot index) where the ON run's `CHARDRV_PLAY` census shows
  instrument-performance clips playing in-song (nonzero plays of drum/strum/groove-
  class clips with sustained playing counts) and OFF reproduces the W29 idle-only
  census. Screenshot pair of a band member mid-performance.
- Gates: touched-decomp-file `batch_objdiff` baseline-exact; `python3
  scripts/native/drawlog-golden.py --fixed-clock --canonical-order` PASS; rb3-tests
  clean; boot A/B flag-ON no-crash. Wii `.o` byte-identical (probes/lever inside
  `HX_NATIVE`).
- Harness: `scripts/native/boot-to-song.py` (guitar-only `part:` verb — do NOT
  charter vocals/drums/keys runs; all members animate regardless of played part).
  Bounds: ≤6 boot runs before requesting recharter.

**Rider (execute AFTER lever/recharter is decided, per Q(e)):** retire probe blocks
`CHARDRV_ENTER`, `CHARDRV_REPLACE`, `CHARDRV_REPLACE_BT`, `CHARDRV_DEFCLIP`,
`CHARDRV_STARVE`, `CHARDRV_LIFE`, `C13_PROBE`, `RB3_CROWD_PANEL_DBG`; KEEP
`CHARDRV_PLAY`, `CHARDRV_PLAY_BT`, `CHARDRV_CLIPSWAP` (+ any new `BANDPERF_*` you
still need). If STEP 0 used a retire-listed probe, KEEP it and say which/why.
Gates re-run after retirement (batch_objdiff Play/Poll baseline-exact).

**Owned surfaces:** `src/system/bandobj/BandDirector.cpp`, `BandCharacter.cpp`,
`src/band3/game/BandPerformer.cpp` (+ their headers, probe-only unless the lever
lands there), `execution/W30-BAND-PERF-CLIP/**`. **READ-ONLY:**
`src/system/char/CharDriver.cpp` (existing probes suffice — request coordinator if
a new CharDriver probe is genuinely needed), CharClip*, WorldCrowd/Crowd.cpp
(PROTECTED :884-1000), RndMesh loader (PROVEN-CORRECT), `scripts/native/boot-to-song.py`.

---

## Lane 2 — W30-PROP-DEFAULT-ON (decision lane, discharge E6(b))

**Question (Q(d) verbatim):** "W30 needs a global finger=1 census + songMs-matched
A/B, or piece 1 re-scoped to prop-chain ikhands" → then the coordinator flips
`RB3_PROP_POSE_FULL` as the 15th default.

**Path A (census first — preferred):** (1) global mFinger census: enumerate EVERY
CharIKHand across a full boot+song window and record which have `mFinger != NULL`
(name, chain, owning character) — the E6(b) worry is piece 1's global scope touching
non-prop chains (the W28 "undisclosed foot-chain change"). (2) songMs-matched
`--fixed-clock` A/B OFF vs ON scoring ALL finger=1 ikhands (not just the analyzer
FOCUS set) + a foot/plant sanity metric. If no non-prop chain regresses →
**DECISION: FLIP-SAFE**.

**Path B (re-scope, only if Path A finds a regressing non-prop chain):** re-scope
piece 1 to prop-chain ikhands inside the SAME flag (default stays OFF; coordinator
flips at close-out), re-run the committed analyzer
(`scripts/native/analyze_prop_ab.py`, FOCUS=strum/fret/right_hand, mechanical
decider = its `ACCEPTANCE (ON): PASS` exit-0) → **DECISION: FLIP-RESCOPED**.

**Acceptance (mechanical):** a DECISION section in STATUS naming FLIP-SAFE /
FLIP-RESCOPED / DO-NOT-FLIP with: the census table (every finger=1 ikhand named),
A/B numbers reproducible from committed raw gz, analyzer exit code, and the explicit
sentence that the right_hand 8×31.4u residual ships with ON (visually acceptable
per W28/W29 closeups). The lane does NOT flip the default; coordinator flips at
close-out iff FLIP-SAFE/FLIP-RESCOPED. Gates: batch_objdiff on CharIKHand.cpp if
edited (baseline-exact), drawlog PASS (flag still default-OFF), rb3-tests clean.
Bounds: ≤4 boot runs. Census probe caps parameterize through existing dbg envs
(W29-CA6: no new getenv unless unavoidable — if unavoidable, name it `RB3_PROP_*DBG`).

**Owned surfaces:** `src/system/char/CharIKHand.cpp` (+header), analyzer script
(extend, don't fork), `execution/W30-PROP-DEFAULT-ON/**`. **READ-ONLY:** everything
Lane 1 owns, CharDriver.cpp.

---

## Binding process rules (carried verbatim from W29 — violations = lane rejection)

1. **E4 rule:** dispatch prompts quote this acceptance text verbatim; lanes quote it
   back in STATUS before self-grading.
2. **A7 raw-log mechanics:** raw stderr gz into `<lane>/evidence/raw/` COMMITTED;
   per-log `grep -c` table for EVERY probe tag in STATUS; coordinator E1 greps RAW,
   never curated excerpts.
3. **Checkpoint-before-fix:** `/tmp/wave30-checkpoints/<lane>-<stage>.json`
   (buildSha, verdict, evidence paths), check-first semantics; also mirror final
   checkpoints into the lane evidence dir before returning.
4. **Discriminator-first:** no lever/edit until the owning STEP-0 discriminator is
   checkpointed.
5. Builds ONLY via `tools/ninja-locked` (Wii) / `flock /tmp/rb3-native-build.lock
   cmake --build native/build-native --target rb3-native` (native). Headless runs:
   `RB3_HTTP=1`, free ports, `--fixed-clock` for A/B.
6. **pgid-only cleanup — NEVER `pkill` by name** (`pkill -f rb3-native` killed a
   concurrent run in W29; the visual-pass agent is live RIGHT NOW).
7. Git: stage ONLY files you created/edited BY PATH; commits under
   `flock /tmp/rb3-git.lock`; never touch `native/src/rb3_session_trace.cpp`, the
   engine's `src/platform/FxSendNative.cpp`, other agents' untracked files
   (`scripts/web/_*.mjs`, `logs/`). No `git add -A`/`.`/`-a`, no stash, no
   Co-Authored-By.
8. **NO default flips, NO pin bumps, NO census regen by lanes** — coordinator only,
   ONCE, at close-out. Engine edits (if any — none expected this wave) commit in
   `../milo-native-engine` first; coordinator bumps the pin.
9. Flags getenv-gated default-OFF `#ifdef HX_NATIVE` with byte-identical `#else`.
10. Report failures faithfully: rc, log tail, honest PARTIAL > gamed PASS. The
    known exit-time teardown SIGSEGV (rc=-11 after work completes) is tolerated —
    note it, don't chase it (menu item 4, not this wave).

## File-ownership / serialization matrix

| Surface | Owner |
|---|---|
| BandDirector.cpp/.h, BandCharacter.cpp/.h, BandPerformer.cpp/.h | Lane 1 |
| CharIKHand.cpp/.h, analyze_prop_ab.py | Lane 2 |
| CharDriver.cpp | READ-ONLY both (probes exist) |
| boot-to-song.py, rb3_game_input.cpp, defaults/pins/census | Coordinator |
| execution/W30-<lane>/** | that lane |

No shared writable file exists between lanes; if one is discovered, STOP and
checkpoint a HANDOFF note instead of editing.

## Coordinator close-out obligations (unchanged)
E1 raw-grep countersign → adversarial close-out review → append-only errata →
README results + Wave-31 menu (fold in W30-VISUAL-PASS candidates) → single census
regen + at most ONE pin bump → PROP default flip iff Lane 2 says FLIP-* → memory →
user summary.

---

## COORDINATOR ACCEPTANCE (adopted verbatim from WAVE30_REVIEW.md — BINDING, overrides any conflicting text above)

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
