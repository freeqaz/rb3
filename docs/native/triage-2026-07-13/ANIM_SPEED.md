# Triage 2026-07-13 — ANIM-SPEED lane

**Report:** "animations seem like they're firing way too fast" (NEW as of 2026-07-13).
**Lane:** ANIM-SPEED · port 8902 · evidence `/tmp/triage-0713/anim/`
**Binary:** `native/build-native/rb3-native` @ HEAD `d699d837+` (headless, `RB3_HTTP=1`)
**Boots used:** 2 of 6 (gameplay, main_hub).

---

## TL;DR / Verdict

**Measured animation rate / expected rate = ~1.00x. I could NOT reproduce "too fast" on
the native build.** Every animation clock advances at real-time 1.0x, independent of the
(uncapped) frame rate.

- The native frame loop is **uncapped** — no vsync, no sleep, no frame limiter
  (`App::RunWithoutDebugging` HX_NATIVE branch, `src/App.cpp:876`). Headless fps measured
  **76–148 in gameplay, ~130 in the hub** (Wii ships 60fps), i.e. ~2–5x the frame rate.
- **But high fps does NOT scale animation speed**, because every subsystem advances by the
  **delta of an absolute clock** (which telescopes to the same total at any fps), not by a
  fixed step per render frame. Proven empirically: gameplay fps swung 76→148 (a 2x range)
  while the song clock held a **constant 1.00x** (see `gameplay_health.jsonl`).

So the fast **frame rate** is real; the fast **animation** is not (on native). The most
likely source of the user's perception is **H3** — the W31 `set_play` dispatch fix
(rb3 `a3916764` / `6ccc36e3`) newly makes the band actually **perform in-song** (previously
idle/frozen); a newly-animating, vigorously-strumming band at very high fps reads as "busy /
fast" even though the clip rate is correct.

---

## Step 1 — QUANTIFY (measurements)

### Gameplay (song-clock domain) — `gameplay_health.jsonl`
27 `/api/health` samples over 14s in `game_screen`, `songMs` from
`MasterAudio::GetTime()` (the real audio playback clock) vs Python wall clock:

| metric | value |
|---|---|
| **songMsRate = ΔsongMs / Δwall** | **1.00x** (range 0.975–1.020, mean ~1.00) |
| fps | 76–148, drifting with load |
| **rate vs fps** | **fps-INVARIANT** — 2x fps swing, 0% rate change |

Because in-song `TheTaskMgr` is driven by this clock, **track scroll, gems, character
clips, venue props and camera are all bound to a verified 1.0x clock**. → H4 refuted.

### Camera cuts — `cur_shot_timeline.txt`
`{rb3_cur_shot}` sampled at 10Hz for 12s: **6 distinct shots = 0.50 cuts/sec** (one shot
every ~1.5–4s). That is a **normal musical camera cadence**, not rapid-fire. → the venue
director is not "firing" fast.

### Menu / hub (wall-clock domain) — `hub_fps.txt`, `hub_t0.png`/`hub_t3.png`
- `main_hub_screen` runs at **~130 fps** (uncapped, ~2.2x Wii).
- Two hub screenshots 3s apart are **byte-identical** → the hub is effectively **static in
  headless** (no visible animation at all — so nothing "fast" to observe there).

**Measured anim rate / expected rate ≈ 1.0x.** Not 2x, not fast.

---

## Step 2 — Root cause of the *mechanism* (why fps is high but rate is not)

Frame loop (uncapped):
- `src/App.cpp:876` — `for (int frame = 0; (unbounded || …); frame++)` runs `RunOneFrame`
  back-to-back with **no present/vsync/sleep**. `RB3_HTTP=1` sets `unbounded`.

Clocks (all real-time, all delta-based):
- `src/system/obj/Task.cpp:415-424` — default `TaskMgr::Poll`:
  `secs = CyclesToMs(mTime.mCycles)/1000` = **real wall time**; `beat = secs*2` (120 BPM in
  menus). In-song this is overridden by `SetSeconds(songMs/1000)`.
