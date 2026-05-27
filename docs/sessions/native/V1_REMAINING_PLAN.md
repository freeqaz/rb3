# V1 Remaining Plan — One Song End-to-End (Xbox 360 path)

**Authored:** 2026-05-27 evening, post-X6 + post-MIDI-bring-up. Plan agent: Opus.

## 1. Current state summary

The audio bring-up has progressed through X1-X6 (asset extraction, mogg-decryption primitive port, VorbisReader native Poll/Decrypt, StreamReceiver bridge to miniaudio `AudioDevice`). MIDI parsing now runs (track names + chart gem analysis confirmed in logs), vocals path is correctly stubbed under `HX_NATIVE`, and `Game::LoadSong` advances all the way to `BandDirector::OnLoadSong: is dlc? no`. **You are here:** the very first call into the .mogg decrypt pipeline asserts at `VorbisReader.cpp:638 (0) <= (keyIndex) && (keyIndex) < (KeyChain::getNumKeys())` because `KeyChain::getNumKeys()` is still the weak no-op stub returning 0 instead of the real 0xC. A parallel subagent is landing the bridge at `rb3/native/src/rb3_keychain_native.cpp` (file exists, contents verified). Once that is wired into CMake and the next ~5-6 ordered steps are done, V1 should complete.

## 2. Critical-path step list (ordered)

### K1 — Wire KeyChain bridge into the build

**What:** Add `rb3_keychain_native.cpp` to the `rb3-native` source list so the real class-shaped `KeyChain::getNumKeys()` returns `0xC` and `getKey`/`getMasher` return real key material.

**Files:**
- `/home/free/code/milohax/rb3/native/CMakeLists.txt` (insert in `add_executable(rb3-native …)` block)
- `/home/free/code/milohax/rb3/native/src/rb3_keychain_native.cpp` (from sibling subagent)

**Estimate:** 1 hour.

**Risk:** `KeyChain::getKey/getMasher/getNumKeys` symbols already have weak no-op stubs in `rndobj_synth_link_stubs.s` — the strong defs in `rb3_keychain_native.cpp` should win, but verify with `nm | grep KeyChain` that the strong symbol is selected. If not, remove the three KeyChain `.weak/.set` lines.

**Acceptance:** `nm build-native/rb3-native | grep getNumKeys` shows a `T` (not `W`) entry; the run reaches past `VorbisReader.cpp:638` without the assert.

**Sizing:** Sonnet, serial (gates everything downstream).

---

### K2 — Define `RndMesh::sUpdateApproxLight` (V4 root cause, real fix)

**What:** The reported V4 "venue-char Draw crash" is actually a **symbol-definition gap**, not a render bug. `static bool RndMesh::sUpdateApproxLight` is declared in `rb3/src/system/rndobj/Mesh.h:345` but never defined in any `.cpp`. The weak stub in `rndobj_synth_link_stubs.s:161` aliases it to a function (instruction bytes) and `Character::DrawLodOrShadow` reads/writes that page → SIGSEGV on the store. Add a single-line definition under HX_NATIVE.

**Files:**
- `/home/free/code/milohax/rb3/src/system/rndobj/Mesh.cpp` (add HX_NATIVE block near top) OR a new tiny `rb3/native/src/rb3_rndobj_natives.cpp`
- `/home/free/code/milohax/rb3/native/src/rndobj_synth_link_stubs.s` (remove the conflicting weak alias)

**Estimate:** 1 hour.

**Risk:** None — same fix pattern as other static members.

**Acceptance:** No SIGSEGV inside `Character::DrawLodOrShadow`; the `sigsetjmp` draw guard no longer fires.

**Sizing:** Sonnet. **Parallel-safe with K1, K3, K4, K5.**

---

### K3 — Bring up real libogg/libvorbis (system libs + `vorbis_synthesis_poll` shim)

**What:** RB3's `VorbisReader.cpp` calls 23 distinct `ogg_*`/`vorbis_*` functions plus the Harmonix-specific `vorbis_synthesis_poll`. All are currently weak-stubbed to `__hmx_rndsynth_noop_stub` in `rndobj_synth_link_stubs.s:47-105`. Mirror DC3:
  1. `target_link_libraries(rb3-native PRIVATE vorbis vorbisfile ogg)`.
  2. Add `rb3/native/src/rb3_vorbis_poll_shim.cpp` (~10 lines, verbatim from DC3 `engine_stubs_generated.cpp:180-185`).
  3. Remove all `ogg_*` / `vorbis_*` `.weak/.set` entries from `rndobj_synth_link_stubs.s`.

**Files:**
- `/home/free/code/milohax/rb3/native/CMakeLists.txt`
- `/home/free/code/milohax/rb3/native/src/rb3_vorbis_poll_shim.cpp` (NEW)
- `/home/free/code/milohax/rb3/native/src/rndobj_synth_link_stubs.s`

**Estimate:** 2 hours.

