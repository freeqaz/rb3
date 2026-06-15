# Wave 6 — scoring-verify: does native SCORING actually work?

**Verdict: native scoring WORKS. The wave-5 "0 score under autohit" was a HARNESS
ARTIFACT (case a), not a broken scorer (case b).** Proven by playing a song to its
natural end with continuous autohit: the score-detail screen renders **156,973 pts
+ 5 stars** for "20TH CENTURY BOY". Deliverable = a reusable real-end scoring
harness `scripts/native/scoring-test.py`.

Worktree: `.claude/worktrees/task-scoring-verify` branch `wt-task-scoring-verify`,
commit `aaaf4c0b`. Evidence under `/tmp/rp6-scoring-verify/`.

---

## The open question (from verify-score-detail.md, residual 1)

> "Zero score values under autohit — harness artifact; widgets render, only the
> numbers are 0; nofail+autohit yields a 0 score; to validate non-zero numbers a
> real-note-hit harness is needed." — does native scoring produce a real, non-zero
> score when notes are actually HIT, or is scoring itself broken?

## Answer: CASE (a) — the harness, not the scorer

### 1. The scorer is live during gameplay (autohit feeds it)

`probe_autohit_score.py` boots to gameplay (guitar/expert), turns on nofail+autohit,
and polls the LIVE score off the active Player via `/api/dta/eval`. The score and
hit counters climb monotonically (two independent runs):

```
  t(s)  songMs   score  notes_hit  streak  auto_play
   0.1     2458       0          0       0          1
  10.5    12882    1855         26      26          1
  24.0    26468   12066         76      76          1
  44.9    47303   26682        164     164          1   <- run 1
   19.0   21044    6269         50      50          1   <- run 2 (reproduced)
```

Autohit is a **genuine hit path**, not a synthetic number injection:
`Player::SetAutoplay(true)` -> `BeatMatcher::SetCheating(true)` ->
`TrackWatcherImpl::CheckForAutoplay` (TrackWatcherImpl.cpp:452) -> `HitGem(...)` ->
`GemPlayer::Hit` (GemPlayer.cpp:496 `AddHeadPoints`) -> `Performer::AddPoints`
(Performer.cpp:240 `mScore += add_points`). It is the SAME pipeline a real strum
drives, with the timing decided by the game. `awesomes=842 streak=842` at the end
of a full song are real per-gem hit counts.

The score scoreboard (`TrackPanel.cpp:611`) reads exactly this `mScore` via
`mainPerformer->GetAccumulatedScore()`.

### 2. Why the endgame screen showed 0 with the old harness — ROOT CAUSE

`song-end-test.py` and `keyboard-to-gameplay.py` reach end-of-song with
`{game jump <past-end>}`. That fast-forward is the engine's **rewind/replay
primitive**, and it zeroes the score by design:

```
Game::Jump(f, true)                        Game.cpp:629
  -> mBand->Restart(false)                 Band.cpp:119  -> mBandPerformer->Restart()
       -> Performer::Restart(b)            Performer.cpp:152:  mScore = 0;   <-- HERE
  -> (each Player)->Jump -> JumpReset       GemPlayer.cpp:1413
```

So the band/player `mScore` is reset to 0 by the jump, *then* the jump lands at EOF,
game-over fires, and `MetaPerformer::TriggerSongCompletion` (MetaPerformer.cpp:1100)
snapshots the now-ZEROED score into the coop_endgame results widget via
`PerformerStatsInfo::Update` (reads `performer->GetScore()`, MetaPerformer.cpp:90).
Hence `score=0 / num_stars=0` on the score-detail screen — purely the jump.

This was confirmed directly with an `RB3_SCORE_PROBE`-gated log at the finalization
site. With the jump harness:

```
PRE-JUMP: band_score=14458 (live)      ... then {game jump 600000} ...
SCORE_PROBE: PerformerStatsInfo::Update ty=2  GetScore=0 -> mScore=0 mStars=0   <- finalized 0
ENDGAME STATE: is_invalid_score=0 frac_completed=1.0 num_active_players=1 is_loaded=1
```

is_invalid_score is 0 (the score is *valid*, just *zeroed by the restart*), so the
widget DOES try to show it — it just shows 0.

### 3. Played to NATURAL end (no jump) — the score is real and survives

`scripts/native/scoring-test.py` plays the highlighted song (20thcenturyboy, ~170s)
fully with continuous autohit and reads the EXACT bindings the coop_endgame widget
uses (`ui/endgame/endgame_helpers.dta`: `score.scr set_values {{beatmatch
main_performer} score}`, `stars.sd ... num_stars`):

```
song ended naturally at songMs=236612; peak in-game band score=156973
endgame screen = coop_endgame_screen
RESULT: is_invalid_score=0 peak_in_game_score=156973 endgame_band_score=156973 endgame_band_stars=5
PASS: native scorer produced a real, non-zero endgame score (156973 pts, 5 stars).
```

