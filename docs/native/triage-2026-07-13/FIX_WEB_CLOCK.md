# WEB-CLOCK lane — triage 2026-07-13

**Lane:** WEB-CLOCK · evidence `/tmp/triage-0713/fix-web/` · **4 web boots used (budget 4).**
Two user-reported symptoms, both measured CLEAN on native by sister lanes
(`ANIM_SPEED.md`, `PAUSE_TIMER.md`), re-tested here on the **web** build via NUMBERS
(no pixels — see environment note).

## Verdict: **REFUTED on web.** Neither symptom reproduces on the current HEAD web build.

| Symptom | Web result |
|---|---|
| "animations firing way too fast" | Song/play clock = **1.0x** on web (0.999–1.000x), same as native. No fast animation class. |
| "pause+resume instantly fails, timer keeps advancing" | On HEAD (debug **and** release): pause **freezes** the song clock, the worklet play cursor, and flags all 15 stems paused. The play-cursor invariant HOLDS. |

Confidence: **high** for the HEAD build (direct instrumented runtime numbers, both
build flavors). The one caveat is the *deployed* build (see §4).

---

## Environment note (why numbers, not pixels)
Host has an NVIDIA/NVML driver-version mismatch → Vulkan `VK_ERROR_INCOMPATIBLE_DRIVER`
→ headless Chromium WebGPU is unreliable/software. GPU capture is not load-bearing for a
clock/rate measurement, so **all evidence here is numeric**: the audio worklet's monotonic
play cursor (`readTotal`, ground-truth real-time since it is clocked by the audio device
sample rate), the audio context rate, per-stem paused flags, and (on the instrumented
build) `MasterAudio::GetTime()` + `Game::IsPaused()`.

## How the numbers are read (web)
Off-main worklet SAB, published by `AudioDevice_Web.cpp` (`js_offmain_alloc`), read live
from the page:
- `window._rb3Audio.offmain.stemHdr[slot]` = Int32Array(8): `[4]=readTotal` (monotonic
  frames consumed), `[0]=writePos`, `[3]=gen`.
- `window._rb3Audio.offmain.ctrlI` : `[3]=ctxRate`; per-stem 4 words @ `(4+s*4)`, `[+2]=flags`
  (bit0=paused).
- Instrumented HEAD build only (my 13-line debug hook, §5): `window.rb3SongMs` =
  `RB3TraceCurrentSongMs()` (= `TheGame->GetBeatMaster()->GetAudio()->GetTime()`),
  `window.rb3GamePaused` = `Game::IsPaused()`.

Boot to `game_screen` via the `scripts/web/lib/core.mjs` splash→hub→song-select→part→game
key sequence (song `20thcenturyboy`). Pause verb = **Start = Space** (`rb3_game_input.cpp`
keymap `' ' → 1<<11 kPad_Start`). ctxRate = 44100.

---

## 1. ANIM-SPEED — REFUTED (all builds)
Play-cursor rate vs ctxRate, sampled ~1 Hz over the song-clock domain:

| build | readTotal rate | songMs rate |
|---|---|---|
| Deployed release (boot 1) | **0.9993x** | (no hook) |
| HEAD debug (boot 3) | 0.9990x | 0.989x |
| HEAD release (boot 4) | **1.0003x** | 0.989x |

The web song/play clock advances at real time (±0.2%), **fps-invariant** and identical to
native's 1.00x. Web headless fps was only **37–58** — if anything web renders *slower* than
60, so a frame-count-driven "too fast" is impossible. In-song char clips / gems / track /
venue props are all bound to this 1.0x clock (`TheTaskMgr.SetSeconds(songMs/1000)`), so none
are temporally fast on web.

**Cross-hypothesis (not chased, per brief):** with a correct 1.0x temporal rate, the "too
fast/violent" perception most likely traces to **W33-V1** — the vignette-clip seed-R
rotation-basis explosion (upperArm ~4.2x *spatial* blow-up, rechartered in `deea5e95`) — a
pose/spatial defect, **not** a clock defect. Same conclusion the native `ANIM_SPEED.md` reached (H3).

## 2. PAUSE-TIMER — REFUTED on HEAD (debug + release)
Instrumented builds, pressing Start (held 300 ms to guarantee the once-per-frame `_rb3Keys`
edge is caught):

| phase | gamePaused | songMs | readTotal | pausedStems |
|---|---|---|---|---|
| PLAY (release, boot 4) | 0 | 5 → 4992 (~1x) | climbs @1.0003x | 0 |
| **PAUSE (release, 14 samples/14 s)** | **1** | **6200, frozen** | **545920, frozen** | **15/15** |
| PLAY (debug, boot 3) | 0 | 0 → 5037 | climbs @0.999x | 0 |
| **PAUSE (debug, 14 samples)** | **1** | **6310, frozen** | **545536, frozen** | **15/15** |

