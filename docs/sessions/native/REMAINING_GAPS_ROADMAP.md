# RB3 Native/Web — Remaining Gaps Tracker (post 2026-06-02 batch + depth fix)

> **Purpose.** A single, checkbox-trackable list of the work that is *genuinely
> still open* after the 2026-06-02 batches landed and after the note-highway depth
> fix. This SUPERSEDES the "open" items in
> [`NATIVE_PORT_NEXT_ROADMAP.md`](NATIVE_PORT_NEXT_ROADMAP.md) — that doc's
> prioritized table and the detailed specs in
> [`roadmap-2026-06-02/`](roadmap-2026-06-02/) remain the per-item design source of
> truth; this file is the live status board layered on top.

**Baseline at authoring (2026-06-02):**
- rb3 `master` = `f3cf5bf9` · engine `milo-native-engine` `main` = `1a1f84e`
  (`MILO_ENGINE_PIN` = `1a1f84e`).
- Note-highway depth/occlusion fix landed: rb3 `4154a9ad`, engine `6498fab`
  (extended by the concurrent Tier-2 postproc layering `132add5`). See memory
  `project_track_depth_occlusion_fix`.
- Verify harness for gameplay visuals: `scripts/native/gameplay-depth-capture.py`.
- Build/validate (canonical): `cmake --build native/build-native --target rb3-native`,
  then drive headless over `RB3_HTTP=1` (`/api/screenshot`, `/api/health`,
  `/api/input`, `/api/dta/eval`).

**Layer legend:** **(a)** matched fork `rb3/src/**` (MWCC, asm-matched — additive
`#ifdef HX_NATIVE` only); **(b)** shared engine `milo-native-engine/src/**`
(clang); **(c)** per-decomp glue `rb3/native/src/**` (clang, **preferred**).

---

## Session conventions (READ FIRST — model tiering + image review)

These apply to every task spawned from this roadmap:

