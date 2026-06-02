# Completeness & Polish Audit — the "anything else" track

**One-line verdict:** The native/web RB3 port plays one song end-to-end with venue +
band + crowd + HUD + per-screen post-processing, but everything *around* the single
guitar play-through — save/profile persistence, A/V calibration, the end-of-song
results screen, options/settings menus, real vocals/mic, multi-player co-op, and a
handful of robustness gaps (teardown SIGSEGV, song-end transition) — is still
stubbed, unverified, or fragile.

**Authored:** 2026-06-02 (completeness/polish investigator, Opus, READ-ONLY on
source — no edits, no build, no commit). Verified against HEAD `ef0e8c28`, engine
pin `1a1f84e`. Live observation: `rb3-native` (built 2026-06-02 00:02) driven over
`/api/health` + `scripts/native/song-end-test.py`.

> **Scope note.** This is a *prioritized gap inventory*, not a full per-gap spec.
> Four gaps already own dedicated roadmap tracks and are **NOT re-spec'd here** —
> reference them: **web-audio output**, **keyboard/gamepad real input**,
> **difficulty/instrument select**, **loader stalls / async load**. This doc covers
> the remaining surface for a *polished + functional* port. Items needing their own
> deep spec later are flagged **[needs-dedicated-spec]**.

---

## 1. Current state — what works vs what's stubbed/missing (file:line evidence)

### Works (verified)
- One song plays end-to-end: synthetic input → menus → song_select → part_difficulty
  → tv3 cinematic → `game_screen` with `songMs` advancing, gem highway, HUD, venue,
  band, crowd. Confirmed live this session: reached `game_screen` with
  `songMs=2832` and frames advancing past 7795 (`/tmp/rb3-song-end-9137.log`).
- Per-screen PostProc + RTT + outfit tints + Tier-2 venue-only B&W grade-layering all
  landed and default-on as of 2026-06-01 (engine `1a1f84e`,
  `SESSION_2026_06_01_RTT_WRAP.md`). Gates: `RB3_PP_OFF`/`RB3_RTT_OFF`/`RB3_NOISE_OFF`/
  `RB3_BLOOM_OFF`/`RB3_NO_TRACK_DEPTH_CLEAR`/`RB3_NO_PRECLEAR`.
- Score HUD (`BandScoreboard` digits + star meter) renders top-center during play
  (`SCORE_HUD.md`; `GamePanel::StartGame()` HX_NATIVE force-show).
- Autohit verb drives real hit detection + score tick (`rb3_game_input.cpp:386`,
  `kVerbAutohit` → `Player::SetAutoplay`).
- Vocals *gameplay* TUs now compile: `VocalPlayer`/`VocalNoteList`/`Singer` stubs
  REMOVED from `band3_link_stubs.s` (see `:299`, `:402`, `:665`, `:832`).

### Stubbed / missing / fragile (the gaps this doc inventories)
- **Save/profile/settings persistence is a total no-op.** `SaveLoadManager` is in
  `_NATIVE_FORK_EXCLUDE` (`native/CMakeLists.txt`, the `set(_NATIVE_FORK_EXCLUDE …)`
  block lists `SaveLoadManager`), and every method is a weak no-op:
  `band3_link_stubs.s:453-458` aliases `SaveLoadManager::{AutoSaveNow,Init,AutoSave}`
  to `__hmx_band3_noop_stub`, and `TheSaveLoadMgr` is a weak null (`:1208-1209`).
  `ProfileMgr` (`ProfileMgr.h:2` includes `SaveLoadManager.h`; `:45` `SaveGlobalOptions`)
  and `GameplayOptions` (`GameplayOptions.h:7` `: FixedSizeSaveable`) therefore never
  persist. Nothing survives a restart.