Finalization log (the value the widget reads):
```
SCORE_PROBE: PerformerStatsInfo::Update ty=2  GetScore=156973 -> mScore=156973 mStars=5 streak=842 awesomes=842
SCORE_PROBE: PerformerStatsInfo::Update ty=10 GetScore=156973 -> mScore=156973 mStars=5
```

Visual: `/tmp/rp6-scoring-verify/scoring_test_endgame.png` (+ `natend_score_9434.png`)
— the score-detail screen renders "20TH CENTURY BOY", **156,973** with **5 filled
stars**, "BAND HIGH SCORE", "100% / EXPERT / FLAWLESS", "SOLO SCORE 156,973 / High
score!", CONTINUE / RESTART. Deterministic 156973/5★ across both natural-end runs.

## The scoring chain (for reference)

```
gem hit -> GemPlayer::Hit (GemPlayer.cpp:496)
        -> AddHeadPoints  (GemPlayer.cpp:2234) : pts = i3 * GetHeadPoints / GetChordPoints (+ pro-drum bonus)
        -> AddPoints      -> Performer::AddPoints (Performer.cpp:217)
             - early-out: if mStats.FailedNoScore() (mNoScorePercent != 0) -> return  (NOT triggered by plain nofail)
             - mScore += points * multiplier
scoreboard: TrackPanel.cpp:611  mainPerformer->GetAccumulatedScore() == mScore
endgame:    MetaPerformer::TriggerSongCompletion -> info.UpdateBandStats(perf) -> PerformerStatsInfo::Update reads perf->GetScore()
            coop_endgame widget binds {{beatmatch main_performer} score / num_stars}
```

## Files changed (worktree `wt-task-scoring-verify`, commit `aaaf4c0b`)

- **`scripts/native/scoring-test.py`** (new, +~230 lines) — the deliverable real-end
  scoring harness. Boot -> play to natural EOF (NO jump) with continuous autohit ->
  assert in-game score climbs AND the coop_endgame score-detail screen has a
  non-zero, valid band score + stars. Exit 0 = scorer works. Self-contained stdlib
  HTTP driver. `--shot <png>` saves the score-detail screenshot. PASS on the
  composed build. (Documents the jump-zeroes-score gotcha in its header so future
  agents don't re-derive it.)
- **`src/band3/meta_band/MetaPerformer.cpp`** (+13 lines) — an `RB3_SCORE_PROBE`-
  gated `MILO_LOG` inside `PerformerStatsInfo::Update` logging the exact
  `GetScore()/mStars/streak/awesomes` captured into the endgame widget at
  finalization. **HX_NATIVE-only + env-gated** -> the Wii MWCC build is byte-
  identical. Verified: MetaPerformer fuzzy match% unchanged at **100.0** before and
  after (worktree `build/SZBE69_B8/report.json`, rebuilt via `tools/ninja-locked`).

## Wii byte-identity

The only shared-source edit (`MetaPerformer.cpp`) is `#ifdef HX_NATIVE` + `getenv`-
gated, invisible to the Wii build. objdiff/report fuzzy match% for MetaPerformer
stays **100.0** (`total_code=45012 matched_code=44808` unchanged). `wiiByteIdentical
= true`.

## Verification

- Two independent autohit runs show the live in-game score climbing non-zero
  (nonzero_score=True, notes_hit_climbed=True both times).
- Two independent natural-end runs reach `coop_endgame_screen` with band_score=156973
  + 5 stars (deterministic for autohit on this song).
- `scripts/native/scoring-test.py` PASSES with NO instrumentation env required.
- Finalization instrumentation proves the widget reads exactly 156973/5★ (natural
  end) vs 0/0★ (jump) — pinpointing the jump as the artifact.
- 0 crashes / aborts across all runs; endgame screen stable.

## Notes for the orchestrator / wave-6 backlog

- This is the answer to the wave-6 backlog line "score-detail chain: zero autohit
  score" — it is NOT a scorer bug; the autohit number is only 0 in harnesses that
  fast-forward with `{game jump}`. Real play (or any harness that lets the song end
  naturally) scores correctly. `scripts/native/scoring-test.py` is the gate.
- The OTHER two items on that backlog line are genuine and SEPARATE (out of scope
  here, both confirmed pre-existing): over-press `load_nextsong` SIGSEGV on the
  score screen, and endgame backdrop tint.
- `MetaPerformer::TriggerSongCompletion` only adds per-PROFILE solo stats
  (MetaPerformer.cpp:1127 `if (profile)`); headless boots profile-less, so the
  per-player SOLO results path (`AddSoloStats`) may be skipped offline — but the
  BAND score path (`UpdateBandStats`, line 1150) runs unconditionally and is what
  the score-detail widget shows. The 156973/5★ above are via the band path; the
  solo widget rendered "SOLO SCORE 156,973 / High score!" too, so both are healthy
  on this run.
