# WEB Audio Verification — 2026-06-08

## HEADLINE

**Web audio could NOT be verified. The web build crashes during App-init async asset
loading before any screen is published, so no web WAV exists to measure.** The
blocker is a thread-safety crash, not the audio path: a worker thread parses
`config/gen/metamaterials.milo_xbox` (on-demand HTTP fetch + async milo parse off
the main thread), that parse calls `Rand`, and `Rand.cpp:111` asserts `MainThread()`.
The assert fails to its 1024-cap at ~0.71s, then a `THREAD-NOTIFY: NewFile(...)
from !MainThread()` at ~1.25s traps the wasm instance and closes the page.

This is **web-specific**: native does the same parse synchronously on the main
thread, which is why native audio is verified-correct and remains the bar
(committed `7d02e80e`). **Both RELEASE and DEBUG die at the identical point** —
debug's faster milo parse does not help, because latency is not the issue.

Net verdict on the question "is web audio the right song, not clipped, right rate,
and does it match native?" → **UNKNOWN / BLOCKED.** Web audio verification is gated
on a boot fix. Native is MATCH on every axis.

---

## Web ↔ Native comparison

Native numbers from the verified baseline (`7d02e80e`,
`baselines/w-verify-{preview,gameplay}-20thc.{txt,json}`). Web is UNKNOWN because
boot crashed before audio — no capture WAV was produced, so `audio_coherence` /
`audio_verify` could not run.

### Preview (vs 20thcenturyboy)
| metric | NATIVE | WEB |
|---|---|---|
| chroma corr (identity) | 0.55 (≥0.45 same song) | UNKNOWN |
| fingerprint BER | 0.31 (≤0.30 same recording) | UNKNOWN |
| clip-ratio / flat-top | 0.00% / 0 | UNKNOWN |
| speed ratio | 1.015x (conf 0.17 — not a chipmunk) | UNKNOWN |
| pitch ratio | 1.015x (mix diff, not pitch bug) | UNKNOWN |
| flatness / zcr | 0.542 / 0.123 (not noise) | UNKNOWN |
| **VERDICT** | **MATCH** | **UNKNOWN (boot blocked)** |

### Song / gameplay (vs 20thcenturyboy)
| metric | NATIVE | WEB |
|---|---|---|
| chroma corr (identity) | 0.54 (26s overlap) | UNKNOWN |
| fingerprint BER | 0.33 / 0.334 | UNKNOWN |
| clip-ratio / flat-top | 0.00% / 0 | UNKNOWN |
| speed ratio | 1.040x (conf 0.10 — not a chipmunk) | UNKNOWN |
| pitch ratio | 1.023x (mix diff, not pitch bug) | UNKNOWN |
| crest / spec-div | 16.8dB / 0.54 | UNKNOWN |
| flatness / zcr | 0.585 / 0.168 (not noise) | UNKNOWN |
| **VERDICT** | **MATCH** | **UNKNOWN (boot blocked)** |

### Rank winner + margin
| | NATIVE | WEB |
|---|---|---|
| rank (gameplay, 20thcenturyboy,25or6to4,antibodies) | **20thcenturyboy** winner, margin **+0.177**, **CONFIDENT** | UNKNOWN |

### Rate / engine context
| | NATIVE | WEB |
|---|---|---|
| tone Hz (rate probe) | flat speed curve, no off-rate peak → not a chipmunk | UNKNOWN |
| ctx sample rate | 44100 Hz | UNKNOWN |

---

## Claims: SURVIVED vs REFUTED

- **SURVIVED adversarial verify: NONE.** No web capture exists, so no web audio
  claim could be put to an adversarial test. Survival count = 0.
- **No web claim was REFUTED** either — they were all **UNTESTABLE** for lack of a
  WAV. The only thing decisively established on web is the *boot crash*, which is
  reproduced and consistent across RELEASE and DEBUG.

Deciding numbers for the boot-crash finding (the one thing we did prove):
- `Rand.cpp:111 MainThread()` asserts: **1024** (hits the assert-spam cap) on both builds.
- Crash trigger file: `config/gen/metamaterials.milo_xbox`, parsed `!MainThread()`.
- Final probe state (DEBUG, 5-min budget): `lastScreen='' maxFrame=0 randAsserts=1024 closed=true`.
- `rb3AppBooted` never set; `rb3FrameCount` stayed **0**; furthest screen = **NONE**.
- Boot reaches the trap at **~1.25s**; this is a crash, not a slow boot.