**Risk:** Vorbis ABI is stable; system lib path is the proven DC3 pattern.

**Acceptance:** `ldd build-native/rb3-native | grep vorbis` shows `libvorbis.so.0`. Re-run logs progress past `vorbis_synthesis_headerin`; first non-empty PCM frame.

**Sizing:** Sonnet. **Parallel-safe with K1, K2, K4, K5.**

---

### K4 — Non-headless run mode + audio device path validation

**What:** `AudioDevice::Init` short-circuits when `MILO_HEADLESS=1` (engine `AudioDevice.cpp:142-145`). V1 acceptance must drive a non-headless mode so miniaudio actually opens output. Add `RB3_AUDIO=1` env override (or treat `RB3_GAME=1` without `MILO_HEADLESS` as audio-on).

**Files:**
- `/home/free/code/milohax/milo-native-engine/src/audio/AudioDevice.cpp` (line 142-145)
- `/home/free/code/milohax/rb3/native/src/main_native.cpp` (env-var matrix in help text)

**Estimate:** 2 hours.

**Risk:** Dev-env ALSA backend; fallback to PulseAudio/null.

**Acceptance:** With `RB3_AUDIO=1 RB3_GAME=1 RB3_DATA=…`, audio device opens, `AddSource` called per channel, log shows `RenderAudio`.

**Sizing:** Sonnet. **Parallel-safe with K1, K2, K3, K5.**

---

### K5 — VorbisReader v0x10 byte-correctness sanity test

**What:** X3/X4 were validated against v0xE in the engine test harness; RB3 ships v0x10. Algorithm IS generic. Verify by running `test_mogg_v0xe.cpp` against a real RB3 `.mogg` after K1+K3 land. If header parse OK but PCM is garbage, compare against Onyx reference.

**Files:**
- `/home/free/code/milohax/milo-native-engine/tests/test_mogg_v0xe.cpp` (possibly add v0x10 variant)

**Estimate:** 2-4 hours.

**Risk:** v0x10 might add a header field DC3's v0xE port didn't anticipate. The decryption table is generic but `CheckHmxHeader` at `VorbisReader.cpp:339-394` reads structured fields.

**Acceptance:** Standalone decode of 1 second of PCM from `20thcenturyboy.mogg` is non-zero and structured.

**Sizing:** Opus (uncertain debug surface). **Serial after K1+K3.**

---

### K6 — Reach `BeatMaster::Poll` + `songMs` advancing

**What:** With audio decoding and KeyChain working, drive `Game::LoadSong` to completion: `MasterAudio::Load` → `mSongStream->IsReady()` true → `BandDirector::OnLoadSong` advances → `Game::mLoadState` past `kLoadingSong` → frame loop's `Game::Poll` calls `BeatMaster::Poll`, ticking `songMs` from `MasterAudio`'s clock. Investigate new asserts in `MasterAudio::SetupTracks`/`SetupChannels` and `BeatMaster::Poll`.

**Files:**
- `/home/free/code/milohax/rb3/src/system/beatmatch/MasterAudio.cpp` (lines 153-228)
- `/home/free/code/milohax/rb3/src/system/beatmatch/BeatMaster.cpp`
- `/home/free/code/milohax/rb3/src/band3/game/Game.cpp`
- `/home/free/code/milohax/rb3/src/system/synth/StandardStream.cpp` (Poll + GetTime)

**Estimate:** 1 day.

**Risk:** STLport vector iteration under clang LP64 (per BOOT_TO_SONG.md K2 patterns). Mirror DC3 sister blocks.

**Acceptance:** Log shows `Game::mLoadState = kPlaying`; `songMs` non-zero and monotonic.

**Sizing:** Opus. **Serial after K1-K5.**

---

### K7 — Residual `_NATIVE_FORK_EXCLUDE` cleanup (V1-priority subset)

Unaltered exclude list: `BandPatchMesh, ClipDistMap, DataResults, SaveLoadManager, Singer, Splash, StorePackedMetadata, TourPerformerLocal, VocalNoteList, VocalPlayer, WaitingUserGate`.

**V1 priority (need):**
- **`DataResults`** (`rb3/src/band3/net_band/DataResults.cpp`) — end-of-song results screen.
- **`ClipDistMap`** (`rb3/src/system/char/ClipDistMap.cpp`) — char-anim LOD distance map; characters may not draw correctly once K2 lands.

**V1 deferrable (stub OK):** Singer, VocalPlayer, VocalNoteList, BandPatchMesh, SaveLoadManager, Splash, StorePackedMetadata, TourPerformerLocal, WaitingUserGate.

**Files (V1-must subset):**
- `/home/free/code/milohax/rb3/src/band3/net_band/DataResults.cpp`
- `/home/free/code/milohax/rb3/src/system/char/ClipDistMap.cpp`
- `/home/free/code/milohax/rb3/native/CMakeLists.txt`
- `/home/free/code/milohax/rb3/native/src/band3_link_stubs.s`

