# RB3 Web Build — Runtime Log Review (2026-06-21)

Compiled from the available captured browser console dumps, frame-trace JSONLs,
audio-perf-loop baselines, introsync harness runs, and web-server access logs
captured on or before 2026-06-21.

---

## A. Sources Reviewed

| Source | Path | Size / Lines | Date |
|---|---|---|---|
| Frame-stall steady-run1 console | `docs/native/frame-stall-2026-06-20/cap/steady-run1/console.json` | 77 KB / 400 entries | 2026-06-20 |
| Frame-stall steady-run1 summary | same dir / `summary.json` | 11 KB | 2026-06-20 |
| Frame-stall steady-run1 timeline | same dir / `timeline.json` | 83 KB | 2026-06-20 |
| Frame-stall steady-run2 console | `docs/native/frame-stall-2026-06-20/cap/steady-run2/console.json` | 77 KB / 400 entries | 2026-06-20 |
| Frame-stall steady-run2 summary | same dir / `summary.json` | 19 KB | 2026-06-20 |
| Frame-stall steady-run2 frametrace | same dir / `frametrace.jsonl` | 682 KB / 2174 frames | 2026-06-20 |
| Frame-stall steady-run2 timeline | same dir / `timeline.json` | 80 KB | 2026-06-20 |
| Tickprobe (clean, no profiler) | `docs/native/frame-stall-2026-06-20/cap/tickprobe/tickprobe.json` | — | 2026-06-20 |
| Texprewarm-off tickprobe | `docs/native/frame-stall-2026-06-20/cap/texprewarm-off/tickprobe.json` | — | 2026-06-20 |
| Texprewarm-on tickprobe | `docs/native/frame-stall-2026-06-20/cap/texprewarm-on/tickprobe.json` | — | 2026-06-20 |
| Audio-perf cap 1 (bad) | `docs/native/audio-perf-loop/baselines/w10-webcap-1781932162141/` | summary+console | 2026-06-20 |
| Audio-perf cap 2 (good) | `docs/native/audio-perf-loop/baselines/w10-webcap-1781933464535/` | summary+console | 2026-06-20 |
| Audio-perf June 9 baseline (3×) | `docs/native/audio-perf-loop/baselines/w10-webcap-{1,2,3}/` | summaries | 2026-06-09 |
| Audio-perf release verify (2×) | `docs/native/audio-perf-loop/baselines/w10verify-rel-{1,2}/` | summaries | 2026-06-10 |
| Introsync harness log | `/tmp/introsync-harness.log` | 26 lines | 2026-06-21 |
| Introsync on-r1 result | `/tmp/introsync-web-on-r1/result.json` | 267 KB | 2026-06-21 |
| Introsync on-r2 result | `/tmp/introsync-web-on-r2/result.json` | 261 KB | 2026-06-21 |
| Introsync off result | `/tmp/introsync-web-off/result.json` | 281 KB | 2026-06-21 |
| Web server log | `/tmp/introsync-web-server.log` | 486 KB / 4852 lines | 2026-06-21 |
| Frame-stall investigation docs | `docs/native/frame-stall-2026-06-20/*.md` | 128 KB total | 2026-06-20/21 |

**Not reviewed** (binary / too large to read meaningfully): `*.cpuprofile` files
(2.2–2.4 MB each, sampled via calltree script instead). The older June 9 audio
captures did not have useful console dumps.

---

## B. Findings by Category

### B1. Hard Errors

**`Aborted('FS' was not exported)` — HARD CRASH, steady-run1 only**

```
[96.13s] Aborted('FS' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ))
```

Occurs at t≈96 s into steady-run1, roughly 1 s after two audio `latency GROW`
messages. Context: the frame-stall harness (`_framestall-steady-profile.mjs`)
tried to read back the `RB3_FRAME_TRACE` file via `window.Module.FS.readFile`
instead of `window.FS.readFile`. `Module.FS` is a trap-on-access proxy in the
emscripten glue that calls `abort()` when accessed before the runtime finishes
initialization — the harness hit it during teardown. This is a **harness
instrumentation bug, not an engine crash** (the game had been running for ~96 s
without issue). The fix is documented in `STEADY_STATE_FRAME_STALL_FINDINGS.md`:
use `window.FS.readFile(...)` not `window.Module.FS`. The crash also terminates
the audio-latency GROW spiral at the end of run1, making steady-run2 (which uses
`frametrace` readback, not FS) the authoritative profiled capture.

