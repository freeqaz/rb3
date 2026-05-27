# RB3 native — v1 milestone tracker (one song end-to-end)

**Goal:** play one song end-to-end on Linux x86_64 — audio + venue + HUD +
scoring, single instrument (guitar is fine). This is the v1 milestone from the
roadmap's Goal & non-goals.

**Guiding principle (non-negotiable, carried over from boot-to-song):** avoid
hacks. Retain the actual game code as much as possible. Diverge only where
native platform differences force it, gated `#ifdef HX_NATIVE`. Mirror DC3 where
a sister fix exists; port DC3's `HX_NATIVE` blocks, don't reinvent.

This doc is the **durable handoff artifact for v1**. The completed predecessor
milestone is `BOOT_TO_SONG.md` (boot → menus → `Game::LoadSong` reached).

---

## Starting state (entry condition for v1 — verified 2026-05-27)

The boot-to-song milestone is COMPLETE. The real game `App` boots end-to-end
through synthetic headless input, reaches `Game::LoadSong()`, runs into
`SongData::Load`, and stops gracefully at the absent `.mid` chart. See
[BOOT_TO_SONG.md](BOOT_TO_SONG.md) for the full per-layer fix history.

**Reproducible:**
```
RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=700 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@450:msg:overshell:end_override_flow:1:0" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

**All regression guards GREEN** (must remain green after every v1 change):
- `RB3_BOOT` → `SystemInit OK — (227 entries)` + `boot complete.`
- `RB3_RENDER_MESH ui/track/gen/tracksystem_meshes.milo_xbox` → `129 meshes, 27878 tris` + PNG
- `rb3-dta songs/songs.dta` → 138 nodes

---

## Asset constraint — the v1 BLOCKER that isn't code

The 360-ARK extraction (`rb3/orig-assets/extracted/`) contains song **visual
milos** (`songs/<id>/gen/<id>.milo_xbox`, 293 files) and a parsed `songs.dta`
(138 entries) — **but ZERO `.mogg` audio and ZERO `.mid` charts**, anywhere on
disk. `rb3/orig/SZBE69_B8/` has only `main.dol` + a zero-magic `.sel` placeholder
(25 MB; no real Wii `.ark` data parts).

**v1 requires obtaining a single song's `.mogg` + `.mid`** (or two, for variety).
Options, in roughly-preferred order:

1. **Locate the real RB3-Wii `.ark` data parts** (`band_r_wii_0.ark`, `_1.ark`,
   …) and re-run `scripts/milo/extract_ark.sh` with them. This is the canonical
   path — the existing `arkhelper`-based extractor already handles Wii `.hdr`/
   `.sel`. **Action:** check if the Wii .ark data is obtainable from the
   original Wii disc image / NAND backup. If yes, re-extract and the asset gap
   closes for the *entire 138-song setlist*.
2. **Extract from a different RB3 platform** (DC3-style 360 retail .ark — if it
   has audio/MIDI, the cached `.mogg`/`.mid` formats are platform-shared with
   per-song variations). The current 360-ARK extract apparently lacked them
   (only the visual milos came through) — investigate whether `arkhelper` was
   run with a complete vs partial ARK, or whether the available 360 ARK file is
   incomplete.
3. **Single-song substitute**: a community-extracted RB3 song pack (`.mogg`+
   `.mid` for one song) dropped into the right `songs/<id>/` layout. The
   load code path already targets `songs/<id>/<id>.mogg` and the `.mid`. Just
   one song unblocks the full v1 milestone proof.
4. **Custom-author a minimal `.mid`+`.mogg`** for one song id (synthesize a
   short audio + chart). Heavier, but bullet-proof for the milestone proof.

**Until a song's assets exist, v1 cannot complete** — the milestone is asset-
gated, not code-gated. But there's substantial code work to prepare for the
moment a song's assets appear, listed below.

---

## Task graph

| # | Task | Tag | Depends on | Acceptance |
|---|------|-----|-----------|------------|
| **V1** | Procure `.mogg`+`.mid` for at least one song | external | — | the files exist on disk in `songs/<id>/`, the cached layout `arkhelper` produces |
| **V2** | Phase 3 audio backend wired up (engine `NativeSynth` reconciled with RB3, or RB3-native audio glue) — RB3 currently uses headless null `Synth` (`rb3_synth_native.cpp` returns base `Synth`) | Opus | — | `RB3_RENDER_MESH` regression stays green; a `BringUpSynth` test plays silence at correct period; sample asset (V1) decodes to PCM via `VorbisReader`/`StreamReceiver` |
| **V3** | Bring up the residual ~20 excluded gameplay TUs clang-LP64-clean (so factories register + objects construct + Load is byte-correct) | Opus | — | each TU removed from `_NATIVE_FORK_EXCLUDE` in `native/CMakeLists.txt`; its weak stubs removed from `band3_link_stubs.s`; build clean |
| **V4** | Fix the venue-character `Draw()` crash (`Character::DrawLodOrShadow`→`RndMesh::SetUpdateApproxLight`) — the Phase 2 RB3 render blocker for animated characters | Opus | — | `RB3_GAME` runs without the sigsetjmp draw-guard catching char-draw crashes; the menu venue band-preview renders (PNG) |
| **V5** | `Game::LoadSong` runs to completion on a real song (with V1's assets) — fix any Load byte-correctness in `SongData`/`MidiParser`/`BeatMaster`/`GemPlayer` paths | Opus | V1, V3 | `Game::LoadSong()` returns; `BeatMaster` populated with parsed MIDI events; song milo loaded; `Game::mLoadState` advances past `kLoadingSong` |
| **V6** | Audio plays through the speakers at correct pitch/speed; `songMs` advances | Opus | V1, V2, V5 | a sample `.mogg` is audible through miniaudio (or the chosen backend); `songMs` matches wall-clock; Phase 3 acceptance met |
| **V7** | Gem-track HUD rendering (RB3-specific `GemPlayer`/`GemTrackDir` paths — `GemTrackDir` already brought up in boot-to-song) | Opus | V3, V4 | the gem-track milo renders with notes positioned from the MIDI; one frame's worth scrolls past at songMs rate |
| **V8** | Scoring + hit detection driven by `songMs` | Opus | V6, V7 | a recorded synthetic-input script hits N notes; score advances; Phase 5 acceptance met |
| **V9** | One song completes end-to-end (the v1 milestone) | Sonnet | V6, V7, V8 | a reproducible `RB3_GAME_INPUT` script picks a song, plays through, returns to results; PNG captures of frames at start/mid/end |

V2/V3/V4 are independent and parallelizable. V5-V9 form the play-through chain.

---

## V2: Audio backend (Phase 3) — concrete

**Current state:** RB3 supplies `rb3_synth_native.cpp` with `CreateNativeSynth()`
returning a base headless `Synth` (a no-op null synth) — the boot-to-song path
runs the real `SynthInit`/`SynthPreInit` against it and works. **No audio plays.**

The engine has `milo-native-engine/src/platform/Synth_Stub.cpp` with a real
miniaudio-backed `NativeSynth` (used by DC3) that decodes `.ogg`/`.mogg` via
`StandardStream`. It's EXCLUDED from the RB3 link because **"RB3 StandardStream
ctor differs; the synth path pulls synth/tomcrypt"** (`native/CMakeLists.txt`
line ~115). Reconciling that is V2's first sub-task.

**Sub-tasks:**
- **V2.1** Reconcile RB3's `StandardStream` ctor with the engine's. Either
  adapt the engine `NativeSynth` to RB3's older shape (HX_NATIVE branches),
  OR write an RB3-native `NativeSynth` alongside `rb3_synth_native.cpp` that
  uses the engine's `GpuDevice`-adjacent miniaudio path but RB3's stream types.
- **V2.2** Wire `VorbisReader` for `.mogg` decode. DC3 Sessions 62-67 ported it
  natively; mirror those HX_NATIVE blocks into `rb3/src/system/oggvorbis/`.
- **V2.3** Hook `StreamReceiver` + `AudioDevice` ring buffer (engine code
  exists). Verify RB3's `Synth::Init` actually wires the device for the
  non-null-synth path.
- **V2.4** `songMs` clock — verify the engine timer drives `TheTaskMgr.SongMs()`
  on native (the headless UI clock fix landed in boot-to-song; check whether it
  affects songMs).

**Acceptance:** swap `CreateNativeSynth()` from base `Synth` to the real
backend; `BringUpSynth` test path plays silence at correct period; V1's sample
`.mogg` decodes to PCM via `VorbisReader`.

---

## V3: Residual ~20 clang-LP64-gap gameplay TUs

These are in `_NATIVE_FORK_EXCLUDE` (`native/CMakeLists.txt` ~295) and currently
weak-stubbed in `native/src/band3_link_stubs.s`. Each needs the same K2-style
bring-up rndobj/synth/MetaPanel got: remove from exclude → fix clang errors with
additive `HX_NATIVE` (K2 patterns: dependent-base `using`, switch jump-over-init
braces, `vector<T,unsigned short>` 2-arg, `Symbol("name")` for POSIX-colliders,
`MSL_Common/extras.h`→`<cstring>`, missing `return` in ptr-returning fns, Wii-GX
include/call gating) → remove its weak stubs from `band3_link_stubs.s`. Some
have MWCC paired-singles asm needing `#ifndef __MWERKS__` C++ fallbacks
(established for `CharForeTwist`/`CharHair`/`BandIKEffector`).

