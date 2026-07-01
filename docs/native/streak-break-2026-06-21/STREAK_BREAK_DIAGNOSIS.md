# Streak / combo breaks at every bar — diagnosis

**Date:** 2026-06-21
**Repo HEAD:** `7d8b51a2` (`fix(native): bound in-song RSS growth`)
**Reported bug:** During gameplay, the player's STREAK/COMBO breaks at the END of
EVERY BAR (measure boundary), even when no notes are missed, preventing the 4x
multiplier. The regularity (every bar, not random) was the key clue.

---

## TL;DR / verdict

**The per-bar streak break does NOT reproduce on HEAD (`7d8b51a2`).** Two
independent headless reproductions — one with engine **autohit** and one with
**real strum input** — both failed to produce a single spurious combo break:

| Run | Input | Result |
|---|---|---|
| Autohit (kiosk autoplay), full song to natural end | engine auto-hits every gem | streak built **monotonically to 842**, **5 stars**, 156,973 pts. `EndHitStreak` fired **once**, at song-end finalize. **Zero** per-bar breaks. |
| Real strum input: 129 empty overstrums + ~100 un-hit passed gems, 40-65s | `pad:` HTTP strum queue (no autoplay) | **Zero** `EndHitStreak` fires from any site (423 / 670 / Stats direct). Streaks, when built, climbed strictly monotonically. |

The streak only resets through `Stats::EndHitStreak()`, called from exactly three
code sites (below). All three were instrumented with `#ifdef HX_NATIVE` printfs and
**none fired per-measure** under either input mode.

There is **no per-measure / downbeat / beat event anywhere in the combo path** —
the measure system (`BeatMaster::CheckBeat`) fires `"downbeat"` DTA callbacks only;
it never touches the streak, the multiplier, or the gem-judgment.

**Conclusion: the reported symptom, as worded, is not present on HEAD.** Either it
was already fixed by an intervening change, it is specific to a play configuration
the two harnesses cannot exercise (the only untested path is a *built* real-input
streak that then breaks — blocked by a harness limitation, see "Residual gap"), or
the original report conflated something else (e.g. the multiplier *meter pulse* /
overdrive deploy visuals) with a combo break.

This is a **DIAGNOSIS, not a fix** — per the task. No source fix was landed. All
investigation instrumentation has been reverted; the working tree is clean and
`rb3-native` rebuilds green.

---

## How the combo / multiplier actually works (so we know what to look for)

The on-screen multiplier the player watches is a *pure function of the current hit
streak* — there is no independent "multiplier" state and no per-bar reset anywhere:

- The streak is `Stats::mCurrentHitStreak.mDuration`
  (`src/band3/game/Stats.cpp:150` `GetCurrentStreak`).
- Each clean hit calls `Stats::BuildHitStreak` → `BuildStreak` → `mDuration++`
  (`Stats.cpp:142,175`).
- The displayed multiplier = `Player::GetIndividualMultiplier()`
  (`src/band3/game/Player.cpp:601`):
  `mult = TheScoring->GetStreakMult(streak, ...)`, clamped to `mMaxMultiplier`.
  `GetStreakMult` (`src/band3/game/Scoring.cpp:210`) maps streak → 2x/3x/4x via the
  config threshold list. So the multiplier rises with streak and **only drops when
  the streak is reset to 0.**
- The visual streak meter is driven once-per-change from `Track::Poll`
  (`src/band3/bandtrack/Track.cpp:58-77`): it calls `bandtrack->SetStreak(...)` only
  when `curstreak != mLastStreakCount`. `BandTrack::SetStreak`
  (`src/system/bandobj/BandTrack.cpp:328`) calls `mStreakMeter->BreakStreak(...)`
  **only when `multiplier == 1`** — i.e. the visible "streak break" animation is
  itself gated on the streak having actually reset. There is no standalone
  beat-driven break animation in `StreakMeter`.

**Therefore a visible combo break == `Stats::EndHitStreak()` was called.** That is
the single chokepoint, and it has exactly three callers reachable during gameplay:

1. `GemPlayer.cpp:425` (`Hit`, **PartialHit** branch) — a multi-slot gem where the
   player hit only some slots: `gemSlots != gem_hit_slots` → `PartialHit` →
   `EndHitStreak()` + `BuildMissStreak`.
2. `GemPlayer.cpp:683` (`Pass`, **phantom-miss** branch) — a gem scrolled past
   un-hit with `phraseFlags == 0` and `!ShouldPenalizeGem` → `EndHitStreak()`.
3. `Player::FinalizeStats` → `Stats::EndHitStreak` (`Player.cpp:932`) — end-of-song
   / `Penalize` finalize. Not a gameplay break.

(Vocals end the streak in `VocalPlayer.cpp:351`; out of scope for a guitar player.)

An **overstrum** — a strum with no gem in the ±`mSlop` (100 ms) window — routes
through `JoypadTrackWatcherImpl::Swing` → `OnMiss` → `SendMiss` →
`GemPlayer::Miss` → `BuildMissStreak`. Note `Miss` does **not** itself call
`EndHitStreak` (the streak is broken by the *passed* gem at site 2, or by a partial
hit at site 1). This matters: pure overstrums in gaps build a *miss* streak but
the *hit* streak is only ended by sites 1/2/3.

