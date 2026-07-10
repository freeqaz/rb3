# WAVE 29 KICKOFF — W29-CROWD-TRIGGER (primary) + W29-GAMEPLAY (PROP-3 / riders / user-directed bug sweep)

**Coordinator:** Fable · **Date:** 2026-07-10 · **Base:** rb3 `ab1e5461`, engine pin
`80e4c0f`, census 411, **14 defaults ON** (unchanged). Process invariants carried
unchanged from W28: discriminator-first, checkpoint-before-fix, A7 raw-log mechanics,
E4 verbatim-quote dispatch rule.

## User directive (this wave's charter modifier)

> "you need to load up a song in order to see this working properly. Please check for
> any other bugs and please setup a script to boot all the way to a song"

Interpretation adopted: gameplay (in-song) is the required validation surface for the
PROP work, and the wave carries a **user-directed gameplay bug sweep**. The
boot-to-song harness was built and verified by the coordinator PRE-WAVE (below), so
both lanes start with a working tool, not a tooling subtask.

## Coordinator pre-work (landed with this kickoff commit)

1. **`scripts/native/boot-to-song.py`** — canonical harness: boots rb3-native headless
   → main_hub → quickplay → song_select (optionally scrolls to a NAMED shortname) →
   real pad-press `part:`/`diff:` overshell flow → gameplay, then holds capturing
   periodic screenshots + `health.jsonl`. Flag A/B via repeatable `--env KEY=VAL`;
   `--part {guitar,bass,drums,vocals,keys}`; `--fixed-clock`; `--keep` leaves the
   engine up and prints port/pgid. Built on the proven `keyboard-to-gameplay.py`
   helpers + `capture_song_gameplay.py` nav. **Verified end-to-end 2026-07-10:** PASS,
   song `'123'`, gameplay reached (songMs 2477→26232), 8/8 shots
   (`/tmp/rb3-boot2song-verify/`).
2. **`CHARDRV_PLAY_BT` probe** added at `CharDriver::Play` (CharDriver.cpp, inside the
   existing `CHARDRV_PLAY` block) — same double gate as the W26 REPLACE_BT probe
   (`CHARDRV_PROBE` name-match × `CHARDRV_BT=1`), `#ifdef HX_NATIVE`, inert by
   default. Rationale: the committed menu declares CharDriver READ-ONLY for Lane 1
   "(probes exist)", but the existing BT probe covered only the Replace/kill site —
   Lane 1's STEP-0(i) needs the caller chain of the PLAY that works. Pre-adding it
   keeps Lane 1 truly read-only on CharDriver.cpp (W28-A2 lesson: never charter
   against probes that don't exist). **Wii gate:** batch_objdiff
   `Play__10CharDriverFP8CharClipifff` 100.0% fuzzy (unchanged),
   `Poll__10CharDriverFv` 93.54% == W28 baseline exact.
3. **Coordinator visual seeds** from the harness verify run — recorded as SWEEP INPUT,
   explicitly NOT conclusions (single venue, single camera cut, no retail pairing):
   - `gameplay_003.png` (guitarist closeup, club venue): no visible arms/hands on the
     instrument. Could be faithful camera framing; sweep must pair with retail
     (`images/retail-screenshots/`) and closeup captures before calling it a bug.
   - `gameplay_006.png`: strong green/olive skin tint on two band-member faces under
     purple venue lighting. Check against the C8-faces family (memory: 4 causes fixed)
     and retail venue-lighting ground truth before calling it a bug.

## Committed Wave-29 menu (verbatim — README §Wave 29 menu / WAVE28_CLOSEOUT_REVIEW.md Q8)

> 1. **W29-CROWD-TRIGGER (primary):** make the 8 hub walkers play `playerN_f/m` per the
>    RECHARTER target set. STEP 0 (blocking): (i) trace the WORKING reference — what
>    mechanism issues the beat-0 `play_clip crowd1-5` on the cityscape side (caller
>    backtrace on `CharDriver::Play`, PathName the scene object/eventanm/trig);
>    (ii) E1 discriminator — is CharCache/FileMerger in that path at all;
>    (iii) enumerate streetslomo's own `.trig`/`.eventanm`/scene-start objects (runtime
>    dump; static milo listing is top-level-only) and name why `nTriggers=0`. THEN one
>    lever at the layer STEP 0 names, flag-gated default-OFF unless the A6-class
>    carve-out fires with countersigned evidence. Acceptance = RECHARTER target set;
>    only then reopen the deferred verts=0/near-black thread. Owned: world/vignette +
>    PanelDir/eventanm surfaces; CharDriver/CharClip READ-ONLY (probes exist).
> 2. *(optional tail, now unblocked)* **W29-PROP-3:** bind/animate the prop-tip clip
>    tracks (CharDriver/CharClip* free this wave — arbitration pre-ruled the reverse of
>    W28). Riders: E-C2 `RB3_CROWD_CLIP_KEEP` removal + the E6 vocalist-mic A/B via
>    `RB3_PROP_FINGER_BYPASS`. Success = right_hand `dst_n→0` on the committed analyzer.
>    Defer-without-guilt.
> 3. No third lane. E-C2 must not survive W29 un-ruled again.