**The list** (from `_NATIVE_FORK_EXCLUDE`; some may have been brought up since
this doc was written — check `native/CMakeLists.txt` for the current set):

| TU | Why excluded (hypothesis) | Priority for v1 |
|----|--------------------------|-----------------|
| `Singer` | Vocal player; vocal subsystem | High (gameplay; Singer.h/.cpp permuter-touched) |
| `VocalPlayer`, `VocalNoteList` | Vocal gameplay paths | Med (vocals optional for v1 if guitar-only) |
| `GameGemList` | Gem-track gameplay data | **High** (v1 needs gems) |
| `GameConfig` | Gameplay config | High |
| `BandPatchMesh` | Patch/sticker mesh on band character | Low (cosmetic) |
| `SaveLoadManager` | Save-load full impl | Med (boot uses native stub idle=true) |
| `Splash` | Wii splash screens | Low (boot gates them out) |
| `Stats` | Stat tracking | Low |
| `WaitingUserGate` | Multiplayer user-gate UI | Low (single-player v1) |
| `StorePackedMetadata` | DLC/store metadata | Low (no DLC in v1) |
| `TourPerformerLocal` | Tour mode performer | Med (tour songs use it; quickplay may not) |
| `ClipDistMap` | Char-clip distance map | Med (char animation) |
| `DataResults` | Results screen data | Med (end-of-song needs this for scoring screen) |
| `AppLabel` | Some app label widget | Low |
| `CharacterTest` | DONE (boot-to-song brought up) | — |
| `CharForeTwist` | DONE | — |
| `CharHair` | DONE | — |
| `ChordShapeGenerator` | DONE | — |
| `GemTrackDir` | DONE | — |
| `MetaPanel`, `MusicLibrary`, `MainHubMessageProvider`, `MetaPerformer`, `AccomplishmentManager`, `AccomplishmentPanel`, `CampaignGoalsLeaderboardChoicePanel`, `TourDescPanel`, `AssetMgr`, `SongSort*`, `QuestFilterPanel`, `InputMgr`, `Band`, `PrefabMgr`, `FileMerger`, `OutfitConfig`, `BandDirector`, `TourProgress`, `StoreMenuPanel`, `AccomplishmentDiscSongConditional`, `AccomplishmentGroup` | DONE | — |