---

## Web boot status

- **bootWorks: false.** Page closes (wasm trap) at ~1.25s.
- **Furthest screen: NONE** — no screen ever published; `rb3AppBooted` never set,
  `rb3FrameCount` stayed 0.
- **Hang/crash signature:** worker-thread parse of
  `config/gen/metamaterials.milo_xbox` (on-demand HTTP fetch → async milo parse off
  the main thread) calls `Rand` → `Rand.cpp:111` asserts `MainThread()` ×1024 at
  ~0.71s → `THREAD-NOTIFY: NewFile(config/gen/metamaterials.milo_xbox) from
  !MainThread()` at ~1.25s → wasm instance traps → page closes.
- **Not parse latency:** DEBUG (faster milo parse, no-store) dies at the identical
  point. This is thread-safety, not throughput.
- **On-demand fetch is the web-only differentiator:** web fetches assets over HTTP
  on worker threads; native does the same parse synchronously on the main thread
  (hence native audio is verifiable and correct, `7d02e80e`).
- The uncommitted `Loader.cpp` change is **benign** (HX_NATIVE frame-trace counters),
  **not** the cause.

---

## NEXT STEPS (to make web audio fully verifiable + correct)

1. **Fix the boot crash (prerequisite for any web audio measurement).** Pick one:
   - Marshal the off-main-thread milo parse of `config/gen/metamaterials.milo_xbox`
     back onto the main thread (match native's synchronous behavior on web), **or**
   - Marshal just the `Rand` call onto the main thread, **or**
   - Make `Rand` thread-safe so `Rand.cpp:111 MainThread()` no longer asserts when
     called from a worker during async milo parse.
   Prefer the parse-on-main-thread option — it most closely mirrors the verified
   native path and avoids broad `Rand` concurrency risk.
2. **Re-run the web preview capture** (`web-song-preview-audio.mjs`) once boot
   reaches `song_select`; confirm a WAV lands at `/tmp/rb3_web_preview.wav`.
3. **Run `audio_coherence.py`** on the web WAV first (reference-free: is it real
   audio / clipped) as a quick gate before the full verifier.
4. **Run `audio_verify.py --rank 20thcenturyboy,25or6to4,antibodies`** on the web
   capture; compare chroma / fp_ber / clip% / rank-margin against the native bar in
   the tables above. Web should land near native (chroma ≥0.45, fp_ber ≤~0.33,
   clip 0.00%, winner 20thcenturyboy).
5. **Capture both preview and gameplay** on web and re-fill the UNKNOWN cells; flag
   any rate/pitch divergence beyond native's informational 1.015–1.04x.

---

## How to re-run

Server is already running at http://localhost:8421 — do **not** rebuild/restart it.

```bash
# Native bar (verified): the /audio-verify skill, or directly:
python3 /home/free/code/milohax/rb3/scripts/native/audio_verify.py --selftest          # 6/6
python3 /home/free/code/milohax/rb3/scripts/native/audio_verify.py \
        --rank 20thcenturyboy,25or6to4,antibodies

# Web capture (RELEASE) — reproduced the crash today (no WAV emitted):
cd /home/free/code/milohax/rb3/scripts/web && \
  node web-song-preview-audio.mjs --phase preview --port 8421
#  -> ERROR: Target page/context/browser has been closed; no /tmp/rb3_web_preview.wav

# Web boot probe (DEBUG) — same crash, 5-min budget:
cd /home/free/code/milohax/rb3/scripts/web && node _debug_boot_probe.mjs
#  -> FINAL: lastScreen='' maxFrame=0 randAsserts=1024 closed=true

# After a boot fix, the web audio gate:
python3 /home/free/code/milohax/rb3/scripts/native/audio_coherence.py /tmp/rb3_web_preview.wav
python3 /home/free/code/milohax/rb3/scripts/native/audio_verify.py \
        --rank 20thcenturyboy,25or6to4,antibodies   # against the web WAV
```

**Artifacts:** `/tmp/web_preview_release.log`, `/tmp/web_debug_probe.log`,
`scripts/web/_debug_boot_probe.mjs`. Boot-establish notes appended under
`### establish boot (2026-06-08)` in
`docs/native/audio-perf-loop/wave-08-web-audio.md`.