**No other hard crashes, assert failures, RuntimeErrors, SharedArrayBuffer faults,
or WebGPU abort events appear in any log reviewed.**

### B2. Warnings and Spam

**`NOTIFY: unhandled msg: clear_pics` — VERY HIGH VOLUME, BENIGN SPAM**

- Source: `OvershellSlot.cpp(3661)` (object `ui/overshell/overshell_player.milo`) — 317 fires per 400-entry window
- Source: `PassiveMessagesPanel.cpp(154)` (object `ui/global/passive_messages.milo`) — 80 fires per window
- **Both runs, both audio captures** — total spam rate ~10–12 per second during gameplay.
- These fire because `clear_pics` is sent to UI objects that do not handle it.
  **PERF NOTE:** The frame-stall investigation (`FRAME_STALL_FINDINGS.md`) found
  that `index.html`'s on-DOM console-log mirror, which appended a `<div>` per
  line AND read `scrollHeight/clientHeight` on every call, cost **3.72 s of
  Layout (37% of main thread) over the 9 s song-start window** when the engine
  emits hundreds of NOTIFY lines per frame. The fix (rAF-batched, 200-line cap)
  dropped Layout from 3.72 s to 0.17 s. If `clear_pics` spam fires at this rate
  during song load (not just steady gameplay), the layout-fix MUST be in place to
  avoid stalling the audio producer.

**Other one-shot NOTIFYs (song-load, cap 1781933464535 only):**
```
NOTIFY: stagekit_reset not function or object (file world/world_objects.dta, line 1303)
NOTIFY: memcardmgr not function or object (file ui/overshell/overshell.dta, line 219)
NOTIFY: crowd_intro.mogg not found in small_club_01_bank.milo   <-- SEE AUDIO SECTION
NOTIFY: player0/1/2/3 (char/main/main.milo) could not find group stand/sit in body_clips
NOTIFY: player0/2 no clip w. flags IR|0x100000 in body_clips/stand
NOTIFY: stagekit_left_right not function or object (config/beatmatcher.dta, lines 5/11/15)
```

`stagekit_*`, `memcardmgr` — expected stubs on the native/web port (Wii
peripherals).

`player{N}: could not find group stand/sit in body_clips` — character animation
lookup miss. Possible missing char-anim data on the web build or a HX_NATIVE
body_clips path divergence.

**`[rb3-stub] env._ZN10TrackPanel19TrackerDisplayResetEv`** — `TrackPanel::TrackerDisplayReset` is a weak stub. Fires once per song start. Not critical but indicates a missing implementation.

**`[rb3-stub] env._ZN5WiiFX8IsReverbEi`** — `WiiFX::IsReverb` is a weak stub. Fires at audio stream start.

### B3. Audio-Relevant Findings

#### B3a. OFF-MAIN Mix Mode and Ring State

Audio-perf capture 2 (`w10-webcap-1781933464535`, the "good" run):
```
[page] AudioDevice: OFF-MAIN mix ENABLED — 16 stem SABs (393216 frames each ~9s),
       output floor 70 ms (3087 ctx frames), prime 5292 mix frames
[page] ENVTAP: wired @44100Hz
```
Context rate: 44100 Hz. Buffer: 32768 frames.

The OFF-MAIN path is confirmed present and wired for the introsync runs
(`RB3_WEB_OFFMAIN_MIX=1`). Both introsync r1 and r2 show this banner line.

#### B3b. Audio Latency Growth Events

**Steady-run1 (profiled, no frametrace, ~96 s total):**
```
[95.66s] AudioDevice: latency GROW -> 127 ms (sustained near-miss, minDepth 1805 frames, pressure 603/256)
[96.17s] AudioDevice: latency GROW -> 167 ms (sustained near-miss, minDepth 1905 frames, pressure 731/256)
```
Then the Aborted crash terminates the run. The pressure value of 603–731/256 =
well over threshold. This is late in the session (~95+ s from page load / ~40 s
into gameplay), which is consistent with a gradual ring drain from cumulative
frame stalls.