- **A/V calibration: code + assets present, never wired/tested on native.**
  `CalibrationPanel` exists in the matched fork (`Calibration.h:18`,
  `CalibrationPanel.cpp`) and is **compiled** (not in the exclude list). The milo
  assets exist: `orig-assets/extracted-xbox-full/ui/calibration/gen/{cal_auto,
  cal_manual,cal_quick,cal_welcome,cal_complete,cal_background}.milo_xbox` +
  `calibration.dtb`. The offset storage/application path exists in `ProfileMgr`:
  `GetExcessAudioLag`/`GetExcessVideoLag`/`SetExcessAudioLag` (`ProfileMgr.h:91-98`),
  `GetJoypadExtraLag`/`SetJoypadExtraLag` (`:79-80`), applied via `GetPadExtraLag`
  (`ProfileMgr.cpp:1154`). BUT: the calibration screen is never navigated to on
  native, the result is never persisted (SaveLoadManager stub), and the
  `CalibrationPanel` needs `StartAudio`/`UpdateStream` (`Stream`/`Fader`) which on
  the headless null-synth path (`main_native.cpp:281` `(use_null_synth 1) (mics 0)`)
  may not produce a real test tone.
- **End-of-song → results screen does NOT fire on the headless jump path.** Live this
  session: `song-end-test.py --require-endgame` injected `{game jump 600000}` and the
  process kept rendering `game_screen` frames (7776→7795+) with **no** transition to
  game-over/results before the test killed it (`-15`). The mechanism exists in source
  (`Game::SetGameOver` → `TheGamePanel->SetGameOver()`, `Game.cpp:856-878`;
  `GamePanel.cpp:551-553` `lost`/`is_game_over`; `NetSession::EndGame` real impl at
  `rb3_netsession_native.cpp:167`) but the jump didn't drive `MasterAudio` past its
  duration → no `WinGame`/`TrulyWinGame` → no results. `DataResults` (results data
  serialization) is also **excluded** (`_NATIVE_FORK_EXCLUDE` lists `DataResults`).
- **Real vocals/mic input absent.** Synth is null with zero mics
  (`main_native.cpp:281` `(synth (mics 0) (use_null_synth 1) …)`); no mic capture
  path. `PitchDetector` is still a weak no-op (`band3_link_stubs.s:38,398-401`;
  `AnalyzeBlock__13PitchDetector…`, `PitchDetectorC1Ei`/`D1Ev`), so even if a mic fed
  in, no pitch is detected. `SingerStats::{SetPartPercentage,SetPitchDeviationInfo}`
  stubbed (`:291-294`).
- **Options/settings UIs not navigated; their milos absent from the canonical
  extract.** `GameplayOptions` (Lefty / VocalStyle / VocalVolume —
  `GameplayOptions.h:14-19`) and `ViewSetting` TUs compile, but a grep of the
  xbox-full extract found **no** `options_screen`/`gameplay_options`/`av_lag` UIs;
  only `ui/calibration/` is present. The settings menu flow is untested.
- **Teardown SIGSEGV (N9) still OPEN.** Root-caused (`N9_TEARDOWN_SIGSEGV_PLAN.md`):
  PipeWire/miniaudio RT thread torn down LAST instead of FIRST; fix is a (c)-glue
  exit-callback reorder in `main_native.cpp::RunGame()`. Single-digit-% per full run.
- **Crowd slivers (N5), hit/flame FX (N8)** — N8 has an autohit verb now (so FX *can*
  trigger); N5 residual char-IK slivers still suppressed by an engine guard.
- **Multiplayer / local co-op untested.** `BandUserMgr(4,3)` builds 4 LocalBandUsers
  (`rb3_game_input.cpp:447`) but the synthetic-input driver only ever wires ONE
  (`gSynthUser`, `:249`/`SynthUser()` `:444`). No second-player input routing.
- **Web beyond audio:** COOP/COEP correctly set in the dev server
  (`native/web/server.py:43-47`) for SharedArrayBuffer; no threading yet. 28M wasm,
  multi-minute brotli build. Web-specific perf/MEMFS not separately audited here.
- **PostProc noise polish:** `kNoiseGain=0.04` conservative
  (`Rnd_Wgpu_RB3.cpp:1477`); only remaining RTT cosmetic item.

---

## 2. Goal — desired/retail behavior