1. **Model tiering.**
   - **Opus** drives all *planning*, *diagnosis*, and *high-complexity*
     implementation (engine rendering/FX, async-loader architecture, anything
     touching the matched fork's asm-shape or cross-camera/depth/postproc logic).
   - **Sonnet** handles *simpler, mechanical* implementation (glue wiring, verb
     plumbing, CMake excludes, comment/stale-doc fixes, screenshot capture runs,
     repetitive ports with a worked example).
   - Each item below is tagged **[Opus]** or **[Sonnet]** (or **[Opus→Sonnet]** =
     Opus plans, Sonnet executes the mechanical part).
2. **Image review is Opus-only.** Sonnet (or a script) may *capture* screenshots,
   but **every screenshot/visual comparison MUST be reviewed/interpreted by Opus.**
   Do not let a Sonnet agent conclude "looks correct" from an image — it captures,
   Opus judges. Visual-parity verdicts (FX present? glow right? occlusion gone?)
   are Opus calls.
3. **Concurrency safety** (carried from NEXT_ROADMAP): shared build dir + matched
   fork; coordinate or `tools/setup-worktree.sh`; all (a) edits additive
   `#ifdef HX_NATIVE` with byte-identical `#else`; never `git add -A`; serialize
   hot files (`rb3_game_input.cpp`, `native/CMakeLists.txt`, `main_native.cpp`,
   `Rnd_Wgpu_RB3.cpp`, `band3_link_stubs.s`). Engine builds: another agent may move
   engine `main` / rb3 `master` / repoint `build-native`'s `MILO_ENGINE_PATH`
   mid-task — re-check live state (`git log`, CMakeCache, `strings` the binary for
   your change) before trusting a build.
4. **Web confirmation:** iterate in `rb3-native` (headless, ~3 s rebuilds), confirm
   each fix once on web (the C10 track).

---

## A. Gameplay FX + HUD visual parity  — biggest *visible* gap

Empirically confirmed (2026-06-02) by comparing `gameplay-depth-capture.py` output
against `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`. Depth/
compositing is correct (highway on top, full-color gems over the graded venue), but
the gameplay *FX layer* is largely missing. Captures used **autohit**, so hits fire
continuously yet produce no flames → the FX are absent, not merely unphotographed.

- [ ] **A1 — Hit/flame FX at the strike line.** No flame burst on gem hit despite
      autohit. Roadmap marks N8/C8 "landed" (`a8924538`) but that was the
      whammy/SIGILL crash fix — the *visual* flames do not render. Re-verify, then
      implement. Refs: `docs/sessions/native/N8_HIT_FLAME_FX_PLAN.md`. **[Opus]**
      (diagnosis: is the FX milo/emitter loaded? drawn? culled? — cross-camera/RTT
      aware).
- [ ] **A2 — Gem / fret-button glow.** Gems and fret buttons render flat-shaded;
      retail gems/buttons glow (prelit/additive). Likely a material/blend or
      missing additive-glow submesh path in the rb3 backend. **[Opus]** (engine
      material/blend).
- [ ] **A3 — Star-power / multiplier HUD gauge.** The retail 4-diamond multiplier +
      SP gauge (top-right) is missing/!thin. Confirm which HUD milo elements draw vs
      are stubbed. Refs: `docs/sessions/native/SCORE_HUD.md`, `SCORE_PCT.md`.
      **[Opus→Sonnet]** (Opus scopes which elements; Sonnet wires draws).
- [ ] **A4 — Highway lane lighting / glow.** Highway reads matte vs retail's lit
      lanes. Lower priority cosmetic; bundle with A2 if same root (prelit/material).
      **[Opus]**.
- [ ] **A5 — Crowd slivers (N5).** Residual char-IK slivers suppressed by an engine
      extent-ratio guard; decide screenshot-diff vs char-IK bring-up. Refs:
      `CROWD_SLIVER_DIAGNOSIS.md`, `CROWD_GUARD_PLAN.md`. **[Opus]**, low stakes.
- [ ] **A6 — PostProc noise-grain fidelity (C9).** `kNoiseGain` conservative; tune
      vs Wii grain. Single engine constant + A/B screenshots. **[Opus→Sonnet]**
      (Sonnet runs A/B captures, Opus picks the value).

---

## B. Web parity + audibility  — the actual deliverable is the browser

Native is well ahead of *verified* web.

- [ ] **B1 — In-browser audibility proof (web-audio Phase 1).** "Song plays on web"
      has never been confirmed audible in a real tab — only that the decode/mix loop
      runs headless. Add a capture-WAV assert using the existing
      `rb3CaptureAudio()`/`rb3DownloadAudio()` hooks (no source change). Refs:
      `roadmap-2026-06-02/web-audio.md` (Phase 1). **[Sonnet]** (test only; Opus
      reviews any waveform/spectrogram image).
- [ ] **B2 — C10 web-parity sweep.** Confirm each native fix renders on web —
      **including the depth/grade fix and the gameplay-FX work from track A.**
      Capture web screenshots; **Opus reviews** the visual parity. Refs:
      `roadmap-2026-06-02/completeness-audit.md` (C10). **[Opus]** for the verdict,
      **[Sonnet]** for the capture runs.
- [ ] **B3 — Web persistence backend (C2 web variant).** C2 persistence is host-FS
      on native; web needs IndexedDB (+ MEMFS-persist). Design behind the existing
      C2 interface. **[Opus→Sonnet]**.
- [ ] **B4 — Menu/UI SFX on web (web-audio Phase 2).** Verify RB3
      `SampleInst::NewInst` menu blips/confirms are audible in-tab. **[Sonnet]**,
      Opus reviews any audio capture.

---

## C. Async loader  — the big architectural item

- [ ] **C1 — LW-1: file I/O off the render thread (async).** Per-song load still
      freezes the frame loop; this overlaps load with rendering and lets the
      `world/Dir.cpp` force-poll hack be removed. ~8–14 d. New engine TU
      (`AsyncLoaderThread_Native` or similar) + matched-fork hooks. Refs:
      `roadmap-2026-06-02/loader-performance.md` (LW-1), `docs/native/PLAN_LOADER_ASYNC.md`.
      Engine pthread paths `#ifndef __EMSCRIPTEN__`. **[Opus]** (architecture).
- [ ] **C2 — LW-2: decompression off-thread.** Only if Phase-0 instrumentation
      (`RB3_LOADER_INSTRUMENT=1`) shows decompression dominates after LW-1. Gated on
      C1's measurement. **[Opus]**, P2.
- [ ] **C3 — QW-3: defer/skip boot crowd + colorpalettes preload.** Still-open quick
      win: removes the biggest synchronous boot burst from the critical path. ~0.5–1
      d, HX_NATIVE-additive. Refs: `roadmap-2026-06-02/loader-performance.md` (QW-3).
      **[Sonnet]** (Opus confirms what's safe to defer).

---

## D. Interactivity verification  — landed but unproven by eye

Input + difficulty + pause landed today on **autohit-tested** paths; confirm a human
can actually play.

- [ ] **D1 — Real keyboard play (no autohit).** Drive `/api/input` (or real key
      events) to strum a note and register a hit without autohit; confirm
      frets/strum/whammy map. Refs: `roadmap-2026-06-02/input-keyboard-gamepad.md`
      (Phase 1/3). **[Sonnet]** capture, **[Opus]** reviews the gameplay image +
      hit registration.
- [ ] **D2 — On-screen difficulty/instrument nav (Difficulty Phase 2).** Keyboard
      arrow-key picking of the real choose-lists (vs the `difficulty:` verb).
      Refs: `roadmap-2026-06-02/difficulty-instrument-select.md` (Phase 2).
      **[Opus→Sonnet]**.
- [ ] **D3 — Pause → resume mid-song (C6 full flow).** Exercise the live
      `overshell:` pause/options/resume end-to-end. **[Sonnet]**, Opus reviews
      screenshots.
- [ ] **D4 — USB gamepad mapping (Input Phase 4).** Play "joypad guitar" on an
      Xbox/PS pad. Additive after D1. **[Sonnet]**.

---

## E. Deferred by design (tracked, not scheduled)

- [ ] **E1 — Vocals / mic input (C5).** `PitchDetector` stubbed
      (`band3_link_stubs.s:38,398-401`); zero mics. 5–8 d. Roadmap non-goal until
      guitar/drums singleplayer fully polished. **[Opus]**.
- [ ] **E2 — Local co-op input routing (C7).** `BandUserMgr(4,3)` builds 4 players
      but only `gSynthUser` is wired. Gated on the input track. 3–5 d. **[Opus→Sonnet]**.
- [ ] **E3 — Interactive calibration test-tone.** `cal_welcome` UIList SIGSEGVs
      headless; needs real audio/input or engine UI bring-up. C3 offsets already
      persist. **[Opus]**.

---

## Recommended sequence (suggested, not binding)

1. **Track A (gameplay FX/HUD)** — most visible gap, builds on the depth/compositing
   work; A1 (flames) → A3 (HUD gauge) → A2/A4 (glow). Opus-led.
2. **Track D (interactivity verification)** — cheap, finds regressions in today's
   input/difficulty landings. Mostly Sonnet under Opus review.
3. **Track B (web parity + audibility)** — after A lands, sweep it onto web (B2) and
   prove audio (B1). The browser is the real target.
4. **Track C1 (async loader)** — the large architectural payoff; schedule when a
   multi-day block is available. C3 (QW-3) is a standalone quick win anytime.
5. **E-track** stays deferred.

---

## Done-today context (do NOT redo)

Landed 2026-06-02 (per `NATIVE_PORT_NEXT_ROADMAP.md` header + git log): web
audio/SFX + XMA→PCM sidecars, keyboard/gamepad input, loader QW-1/QW-2,
difficulty/instrument select, **C1 results screen** (`f3cf5bf9`), C2 persistence,
C3 calibration offsets, C4 teardown SIGSEGV, C6 options/pause, C8 gameplay crash
fixes (HitGemHook SIGILL + whammy MILO_FAIL), and the note-highway depth fix
(`4154a9ad`/`6498fab`).