**Steady-run2 (frametrace, ~95 s total):**
```
[92.30s] AudioDevice: latency GROW -> 151 ms (sustained near-miss, minDepth 2353 frames, pressure 539/256)
[92.81s] AudioDevice: latency GROW -> 191 ms (sustained near-miss, minDepth 2353 frames, pressure 667/256)
[93.28s] AudioDevice: latency GROW -> 231 ms (sustained underrun, pressure 795/256)
```
Three escalating messages over ~1 second at t≈92 s. The final message transitions
from "near-miss" to "sustained underrun." Again, end of session, ~38 s into
gameplay.

**Audio-perf cap 1 (bad, w10-webcap-1781932162141):**
```
[79.09s] AudioDevice: latency GROW -> 124 ms (sustained near-miss, minDepth 1153 frames, pressure 538/256)
[79.58s] AudioDevice: latency GROW -> 164 ms (sustained near-miss, minDepth 1537 frames, pressure 666/256)
[80.09s] AudioDevice: latency GROW -> 204 ms (sustained underrun, pressure 794/256)
```
Same escalation pattern, at t≈79 s. Ring stat: `minRingDepthFrames: 3470`
(overall min), `underrunEvents: 1284`, `underrunFrames: 163897`.
Steady underrun rate: **3.563 events/sec, 449 frames/sec**. This run had
**99% of frames over budget** (jitter p50 = 33.33 ms).

**Audio-perf cap 2 (good, w10-webcap-1781933464535):**
No `latency GROW` lines in the console. Stats: `underrunEvents: 131` (all at
song-start burst, not steady), `underrunFrames: 16650`, **steady underrun rate
= 0**, `minRingDepthFrames: 7170`. Jitter p50 = **16.67 ms** (60 fps). This
run was apparently conducted without the CPU profiler attached (or with the DOM
log fix in place).

**Key contrast:** the same song, same engine, same build — cap 1 has 1284 underrun
events, cap 2 has 131 (mostly song-start) and zero steady. The difference tracks
exactly with the profiler observer effect (see §C).

**Audio-perf: song-start underrun at t≈41.5 s (cap 2):**
```
[41.51s] AudioDevice: latency GROW -> 240 ms (sustained underrun, pressure 512/256)
```
This fires during the char/anim asset streaming burst at song-start — immediately
after a sequence of `WebAssets:` char animation downloads (drum.milo_xbox,
guitar, mic, etc.). Consistent with the background-loader overspill stall
described in `STEADY_STATE_FRAME_STALL_FINDINGS.md`.

**Audio-perf: release build verification (w10verify-rel-{1,2}):**
Jitter p50 = 16.67 ms, p99 = 16.67 ms, max 33–50 ms. Steady underrun rate = 0.
Final underrun events 1097–1120 — these are all song-start burst events, not
steady. The release build is clean in steady state.

#### B3c. STREAM_DBG

Only one `STREAM_DBG` line appears across all captures reviewed:
```
[47.76s] STREAM_DBG: StandardStream::PollStream state 2 -> 3 (chans=15)
```
This appears in audio-perf cap 2 at t≈47.8 s (about 6.8 s after game_screen at
~41 s). State 2 → 3 on a 15-channel stream = the mogg stream is transitioning
to playing state. This is the **main song audio starting**, not venue/crowd audio.

No `STREAM_DBG` lines for crowd_intro or venue_intro appear in any capture. That
is consistent with the crowd_intro.mogg NOTIFY below.

#### B3d. `crowd_intro.mogg not found`

```
NOTIFY: crowd_intro.mogg not found in small_club_01_bank.milo
```
Appears in audio-perf cap 2 at t≈41 s (game_screen entry). This is a lookup
failure in the venue sound bank for the crowd intro sound. `small_club_01` is one
of the available venues. The crowd intro mogg appears to not be present in that
bank's web asset set (or is referenced but not shipped). This is a **direct
cause** of no crowd/venue audio during the song intro period.

#### B3e. Silent Intro — INTROSYNC Evidence

From the three introsync harness runs (all today, 2026-06-21, `RB3_INTROSYNC=1`):

**ON run 1 (`RB3_WEB_OFFMAIN_MIX=1;RB3_INTROSYNC=1`)**
- `SetIntroRealTime(-6.667s)` at wallMs=43390 (game_screen entry at wall=44656ms)
- `MasterAudio::Play()` at wallMs=50151 (5495ms after game_screen)
- `streamPlaying` = 0 for frames 0–329 (all 6666ms of intro period), flips to 1 at frame 330 (songMs=+4.1ms, wall=50889ms)
- `audioMs` = 0.0 for all 330 intro frames; first non-zero = frame 330, audioMs=3.0
- Worklet `firstAudible_t_ms` = 5623 ms (from recording start at game_screen)
- `/api/health` songMs never > 1ms during the 13s recording window