A *polished and functional* RB3 where, beyond the single play-through: the game saves
and reloads your profile/scores/settings; A/V calibration is reachable and persists an
offset that actually shifts gem timing; finishing a song shows a real results/score
screen (stars, %, hit streak) and returns to the menu; the options menu and a pause
menu work; vocals are at least gracefully degraded (not crash) and ideally mic-driven;
local co-op routes a 2nd player; and the open robustness gaps (teardown crash, song-end
transition reliability) are closed. Web reaches the same fidelity as native.

---

## 3. Proposed approach (prioritized gap inventory — 2-4 sentences each)

Each gap: current state · why it matters · effort/priority · whether it needs a
dedicated spec. Layer tags: (a) matched-fork `src/**`, (b) engine
`milo-native-engine/src/**`, (c) glue `rb3/native/src/**`.

### C1 — End-of-song → results/score screen  ·  P0  ·  ~3-5 person-days  ·  [needs-dedicated-spec]
The whole "complete a song" loop is unfinished: the headless jump-to-end does not
trigger `SetGameOver`/results (verified live — frames kept rendering `game_screen`
after `{game jump 600000}`). `DataResults` is excluded so even if the transition
fired, the results-data serialization is a stub. **Why it matters:** without a results
screen the game has no closure — you can't see your stars/%/score, can't exit cleanly
to the menu, and can't verify scoring at all. **Approach:** (i) diagnose why
`MasterAudio::Jump` past song-end doesn't reach `WinGame`→`TrulyWinGame` on native
(likely the song clock / `IsReady` gate or `CanEndGame`); (ii) un-exclude `DataResults`
and bring it up clang-LP64-clean (layer a); (iii) bring up `coop_endgame`/results
screen milos and confirm the `game_screen`→results UI transition. Mostly (a)+(c).
**Needs its own spec** — multi-system (audio clock + UI flow + results data).

### C2 — Save / profile / settings persistence  ·  P0  ·  ~4-7 person-days  ·  [needs-dedicated-spec]
`SaveLoadManager` is wholly excluded + no-op stubbed (`band3_link_stubs.s:453-458,
1208`); `ProfileMgr`/`GameplayOptions`/`Calibration` all persist through it via
`FixedSizeSaveable`, so nothing survives a restart and `[objects]/[visibles]/[xfms]`
empty-save-arrays (N11) bandaids remain. **Why it matters:** persistence is table
stakes — calibration offset, unlocked songs, scores, options all vanish on exit;
several existing HX_NATIVE C++ bandaids exist only because the data layer is dead.
**Approach:** bring up `SaveLoadManager` clang-LP64-clean (a), then back it with a
host-filesystem blob store in (c) glue (write the `FixedSizeSaveable` buffers to a
file under `RB3_DATA` or XDG state dir) instead of the Wii NAND/memcard path. DC3 has a
sister `SaveLoad` native treatment to mirror. **Needs its own spec** — touches the
matched-fork save subsystem + a new glue persistence backend.

### C3 — A/V calibration UI + offset application  ·  P0 for a rhythm game  ·  ~3-4 person-days  ·  [needs-dedicated-spec]
Code (`CalibrationPanel`, `ProfileMgr::Get/SetExcessAudio/VideoLag`,
`GetJoypadExtraLag`) and assets (`ui/calibration/gen/cal_*.milo_xbox`) all exist and
compile, but the screen is never navigated to, the offset never persists (blocked on
C2), and on the null-synth headless path the calibration test-tone may not play.
**Why it matters:** a rhythm game is unplayable-to-spec without latency calibration;
host audio/video latency varies wildly and is the difference between "feels right" and
"feels broken". **Approach:** (i) add a synthetic-input verb to navigate to the
calibration screen (c); (ii) confirm `CalibrationPanel::StartAudio`/`UpdateStream`
produce an audible click on the real (non-null) synth in a headed/web env (b/c);
(iii) verify the resulting `ExcessAudioLag`/`ExcessVideoLag` flows into gem-timing
(`ProfileMgr::GetPadExtraLag`→beatmatcher) and persists (depends on C2). **Needs its
own spec**; gated behind C2 for persistence.