**Estimate:** 1 day each.

**Risk:** Standard clang LP64 patterns from BOOT_TO_SONG.md K2.

**Acceptance:** Each TU compiles clean; weak stub removed; run reaches end-of-song.

**Sizing:** Two parallel Opus subagents. **Parallel-safe with each other and K6.**

---

### K8 — Gem-track HUD + scoring (V7 + V8)

**What:** With `BeatMaster::Poll` ticking and `songMs` flowing, `GemTrackDir` + `GameGemList` render gems. Hook scoring via `ScoreTracker`.

**Files (read-only audit):**
- `rb3/src/system/bandtrack/` (GemTrackDir, GemPlayer)
- `rb3/src/system/beatmatch/GameGemList.cpp` (already enabled)
- `rb3/src/band3/game/ScoreTracker.cpp`, `HitTracker.cpp`

**Estimate:** 2-3 days.

**Risk:** Highest — chart-load byte-correctness iceberg.

**Acceptance:** Gem-track milo renders notes; synthetic-input hits at `songMs` time produce `score++`.

**Sizing:** Opus, serial after K6 + K7.

---

### K9 — End-to-end V1 acceptance run

**What:** Synthetic-input script: boots → song-select → song → plays → results. PNG frames at start/middle/end.

**Files:**
- `/home/free/code/milohax/rb3/native/src/rb3_game_input.cpp` (extend if more verbs needed)
- `docs/sessions/native/v1-screenshots/`

**Estimate:** 1 day.

**Acceptance:** Reproducible command line in docs; PNG artifacts captured.

**Sizing:** Sonnet. Serial after K8.

---

## 3. Parallel-safe groupings

| Wave | Steps | Parallel? | Reason |
|------|-------|-----------|--------|
| Wave A | K1, K2, K3, K4 | YES (4-up) | Zero file overlap. Launch 4 Sonnet subagents simultaneously. |
| Wave B | K5 | NO | Validates K1+K3 jointly. Opus single. |
| Wave C | K6 | NO | Drives full LoadSong; bugs not separable. Opus single. |
| Wave D | K7-DataResults, K7-ClipDistMap | YES (2-up) | Independent TUs + CMakeLists lines. Can run concurrently with K6. |
| Wave E | K8 | NO | Sequential bug-hunt. Opus single. |
| Wave F | K9 | NO | Sonnet, serial. |

**Recommended dispatch:** launch Wave A (4 Sonnet subagents) immediately. After all four land + Wave B verifies the audio chain, dispatch Wave C + Wave D concurrently (3 Opus subagents). Wave E + F serial.

## 4. Anticipated post-V1 follow-ups (explicit deferrals)

1. Full vocals — `Singer`, `VocalPlayer`, `VocalNoteList` + `PostLoadVocals` un-stub.
2. Full venue characters — `UpdateApproxLighting` per-vertex from RndEnviron.
3. Multi-song / tour mode — `TourPerformerLocal`, tour progress paths.
4. DLC + store — `StorePackedMetadata`, `WaitingUserGate`.
5. Save/load — real `SaveLoadManager`.
6. Wii canonical path — BIK XTEA + CMPR/TPL decode deferred.
7. Drums/keys instrument — gem-track variants beyond guitar.
8. Real keyboard/MIDI guitar input — `os/UsbMidiGuitar.cpp` post-V1.
9. Pro-instruments scoring — `AccuracyTracker`, `ChordPreview`.
10. **Working-tree commit pass** — 112 modified + 33 untracked files outstanding. Whitelisted commit pass before V1 work compounds further. Consider between Wave A and Wave C.

## 5. Open questions

1. **K1 serial or in Wave A?** Recommend serial gate — wire-up is mechanical. The KeyChain implementation subagent can wire CMakeLists itself.
2. **System libvorbis vs bundled `oggvorbis/*.c`** (K3): system libvorbis is the proven DC3 pattern. Recommend system libs.
3. **`RB3_AUDIO=1` non-headless mode require Dawn window mode** (K4)? Audio + WebGPU are decoupled — confirm `MILO_HEADLESS=1 RB3_AUDIO=1` combo in K4.
4. **Working-tree commit pass timing** — coordinator decides whether to insert between Wave A and Wave C.
5. **V0x10 mogg validation in K5** — standalone `test_mogg_v0x10` harness vs live-game-path validation. Standalone is faster.

---

### Critical Files for Implementation

- `/home/free/code/milohax/rb3/native/CMakeLists.txt`
- `/home/free/code/milohax/rb3/native/src/rb3_keychain_native.cpp`
- `/home/free/code/milohax/rb3/native/src/rndobj_synth_link_stubs.s`
- `/home/free/code/milohax/rb3/src/system/rndobj/Mesh.cpp`
- `/home/free/code/milohax/rb3/src/system/synth/VorbisReader.cpp`