- `src/system/os/Timer.cpp:203-210` — native `Timer::Init` sets `sLowCycles2Ms = 1e-3` and
  `HxNativeMftb()` returns `steady_clock` **microseconds**, so cycles→ms is exactly ×1e-3.
  **Provably 1x.**
- `src/system/char/CharClipDriver.cpp:238-283` — `PreEvaluate` advances
  `mBeat += mTimeScale * deltaBeat` (or `deltaSeconds`), where the delta is the difference
  of an absolute clock read once per frame. **Telescopes → fps-invariant.**
- `src/system/rndobj/Part.cpp:1016-1020` — particle advance = `DeltaSeconds*30` (time-based).
- `src/system/synth/StandardStream.cpp:784-822` — song clock = audio sample cursor, with an
  explicit **wall-clock fallback** in headless (both real-time-referenced). This is also why
  the web build is not at risk of a frame-count song clock: `GetTime` tracks the audio
  cursor, not a 60fps counter.

The one raw per-frame `mFrame++` in the codebase (`CharLipSync.cpp:191`) is a
**catch-up-to-an-absolute-target** loop (`while (mFrame < frameIdx)` where `frameIdx` is a
time-derived index), not a per-render increment. Time-locked.

---

## Hypothesis verdicts

| # | Hypothesis | Verdict | Basis |
|---|---|---|---|
| **H1** | per-frame tick assumed 1/60s while native runs uncapped → everything scales with fps | **REFUTED** | fps uncapped (confirmed 76–300+), but no subsystem steps a fixed amount per render frame; all read absolute-clock deltas. Direct proof: 76→148 fps swing at constant 1.00x rate. |
| **H2** | unit bug (ms vs s, or 30fps clip frames advanced once per render frame) | **REFUTED** | `Timer` 1e-3 correct; char clips + lipsync advance to an absolute time target, not per render frame. |
| **H3** | perception / recent regression (W31 `set_play` → band performs in-song) | **LEADING** | Clip rate measured correct (1.0x). W31 newly animates the band in-song; a performing band at high fps reads as fast. **Perception, not a rate defect.** |
| **H4** | song/audio clock running fast | **REFUTED** | `songMs` = 1.00x over 14s; track/gems would track it and are also 1.0x. |

### Which animations are fast? (the discriminator)
**None, on native headless.** Character clips = 1.0x; venue props = 1.0x; camera cuts = normal
cadence; track/gems = song-locked 1.0x; UI = wall-clock time-locked; hub = static in headless.

---

## Caveat / what remains unproven

I measured **native headless**. If the user's report comes from a **windowed native** or the
**web** build, the shared clock logic predicts the same 1.0x (song clock = audio sample
cursor; wall clock = `steady_clock`). The only residual web-specific risk would be a display
> 60Hz combined with a frame-count-derived clock — but `StandardStream::GetTime` reads the
audio cursor, so that risk is low. **Recommend confirming the reporter's platform + display
refresh rate before any code change**, since no native measurement reproduces the symptom.

If a fix is genuinely wanted for the *frame-rate* (not the animation rate — e.g. to stop the
CPU spinning at 300fps and to make on-screen motion cadence match Wii exactly), the surgical
lever is a frame-rate cap in the `src/App.cpp:876` loop (sleep to a ~16.67ms budget). This
is a **behavior/perf change, not a bug fix** — it would not change measured animation speed,
which is already 1.0x.

---

## Evidence files (`/tmp/triage-0713/anim/`)
- `EVIDENCE_SUMMARY.txt` — index + verdicts
- `gameplay_health.jsonl` — 27 gameplay health samples (songMs rate = 1.00x)
- `cur_shot_timeline.txt` — camera cut cadence (0.5 cuts/s)
- `hub_fps.txt` — main_hub fps (~130)
- `hub_t0.png` / `hub_t3.png` — hub static over 3s (byte-identical)
- `hub.png`, `boot/` — screenshots; `hub_engine.log`, `boot/engine.log` — engine logs
- `hub_measure.py` — hub harness (copied to /tmp, not a repo edit)