---

## Evidence: streak-vs-time traces

### Run A — autohit (full song, natural end)

Instrumentation: `#ifdef HX_NATIVE` printf in `Stats::EndHitStreak`,
`GemPlayer.cpp:425`, `GemPlayer.cpp:683`. Driver:
`scripts/native/scoring-test.py` (nav → guitar/expert on **20thcenturyboy** (a real
charted song, `get_gem_count`≈805) → `nofail` → `autohit`, plays to natural end, NO
jump).

Result — the entire ~236 s run produced exactly **one** `EndHitStreak`, at finalize:

```
RB3 Native: frame 15744 complete
STREAKDBG Stats::EndHitStreak call=1 streakBefore=842 persistentBefore=842
RB3 screen: frame 15745  currentScreen = 'endgame_waiting_screen'  (in transition)
```

`site=423` fires: 0. `site=670` fires: 0. Endgame: `band_score=156973`,
`band_stars=5`, `is_invalid_score=0`. The streak ran to **842 unbroken** → the 4x
multiplier was reached and held. **No per-bar break.**

### Run B — real strum input (no autoplay)

Driver: launched `rb3-native RB3_HTTP=1 MILO_HEADLESS=1`, navigated to
guitar/expert on 20thcenturyboy, `nofail` on, **no autohit**, then injected real
input via the `pad:<bit>` HTTP strum queue (`RB3JoypadEnqueuePad`, single-bit
clean-edge presses) — 129 empty strum-downs (`pad:14` = `kPad_DDown`) plus a run
that let ~100+ gems pass un-hit (`percent_complete` 5%→13%).

Result: **zero `STREAKDBG` lines** across every scenario, from every site. Gems
passing un-hit took the `Penalize` branch (`GemPlayer.cpp:676`) or the
`phraseFlags != 0` branch (`GemPlayer.cpp:688`), **not** the instrumented site-670.

Methodology note (true negative, not a capture artifact): the first pass printed to
**stdout** but native `MILO_LOG`/`OSReport` go to **stderr**
(`native/src/rvl_shims.cpp:30`); the printf was switched to
`fprintf(stderr)+fflush` and capture was then proven (stderr/stdout both captured,
boot/`frame N complete` lines visible) — fires were still zero.

---

## Why the suspect commits are not the cause

| Commit | Claim | Assessment |
|---|---|---|
| `d6f56d23` "lock note-highway clock to audio (kill Wii -20ms AV-cal)" | shifts gameplay clock +20 ms | **Not the cause.** The shift is applied to the single `songMs` in `Game::Poll` (`Game.cpp:1712-1719`) that drives BOTH the highway scroll AND `mMaster->Poll(songMs)` (the beatmatch judge clock `BeatMatcher::mNow`). Gem visual position and the judge clock move **together**; the player strums to the (shifted) visuals and the gem is judged at the same (shifted) position. The hit window is ±100 ms (`mSlop`), so a coherent 20 ms offset cannot cause misses. The commit's own test recorded "full-song autohit → 156973 pts / 5 stars" — matching Run A exactly. It introduces no judge-vs-position divergence. |
| `f41ab48b` "N8 hit/flame FX — autohit verb + fix autoplay node-overrun/ud2" | autoplay path / `HitGemHook` returns | **Not the cause.** The `GemPlayer::Hit` changes only widen a `Message`'s arg-array by one slot (`Node(5)` was an OOB write, now in-bounds) — no streak/judgment effect. `JoypadTrackWatcherImpl::HitGemHook` / `TrackWatcherImpl::HitGemHook` just return a defined `0.0f` (the caller discards it; it already ran `ResetChordInProgress()`). All additive `#ifdef HX_NATIVE`. |
| `809fe00d` / `691f8eff` ".vfv guard" | skips a vocal sidecar open | **Not the cause.** Vocal-feature-vector data only (`TalkyMatcher`); behaviour-identical to the open-and-fail it replaces; doesn't touch guitar gems/streak. |
| `7f7262d0` "no-chart song track-load SIGABRT (TrackInfo OOB)" | early-return for songs with no chart | **Not the cause.** Only affects the ~80 *chartless* songs (returns before any track parse). A charted song (the only kind that has gems to streak) is unaffected. |
| `7d8b51a2` "bound in-song RSS growth" (HEAD) | SFX PCM cache + `malloc_trim` every 240 frames | **Not the cause.** Native memory hygiene only; `malloc_trim` cadence is 240 frames (~4 s), not per-bar, and doesn't touch the song clock or gem judgment. |

`BeatMaster::CheckBeat` (`src/system/beatmatch/BeatMaster.cpp:185-208`) is the only
per-measure machinery: on a measure change it sets the `measure` DataVariable and
fires the `"downbeat"` DTA callback. **It never calls into the streak, the
multiplier, the gem list, or any miss/pass path.** A "TrackInfo/measure-array
off-by-one firing a spurious event per measure" (a hypothesis in the brief) does
not exist in the combo path on HEAD.

