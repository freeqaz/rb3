# B1/B4 — Web Audio Audibility Proof Recipe (Headless, No-Display Host)

**Goal:** capture the web build's *mixed* PCM to a WAV/stats and assert
non-silence, distinguishing **B1 (song MOGG stream)** from **B4 (menu/UI SFX
one-shots)**. READ-ONLY task: no source edits. The engine already ships every
hook needed; this is pure test tooling + a runbook.

## TL;DR — chosen path (PROVEN on this host)

**Path (a): headless Chromium + WebGPU + the engine's JS capture hook, driven by
Playwright.** Feasibility was verified live this session (see "Host probe").
Paths (b) xvfb and (c) manual desktop are documented as fallbacks but are **not
needed here**.

Why it works with **no `$DISPLAY` and no PulseAudio/PipeWire daemon:** the
capture path records `sMixBuffer` — the *MixSources output* — at
`AudioDevice_Web.cpp:442-453`, **before** it is copied to the SAB ring and
**before** the AudioWorklet routes it to `ctx.destination`. So capture is fully
decoupled from speakers, audio daemons, and `--mute-audio`. We assert on the
decoded+mixed PCM, which is exactly "did audio get produced."

## Host probe (run this session — results)

- `chromium` 148, `node` v26, `npx`, **Playwright installed**
  (`~/.cache/ms-playwright/chromium-*`), `xvfb-run`+`Xvfb` present.
- No `$DISPLAY`; AMD `card2` + NVIDIA GPU; `vulkaninfo` = Vulkan 1.4.
- **Against the SERVED origin** (COOP/COEP from `server.py`), headless Chromium
  with the launch flags below returned:
  `crossOriginIsolated:true, hasSAB:true, hasAudioWorklet:true, hasGpu:true,
  adapter:true, device:true (nvidia/ampere via ANGLE-Vulkan),
  audioCtxState:"running"`, and the engine logged `GpuDevice: adapter acquired`.
- On `about:blank` the same probe returns `hasGpu:false, hasSAB:false` — **you
  MUST navigate to the `server.py` origin**; isolation+WebGPU only exist there.

## Prerequisites

```bash
# 1. A web build must exist (already present this session):
ls native/web/build/rb3-web.wasm   # ~28MB
# else: scripts/web/build.sh   # slow (brotli q11), only if missing/stale

# 2. Assets (song mogg + SFX) — orig-assets/extracted is auto-detected.
ls orig-assets/extracted/songs/20thcenturyboy/20thcenturyboy.mogg  # symlink, OK
ls orig-assets/derived/sfx_pcm | wc -l   # 678 XMA->PCM SFX sidecars (for B4)

# 3. Playwright is already installed under scripts/web/node_modules.
```

## The capture hooks (engine, namespace = `rb3`, confirmed in built JS)

`MILO_WEB_AUDIO_NS=rb3` (`native/CMakeLists.txt:740`). At `AudioDevice::Init`
the engine registers (via `EM_ASM`, so they are runtime-created, not JS
literals):

| `window.` helper        | C export             | effect |
|-------------------------|----------------------|--------|
| `rb3CaptureAudio()`     | `rb3_start_capture`  | records next **3 s** of MixSources output into `sCaptureBuffer` |
| `rb3DownloadAudio()`    | `rb3_download_capture`| builds `rb3_web_capture.wav` (16-bit stereo) + `a.click()` download |
| `rb3DumpSAB(n)`         | `rb3_dump_sab`       | logs `SAB stats: min=.. max=.. nonZero=N/total` |
| `rb3AudioStats()`       | `rb3_audio_stats`    | logs `pumpCount`, capture pos, and **per-source** `DebugDumpSources` (`active source count=K`) |

3 s capture window = `CAPTURE_SECONDS` (`AudioDevice_Web.cpp:52`).

## Launch flags (PROVEN — copy verbatim from w3c-gameplay-test.mjs:110-118)

```js
browser = await chromium.launch({
  headless: !process.env.DISPLAY,   // true on this box
  args: [
    '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--disable-extensions',
    '--disable-background-networking', '--disable-default-apps',
    '--disable-sync', '--mute-audio',   // safe: capture is pre-destination
  ],
});
```

## The capture + assert snippet (drop into a new `scripts/web/web-audio-capture.mjs`)

Fork `w3c-gameplay-test.mjs`'s nav (splash→main_hub→song_select→
part_difficulty→game_screen, lines 159-246) to reach `game_screen`, then:

```js
// In headless automation the AudioContext is already "running" (proven), but a
// real-page gesture is harmless: the nav already clicks the canvas (line 159).

// --- arm + intercept the WAV download BEFORE pressing capture ---
const dlPromise = page.waitForEvent('download', { timeout: 10000 }).catch(() => null);

await page.evaluate(() => window.rb3CaptureAudio());   // starts 3s record
await new Promise(r => setTimeout(r, 4000));           // > CAPTURE_SECONDS(3s)

// Assert #1 — pipeline is live (distinguishes "mix loop runs"):
const stats = await page.evaluate(() => {
  // rb3AudioStats/DumpSAB only console.log; capture their output via page.on('console').
  window.rb3AudioStats();    // -> pumpCount=N, active source count=K
  window.rb3DumpSAB(32);     // -> SAB stats: min/max/nonZero=N/total
  return true;
});
// (Parse pumpCount>0, active source count>0, nonZero>0 from the console log lines.)

// Assert #2 — the WAV itself is non-silent (the real audibility gate):
await page.evaluate(() => window.rb3DownloadAudio());
const dl = await dlPromise;
if (!dl) throw new Error('no capture WAV produced (capture never readied)');
const wavPath = '/tmp/rb3_web_capture.wav';
await dl.saveAs(wavPath);

// Decode the 44-byte-header 16-bit PCM WAV and assert energy:
import { readFileSync } from 'fs';
const buf = readFileSync(wavPath);
const samples = new Int16Array(buf.buffer, buf.byteOffset + 44,
                               (buf.length - 44) >> 1);
let peak = 0, nonZero = 0;
for (const s of samples) { const a = Math.abs(s); if (a > peak) peak = a; if (a > 64) nonZero++; }
const pass = peak > 1000 && nonZero > samples.length * 0.05;   // ~ -30 dBFS, >5% active
console.log(`WAV peak=${peak} nonZero=${nonZero}/${samples.length} -> ${pass ? 'PASS' : 'FAIL (SILENT)'}`);
if (!pass) process.exit(1);
```

