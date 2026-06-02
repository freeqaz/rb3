# RB3 Native/Web — Next Roadmap (2026-06-02)

> ## LANDED 2026-06-02 (3 of the tracks below)
> - **Web audio / SFX** — dup `TheSynth` removed; RB3 `SampleInst` plays PCM SFX;
>   **XMA→PCM offline sidecar conversion** (`1c5c187c`, the genuine "port from DC3"):
>   1706 kXMA blobs converted (677 sidecars), native logs `playing XMA->PCM sidecar`.
> - **Keyboard/gamepad input** (`ce2cab80`) — `native/src/rb3_joypad_native.cpp` real
>   `JoypadPoll`→`SendButtonMessages`; breed `wii_guitar`→5-lane GuitarController.
> - **Loader stalls** (`3cef2790`) — QW-1 time-budget (`RB3_LOADER_BUDGET_MS`) + QW-2
>   buffering; main_hub 9.9s→7.8s, boot worst-case 8→13 fps.
>
> All native-verified; web builds clean with all three.
>
> ## LANDED 2026-06-02 (batch 2 — pushed, master `1d6aa7d4`)
> - **Difficulty + instrument select** (`3ffa6ff9`) — `difficulty:<easy|med|hard|expert>` verb,
>   un-hardcoded forced Expert, real overshell part-select (glue-only).
> - **Web XMA-SFX serving** (`5da36eeb`) — on-demand sidecar fetch (lazy XHR per SFX, no 180MB
>   upfront) + `server.py` serves the derived tree.
> - **Teardown SIGSEGV** (`3424b4e1`) — `AudioDevice::Suspend()` quiesces the audio RT thread
>   before `Debug::Exit`; 30/30 clean exits. (completeness-audit C4 = DONE.)
> - **Gameplay-start OSFatal fix** (`1d6aa7d4`) — the input breed-pin (`wii_guitar`) crashed at
>   song start. NON-OBVIOUS ROOT CAUSE: native uses the `HX_WII` DTA define, so the loaded
>   `joypad.dta` is a MERGE of an HX_WII fragment (`controllers`/`button_meanings` — has
>   `wii_guitar`; `GuitarController::Poll` fatal-looks-it-up EVERY FRAME) and an Xbox-flavoured
>   fragment (`controller_mapping`/`instrument_mapping` — NO `wii_guitar`). No single 5-lane
>   breed is in both. Fix = glue-only runtime DataArray injection (`EnsureWiiGuitarMapped()` in
>   rb3_joypad_native.cpp adds the two missing rows). **`song-end-test.py` PASSES** — gameplay
>   starts, songMs advances, clean exit.
>
> **Open follow-ups:** windowed gameplay key-press + in-browser audibility (no `$DISPLAY`/audio
> output here); DC3 sidecar generation + landing (blocked by a pre-existing dc3 build-env issue,
> committed on `wt-xma-dc3`, NOT pushed). **Still unstarted:** the rest of the completeness-audit
> track (results screen, persistence, A/V calibration, options/pause, vocals, multiplayer).

This is the master handoff that consolidates the five investigator specs written into
[`roadmap-2026-06-02/`](roadmap-2026-06-02/) on 2026-06-02, building on the prior
RTT/PostProc bring-up captured in
[`SESSION_2026_06_01_RTT_WRAP.md`](SESSION_2026_06_01_RTT_WRAP.md). As of that wrap the
port boots, renders a fully-graded venue + band + crowd + HUD, and plays one guitar song
end-to-end on **autoplay** — all the offscreen-RTT/PostProc target-hacks are retired
(engine pin `1a1f84e`). The next frontier is everything *around* that single auto-played
song: making it interactive (real input + difficulty/instrument choice), audible
(web-audio confirmation + menu SFX), responsive (kill loader stalls), and complete
(results screen, persistence, calibration, teardown robustness). The five specs are:
[loader-performance.md](roadmap-2026-06-02/loader-performance.md),
[input-keyboard-gamepad.md](roadmap-2026-06-02/input-keyboard-gamepad.md),
[difficulty-instrument-select.md](roadmap-2026-06-02/difficulty-instrument-select.md),
[web-audio.md](roadmap-2026-06-02/web-audio.md), and
[completeness-audit.md](roadmap-2026-06-02/completeness-audit.md) (the "anything else"
inventory: results screen, save/persistence, A/V calibration, teardown SIGSEGV, vocals,
options/pause, co-op, web-parity, FX/noise polish).