### C4 — Teardown SIGSEGV (N9)  ·  P1  ·  **DONE 2026-06-02**  ·  wt-teardown
Fixed. The prior exit-callback reorder (`a25ab7bd`) was INERT — App-ctor-registered
`SynthTerminate` (push_front) always ran first and already joined the device. The genuine
residual was `SynthTerminate`'s `TheSynth->Poll()` racing the live PipeWire RT thread.
Fix: `AudioDevice::GetInstance().Suspend()` added in `App.cpp RunWithoutDebugging()`
HX_NATIVE frame-loop exit (before `RB3HttpServerShutdown(); return;`) — quiesces the RT
thread before Debug::Exit fires. Comment in `main_native.cpp` corrected. 30/30 clean exits
with live null-backend RT thread; zero new coredumps. See `N9_TEARDOWN_SIGSEGV_PLAN.md`
§ RESOLUTION for full ordering analysis.

### C5 — Vocals / mic input path  ·  P2  ·  ~5-8 person-days  ·  [needs-dedicated-spec]
Gameplay vocal TUs now compile (Singer/VocalPlayer/VocalNoteList un-stubbed) but
`PitchDetector` is still a no-op (`band3_link_stubs.s:38,398-401`) and the synth has
zero mics (`main_native.cpp:281`). **Why it matters:** vocals are a core RB3
instrument; without it the port is guitar/bass/drums/keys-only. **Approach:** (i) wire
a real mic capture source into the engine audio device (b); (ii) bring up
`PitchDetector` clang-LP64-clean (a); (iii) feed pitch into `VocalPlayer`/`SingerStats`.
Defer until guitar/drum singleplayer is fully polished (per roadmap non-goals).
**Needs its own spec.**

### C6 — Options / settings + pause menus  ·  P2  ·  ~2-4 person-days  ·  partial-spec
`GameplayOptions` (Lefty/VocalStyle/VocalVolume) + `ViewSetting` compile, and the game
has a real `mIsPaused`/`kGamePaused` path (`Game.cpp`), but the options/settings UI
milos are absent from the canonical extract and the menus are untested on native.
**Why it matters:** Lefty flip, vocal style, crowd volume, HUD toggles are expected
polish; a pause menu is needed to quit mid-song. **Approach:** (i) confirm whether the
options milos exist in a fuller extract (the Wii ark) and add them; (ii) add input
verbs to navigate options + pause; (iii) confirm settings persist (depends on C2).
Mostly (c) navigation + (a) any Load-correctness. Bundle the survey into one
investigation dispatch before committing to a full spec.

### C7 — Multiplayer / local co-op input routing  ·  P2  ·  ~3-5 person-days  ·  [needs-dedicated-spec]
Untested. `BandUserMgr(4,3)` makes 4 LocalBandUsers but the synthetic driver wires
only `gSynthUser` (`rb3_game_input.cpp:249,444`); no 2nd-player track-assign or input
routing. **Why it matters:** local co-op (the band) is RB3's identity. **Approach:**
extend the input driver to assign + route N players (c), confirm `Band::Band` builds
>1 active player and the HUD multiplier popup (currently correctly hidden at 1 player,
`SCORE_HUD.md`) shows. Online MP stays out of scope (roadmap non-goal). Gated behind
real input (the input track). **Needs its own spec.**

### C8 — Hit / flame FX (N8) + crowd slivers (N5)  ·  P2  ·  ~2-3 person-days  ·  no spec
N8: autohit verb now exists so the hit path *can* fire (`GemSmasher::Hit` →
`hit.trig` particles); needs a visual confirm that flame/burst FX actually render and a
star-power FX pass. N5: residual char-IK crowd slivers still suppressed by an engine
extent-ratio guard (`CharIKHand`/`BandWardrobe` proxy resolution). **Why it matters:**
visible gameplay polish, lower stakes than C1-C3. (a)+(b)+(c); serialize on
`Rnd_Wgpu_RB3.cpp` + `rb3_game_input.cpp`.

### C9 — PostProc noise full-fidelity polish  ·  P2  ·  ~0.5-1 person-day  ·  no spec
`kNoiseGain=0.04` (`Rnd_Wgpu_RB3.cpp:1477`) is conservative vs the Wii's
texture-driven grain; the only remaining RTT cosmetic item per
`SESSION_2026_06_01_RTT_WRAP.md`. **Why it matters:** lowest-stakes cosmetic. Tune the
gain + midtone mask, A/B vs `images/retail-screenshots/`. Layer (b), isolated.