Parse the console lines with a `page.on('console')` collector (see
w3c-gameplay-test.mjs:124-130) and pull `pumpCount`, `active source count`,
`nonZero=`.

**Thresholds:** silence ⇒ `peak≈0, nonZero≈0`. A real MOGG mix sits near
full-scale, so `peak>1000` (≈ -30 dBFS) with >5% active samples is a robust
non-silence gate. Tune up if the song has a quiet intro — capture *during*
sustained gameplay, not the first second.

## Distinguishing B1 (song) from B4 (menu/UI SFX) — TWO runs

The web build does **not** expose a per-channel split, so isolate by *what is
allowed to make sound*, using the existing `RB3_NO_SFX` gate
(`rb3_platform_native.cpp:74-78`, default SFX **on**). `RB3_NO_SFX` is a build-time
env for native; **for web it is baked at build time**, so the clean isolation is:

| Capture | Where | What proves |
|---|---|---|
| **B1 (song)** | At `game_screen`, after the MOGG is streaming, capture 3 s. The only `AudioSource` is `RB3StreamReceiverNative` → expect `active source count=1`, large `peak`. | Song MOGG is audible. |
| **B4 (SFX)** | At `main_hub_screen`/`song_select_screen` (NO song playing → song source absent), capture 3 s **while pressing nav/confirm keys** (`page.keyboard.press('Enter')` in a loop during the window). Each blip adds a transient `SampleInst`/sidecar source → expect `active source count` to spike >0 and `nonZero>0` *only when a key fires*. | One-shot SFX are audible. |

**Per-source confirmation:** `rb3AudioStats()` → `DebugDumpSources` prints
`active source count=K` and a line per source. In the B4 menu capture, a confirm
press must bump that count (transient). In B1 the song source persists. If B4
shows `active source count=0` during nav, SFX are still silent (the Phase-2
`SampleInst` gap / XMA-decode follow-up) — that is a *negative* result the recipe
correctly surfaces, not a test bug.

**XMA SFX note (B4):** many 360 SFX banks are XMA (skipped by the in-wasm PCM
path). `server.py` serves the 678 pre-decoded `orig-assets/derived/sfx_pcm/*.pcm`
sidecars on demand at `/api/file/sfx/gen/xma_pcm/<hex>.pcm`, so XMA one-shots can
be audible too — watch the server log for those GETs during the B4 menu capture
to confirm a sidecar was fetched for the blip.

## Runbook (exact commands)

```bash
# Terminal 1 — server (auto-detects orig-assets/extracted + sfx_pcm sidecars):
python3 native/web/server.py --port 8455

# Terminal 2 — capture + assert (B1 song first):
cd scripts/web && node web-audio-capture.mjs --port 8455 --phase song
#   -> reaches game_screen, captures 3s, saves /tmp/rb3_web_capture.wav,
#      prints "WAV peak=.. nonZero=.. -> PASS"

# B4 menu SFX (no song; presses keys during capture):
node web-audio-capture.mjs --port 8455 --phase menu
#   -> at main_hub, captures while pressing Enter; asserts source-count spike
```

Stop the server when done (`Ctrl-C` / `pkill -f "server.py --port 8455"`).

## Fallback paths (NOT needed on this host)

- **(b) xvfb-run + real Chromium** — only if headless WebGPU regresses:
  `xvfb-run -s "-screen 0 1280x720x24" node web-audio-capture.mjs` and set
  `headless:false`. The host already proved headless WebGPU works, so this is a
  safety net, not the primary path.
- **(c) Manual desktop (user's machine)** — open `http://localhost:8455` in
  Chrome with `--enable-unsafe-webgpu --enable-features=Vulkan`, reach gameplay,
  open DevTools console, run `rb3CaptureAudio()`, wait 3 s, `rb3DownloadAudio()`
  (saves `rb3_web_capture.wav`), then off-box: `ffprobe rb3_web_capture.wav` /
  open in Audacity — must show signal, not a flat line. Also `rb3DumpSAB(32)`
  must print `nonZero>0`. Distinguish B1/B4 by capturing during gameplay vs.
  during menu nav exactly as the automated phases above.

## Gotchas / acceptance

- **Navigate to the served origin** — `about:blank` has no WebGPU/SAB here.
- **`waitForEvent('download')` must be armed before `rb3DownloadAudio()`** — the
  WAV is delivered via `a.click()` (`AudioDevice_Web.cpp:235-241`), an actual DOM
  download Playwright only sees if pre-armed.
- **Capture is 3 s fixed**; wait ≥4 s before `rb3DownloadAudio()` (it returns "no
  capture ready" otherwise — `AudioDevice_Web.cpp:489`).
- **`--mute-audio` does NOT affect the assert** (capture is pre-`ctx.destination`).
- **Pass/fail gate:** B1 = WAV `peak>1000 && nonZero>5%` at `game_screen`.
  B4 = `active source count` spikes during menu key presses with `nonZero>0`.
  A FAIL on B4 with `active source count=0` is the *real, expected* SFX gap
  (Phase 2 / XMA), not a tooling error.