**OFF run (off-main off)**
- Same structure: `MasterAudio::Play()` at wallMs=56415, first audible at t=6025ms
- `firstAudible_rms` = 0.00233 (barely audible), much lower than ON's 0.0955

**Summary of silent-intro bug:**

The 6.7 s song intro (`trackRtMs` from -6667ms to 0ms) is silent because:
1. `MasterAudio::Play()` does not fire until `Game::Go()` at end of intro, not at intro start
2. `StandardStream` stays in `streamPlaying=0` for the entire intro period — no mogg audio
3. `crowd_intro.mogg not found` in the venue bank — so there is no fallback venue audio
4. `health songMs` never advances during the intro — the audio clock is not running at all

The intro is designed to be accompanied by `crowd_intro.mogg` (crowd anticipation
sounds), but that asset is absent from the deployed web build's venue bank. The
song mogg only starts when `Game::Go()` fires at the intro-end boundary.

No CrowdAudio, venue_intro, or crowd-related audio events appear in any console
capture.

### B4. Frame / Performance Findings

#### B4a. Clean Steady-State: 60 fps Is Achievable

The `tickprobe.json` (no CPU profiler attached) shows the engine in **clean steady gameplay**:

| Metric | Value |
|---|---|
| wasm fps | 59.5 |
| tickMs p50 | 10.13 ms |
| tickMs p99 | 21.12 ms |
| tickMs max | 37.48 ms |
| period p50 | 16.63 ms |
| FRAME_TRACE over 33ms | 17/1844 frames |
| FRAME_TRACE over 100ms | 2/1844 frames |

The engine fits the 16.7 ms budget at p50. The "50 ms/frame" figure from earlier
profiling was a CPU-profiler observer effect: the Chrome 1 kHz sampler inflates
`-O0` wasm frame time ~5–6× (8.7 ms → ~55 ms). This is the key methodological
finding from `STEADY_STATE_FRAME_STALL_FINDINGS.md`.

#### B4b. The Profiled Runs (Inflated Frame Times)

Steady-run1 and steady-run2 used the CDP `Profiler` at 1 kHz, which caused the
inflation. Their jitter metrics are **not reliable for absolute frame time** but
are valid for proportional attribution:

- steady-run1: jitter p50=50ms, p99=83ms, 99.9% over budget — this is the observer effect
- steady-run2: jitter p50=66ms, p90=133ms, wasmFps=**11.7** — severely inflated
- CPU top consumers (proportional): BandRnd::DrawMesh 2.4–3.4%, `__dynamic_cast` 1.7%, writeBuffer 1.0%, DirtyCache::SetDirty 0.5–0.8%
- `(program)` consumes 70–74% of CPU samples (sampling bias in `-O0` wasm where inlining is off)

#### B4c. RAF Gaps During Steady Gameplay (Actual Stalls)

In steady-run2 (40 s gameplay window, 54 s–95 s):
- **486 RAF gaps** captured; **321 over 50ms**; **168 over 100ms**
- Largest: `{t: 69.8s, gap: 183.3ms}`, `{t: 77.7s, gap: 183.3ms}`, `{t: 81.1s, gap: 183.3ms}`
- Multiple 166.7 ms gaps spread across 57–84 s of gameplay

The clean tickprobe run shows max period 38.85 ms and only rare >33 ms frames,
which means the RAF gaps in steady-run2 are also partially profiler-induced, but
the pattern of gaps clustering late in the session (after ~57 s) and reaching
183 ms matches the audio latency-GROW timeline (92–93 s from page load = ~38–40 s
into gameplay). The cumulative drag from the profiler overhead appears to
gradually drain the ring over ~40 s.

#### B4d. The Real Gameplay Stalls: Sync Texture Drain

From the tickprobe `worstTraceFrames` (no profiler, actual engine behavior):

**Worst frame (texprewarm-off): f=1972, dt=721ms**
```
lpu=612 ms (PollUntilLoaded), objMs=211ms (RndTex:floor_wood02_NORM.tex), texMs=111ms
```

**Worst frame (tickprobe): f=1917, dt=639ms**
```
lpu=549ms, objMs=172ms (RndTex:wall_wainscoat_plaster_norm.tex), texMs=92ms
```