### C10 — Web parity sweep (beyond audio)  ·  P1  ·  ~2-3 person-days  ·  partial-spec
COOP/COEP handled (`server.py:43-47`); web shares the same `src/`+engine so logic bugs
reproduce identically in native (the canonical debug loop). **Why it matters:** the web
build is the public-facing target; needs a parity pass once C1-C4 land natively (the
song-complete loop, save persistence via IndexedDB/MEMFS-persist instead of host FS,
teardown). Confirm each native fix on web once, per CLAUDE.md workflow. Mostly
verification + a web-specific persistence backend for C2.

---

## 4. Key files (one-line role each)

- `native/CMakeLists.txt` — `_NATIVE_FORK_EXCLUDE` block: `ClipDistMap DataResults
  SaveLoadManager Splash StorePackedMetadata TourPerformerLocal WaitingUserGate` are
  the still-excluded TUs (C1 `DataResults`, C2 `SaveLoadManager`).
- `native/src/band3_link_stubs.s` — weak no-op stubs; `:453-458,1208` SaveLoadManager,
  `:38,398-401` PitchDetector, `:291-294,828-831` SingerStats.
- `native/src/rb3_netsession_native.cpp:167` — real `NetSession::EndGame` (song-end
  trigger) + `IsInGame` semantics (C1).
- `native/src/main_native.cpp:281` — null-synth `(mics 0) (use_null_synth 1)` (C5);
  `RunGame()` ~`:539` — exit-callback registration (C4 fix site).
- `native/src/rb3_game_input.cpp` — synthetic input driver; `:249,444` single
  `gSynthUser` (C7), `:386` autohit verb (C8).
- `src/band3/game/Game.cpp:856-878` `SetGameOver`; `GamePanel.cpp:551-553`
  game-over expressions (C1).
- `src/band3/meta_band/{SaveLoadManager,ProfileMgr,GameplayOptions,Calibration*}.{h,cpp}`
  — save/profile/options/calibration subsystem (C1/C2/C3/C6).
- `src/band3/meta_band/ProfileMgr.h:79-98` — A/V lag get/set surface (C3).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1477` — `kNoiseGain` (C9).
- `orig-assets/extracted-xbox-full/ui/calibration/gen/cal_*.milo_xbox` — calibration
  assets, present (C3).
- `scripts/native/song-end-test.py` — `--require-endgame` harness; reproduces the C1
  gap. `docs/sessions/native/N9_TEARDOWN_SIGSEGV_PLAN.md` — C4 build-ready plan.

---

## 5. Quick wins (< 1 day) vs larger work

**Quick wins (< 1 day):**
- **C4 teardown SIGSEGV** — exit-callback reorder in `main_native.cpp`, plan already
  written, (c) only. Verify with 30-50 runs + coredump count.
- **C9 noise gain** — tune one constant, A/B screenshot. (b), isolated.
- **C1 step (i) diagnosis** — *why* the jump doesn't trigger game-over is cheap to
  localize (instrument the `WinGame`/`CanEndGame` gate); fixing it may itself be small
  even though the full results screen is larger.
- **C6 survey** — a single investigation dispatch to confirm whether options/pause
  milos exist + are navigable.

**Larger work (multi-day, [needs-dedicated-spec]):** C1 results screen (full),
C2 persistence backend, C3 calibration flow, C5 vocals/mic, C7 co-op.

---

## 6. Dependencies & risks

- **C3 calibration depends on C2 persistence** (an offset you can't save is useless) and
  on real audio (null-synth has no test tone) → also touches the web-audio track.
- **C1 results depends on the song-end transition** firing (audio-clock-driven) — may
  intersect the loader-stall / async-load track.
- **C2 is the keystone**: persistence unblocks C3, retires N11 + several HX_NATIVE
  layout bandaids, and is a prerequisite for any "your progress is saved" polish.
- **Permuter risk:** `SaveLoadManager`, `ProfileMgr`, `Calibration*`, `Game.cpp`,
  `GamePanel.cpp` are matched-fork — all edits ADDITIVE `#ifdef HX_NATIVE`, never
  `git add -A`, re-apply if the permuter shifts them.