Layer legend (per the three-layer model): **(a)** matched fork `rb3/src/**` (MWCC,
asm-matched — additive `#ifdef HX_NATIVE` only); **(b)** shared engine
`milo-native-engine/src/**` (clang, shared by rb3/dc3/xenon); **(c)** per-decomp glue
`rb3/native/src/**` (clang, no asm-match — **preferred**).

---

## Prioritized track table

| Track | Item | Priority | Effort | One-line goal | Layer(s) | Spec |
|---|---|---|---|---|---|---|
| Loader | **QW-1** time-budget native `LoadMgr::Poll` drain | **P0** | 0.5 d | bound every per-frame loader stall to ~8 ms | (a) | [loader](roadmap-2026-06-02/loader-performance.md) |
| Loader | **QW-2** `setvbuf` 64 KiB + cache `Size()` | P1 | 0.25 d | cut fread syscalls + per-`Eof` triple-fseek | (c) | [loader](roadmap-2026-06-02/loader-performance.md) |
| Loader | **QW-3** defer/skip boot crowd+colorpalettes preload | P1 | 0.5–1 d | remove biggest boot burst from critical path | (a)/(c) | [loader](roadmap-2026-06-02/loader-performance.md) |
| Loader | **LW-1** file I/O off the render thread (async) | P1 | 8–14 d | overlap per-song load with rendering; drop tv3 force-poll hack | (b)+(a) | [loader](roadmap-2026-06-02/loader-performance.md) |
| Loader | **LW-2** decompression off-thread (gated on §8 measure) | P2 | 3–5 d | only if I/O isn't the dominant cost | (a) | [loader](roadmap-2026-06-02/loader-performance.md) |
| Input | **Phase 1** desktop keyboard → real `JoypadPoll` | **P1** | 0.5–1 d | strum a note with the keyboard (menu + gameplay) | (c) | [input](roadmap-2026-06-02/input-keyboard-gamepad.md) |
| Input | **Phase 3** guitar-controller breed/config verify | P1 | 0.5–1 d | ensure `NewController` builds `GuitarController` + correct keymap | (c) | [input](roadmap-2026-06-02/input-keyboard-gamepad.md) |
| Input | **Phase 2** web keyboard/gamepad → unified path | P1 | 1 d | one input path on web (retire menu-only injection) | (c) | [input](roadmap-2026-06-02/input-keyboard-gamepad.md) |
| Input | **Phase 4** USB gamepad mapping | P2 | 1 d | play "joypad guitar" on an Xbox/PS pad | (c) | [input](roadmap-2026-06-02/input-keyboard-gamepad.md) |
| Difficulty | **Phase 0** un-hardcode Expert + `difficulty:` verb | **P1** | 0.5 d | difficulty/part fully controllable from harness/HTTP | (c) | [difficulty](roadmap-2026-06-02/difficulty-instrument-select.md) |
| Difficulty | **Phase 1** drive real overshell `SelectPart`/`SelectDifficulty` | P1 | 1 d | exercise the genuine retail selector chain | (c) | [difficulty](roadmap-2026-06-02/difficulty-instrument-select.md) |
| Difficulty | **Phase 2** keyboard nav of on-screen choose lists | P2 | 2–3 d | retail-identical arrow-key picking | (c) | [difficulty](roadmap-2026-06-02/difficulty-instrument-select.md) |
| Web-Audio | **Phase 0** delete dup `TheSynth` + stale comment | **P0** | 0.25 d | remove latent link-order UB | (c)+(a comment) | [web-audio](roadmap-2026-06-02/web-audio.md) |
| Web-Audio | **Phase 1** prove audibility (capture-WAV assert) | **P0** | 0.5 d | confirm PCM actually reaches the worklet | test only | [web-audio](roadmap-2026-06-02/web-audio.md) |
| Web-Audio | **Phase 2** menu/UI SFX via RB3 `SampleInst::NewInst` | P1 | 2–3 d | menu blips/confirms audible (native + web) | (c) | [web-audio](roadmap-2026-06-02/web-audio.md) |
| Web-Audio | **Phase 3** streamed MOGG fetch (Range/worker) | P2 | multi-d | hide first-note network stall (W4) | (b)/(c) | [web-audio](roadmap-2026-06-02/web-audio.md) |
| Complete | **C4** teardown SIGSEGV (N9) exit-cb reorder | **P0** | 0.5 d | clean exit (plan already written) | (c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C1** end-of-song → results/score screen | **P0** | 3–5 d | close the play loop; show stars/%/score | (a)+(c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C2** save/profile/settings persistence | **P0** | 4–7 d | nothing survives restart today (keystone) | (a)+(c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C3** A/V calibration UI + offset apply | P0* | 3–4 d | rhythm-critical latency calibration (gated on C2) | (a)/(b)/(c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C10** web parity sweep (beyond audio) | P1 | 2–3 d | confirm native fixes on web; web persistence backend | (c)+verify | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C9** PostProc noise full-fidelity tune | P2 | 0.5–1 d | match Wii grain (last RTT cosmetic) | (b) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C6** options/settings + pause menus | P2 | 2–4 d | Lefty/vocal/HUD toggles + mid-song quit | (c)+(a) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C8** hit/flame FX (N8) + crowd slivers (N5) | P2 | 2–3 d | visible gameplay FX polish | (a)/(b)/(c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C5** vocals / mic input path | P2 | 5–8 d | mic-driven vocals (deferred per roadmap non-goals) | (a)+(b) | [completeness](roadmap-2026-06-02/completeness-audit.md) |
| Complete | **C7** multiplayer / local co-op input routing | P2 | 3–5 d | route a 2nd player (gated on input track) | (c) | [completeness](roadmap-2026-06-02/completeness-audit.md) |

\* C3 is rhythm-critical (P0 by importance) but is hard-gated on C2 persistence, so it
sequences after C2/C1 in practice.

---

## Dependency / sequencing — what unblocks what

The tracks split into a mostly-independent **interactivity bundle** and a partly-chained
**completeness chain**, plus two cheap standalone de-risk items.

**Standalone, do-immediately (no deps):**
- **Web-Audio Phase 0 + Phase 1** — pure cleanup + a test; touches only glue/test files.
  Phase 1 doubles as the regression guard that catches a re-introduced dup-`TheSynth`.
- **Complete C4 (teardown SIGSEGV)** — isolated (c) exit-callback reorder, plan already
  written. Independent of everything; do it before anything that adds save-on-exit (it
  would otherwise corrupt those saves) or extra exit callbacks.
- **Loader QW-2** — pure (c) native-file shim, zero semantic change, no deps.

**Interactivity bundle — input + difficulty PAIR tightly:**
- **Input Phase 1** (real `JoypadPoll` → `SendButtonMessages`) is the keystone of the
  whole interactivity story: it makes the *one* engine broadcast chokepoint live, so menus
  and gameplay both flow from it. Difficulty Phase 0/1 *and* Input Phase 1 share the same
  `rb3_game_input.cpp` / `SynthUser()` plumbing (controller-type pin, pad-0 association),
  so they should be done by the same person back-to-back to avoid double-editing that file.
- **Input Phase 3** (guitar breed/config verify) gates correct fret-key mapping for Input
  Phase 1 to be *actually playable* — do Phase 1 then immediately Phase 3 (they're the same
  investigation).
- **Difficulty Phase 0** (un-hardcode + `difficulty:` verb) is independent of input and is
  the cheapest way to make the *whole difficulty chain* testable; do it first within the
  difficulty track. Phase 1 (real overshell selector) layers on top.
- **Input Phase 2 (web)** depends on Phase 1 landing first (it funnels the same path through
  the browser). **Phase 4 (gamepad)** is additive after Phase 1.
- **Complete C7 (co-op)** is hard-gated on the input track (need real per-player input
  before routing a 2nd player). **Complete C6 (options/pause)** wants input + persistence.

**Loader chain (quick-wins-first, then measure, then architecture):**
- **QW-1 (P0 time-budget)** is the highest loader value/effort and stands alone. Do it
  before QW-3 so you can measure QW-3's effect against a budgeted baseline.
- Recommended loader order per spec: **QW-2 → QW-1 → instrument (§8) → QW-3 → LW-1**.
  **LW-1 (async I/O)** is the only fix that makes per-song load overlap rendering and lets
  the `WorldDir::Poll` force-poll hack be removed; it is large (8–14 d) and should be
  sequenced *after* the QWs + the Phase-0 instrumentation confirm the I/O-vs-CPU split.
  **LW-2** only if that measurement shows decompression dominates.

**Completeness chain — C2 is the keystone:**
- **C2 (persistence)** unblocks **C3 (calibration)** (an offset you can't save is useless),
  retires N11 + several HX_NATIVE save-array bandaids, and is a prerequisite for any "your
  progress is saved" polish. It is the long pole of the P0 set.
- **C1 (results screen)** depends on the song-end transition firing — which is
  *audio-clock-driven*, so it **intersects the loader/audio tracks**. Its step-(i)
  diagnosis (why `{game jump}` doesn't reach `WinGame`→`TrulyWinGame`) is a cheap quick win
  worth doing early even before the full results-screen build.
- **C3 (calibration)** depends on C2 (persist) *and* real audio (the null-synth headless
  path has no test tone) → it also touches the web-audio track. Sequence after C2/C1.
- **C10 (web parity)** is a verification pass that should run *after* each native fix lands
  (per CLAUDE.md "confirm once on web"); it also needs a web-specific persistence backend
  (IndexedDB) for C2 — design C2's glue behind an interface so both host-FS and IndexedDB
  plug in.

**Recommended global order:**
1. (parallel, day 1) Web-Audio Phase 0+1 · Complete C4 · Loader QW-2 — three cheap de-risk
   items, no contention.
2. Loader QW-1 (P0 stall fix) — biggest responsiveness win.
3. Input Phase 1 + Phase 3, and Difficulty Phase 0 + Phase 1 — the interactivity bundle
   (same person/file), making the port actually *playable* and *choosable*.
4. Complete C1 step-(i) diagnosis (cheap) → C2 persistence (keystone) → C1 full results →
   C3 calibration.
5. Loader instrument → QW-3 → LW-1 (architectural async, large) in parallel with the C-chain.
6. Web-Audio Phase 2 (menu SFX) · Input Phase 2/4 · C10 web parity · then P2 polish
   (C9/C6/C8) and deferred C5/C7.

---

## DO FIRST — the 2–3 highest-leverage items

1. **Input Phase 1 — real `JoypadPoll()` driving `SendButtonMessages`** (P1, 0.5–1 d, one
   new glue TU `native/src/rb3_joypad_native.cpp` + one CMake line). *Reasoning:* this is
   the single change that converts the port from an autoplay tech-demo into something a
   human can *play*. Every consumer (menu focus, `GuitarController` frets/strum/whammy) is
   already subscribed to the one `gJoypadMsgSource` chokepoint; today it's dead only because
   `JoypadPoll` is a weak no-op stub. Smallest correct layer (c), zero matched-fork or
   engine risk, and it unblocks the entire interactivity story (co-op, gamepad, web input).
   Pair it immediately with Phase 3 (breed verify) so the frets actually map.

2. **Loader QW-1 — time-budget the native `LoadMgr::Poll` drain** (P0, 0.5 d, ~30 LOC,
   HX_NATIVE-additive). *Reasoning:* the loader currently drains the *whole* queue per Poll
   on the render thread, freezing the frame loop at boot, on every song load, and during
   the splash crowd-merge burst (~14 fps observed). The web build already ships the exact
   budgeted-slice shape to port (minus `emscripten_sleep`); it's the highest value/effort
   ratio in the whole roadmap and is reversible behind `RB3_LOADER_BUDGET_MS`. Makes the
   demo *feel* finished without the 8–14-day async rewrite.

3. **Web-Audio Phase 0+1 — kill the duplicate `TheSynth` and prove audibility** (P0,
   ~0.75 d combined). *Reasoning:* the headline "song plays on web" milestone has **never
   been confirmed audible in a real tab** — only that the decode/mix loop runs headless. A
   capture-WAV assert (using the engine's existing `rb3CaptureAudio()`/`rb3DownloadAudio()`
   hooks, no source changes) turns a claim into a verified fact, and deleting the
   `--allow-multiple-definition`-masked duplicate `TheSynth` removes a real latent
   link-order UB. Cheap, and it locks in a regression guard for the audio path.

(Honorable mention: **Complete C4 teardown SIGSEGV** — 0.5 d, plan already written, fully
isolated; fold it into day 1 alongside the above since it's free and must precede any
save-on-exit work.)

---

## Consolidated QUICK-WINS (shippable in < 1 day each)

| Item | Track | Effort | Layer | What it ships |
|---|---|---|---|---|
| Delete dup `TheSynth` in `main_web.cpp:86-98` | Web-Audio P0 | minutes | (c) | removes link-order UB |
| Fix stale "audio-free" comment `App.cpp:250-262` | Web-Audio P0 | minutes | (a) comment-only | unblocks next implementer |
| Web `DebugDescribe` override on stream receiver | Web-Audio P2 | minutes | (c) | meaningful `rb3AudioStats()` |
| Capture-WAV audibility assert in web gameplay test | Web-Audio P1 | hours | test | proves "web has sound" |
| `setvbuf` 64 KiB + cache `Size()` in `native_file.cpp` | Loader QW-2 | 0.25 d | (c) | fewer fread syscalls, no triple-fseek |
| Time-budget `LoadMgr::Poll` native drain | Loader QW-1 | 0.5 d | (a) | bounds per-frame loader stalls (~8 ms) |
| Un-hardcode Expert + add `difficulty:` verb | Difficulty P0 | 0.5 d | (c) | difficulty fully controllable end-to-end |
| Desktop keyboard `JoypadPoll` (core fix) | Input P1 | 0.5–1 d | (c) | menu + gameplay frets/strum/whammy live |
| Whammy as held key (`mSticks[0][1] = held ? -1 : 0`) | Input P1 | 1 line | (c) | analog whammy from keyboard |
| Pause / star-power keys in the same bitmask | Input P1 | minutes | (c) | reuse `kPad_Start` / `mMercuryButton` |
| Teardown SIGSEGV exit-callback reorder (N9/C4) | Complete C4 | 0.5 d | (c) | clean exit (plan written) |
| C1 step-(i): localize why `{game jump}` ≠ game-over | Complete C1 | <1 d | (a)/(c) diag | unblocks results-screen build |
| PostProc `kNoiseGain` tune + A/B screenshot | Complete C9 | 0.5–1 d | (b) | closer to Wii grain |
| C6 survey: do options/pause milos exist + navigate? | Complete C6 | <1 d | investigate | scopes the options track |

**Same-day cluster:** the three day-1 de-risk items (Web-Audio P0+P1, Loader QW-2, Complete
C4) plus Loader QW-1 and Difficulty P0 are *all* sub-day and touch non-overlapping files
(modulo serializing `rb3_game_input.cpp` for the input/difficulty pair) — a single focused
day can land most of the quick-win column and leave the port noticeably more interactive,
responsive, audible, and robust before any multi-day work begins.

---

### Cross-cutting constraints (apply to every track)

- Shared build dir + matched fork are touched by concurrent agents — coordinate or use
  `tools/setup-worktree.sh`; all (a) edits ADDITIVE `#ifdef HX_NATIVE` with byte-identical
  `#else`. Never `git add -A`; re-apply if the permuter shifts a matched file.
- Hot files multiple tracks edit — serialize: `rb3_game_input.cpp` (input + difficulty +
  co-op), `native/CMakeLists.txt` (new TUs + excludes), `band3_link_stubs.s`,
  `main_native.cpp`, `Rnd_Wgpu_RB3.cpp`.
- Web vs native: reproduce/iterate in `rb3-native` (headless, ~3 s rebuilds), confirm each
  fix on web once (C10). Engine pthread paths must be `#ifndef __EMSCRIPTEN__`; web keeps
  its `emscripten_sleep` arm.
- Don't touch `src/system/char/CharBones.cpp` or `src/system/world/LightPreset.cpp` —
  another session's uncommitted work.