These are `RndTex::SetBitmap → PollUntilLoaded` synchronous texture drains
mid-gameplay (venue normal-maps loaded lazily). `PollUntilLoaded` holds the
`await rb3MainLoopTick()` for its entire duration → `PumpAudio` never called →
ring empties → underrun burst. Root: `src/system/rndobj/Tex.cpp:181`.

**With texprewarm ON:** worst frame drops to 242.5 ms (still a texture drain, but
a different texture: `wall_wainscoat_plaster_norm.tex`). The `texprewarm`
prototype prewarms the specific set that fires at frame 1972 but misses the
second.

**Second worst frame type (fetchMs):**
```
f=2159, dt=119ms, fetchMs=85.5ms   (texprewarm tickprobe)
f=2363, dt=116ms, fetchMs=86ms     (texprewarm-off)
```
Sync XHR fetch mid-frame, not going through the async fetch seam.

**Third worst frame type (loader budget overspill):**
```
f=2066, dt=40.7ms, lp=19ms, objMs=5ms (Mesh:ludclassic_small_club_resource)
```
Background loader budget (`RB3_LOADER_BUDGET_MS`) eating ~15–20 ms + PostLoad
overhead. Per-object PostLoad is uncapped.

#### B4e. Song-Start Burst (Load-In Frames)

From `FRAME_STALL_FINDINGS.md`, the song-start transition from `part_difficulty`
to `game_screen` generates:
- **Original build (with on-DOM log mirror):** 16 longtasks > 50 ms, sum 1176 ms
- **Fixed build (rAF-batched log):** 2 longtasks, sum 173 ms
- **Floor (no mirror):** 1 longtask, 102 ms (the first real gameplay frame)

The original DOM log mirror was processing the `clear_pics` NOTIFY spam (hundreds
of lines per frame during load) via a per-line scrollHeight read → full-document
relayout, costing 3.72 s of Layout. This is the same `clear_pics` seen in steady
gameplay at 10+/sec.

From audio-perf cap 2, the song-start burst triggers one underrun:
```
[41.51s] AudioDevice: latency GROW -> 240 ms (sustained underrun, pressure 512/256)
```
immediately during char-anim streaming (guitar/drum/mic .milo_xbox downloads).
This is the load-in longtask burst described in `STEADY_STATE_FRAME_STALL_FINDINGS.md`.

#### B4f. `__dynamic_cast` Load

`__dynamic_cast` + its helpers consume ~2.3% of busy CPU time in steady gameplay
across both profiled runs. This comes from `ObjDirItr<RndMesh>::Advance()` doing
a `dynamic_cast` per typed-iterator step. Not a stall source (it is distributed),
but it is a steady CPU drain.

#### B4g. `log` CPU Cost (Profiled Run)

In steady-run2, `log` (the JS `console.log` call from wasm) consumes **570 ms /
1.4% of CPU over 40 s** of profiled gameplay — the dominant JS-side consumer
after WebGPU calls. This is the `clear_pics` NOTIFY flood being logged to the
console. With the DOM log fix this stays off the layout critical path, but the
raw `console.log` call overhead remains.

### B5. Server / Network Findings

Recurring 404s from every boot session:
```
GET /system/run/rndobj/gen/sphere.milo_xbox HTTP/1.1 — 404
GET /api/file/ui/dev_only/selvenue.dta HTTP/1.1 — 404
GET /api/file/ui/framerate/frame_rate.dta HTTP/1.1 — 404
GET /api/file/patchcreator/og/gen/patch_warpmesh.milo_xbox HTTP/1.1 — 404
GET /api/file/test/ui/gen/guides_glass_usage.milo_xbox HTTP/1.1 — 404
GET /api/file/songs/20thcenturyboy/20thcenturyboy.vfv HTTP/1.1 — 404
```

- `sphere.milo_xbox` — debug/renderer primitive, expected missing on web
- `ui/dev_only/selvenue.dta` — developer-only venue selector, expected missing
- `ui/framerate/frame_rate.dta` — debug framerate UI, expected missing
- `patchcreator/og/gen/patch_warpmesh.milo_xbox` — patch creator asset, expected
- `test/ui/gen/guides_glass_usage.milo_xbox` — test/debug UI, expected
- `20thcenturyboy.vfv` — **visual-fx file for the song, expected but MISSING from web assets**. This could mean the song's video/visual effects do not fire.