The user directive expands menu item 2 into the W29-GAMEPLAY lane (PROP-3 + riders +
bug sweep). Still two lanes; menu item 3 (no third lane) holds.

---

## Lane 1 — W29-CROWD-TRIGGER (primary; world/vignette scene-trigger lane)

**Goal:** make the 8 hub walkers animate per the RECHARTER acceptance target set.

**STEP 0 (BLOCKING — all three checkpointed to
`/tmp/wave29-checkpoints/crowd-step0-{i,ii,iii}.json` BEFORE any lever code):**

- **(i) Working-reference trace:** boot with `CHARDRV_PROBE='*' CHARDRV_BT=1` (plus
  the panel probes) and symbolize the `CHARDRV_PLAY_BT` chains for the beat-0
  cityscape `crowd1-5` plays (addr2line, same recipe as W28's
  `kill-backtrace-symbolized.txt`). Name the ISSUING mechanism with PathName — scene
  object / eventanm / trig / DataArray handler — not just the call chain.
- **(ii) CharCache/FileMerger discriminator (W28-E1):** state, with grep evidence,
  whether CharCache/FileMerger (`C13_PROBE` = CharCache.cpp:68 band-member slots —
  name-collision risk) is or is not in the working play path.
- **(iii) streetslomo trigger census:** runtime-dump streetslomo's own
  `.trig`/`.eventanm`/scene-start objects (static milo listing is top-level-only) and
  name precisely why `PanelDir::Enter streetslomo_ao` reports `nTriggers=0` — missing
  object, unloaded subdir, skipped registration path, or faithful-but-elsewhere.

**THEN one lever** at the layer STEP 0 names — flag-gated default-OFF (`#ifdef
HX_NATIVE`, byte-identical `#else`) unless the A6-class carve-out fires with
countersigned coordinator evidence. Choose **trigger-layer flag vocabulary**
(`RB3_HUB_CROWD_CLIPBIND` was released and is the WRONG vocabulary — do not reuse).

**Acceptance (verbatim from W28-CROWD-OWNER/RECHARTER.md §W29 ACCEPTANCE TARGET SET):**

> 1. **Target drivers:** the 8 `char/crowd/crowd_{male,female}0N` `main.drv` CharDrivers,
>    while `main_hub_screen` is active and their `mClips` resolves to
>    `streetslomo_clips.milo` (assert the PathName, not just the count).
> 2. **Animating criterion:** each target driver has `CHARDRV_PLAY` of a `playerN_{f,m}`
>    clip AFTER beat 2.433 and `FirstPlaying() != NULL` (`animating > 0`) sustained on
>    main_hub — NOT the transient cityscape `crowd1-5` plays at beat 0.
> 3. **Do NOT** count the splash/cityscape crowd (crowd1-5, sv8) as main_hub walkers; it
>    correctly animates during splash and correctly dies at the transition. Any census that
>    measures `animating` at/around beat 2.433 without pinning `mClips==streetslomo_clips`
>    is measuring the wrong crowd (the W23 ambiguity that caused this supersession chain).

Note the E2 carry-over: `playerN_{f,m}` as the walk-clip names is still a HYPOTHESIS
(runtime NOTIFY + raw strings, not yet a confirmed Play). If STEP 0 shows the real
clip names differ, the acceptance set updates to the proven names — with evidence,
in STATUS, not silently.

**Only after** the acceptance set is met may the deferred verts=0/near-black thread be
reopened (separate discriminator; do not fold into the lever).