**Priority for v1 play-through:** `GameConfig`, `GameGemList`, `Singer` (if needed
for the band drummer/vocalist animation during a guitar play-through),
`DataResults`. The others can stay stubbed and still get a working song.

---

## V4: Venue-character Draw crash (Phase 2 render blocker)

**Crash:** `Character::DrawLodOrShadow` → `RndMesh::SetUpdateApproxLight`. The App
frame loop's `sigsetjmp` guard catches it so the boot proceeds, but it means the
animated venue characters never render. This is RB3-specific gfx path work; DC3
doesn't have the same issue because DC3's `Character` draw path went through
different infrastructure.

**Approach (mirror Strategy B):** RB3's `BandRnd : Rnd` (`native/src/rb3_band_rnd.cpp`)
provides the real bodies for the weak-stubbed `RndMesh`/`RndMat`/`RndCam`. The
`RndMesh::SetUpdateApproxLight` call is on the same surface — provide a real
(or no-op + log) body. Investigate what the call expects to do (approximate
lighting per-vertex from the env's lights?) and either implement it via the
engine's lighting buffers, or no-op it cleanly so character meshes draw with
default light. For the v1 milestone, no-op lighting is acceptable; correct
lighting is post-v1 polish.

---

## V5: `Game::LoadSong` completion + chart parse

With V1's assets present, `SongData::Load` will succeed reading the `.mid` and
`SongMgr::PostLoad` will get a populated `MidiParserMgr` `EventsList` (Game.cpp:264).
Expect Load byte-correctness issues in the MIDI parse path under clang LP64:

- `beatmatch/MidiParser.cpp`, `MidiParserMgr.cpp`, `BeatMaster.cpp` — Load/parse
  byte-symmetry under LP64. Compare RB3 vs DC3 sister files for HX_NATIVE blocks
  (DC3 plays songs, so its native MIDI parse works; port).
- `SongData::Load` may have rev-gated branches that differ for RB3.
- Audio-channel-config (from `songs.dta`'s per-song `tracks` array) drives
  `MasterAudio::SetupTracks` — the `TrackHasIndependentSlots` empty-`mTrackInfos`
  guard already added handles the no-chart case, but with a real chart the
  iteration needs to populate correctly.

---

## V6/V7/V8/V9: Play-through (audio + gem-track + scoring + completion)

After V5 lands, this is sequential:
- V6: actual audio output via the V2 backend.
- V7: gem-track HUD via `GemPlayer`/`GemTrackDir` (`GemTrackDir` already up).
- V8: hit detection (synthetic input timed to `songMs`) + score updates.
- V9: end-to-end one song — synthetic input picks a song, plays through, returns
  to `results_screen`; capture frames.

The RB3 game-layer source for these already exists in `src/band3/{game,bandtrack}/`
— most of it is brought up already (`Game`, `GamePanel`, `BandTrack` etc.). The
gaps are V3 (the residual TUs) + the Load-correctness work that emerges with
real chart data.

---

## Open deferrals carried forward from boot-to-song

These were deferred during boot-to-song; reckon with them for v1:

1. **Cosmetic main_hub venue backdrop (`world/shared/chars.milo` rev-15 band
   preview)** — currently DEFERRED at its specific load site (mirroring
   `CharCache::InitMe`'s precedent). The desync is in `Character::PostLoad` →
   `RndDir::PostLoad` for the inlined-proxy WorldInstance: `p->from->Dir()` is
   null on native. DC3 passes the SAME check with no `HX_NATIVE` — so the bug is
   in RB3's inlined-dir LOAD wiring (the inlined sub-dir's parent `Dir()` isn't
   set on native). Recurring blocker for song venues too. **v1 needs this fixed**
   or a clean per-venue deferral. Investigate the `DirLoader` inlined/proxy
   sub-dir load path — RB3 vs DC3 diff under HX_NATIVE.
2. **Venue-char `Draw()` crash** — see V4.
3. **`saveload_mgr` native stub** — `splash.dta` polls `is_idle` to advance;
   the native stub returns true. With a real `SaveLoadManager` (V3) this should
   work natively, removing the stub.

---

## Working-tree hygiene

The boot-to-song session left **~112 modified + ~33 untracked files** in the
working tree across `src/system/**`, `src/band3/**`, and `native/src/**`.
**Nothing committed.** This is per the convention to never `git add -A` while
the permuter is rewriting `src/system/**` + `src/band3/**`.

Before v1 work, a **whitelisted commit pass** is appropriate to checkpoint the
boot-to-song achievement. Approach:
1. Identify the agent-authored edits vs permuter churn. Permuter only touches
   `src/system/**` + `src/band3/**` and only the matched (`#else`) path — but
   our edits added `#ifdef HX_NATIVE` blocks that the permuter shouldn't move.
2. `git diff <file>` per touched file; commit only files where the diff is the
   HX_NATIVE blocks (and any directory/world milo Load fixes).
3. For `native/CMakeLists.txt`, `native/src/*.cpp`, `native/src/*.s` — these are
   per-decomp native glue, not permuter-touched; safe to commit explicit paths.
4. Use multiple small commits per logical area (loader native blocks; manager
   globals; menu-class bring-up; char-Load; screen flow; etc.) so history is
   reviewable.

**Do NOT** `git add -A` / `git add .`. Always whitelist.

---

## Quick-start commands

```bash
# Build (clang LP64)
cmake --build /home/free/code/milohax/rb3/native/build-native -j"$(nproc)"

# Regression guards — must stay green after every v1 change
RB3_BOOT=1 MILO_HEADLESS=1 RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
# → "SystemInit OK — (227 entries)" + "boot complete."

RB3_RENDER_MESH=1 MILO_HEADLESS=1 RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  /home/free/code/milohax/rb3/native/build-native/rb3-native \
  /home/free/code/milohax/rb3/orig-assets/extracted/ui/track/gen/tracksystem_meshes.milo_xbox
# → "129 meshes, 27878 tris" + /tmp/rb3_render_mesh.png

/home/free/code/milohax/rb3/native/build-native/rb3-dta \
  /home/free/code/milohax/rb3/orig-assets/extracted/songs/songs.dta 138
# → "138 top-level nodes"

# Boot-to-song reproduction (entry condition for v1)
GAME_DBG=1 RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=700 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@450:msg:overshell:end_override_flow:1:0" \
  timeout 280 /home/free/code/milohax/rb3/native/build-native/rb3-native 2>&1 | \
  grep -iE "GAME_DBG: \*|missing|currentScreen.*main_hub|currentScreen.*song_select|Game::LoadSong"
# → ... "Game::LoadSong() ENTERED — song='20thcenturyboy'" + "SONG-LOAD REACHED THE MISSING CHART ASSET"
```

---

## Conventions (carried over — non-negotiable)

- Background permuter continuously rewrites `src/system/**` + `src/band3/**`.
  **NEVER** `git add -A` / `git add .` — always whitelist explicit paths.
- Matched-fork edits are ADDITIVE `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE`
  blocks ONLY. Never alter the `#else`/asm-match path. `HX_NATIVE` is defined
  only on the clang LP64 native build.
- No `Co-Authored-By` lines in commit messages.
- Mirror DC3 sister-file `HX_NATIVE` blocks where they exist; port them rather
  than reinvent.
- Asset paths absolute.

## References

- [BOOT_TO_SONG.md](BOOT_TO_SONG.md) — completed predecessor milestone (full
  per-layer fix history, ~1100 lines).
- [DTA_MANAGER_STUBS.md](DTA_MANAGER_STUBS.md) — DTA-manager + UI-init bypass
  spec (still authoritative for any further manager-global work).
- [../native/NATIVE_PORT_ROADMAP.md](../../native/NATIVE_PORT_ROADMAP.md) —
  the canonical roadmap (Phase 3/5 sections + Critical-Path table).
- [../../native/NATIVE_PORT_INVENTORY.md](../../native/NATIVE_PORT_INVENTORY.md)
  — per-file native-port disposition.
- DC3 model (the precedent): `/home/free/code/milohax/dc3-decomp/` — DC3
  reaches boot-to-gameplay with audio. For each v1 gap, check the DC3 sister
  file first for an `HX_NATIVE` template.
- DC3 native port status: `/home/free/code/milohax/dc3-decomp/docs/native/NATIVE_PORT_STATUS.md`.