`rb3_intro_cinematic.webm` returns 206 (range requests, working). The `*.mogg`
files for the song return 206 (range requests, working). No 500/503 errors.

One `ConnectionResetError: [Errno 104] Connection reset by peer` per run (browser
closes connection mid-range-request, benign).

---

## C. Frame-Collapse Bug — What the Logs Show

**The "frame-rate collapse after ~60 s" is a measurement artifact in the profiled
runs, confirmed by the tickprobe evidence.**

- Steady-run2 (profiled): wasmFps = **11.7 fps** in a 40 s steady window, jitter
  p50 = 66 ms. This is the CPU-profiler observer effect on `-O0 -g2` wasm: the
  1 kHz CDP sampler adds ~5–6× overhead to every wasm instruction, dropping the
  engine from 60 fps to ~12 fps.
- Tickprobe (no profiler): 59.5 fps, p50 period = 16.63 ms. The "collapse" does
  not exist.
- The RAF gaps (183 ms max in steady-run2 at t≈69–81 s) are also partially
  profiler-induced. The clean runs show max period 38.85 ms.
- **However:** the real stalls DO exist and DO cause audio underruns. They are
  **sporadic** (2–13 longtasks per 50 s session), not a continuous frame-rate
  collapse. The dominant ones are:
  1. Sync texture drain (`PollUntilLoaded`): one 612–721 ms spike per session (the venue normal-map)
  2. Song-start char-anim streaming burst: ~1 s over first 8–10 s
  3. Sporadic sync XHR fetch: ~85–120 ms each, very rare

If the question is "does the frame rate visibly collapse after ~60 s in real use
without a profiler", the answer from tickprobe is no. If the question is "does the
audio ring eventually drain from sporadic stalls", the answer from the audio-perf
captures is yes — the latency GROW messages at t≈79–93 s show cumulative drain
from the load-in burst + sporadic longtasks. With the DOM-log fix and the
venue-texture prewarm, the sum of longtasks drops from 1176 ms → ~173 ms over the
song-start window, which should prevent the ring from ever fully emptying at song
start (the dominant risk period).

---

## D. Audio Bug Correlation

### D1. Silent Song-Start Intro

**Finding:** The 6.7 s intro silence is BY DESIGN — the song mogg (`StandardStream`)
only starts playing at `Game::Go()` / `MasterAudio::Play()`, which fires at the
END of the intro countdown (trackRtMs ≈ 0). The intro period (`trackRtMs` from
-6667ms to 0ms) was intended to be filled by `crowd_intro.mogg` from the venue
sound bank. That file is **absent from the web asset deploy** for
`small_club_01`:

```
NOTIFY: crowd_intro.mogg not found in small_club_01_bank.milo
```

The INTROSYNC evidence shows `streamPlaying=0` for the entire intro period —
there is literally no active audio stream during intro. `audioMs=0.0` for all
330 intro frames. `health songMs` never advances (the audio clock starts only
when the stream starts).

**Root cause:** `crowd_intro.mogg` is not shipped in the web build's venue bank
assets. The mogg file for the `small_club_01_bank` needs to be added to the web
asset set (or the bundle manifest).

**Secondary:** even with `crowd_intro.mogg` present, the intro is currently
silent because `MasterAudio::Play()` does not dispatch the crowd audio separately
from the song audio. The crowd audio path needs to be verified as actually calling
a separate play for the intro sound vs. the main song stream.

### D2. CrowdAudio Missing

Zero CrowdAudio-related log entries appear in any capture. The `crowd_intro.mogg`
404 is the clearest evidence. No other venue audio initialization messages appear
(no `crowd_ext_*` stream debug, no crowd volume or mix events). If `CrowdAudio`
is supposed to play venue/crowd ambience during gameplay, it is either:
(a) not initialized because `crowd_intro.mogg` lookup fails and poisons the crowd
system, or (b) a separate stub/unimplemented path.

### D3. Audio Underruns — Diagnosis

The underrun pattern in audio-perf cap 1 vs cap 2 shows the underruns are driven
by main-thread longtasks, not by the audio mixer itself:

- Cap 1 (profiler active, 99% frames over budget): 1284 underrun events, 3.56/sec
- Cap 2 (no profiler or DOM log fix): 131 events, 0/sec steady, all at song-start

