# Verify: `all-inst-crash` / vocals — WAVE-3 adversarial verification

**Verdict: PASS (with one important scope caveat) — the vocal-load crash fix is
solid on the composed build; vocals loads + plays + the HUD renders. BUT a FULL
song-to-completion still aborts at the endgame/score screen — and that abort is
PRE-EXISTING and INSTRUMENT-AGNOSTIC (guitar hits it too), not a vocal regression.**

Verifier: wave-3, read-only, composed build (rb3 master `c011c886`, engine pin
`469c550`, binary built 21:00). Ports 8861–8865. Evidence under `/tmp/rp3-vocals/`.

---

## What was claimed (task-all-inst-crash-impl.md)

Three native-only fixes (`d598abdd`): (1) wire `TheNet.mSession = TheNetSession`
(Layer-1 SIGSEGV in `Singer::CreateMicClientID`), (2) guard the four empty
`&mAmbiguousData[0]` pointer-loops in `Singer.cpp` (Layer-2a SIGABRT), (3) the
`VN_PTR` macro in `VocalTrack.cpp` so `&mNotes[i]` end-pointer formation doesn't
trip `_GLIBCXX_ASSERTIONS` (Layer-2b SIGABRT, the one that actually fired in
gameplay). Claim: vocals loads, HUD renders, survives ~21.5 s, Wii byte-identical.

All three are present in the composed build (verified by grep):
- `native/src/rb3_netsession_native.cpp:306` — `TheNet.mSession = TheNetSession;`
- `src/band3/game/Singer.cpp:314/465/615/679` — `#ifdef HX_NATIVE if (!mAmbiguousData.empty())`
- `src/band3/bandtrack/VocalTrack.cpp:55` — `VN_PTR` macro, 8 call sites updated.

---

## (1) Vocal load + play — PASS, reproduced twice

Repro: `/tmp/rp2-all-inst-crash/repro_vocals_ext.py` (boot → song_select →
part_difficulty → `track:vocals` → `end_override_flow` → nofail + 40× autohit,
~21 s of gameplay past the two historical abort frames 1585 & 2753).

| run | port | result | crash signatures in log |
|---|---|---|---|
| run1 | 8861 | reached game_screen, overshell `('hidden','vocals','expert')`, survived to frame 7177 / songMs 21017 | **0** (8533-line log clean) |
| run2 | 8865 | reached game_screen, overshell vocals, survived to frame 9479 / songMs 21297 | **0** |

BEFORE this fix the same script SIGSEGV'd during load (the scout/impl docs); now
it loads and plays cleanly, reproducibly. The two Layer-2 aborts (`mAmbiguousData`
@frame~2753, `VocalTrack::UpdateScrolling` @frame~1585) are both gone — the run
sails past both frame numbers without aborting. **Layers 1, 2a, 2b all verified
on the COMPOSED build.**

