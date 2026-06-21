# Venue-aware prewarm — build + measurement verification (2026-06-21)

Independent build + measurement pass over the venue-aware prewarm fix
([`VENUE_PREDICTION_FIX.md`](VENUE_PREDICTION_FIX.md)). Goal: confirm the random-
quickplay reveal-frame win reproduces from a clean build, that the prewarm targets
the **actually-loaded** venue (no wrong-venue waste), that the venue renders
correctly, and that there is no load-duration regression. **All confirmed.**

Branch **`wt-texwarm-venuepredict`** — fix at **`3686836e`** (rb3), probe artifact
at **`a8ea9eef`**. Engine **untouched** (`5cbe855`, no `MILO_ENGINE_PIN` bump).

## Builds (clean, from the worktree)

- **Native** (`cmake --build native/build-native --target rb3-native`): builds
  clean against the paired engine worktree
  (`milo-native-engine-worktrees/texwarm-venuepredict` @ `5cbe855`).
- **Web debug** (`scripts/web/build.sh --debug`,
  `MILO_ENGINE_PATH_OVERRIDE=...texwarm-venuepredict`): `rb3-web.wasm` 29M deployed
  to `native/web/build/debug/`. Served by `native/web/server.py --port 8931`.

## The mechanism — prewarm hits the loaded venue, not the phantom (native, RB3_TEX_PREWARM_DBG + VENUE_DBG)

Native quickplay run (`scripts/native/_venue-commit-probe.py`, prewarm ON):

```
[part_difficulty] get_venue=small_club_05  get_venue_override=no_venue_override
RB3_TEX_PREWARM: kicked background DirLoader for venue world/venue/small_club/small_club_01/small_club_01.milo
VENUE_DBG: EnterVenue force-loading venue='small_club_01'
DWELL: part_difficulty(commit) -> reveal = 16.28 s
```

The phantom `mVenue` (`MetaPerformer::GetVenue()`) was `small_club_05`, but the
prewarm correctly kicked **`small_club_01`** (resolved via the world-prop, exactly
what `EnterVenue` reads) and `EnterVenue` force-loaded **`small_club_01`** — match.
No `Can't share unloaded dir world/...` warning for the venue (only the unrelated
`ui/resource/fonts/default.milo`, present in both arms). The prototype's
wrong-venue 12.9 MB waste is **gone**.

## The win — random/default quickplay path (web debug, song `20thcenturyboy`)

Harness: `scripts/web/_framestall-texprewarm-ab.mjs --port 8931 --prewarm on|off`
(random venue — the harness forces only the *song*, never a venue). FRAME_TRACE
(wasm-side, no profiler observer effect). Two reproducible A/B pairs:

| metric (reveal frame) | OFF run1 | OFF run2 | ON run1 | ON run2 |
|---|---|---|---|---|
| reveal `dt` | **615.8 ms** | **721.2 ms** | **274.9 ms** | **242.5 ms** |
| reveal `lpu` (sync drain) | **532.6 ms** | **612.1 ms** | **178.4 ms** | **170.6 ms** |
| reveal `obj` (worst RndTex parse) | 170 (wainscoat) | 211 (floor_wood02) | 150 (wainscoat) | 143 (wainscoat) |
| longtasks >50ms (count / Σms) | 13 / 1317 | — | 11 / 1176 | — |

→ **reveal `lpu` −66% to −72%** (~530–610 ms → ~170–180 ms); **reveal `dt` −55%
to −66%**. The ON values (`lpu ~170–180 ms`) sit **well below** the OFF
run-to-run spread (`lpu 530–612 ms`), so — unlike the prototype — the win is
**separable from venue-size variance**. The two OFF runs even loaded the venue's
textures in a different order (`wall_wainscoat` vs `floor_wood02` as worst object),
the exact variance the venue-aware targeting now defeats.

## No regression, venue renders correctly

- **Load duration:** web longtask Σ *dropped* (OFF 1317 ms → ON 1176 ms); the
  prewarm only moves the venue fetch earlier into genuinely-idle dwell frames.
  Reveal-frame index is comparable across arms (no added load latency); gameplay
  reaches `songMs > 2000` in every native run.
- **Render:** native gameplay screenshot (`/tmp/twvp-gameplay.png`) shows the note
  highway with gems + now-bar + venue background, all textures present, no
  missing-texture artifacts. (The bright native-`-O2` venue look is the separate,
  known venue-lighting lever — not a prewarm artifact.)

## Residual (unchanged from the fix doc — separate lever)

ON still pays `lpu ~170–180 ms` / `obj ~143–150 ms` on the reveal frame — the
venue **object-parse** (RndTex PostLoads of the wainscoat/wood normal maps).
`DirLoader::Find` shares the *loaded* loader, but the dir-object *construction*
still runs there. Eliminating it needs the parse spread across rAF ticks (so
`PollUntilLoaded` returns to the scheduler and `PumpAudio` runs between slices) or
a pre-parsed/cached scene — venue-size-independent, out of scope here.

## Match-neutrality (re-confirmed)

- `native/src/rb3_gamewarm_native.cpp` — native-only glue TU.
- `src/band3/game/Game.cpp` — only edit is `RB3VenuePrewarmPoll()` call + extern
  decl, both inside `#ifdef HX_NATIVE`. Wii byte-identical.
- `src/system/bandobj/BandDirector.cpp` `VENUE_DBG` logging is pre-existing
  (`MILO_LOG`, diagnostic), not added by this change.
- Engine (`milo-native-engine`) untouched (`5cbe855`).

## Repro

```bash
# native (mechanism): prewarm kicks the loaded venue, not the phantom
RB3_TEX_PREWARM_DBG=1 VENUE_DBG=1 python3 scripts/native/_venue-commit-probe.py

# web A/B (the win): serve the worktree debug build, run both arms
python3 native/web/server.py --port 8931
node scripts/web/_framestall-texprewarm-ab.mjs --port 8931 --prewarm off --play 20
node scripts/web/_framestall-texprewarm-ab.mjs --port 8931 --prewarm on  --play 20
#   -> docs/native/frame-stall-2026-06-20/cap/texprewarm-{on,off}/tickprobe.json
```

Flags: `RB3_TEX_PREWARM_OFF=1` disables; `RB3_TEX_PREWARM_DBG=1` logs the kicked
path; `VENUE_DBG=1` logs the venue `EnterVenue` force-loads.