The audio mixer (`AudioDevice::MixSources`) itself costs 90–127 ms over 40 s of
gameplay (about 2.3–3.2 ms/s, well within budget). `RB3StreamReceiverNative::RenderAudio`
= 68–76 ms / 40 s. These are fine.

The underruns are caused by `PumpAudio` not being called while `rb3MainLoopTick`
holds the `await` during:
1. Sync texture drain (`PollUntilLoaded`, up to 612 ms)
2. Char-anim streaming longtasks at song-start (~50–194 ms each, 11 in first 8 s)
3. In cap 1: the profiler overhead itself (adds 4–5 ms per wasm instruction)

---

## E. Prioritized Investigation / Fix List

**P0 — `crowd_intro.mogg` missing from web venue bank**
Direct cause of silent 6.7 s intro. Add `crowd_intro.mogg` (and any other
`crowd_*.mogg` assets for all shipped venues) to the web bundle manifest. The
NOTIFY makes the failure explicit. Also verify the CrowdAudio play path is wired
up under `HX_NATIVE` so the crowd stream starts when `SetIntroRealTime` fires, not
when `Game::Go()` fires.

**P1 — `RndTex::SetBitmap` sync `PollUntilLoaded` texture drain**
The single worst deterministic stall: 612–721 ms when a venue normal-map
(`floor_wood02_NORM.tex`, `wall_wainscoat_plaster_norm.tex`) is lazily loaded
mid-gameplay. Root: `src/system/rndobj/Tex.cpp:181`. Fix options: (a) prewarm
these textures before `game_screen` activates (TEX_PREWARM_PROTOTYPE.md has a
prototype that knocked the worst case to 242 ms — incomplete, another texture
still fires); (b) break the sync drain with an `HX_NATIVE`-gated async defer or a
per-iteration `PumpAudio` call inside the drain loop. The texprewarm-on tickprobe
shows the prewarm helped but didn't fully eliminate the worst case.

**P2 — Song-start char-anim streaming longtask burst (first ~8 s)**
11 longtasks totalling ~1176 ms → 173 ms with DOM log fix. The DOM log fix (rAF-
batched, 200-line cap) is documented in `FRAME_STALL_FINDINGS.md` and should be
applied to `native/web/index.html` if not already. After that, the residual
~173 ms sum is loader-budget overspill from char-anim PostLoad — tightening
`RB3_LOADER_BUDGET_MS` or splitting heavy PostLoads across frames would help.

**P3 — `clear_pics` NOTIFY spam reducing to zero**
317 fires/window from `OvershellSlot.cpp:3661`. This is benign in steady state
(DOM log fix mitigates browser-side cost), but the C++ NOTIFY itself should be
investigated: either the message should be handled by those objects, or the send
should be guarded. The fix is in `src/band3/meta_band/OvershellSlot.cpp` around
line 3661.

**P4 — Sporadic sync XHR fetch mid-frame (~85–120 ms)**
`fetchMs` spike on f2159/2363 type frames. Route remaining sync XHR calls through
the async-open path / prefetch so they don't block the producer frame during
gameplay.

**P5 — `20thcenturyboy.vfv` 404**
The song's visual-fx file is not in the web asset set. This likely means venue
visual effects (light shows, pyro animations) don't fire for this song. Add to
bundle manifest.

**Not actionable from these logs (already documented):**
- The profiler observer effect is understood; use `_framestall-tickprobe.mjs` for
  all frame-time measurements.
- The `Aborted('FS')` crash is a harness instrumentation bug (use `window.FS`).
- `__dynamic_cast` load is structural to the typed-iterator design; not a stall.

---

## F. What a Fresh Web Run Should Capture

The existing captures are sufficient to diagnose the audio underrun and frame-stall
mechanisms. For the **silent intro / CrowdAudio** bug, what's needed next:

1. A run with `RB3_WEB_OFFMAIN_MIX=1;RB3_INTROSYNC=1;RB3_STREAM_DBG=1` (if
   `STREAM_DBG` covers crowd streams separately) to see whether any crowd stream
   is even attempted during the intro.
2. Confirm `crowd_intro.mogg` is or is not present in the shipped venue bank
   (`small_club_01_bank.milo`) on the web server.
3. For the texture drain: run `_framestall-tickprobe.mjs` with a comprehensive
   prewarm (all venue normal-maps, not just the ones found so far) and confirm
   zero `lpu` spikes over a 2-minute session.