- **Concurrency hot files:** `band3_link_stubs.s`, `native/CMakeLists.txt`,
  `rb3_game_input.cpp`, `Rnd_Wgpu_RB3.cpp`, `main_native.cpp` — serialize edits.
- **Web persistence** needs a different backend (IndexedDB) than native host FS — design
  C2's glue layer behind an interface so both plug in.
- **Risk of regressing v1:** C1/C8 touch the gameplay path an independent review called
  "clearly RB3" — gate behind env opt-outs and A/B against `images/retail-screenshots/`.

---

## 7. Effort & priority (P0/P1/P2, rough person-days)

| Gap | Title | Priority | Effort (pd) | Dedicated spec? |
|---|---|---|---|---|
| C1 | End-of-song → results screen | **P0** | 3-5 | yes |
| C2 | Save/profile/settings persistence | **P0** | 4-7 | yes |
| C3 | A/V calibration UI + offset apply | **P0** (rhythm-critical) | 3-4 | yes |
| C4 | Teardown SIGSEGV (N9) | P1 | 0.5 | no (plan exists) |
| C10 | Web parity sweep | P1 | 2-3 | partial |
| C5 | Vocals / mic input | P2 | 5-8 | yes |
| C6 | Options / settings + pause menus | P2 | 2-4 | partial |
| C7 | Multiplayer / local co-op | P2 | 3-5 | yes |
| C8 | Hit/flame FX (N8) + crowd slivers (N5) | P2 | 2-3 | no |
| C9 | PostProc noise full-fidelity | P2 | 0.5-1 | no |

**Recommended sequence:** C4 (quick robustness) → C2 (keystone persistence) →
C1 (close the play-loop) → C3 (calibration, on top of C2) → C10 (web parity) →
C6/C8/C9 polish → C5/C7 (deferred per roadmap non-goals).

---

## 8. Verification plan (native-harness / web)

- **C1 results:** `python3 scripts/native/song-end-test.py --require-endgame --verbose`
  must reach an `is_endgame_screen` (`results`/`endgame`/`score_screen`) and stay live
  ≥25 s / ≥30 frames without exit. Today it FAILS (frames keep rendering `game_screen`
  after `{game jump 600000}`; process killed `-15`). That is the exact pass/fail gate.
- **C2 persistence:** boot, change an option/score, exit clean (code 0), re-boot, assert
  the value is read back. Add a `/api/dta/eval` probe of `ProfileMgr`/`GameplayOptions`
  state before/after restart. Confirm a save file appears under `RB3_DATA`/state dir.
- **C3 calibration:** drive the calibration screen via a new input verb; on a headed/web
  env confirm an audible click; assert `ProfileMgr::GetExcessAudioLag()` changes and (with
  C2) survives restart; verify gem-timing shifts in a before/after autohit run.
- **C4 teardown:** baseline `coredumpctl list | grep build-native/rb3-native`; apply fix;
  run the canonical full-song reproducer 30-50× (`MILO_AUDIO=1`, `MILO_MAX_FRAMES`
  8500-9500); pass = 0 new coredumps + every log ends `APP EXITED, EXIT CODE 0`
  (`N9_TEARDOWN_SIGSEGV_PLAN.md` §5).
- **C6/C7/C8:** headless `/api/screenshot` at the relevant screen/frame, diff vs
  `images/retail-screenshots/`; C7 also assert `Band::Band` builds >1 active player and
  the multiplier popup shows.
- **C9:** screenshot a steady-state gameplay frame (songMs > 25000), A/B `kNoiseGain`
  values against retail grain.
- **C10 web:** after each native fix, `scripts/web/build.sh` + `python3
  native/web/server.py`, confirm the same behavior in-browser once (per CLAUDE.md).
- **Regression guards (must stay GREEN after every change):** `RB3_BOOT` →
  "SystemInit OK — (227 entries)" + "boot complete."; `RB3_RENDER_MESH` → 129
  meshes/27878 tris; `rb3-dta songs/songs.dta` → 138 nodes.