**Owned surfaces:** `src/system/world/`, vignette/scene load, `PanelDir`/`WorldDir`
eventanm/trigger execution, `src/system/ui/` trigger registration if STEP 0 names it
(prewarm gate then applies). **READ-ONLY:** `CharDriver.cpp`/`CharClip*.cpp` (probes
pre-added), `CharIKHand.cpp` (Lane 2's). **PROTECTED:** gameplay WorldCrowd oracle
(`Crowd.cpp:884-1000`), RndMesh loader.

**Fallback:** if the lever cannot land inside the wave, a Lever-B-style honest
re-charter naming the blocker at the STEP-0-named layer is a valid full-success
deliverable (W28 precedent).

## Lane 2 — W29-GAMEPLAY (PROP-3 + riders + user-directed bug sweep)

**Part A — PROP piece 3 (prop-tip clip-track binding).** The deferred third piece per
W28-PROP-FIX/PLAN.md (exact site enumerated there). CharDriver/CharClip* are free for
this lane this wave (arbitration pre-ruled). Land under the existing
`RB3_PROP_POSE_FULL` flag (still default-OFF) or a separable sub-flag if the lane can
justify it in STATUS. **Success bar (A8(ii), verbatim carry):** on the committed
`analyze_prop_ab.py`, flag-ON shows `skip=0` AND `0` dst>30u entries for ALL ikhands —
i.e. right_hand `dst_n→0` (the W28 residual was 12-13 entries at ~32-33u). Honest
partial with numbers remains a reportable outcome **but the label is then PARTIAL,
not fix-landed** (E4/E5 lesson — the analyzer's ACCEPTANCE line decides).

**Part B — E6 vocalist-mic A/B (rider).** Discriminate the piece-1 global-scope
default-ON blocker: `boot-to-song.py --part vocals` with
`--env RB3_PROP_FINGER_BYPASS=1` vs OFF (and `RB3_PROP_POSE_FULL=1` vs OFF),
screenshots + PROP probe numbers on the vocalist mic hand. Deliverable: a stated
verdict — does piece (1) damage the mic-hand pose (blocker confirmed) or not
(blocker retired) — with raw evidence.

**Part C — E-C2 rider (MUST NOT survive un-ruled):** REMOVE `RB3_CROWD_CLIP_KEEP` —
delete the W25 scaffolding in CharDriver.cpp (getenv helper, CrowdKeepState snapshot
in Play, the Poll re-arm block). The close-out ruled it guards a proven-faithful kill
and is a pure cross-attribution hazard. Wii gate: batch_objdiff on the touched
functions == baseline. The classjson row removal + census regen is COORDINATOR-ONLY
at close-out — the lane only reports the deletion.

**Part D — user-directed gameplay bug sweep (NO fixes; findings only).** Using
`boot-to-song.py`: at least guitar + vocals runs (drums if time), plus
`band-closeup-capture.py` member closeups; hold ≥30s per run. Deliverables:
- `W29-GAMEPLAY/SWEEP_FINDINGS.md` — one entry per anomaly: symptom, evidence path
  (screenshot/log line), retail comparison (`images/retail-screenshots/`) or explicit
  "no retail pair", suspected layer, proposed Wave-30 lane. Triage the two coordinator
  seeds (guitarist arms; green face tint) FIRST — confirm or clear each with a retail
  pairing and a second camera angle before labeling.
- Log sweep: grep the engine logs of every run for `FAIL-MSG|abort|MILO_NOTIFY|OOB`
  anomaly classes; count table in STATUS (A7 style).
- NO fix code in Part D. Findings feed the Wave-30 menu.

**Owned surfaces:** `CharIKHand.cpp`, `CharDriver.cpp`/`CharClip*.cpp` (Parts A/C),
`scripts/native/` additions. **NOT** world/vignette (Lane 1's). Same protected list.

---

## Binding process rules (carried unchanged from W28)

1. **Checkpoint-before-fix:** every STEP-0/discriminator result is written to
   `/tmp/wave29-checkpoints/<lane>-<stage>.json` BEFORE any lever/fix code; agents
   check-first and return the checkpoint if valid.
2. **A7 raw-log mechanics:** (i) raw stderr of every evidentiary run gzipped into
   `<lane>/evidence/raw/` as committed deliverables (or sha256+bytes+durable path if
   too large); (ii) STATUS carries a per-log `grep -c` count table for EVERY probe tag
   (zeros/omissions mechanically visible); (iii) coordinator greps RAW artifacts
   before accepting headlines — excerpts are illustrations, never evidence.
3. **E4 rule:** dispatch prompts quote this kickoff's acceptance blocks VERBATIM
   (or link the committed file+section) — never paraphrase acceptance criteria.
4. **Flags:** getenv-gated, default-OFF, `#ifdef HX_NATIVE` with byte-identical
   `#else`. NO default flips, NO pin bumps, NO census/classjson edits by lanes.
5. **Git:** stage own files BY PATH under `flock /tmp/rb3-git.lock` (engine:
   `/tmp/milo-engine-git.lock`, engine commits FIRST, coordinator bumps the pin ONCE
   at close-out). Never touch `native/src/rb3_session_trace.cpp` or engine
   `FxSendNative.cpp` (concurrent agents). No `Co-Authored-By`. No stash/revert.
6. **Builds:** Wii via `tools/ninja-locked` only; native under
   `flock /tmp/rb3-native-build.lock`. Headless runs on free ports; pgid-only cleanup.
7. **Gates (every code-touching lane):** batch_objdiff 3-case table on touched
   functions (== baseline), `python3 scripts/native/drawlog-golden.py --fixed-clock
   --canonical-order` (792 draws; A6 carve-out escalation: over-eps EXCLUSIVELY
   world-field crowd-name values with count=792 = expected animating-crowd signature →
   coordinator countersign, do NOT edit the sidecar), `rb3-tests` 116/0, prewarm boot
   `RB3_PREWARM_SCREENS=1` on any `ui/*.cpp` edit, boot A/B flag-ON.

## COORDINATOR ACCEPTANCE

*(appended after the Fable pre-dispatch review — the acceptance block WINS over any
draft text above.)*