On pause the WASM publishes `mPaused` into the ctrl-SAB flag for every stem
(`PublishOffMainStem` `flags = st.paused?1:0`), the worklet honors it and **holds the cursor**
(`readTotal` frozen), and `MasterAudio::GetTime()` (→ `rb3SongMs`) freezes too. **The single
invariant — the worklet play cursor must not advance while paused — HOLDS.** The chain the
native `PAUSE_TIMER.md` verified only by code-reading (device/worklet cursor-hold) is now
**runtime-confirmed on web**.

---

## 3. The earlier "readTotal advancing during pause" was a TEST ARTIFACT
Boots 1–2 (deployed release, no pause hook) showed `readTotal` still climbing after a Space
press and the overshell stuck at `hidden` (`pause_probe.json`). Root cause: those runs used
`page.keyboard.press('Space')` — a sub-frame down+up. The web build edge-detects `_rb3Keys`
**once per frame**; at the low headless fps a too-quick press sets and clears the Start bit
between frame reads, so the rising edge is **missed and pause never engages** — the song
correctly keeps playing (this is exactly why `core.mjs pressKey` HOLDS keys). Once boot 4
held Space 300 ms, pause engaged on the **first** press and everything froze. So the "bug"
signature in boots 1–2 was *no pause at all*, not an advancing-during-pause defect.

## 4. Deployed-build provenance (the one caveat)
The build served by `native/web/server.py` on 8421 is `native/web/build/{release,debug}`,
release wasm dated **2026-07-12 08:26** — built at ~`9eb50c3c`/`69103c77`, only **2 commits**
behind HEAD (`88e1928b`), and those 2 commits (`69103c77` render-hook TU restore + MIDI
default; `88e1928b` results-screen crash) touch **neither audio-pause nor input**. The
deployed `audio-worklet.js` is **byte-identical** to HEAD's engine worklet (0-diff). So the
deployed build is functionally identical in the pause/clock path — **no meaningful staleness**.
A rebuild+redeploy of HEAD is still recommended as hygiene (it is what I measured as clean),
but it is not required to fix a defect, because there is no defect in this path.

## 5. Handback — instrumentation only, NO fix (landed=false)
No code fix is warranted — nothing reproduced. The worktree diff is a **13-line diagnostic
hook** in `native/src/main_web.cpp` (publishes `window.rb3SongMs` + `window.rb3GamePaused`
each frame) that made the pause experiment decisive and is reusable for any future web
pause/clock triage. It is web-only (`main_web.cpp` is the Emscripten entry TU), touches no
Wii-matched code and no engine code (engine diff empty), so the objdiff gate is unaffected.
Fable's call whether to keep it.

- Worktree (rb3): `/home/free/code/milohax/rb3/.claude/worktrees/webclock` (branch `wt-webclock`, based `88e1928b`)
- Paired engine worktree: `/home/free/code/milohax/milo-native-engine-worktrees/webclock` (UNCHANGED)
- Saved diff: `/tmp/triage-0713/fix-web/webclock_debug_hooks.diff` (main_web.cpp, +13)

## 6. Recommendation
Close both symptoms as **not-reproducible on current web source**. If the reporter still
sees "too fast", it is the **W33-V1 spatial pose explosion** perception (a live, separately
tracked defect), not a temporal/clock bug. If the reporter still sees a pause failure, ask
them to (a) confirm they are on a build ≥ `88e1928b` and (b) capture whether the on-screen
Start actually opens the pause menu — the failure mode I could induce was pause **not
engaging** on a too-brief button tap, never the clock advancing through an engaged pause.

## Evidence files (`/tmp/triage-0713/fix-web/`)
- `measure.mjs`, `samples.json` — boot 1 (deployed release): play 0.999x, pause-attempt kept advancing (missed edge)
- `pause_probe.mjs`, `pause_probe.json` — boot 2 (deployed release): overshell stayed `hidden`, no pause menu
- `measure_dbg.mjs` — instrumented harness (retry-until-gamePaused; SUFFIX param for debug/release)
- `measure_dbg.json` — boot 3 (HEAD debug): pause froze songMs=6310, readTotal=545536, pausedStems=15
- `measure_rel.json` — boot 4 (HEAD release): pause froze songMs=6200, readTotal=545920, pausedStems=15
- `webclock_debug_hooks.diff` — the +13 main_web.cpp instrumentation
- `build_debug.log`, `build_release.log`, `server8862.log` — build/serve logs