---

## Residual gap (the one scenario the harnesses could not test)

Both runs are conclusive for "autohit streak" and "overstrum / passed-gem under no
autoplay," but **neither could test a real-input *built* streak that then breaks
per-bar**, because:

- The native `pad:<bit>` queue (`native/src/rb3_joypad_native.cpp`,
  `RB3JoypadEnqueuePad`) is strictly **single-bit, one press at a time** with a
  forced release between presses. It physically cannot hold a fret + strum
  simultaneously, so it can never *hit* a real gem — a real streak can't be built
  through it.
- `autohit` is a sticky latch (`Player::SetAutoplay(true)` with no off path,
  `native/src/rb3_game_input.cpp:803`), so it can't be toggled mid-song to seed a
  streak and then hand off to real input.

If the bug is real for a live keyboard/guitar player, the most plausible
*remaining* mechanism is **site 1 (`GemPlayer.cpp:425`, PartialHit)**: a player who
holds a *chord* and strums could, on the shifted clock, register a partial hit
(some slots) on a chord that sits at/near bar boundaries — `gemSlots != gem_hit_slots`
→ `EndHitStreak`. This is the one site that an imperfect *real* chord strum can hit
that autohit (always full-slot) and the single-bit harness (never chords) both
cannot. **Recommended next step before any code change:** reproduce with a real
keyboard (multi-key chord + strum) on 20thcenturyboy/expert, or extend the harness
to a multi-bit press queue, and watch site 1 vs site 2.

---

## Proposed action

1. **Do not land a code fix yet.** There is no reproduced root cause on HEAD; a
   speculative edit to `src/band3/game/` (Wii-match-sensitive gameplay logic) risks
   breaking the byte-identical Wii build for a symptom that isn't currently present.

2. **Close the residual gap with a real chord-capable repro** (multi-bit input)
   before concluding. Concretely: extend `RB3JoypadEnqueuePad` / add a
   `chord:<mask>` HTTP verb that holds a fret-mask + strum together for a few polls,
   drive it at the gem times on 20thcenturyboy/expert, and re-instrument the three
   `EndHitStreak` sites (the exact temporary printfs used here are recorded above).
   If **site 1 (PartialHit)** lights up at bar boundaries, the fix is a native-only
   chord-hit timing/coalescing issue, *not* a Wii-logic change.

3. **If it still does not reproduce**, treat the report as stale / mis-attributed
   (likely already addressed by `d6f56d23`, whose own measurement shows a clean
   5-star autohit run) and ask the reporter for the exact song, instrument,
   difficulty, build (native vs web), and whether they were using aids.

---

## Files referenced (all absolute)

- `/home/free/code/milohax/rb3/src/band3/game/Stats.cpp` — `EndHitStreak` (the
  single combo-reset chokepoint), `BuildHitStreak`, `GetCurrentStreak`.
- `/home/free/code/milohax/rb3/src/band3/game/GemPlayer.cpp` — `Hit` (PartialHit
  site, line ~425), `Pass` (phantom-miss site, line ~683), `Miss`, `Penalize`.
- `/home/free/code/milohax/rb3/src/band3/game/Player.cpp` — `GetIndividualMultiplier`
  (line 601), `PollMultiplier` (207), `FinalizeStats` (914).
- `/home/free/code/milohax/rb3/src/band3/game/Scoring.cpp` — `GetStreakMult` (210).
- `/home/free/code/milohax/rb3/src/band3/bandtrack/Track.cpp` — streak meter drive
  (line 58-77).
- `/home/free/code/milohax/rb3/src/system/bandobj/BandTrack.cpp` — `SetStreak`
  (328), the `multiplier == 1` → `BreakStreak` visual.
- `/home/free/code/milohax/rb3/src/system/beatmatch/JoypadTrackWatcherImpl.cpp` —
  `Swing` / `OnMiss` overstrum judgment.
- `/home/free/code/milohax/rb3/src/system/beatmatch/TrackWatcherImpl.cpp` —
  `OnHit`/`OnMiss`/`OnPass`, `CheckForAutoplay`, `InSlopWindow` (297, ±`mSlop`=100ms).
- `/home/free/code/milohax/rb3/src/system/beatmatch/BeatMaster.cpp` — `CheckBeat`
  (185-208): the ONLY per-measure machinery, fires `"downbeat"` DTA callback only,
  does not touch the streak.
- `/home/free/code/milohax/rb3/src/band3/game/Game.cpp` — `Poll` clock derivation
  (1700-1722); the single `songMs` driving both scroll and judge clock.
- `/home/free/code/milohax/rb3/native/src/rb3_synth_native.cpp` —
  `RB3ApplyNativeAVCalibration` (the `d6f56d23` +20 ms clock shift).
- `/home/free/code/milohax/rb3/native/src/rb3_joypad_native.cpp` — `RB3JoypadEnqueuePad`
  (single-bit `pad:` queue; the chord limitation).
