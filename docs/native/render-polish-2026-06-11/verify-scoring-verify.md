# Wave 6 — INDEPENDENT REVIEW: scoring-verify

**Verdict: CONFIRM.** Native scoring genuinely works end-to-end. Independently
reproduced the impl's central result (156,973 pts + 5 stars for "20th Century Boy")
AND cross-validated a second charted song. This was a NEGATIVE result (the scorer was
never broken; the wave-5 "0 score" was a `{game jump}` harness artifact) — there is no
crash to reproduce, so `crashReproducesPostFix = false` (N/A).

Reviewer: independent Opus adversarial verifier. Composed master build
(`7a0b9595`, scoring commit `736d89d5` in history, engine pin `15ce606`). Binary
`native/build-native/rb3-native` (rebuilt 2026-06-15 19:29). Evidence under
`/tmp/rp6rev-scoring-verify/`.

---

## What I verified independently (not trusting the impl doc)

### (a) `scripts/native/scoring-test.py` PASSES — 20th Century Boy, natural EOF

Ran the deliverable harness verbatim (port 9531, `RB3_SCORE_PROBE=1` for the
finalization log, `--shot`). It booted, navigated to song-select, started
20thcenturyboy (guitar/expert, nofail+autohit), and played to its **natural** end
(`songMs=236625`, NO `{game jump}`):

```
song ended naturally at songMs=236625; peak in-game band score=156973
endgame screen = coop_endgame_screen
RESULT: is_invalid_score=0 peak_in_game_score=156973 endgame_band_score=156973 endgame_band_stars=5
PASS: native scorer produced a real, non-zero endgame score (156973 pts, 5 stars).
```

Exit 0. This is byte-for-byte the impl's claim (156,973 pts + 5 stars,
`is_invalid_score=0`). The screenshot `scoring_test_endgame.png` (1280x720) visually
renders "20TH CENTURY BOY", "BAND HIGH SCORE", **156,973**, **5 filled gold stars**,
"100% / EXPERT / FLAWLESS", "SOLO SCORE 156,973 / High score!", CONTINUE / RESTART.

The `RB3_SCORE_PROBE` log at the finalization site (the exact value the coop_endgame
`score.scr` / `stars.sd` widgets bind-read) matches:
```
RB3 SCORE_PROBE: PerformerStatsInfo::Update ty=2  GetScore=156973 -> mScore=156973 mStars=5 streak=842 awesomes=842
RB3 SCORE_PROBE: PerformerStatsInfo::Update ty=10 GetScore=156973 -> mScore=156973 mStars=5
```

### (b) Live score CLIMBS during play (scorer is live, not an end-only snapshot)

The harness polls `{{beatmatch main_performer} score}` once per loop. The score rises
monotonically across the entire song, in lockstep with songMs — proving autohit
genuinely drives the hit→AddPoints pipeline frame by frame, not a single endgame
injection:

```
 songMs=0      score=0
 songMs=23474  score=8366
 songMs=53465  score=30217
 songMs=98234  score=60250
 songMs=158536 score=96793
 songMs=233627 score=156973   (peak, survives into the endgame screen)
```

### (c) Second charted song (25or6to4) — non-zero endgame score, NOT song-specific

To rule out a 20thcenturyboy-only fluke, I drove a different charted song to natural
EOF with the same autohit path (my `run_xcheck.sh`, port 9533). Verified the song at
`game_screen` via `{meta_performer song}` = **25or6to4** (a distinct chart):

```
GAME_SCREEN song=25or6to4
... live score climbs 2975 -> 17775 -> ... -> 277014 ...
ENDED songMs=296010 peak=277014 scr=coop_endgame_popups_screen
RESULT song=25or6to4 endscr=coop_endgame_screen is_invalid=0 peak=277014 endgame_score=277014 endgame_stars=5
PROBE: GetScore=277014 mStars=5 streak=1423 awesomes=1423
```

Screenshot `xcheck_antibodies.png` renders "25 OR 6 TO 4", **277,014**, 5 gold stars,
"SOLO SCORE 277,014 / High score!". A distinct, larger non-zero score (277,014 ≠
156,973) on an independent chart confirms the score path is generic, not tuned to one
song. (The down-nav landed on 25or6to4 rather than antibodies — fine; the point is a
SECOND charted song, and I confirmed it by name.)

## Crash check — clean

`crashReproducesPostFix = false` (this is a negative result; nothing crashes here).
Both full-length runs (240s + 480s of real gameplay each, with continuous autohit and
hundreds of `/api/dta/eval` polls) had **zero** aborts / SIGABRT / SIGSEGV / MILO_FAIL
in their logs. Endgame screen stable through the 3s settle + all post-game evals.

## Match-neutrality (independently re-checked)

The only shared-source edit is `src/band3/meta_band/MetaPerformer.cpp` lines 94-106:
the probe is wrapped in `#ifdef HX_NATIVE` AND gated on `getenv("RB3_SCORE_PROBE")`,
so the Wii MWCC build never sees it. I read the source to confirm the gating; the
impl's "MetaPerformer fuzzy 100.0 unchanged" claim is consistent with an
HX_NATIVE+getenv-gated debug `MILO_LOG`. `scoring-test.py` is a new test script (no
build impact). `wiiByteIdentical = true` is credible.

## Process hygiene

Used only ports 9531/9532/9533 (in my 9531-9539 range). Verified the two sibling
`task-dta-eval-crash` rb3-native procs (ports 9425/9426) and another agent's 9502 proc
were left untouched — I killed ONLY my own PIDs after confirming each one's
`RB3_HTTP_PORT` was in my range. No lingering procs in my range post-run. Pruned the
song-select mapping probe shots from the evidence dir (16M → 2.1M) to respect the
shared tmpfs quota.

## Residuals / follow-ups (NOT blocking — none are regressions)

- **score-detail backlog (already tracked, separate)**: the wave-6 backlog line
  "score-detail chain: zero autohit score, over-press `load_nextsong` SIGSEGV, endgame
  backdrop tint" — the "zero autohit score" item is now SETTLED (this review). The
  other two (score-screen over-press SIGSEGV — the synth off-by-one fixed separately in
  `b4bcbe39`; endgame backdrop tint) are genuine and out of scope here.
- **Solo-stats offline path**: the impl notes `AddSoloStats` is guarded behind a
  per-PROFILE check (`MetaPerformer.cpp:1127 if (profile)`) and headless boots are
  profile-less. The BAND score path (`UpdateBandStats`) ran unconditionally and is what
  the score-detail widget shows — both runs rendered the SOLO SCORE row too, so it was
  healthy on these runs, but the profile-less solo path is worth a note for any future
  per-player-stats work.
- **Endgame screen rendering**: visually the score-detail screen is correct (title,
  score, stars, accuracy, solo score, CONTINUE/RESTART all legible). No rendering gaps
  observed in either capture; the venue/backdrop is dark behind the panel (consistent
  with the separately-tracked endgame backdrop-tint backlog item).

## Verdict

**CONFIRM.** Independently reproduced the impl's headline result on the deliverable
harness (PASS, 156,973 / 5★, `is_invalid_score=0`), confirmed the live score climbs
during play (scorer is genuinely wired), and cross-validated a second charted song
(25or6to4 → 277,014 / 5★). Native scoring works end-to-end; the wave-5 "0 score" was a
`{game jump}` harness artifact as claimed. No crash exists to reproduce
(`crashReproducesPostFix = false`). The probe is HX_NATIVE+getenv-gated → Wii-neutral.
