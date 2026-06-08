# Wave 08 — WEB audio verification (2026-06-08)
Goal: verify WEB audio = same song, correct rate, no clipping (audio_verify.py on
in-browser captures), matching the native baseline (commit 7d02e80e). Orchestrated
by the web-audio-verify ultracode workflow. Agents append sections below.

### establish boot (2026-06-08)

**Goal:** produce ONE valid `/tmp/rb3_web_preview.wav` from a web song-select
preview, or prove boot is blocked.

**Result: BOOT IS BLOCKED on web. NO WAV produced.** Both the RELEASE and DEBUG
builds crash during App-init async asset loading, before ANY screen is published
(no `intro_movie_screen`/`splash_screen`, `rb3AppBooted` never set,
`rb3FrameCount` stays 0). It is a hard crash (the page `close`s / wasm traps), not
slowness.

**What I tried**
1. RELEASE: `timeout 600 node web-song-preview-audio.mjs --phase preview --port 8421`
   → `ERROR: Target page, context or browser has been closed`. Last console lines:
   a burst of `Rand.cpp:111 MainThread()` asserts, then
   `THREAD-NOTIFY: NewFile(config/gen/metamaterials.milo_xbox) from !MainThread()`.
   Never logged a single `screen=` change. No `/tmp/rb3_web_preview.wav`.
2. DEBUG (`launchBrowser(8421, {query:'debug=true'})`, custom probe
   `scripts/web/_debug_boot_probe.mjs`, 5-min budget) → **same crash, identical
   point.** Probe final: `lastScreen='' maxFrame=0 randAsserts=1024 closed=true`.
   Debug's faster milo parse did NOT help — the failure is a thread-safety crash,
   not parse latency.

**Exact boot sequence (both builds, timestamps from debug probe)**
```
[0.71s] reading config/ham_preinit_keep.dta took 484ms
[0.71s] [Native] JoypadInit
[0.71s] [Native] KeyboardInit
[0.71s] FAIL: .../system/math/Rand.cpp Line:111 Error: MainThread()   ← ×1024 (burst)
[1.25s] THREAD-NOTIFY: NewFile(config/gen/metamaterials.milo_xbox) from !MainThread()
        <page closes / wasm traps — process gone>
```

**Hang signature / root cause (web-specific):** the web build loads+parses
`config/gen/metamaterials.milo_xbox` on a WORKER thread (on-demand HTTP fetch +
async milo parse off the main thread). That parse calls `Rand`, whose
`Rand.cpp:111` asserts `MainThread()`. The assert fails repeatedly (exactly 1024
times — looks like an assert-spam cap), then the wasm instance traps and the
browser page dies. Native does this same parse synchronously on the main thread,
which is why native audio is verified-correct (commit 7d02e80e) while web cannot
even boot. The uncommitted `Loader.cpp` change is benign (HX_NATIVE frame-trace
counters) and not involved.

**Conclusion:** Web preview-audio verification is GATED on a boot fix — the
off-main-thread milo parse (or the `Rand`-on-worker call inside it) must run on /
be marshalled to the main thread, or `Rand` made thread-safe. Until then no web
WAV can be captured. Native remains the audio bar (chroma 0.55 / fp_ber 0.31
MATCH, not chipmunk, 0% clip).

Furthest screen reached: NONE (crash before first published screen). Artifacts:
`/tmp/web_preview_release.log`, `/tmp/web_debug_probe.log`,
`scripts/web/_debug_boot_probe.mjs`.