### Vocal HUD renders (screenshots)
`/tmp/rp3-vocals/run1/play_*.png` (+ upscaled HUD crops
`/tmp/rp3-vocals/play_28_hud.png`, `run1_play_20_hud.png`, `run1_play_24_hud.png`).
The vocal HUD draws and is live:
- **Pitch tube** — the wide horizontal grey band with frame/border + divider lines, at screen top.
- **Pitch cursor** — circle-with-dash on the left edge (the player's pitch marker).
- **Note tubes** — rounded white "pill" phrase markers; their position changes across frames → the lane is scrolling.
- **A gold/yellow note element** visible at play_24 (an active vocal note).
- **Score/multiplier bar** — vertical green/yellow element, left side.
- **"Connect a Mic"** prompt + arrow icon — EXPECTED (headless, no physical mic; autohit drives scoring).

Visual-quality notes for future polish:
- **Lyric TEXT is sparse / not clearly legible** in the captured frames. Likely
  the no-mic "Connect a Mic" state or song sections without active lyric; could
  also be a genuine lyric-rendering gap. Can't adjudicate without a retail vocals
  reference (none exists in `images/retail-screenshots/` — see below).
- HUD alignment/proportions look plausible (top-anchored pitch tube, centered note
  tubes) but are unverified against ground truth.

---

## (2) Endgame / score screen — **FAILS, deterministic SIGABRT (pre-existing, NOT vocal-specific)**

This is the headline finding. The vocal verification in the impl doc only ran
autohit for ~21 s and **never jumped to song end**, so it never reached the
endgame. I drove a full song-to-completion with vocals (`/tmp/rp3-vocals/vocal_to_end.py`,
port 8862: load vocals → `{game jump 600000}` → watch endgame). Result:

```
song ended -> ('state', 'is_game_over==1')      # song-end transition WORKS
... post-jump screen='game_screen' (frames advancing while win-anim plays) ...
*** PROC EXIT code=134 reaching endgame ***     # SIGABRT during the endgame transition
```

The DTA + native stack traces are decisive and match the crowd scout's report:

```
Data Stack Trace
   ui/endgame/endgame_helpers.dta(64):meta_performer
   ui/endgame/endgame_helpers.dta(61):if_else
   ui/endgame/endgame_helpers.dta(59):transition_complete
```
Symbolized native backtrace (addr2line on the composed binary):
```
OSFatal                                  rvl_shims.cpp:51
Debug::Fail                              Debug.cpp:190        <- a MILO_ASSERT fired
MetaPerformer::UpdateScores              MetaPerformer.cpp:702   <- MILO_ASSERT(netServer,0x437) @701
MetaPerformer::SaveAndUploadScores       MetaPerformer.cpp:734
MetaPerformer::PotentiallyUpdateLeaderboards  MetaPerformer.cpp:1369
MetaPerformer::CompleteSong              MetaPerformer.cpp:1180
MetaPerformer::TriggerSongCompletion     MetaPerformer.cpp:1164
MetaPerformer::Handle                    MetaPerformer.cpp:1694  (meta_performer DTA handler)
```

**Root cause:** `MetaPerformer::UpdateScores` does `MILO_ASSERT(netServer, 0x437)`
at `MetaPerformer.cpp:701` where `netServer = TheNet.GetServer()`. Natively
`TheNet` is the same zero-filled weak stub that caused the original vocal SIGSEGV;
the wave-2 fix wired `TheNet.mSession` but **`TheNet.mServer` is still null**
(`network/net/*.cpp` isn't compiled). So `GetServer()` returns null → the assert
aborts. This is the **same `TheNet`-stub family** as the original bug, just a
different member (`mServer` vs `mSession`). Seven `TheNet.GetServer()` sites exist
in `MetaPerformer.cpp` alone (638/654/681/699/743/753/780); the score-upload path
hits one unconditionally at song completion.

**This is NOT a vocal regression and NOT introduced by the vocal fix.** I confirmed
it crashes the **GUITAR** endgame identically:

```
scripts/native/song-end-test.py --port 8863 --require-endgame
  -> PASS: song ended -> game-over reached
  -> FAIL: process exited (code 134) before the endgame screen loaded
```
Guitar backtrace = byte-identical offsets (`MetaPerformer::UpdateScores` @0xcb1d01
→ `Debug::Fail` @0x47c977 → `OSFatal` @0xf3d679). So the endgame SIGABRT is a
**pre-existing, instrument-agnostic gap**: the score screen has never been
reachable to completion on this native build via the score-upload path. The
existing `song-end-test.py` *without* `--require-endgame` masks it (it accepts the
`is_game_over==1` state and exits before the endgame milos load — and even treats
a post-jump abort as a soft PASS).

This is the `endgame SIGABRT (endgame_helpers.dta(64):meta_performer)` already
listed as a wave-3 follow-up in PLAN.md — now with the exact C++ site and a clear
fix direction.

### Suggested fix (for the next implementer; out of scope to apply here)
Mirror what `RB3InitNativeNetSession()` did for `mSession`: wire
`TheNet.mServer` to a native `Server` stub (or, narrower/safer, native-guard the
`TheNet.GetServer()` derefs in `MetaPerformer` the same way the `mAmbiguousData`
loops were guarded — treat null server as "offline / no leaderboard upload",
which is the correct native behavior). The console invariant is
`GetPlayerID(padnum)` gating; offline, the local profile score path
(`profile->UpdateScore`) should still run with `b8=false`. A null-server early-out
that still calls `profile->UpdateScore(...false)` is the likely match-neutral
native fix. **rb3-side, no engine change.**

---

## (3) "All Instruments" / vocals-alongside-guitar — NOT SUPPORTED by this harness

The original user phrasing was "All Instruments mode". In this native port there
is a **single local synth user** (`SynthUser()` in
`native/src/rb3_game_input.cpp:744 ExecTrack`); the `track:` verb switches that one
user's instrument. There is no path to run vocals AND guitar as two concurrent
local players through the headless harness — selecting vocals = the single user
plays vocals (the scout's interpretation). So "enable vocals alongside guitar" is
not testable here; I confirmed the single-user model from source rather than
fabricate a multi-player run. If true multi-instrument local co-op is a port goal,
it's a separate, larger piece of work (multiple BandUsers + per-slot input
routing) — flag for the roadmap, not this fix.

---

## Match-neutrality (spot-confirmed)
The impl doc reports `main/band3/game/Singer` and
`main/band3/bandtrack/VocalTrack` unchanged (56.523% / 45.507%) — all edits are
`#ifdef HX_NATIVE` or textually-identical Wii macro branches. I did not re-run
objdiff (read-only, no main-repo build), but the source guards are visibly
native-only, consistent with the claim.

---

## VERDICT

- **The assigned vocal-load crash fix: PASS.** Both crash layers (load SIGSEGV +
  in-gameplay SIGABRT) are gone on the composed build, reproduced twice, 0 crash
  signatures across ~42 s of vocal gameplay total. The vocal HUD (pitch tube,
  cursor, note tubes, score bar) renders and scrolls. The fix composes cleanly
  with the rest of wave 2 (no interaction observed).
- **NEW/residual issue (separate, P1): the endgame score screen aborts for ALL
  instruments** at `MetaPerformer::UpdateScores` (`MILO_ASSERT(TheNet.GetServer())`,
  null `mServer`). Pre-existing, not a vocal regression, but it means NO full
  song → score-screen playthrough completes natively today. Reproduced on both
  vocals (8862) and guitar (8863). This is the `endgame_helpers.dta(64):meta_performer`
  SIGABRT the crowd scout saw — confirmed, root-caused, fix direction given.

## Residual gaps / follow-ups
1. **(P1) Endgame `MetaPerformer::UpdateScores` SIGABRT** — wire `TheNet.mServer`
   or native-guard the `GetServer()` derefs. Blocks the entire score screen.
   `scripts/native/song-end-test.py --require-endgame` is the regression gate.
2. **(polish) Vocal lyric text** appears sparse/illegible in captures — verify it's
   the no-mic state vs a real lyric-render gap once a mic/ground-truth path exists.
3. **REFERENCE SCREENSHOT NEEDED:** a retail **vocals-gameplay** shot (pitch-tube
   HUD with lyrics + note arrows mid-song). `images/retail-screenshots/` has none;
   without it the vocal HUD can't be pixel-diffed. Capture from `../xenia`.

## Evidence index
- `/tmp/rp3-vocals/run1/play_*.png` + `/tmp/rp3-vocals/*_hud.png` — vocal HUD frames (clean 21 s run).
- `/tmp/rp3-vocals/run2/` — second clean vocal run.
- `/tmp/rp3-vocals/v2e/` + `/tmp/rb3-v2e-8862.log` — vocal full-song → endgame SIGABRT (DTA stack trace at log line ~6796).
- `/tmp/rb3-song-end-8863.log` — guitar full-song → IDENTICAL endgame SIGABRT (proves instrument-agnostic).
- `/tmp/rb3-repro-8861.log`, `/tmp/rb3-repro-8865.log` — clean 21 s vocal gameplay logs (0 crash signatures).
- `/tmp/rp3-vocals/vocal_to_end.py` — the vocal-to-end + endgame-stability harness I wrote.
