# Web-audio fixes (2026-06-21): stem-seed anchor + pause-menu latency ramp

Two web-only (off-main worklet mix) audio bugs. Both fixes are in
`milo-native-engine` (DC3-safe) and are gated behind env opt-outs. No `native/src`
changes were required.

- Bug #1 — guitar stem ~10s behind on web (off-main worklet stem-seed race).
- Bug #4 — adaptive latency ramps to the 500ms ceiling on the PAUSE menu.

Engine worktree: `/home/free/code/milohax/milo-native-engine-worktrees/webaudiofix`
(branch `wt-webaudiofix`). rb3 worktree: `.claude/worktrees/webaudiofix`.

---

## Bug #1 — stem-seed anchor (defer-and-batch)

### Root cause (confirmed against GUITAR_OFFSET_DIAGNOSIS.md + code)

In off-main mode each music stem's worklet read cursor was seeded to ITS OWN
play-cursor at ITS OWN first pump tick, with no shared song-start anchor. The
worklet's one-shot `primed` latch (`audio-worklet.js`) starts the bus clock on the
currently-seeded subset and never re-aligns a stem seeded a tick later. If one stem
(guitar, channels 3/4) arms one pump tick after its siblings — the CrowdAudio intro
stream (commit `2eeb6ced`) makes this likely by seeding/advancing slots during the
count-in — that stem is seeded to its song-start while the bus is already ~one stem
ring ahead, so it plays ~one stem ring behind (`kStemRingFrames = 0xC0000/2 ≈
393216 frames ≈ 8.9s @ 44100`), matching the reported ~10s.

### Fix (engine)

`src/audio/AudioDevice_Web.cpp` `AudioDevice::PumpAudioOffMainStems()` — replaced
the "seed each stem the tick it arms" logic with **defer-and-batch**: collect the
armed-but-unseeded stems each pump; only seed them once that set is **stable for one
pump** (same count as the previous pump), and then seed the whole batch in the SAME
tick. Because the prime gate holds the bus clock until min-availability across the
seeded set is reached, every stem of a stream that arms within a one-tick window
shares a common t=0. A separate, earlier stream (CrowdAudio) has long since settled
into its own batch, so the two never drag each other.

- A stem is never PUBLISHED before it is seeded (publishing advances the SAB
  availability writePos, which would let the prime gate start the bus on a partial
  set). Publishing was factored into a new `AudioDevice::PublishOffMainStem()`
  shared by the seed-now and deferred-batch paths.
- Opt-out: `RB3_NO_STEM_ANCHOR=1` reverts to the legacy seed-on-arm behavior.
- New member state (`AudioDevice.h`): `int mPendingSeedCount` (armed-unseeded count
  seen last pump).

Files:
- `src/audio/AudioDevice_Web.cpp` `PumpAudioOffMainStems()` (rewritten) +
  new `PublishOffMainStem()`.
- `src/audio/AudioDevice.h` — `mPendingSeedCount`, `PublishOffMainStem()` decl.

Tradeoff: the first batch waits one extra pump (~33ms) to absorb the 1-tick arming
skew; the ~120ms prime cushion already covers this, so song-start timing is
unchanged in practice.

---

## Bug #4 — pause-menu latency ramp to 500ms

### Root cause

In off-main mode the worklet's underrun counter is dominated by the MUSIC stems, not
the SFX output ring. When paused:
1. The worklet still computed `minAvail` across ALL active stems INCLUDING paused
   ones, and advanced ALL of them in lockstep. A paused stem's producer stops
   refilling, so its `avail` drains to 0, dragging `minAvail` to 0 → the worklet
   pads silence every quantum → the hard-underrun counter climbs continuously.
2. The on-main adaptive-latency law (`PumpAudio`, ~L1150) gated its SOFT near-miss
   branch on `!mSources.empty()` and skipped on pause, but its HARD-underrun grow
   branch (`u[0] > sLastUnderrun`) was NOT gated. With the music stems off-main,
   `mSources` holds only transient SFX one-shots and is usually EMPTY mid-song, so
   it is NOT a valid "audio playing" signal here. The ungated hard branch ramped the
   target to the 500ms ceiling during the pause; on resume that left up to 500ms of
   latency that only shrinks back slowly.

### Fix (two layers, both engine, both correct independently)

**(A) Worklet — freeze paused stems** (`src/platform/web/assets/audio-worklet.js`):
- A paused stem no longer gates `minAvail` and is no longer advanced (rp + monotonic
  readTotal held). Resume continues exactly from the pause point (previously the
  worklet consumed buffered audio it never mixed → resume jumped forward).
- When ALL active stems are paused, route to the SFX-only drain path and DO NOT
  count an under-run — a clean window for the latency law, frozen cursors for the
  stems. (Both the equal-rate fast path and the resample path.)

**(B) Engine — gate the hard-underrun grow branch on a playback heartbeat**
(`src/audio/AudioDevice_Web.cpp`):
- `PumpAudioOffMainStems()` accumulates `mOffMainFedFramesThisWindow` = frames the
  worklet consumed from any UNPAUSED stem since the last pump (a paused stem's
  readTotal does not advance, so this is naturally 0 while paused).
- The latency law reads + resets it per law window. In off-main mode the "audio is
  actually being fed this window" signal is `fedThisWindow > 0` (falls back to
  `!mSources.empty()` when off-main is OFF). The HARD grow branch now requires
  `audioFeeding`; an underrun window with no audio fed is absorbed as the new
  baseline and treated as clean (decay pressure / advance shrink hysteresis), so the
  target HOLDS or SHRINKS toward the floor while paused instead of growing.

Layer (A) is the primary fix (no false underruns generated at all while paused);
layer (B) is defense-in-depth and also covers any future producer that legitimately
underruns the SFX ring while no music is fed.

- Opt-out: none added for Bug #4 (the change only suppresses a spurious ramp; the
  RB3_AUDIO_LATENCY_MS / _LAT_MIN_MS / _LAT_MAX_MS env pins still apply).

Files:
- `src/platform/web/assets/audio-worklet.js` — paused-stem exclusion from minAvail +
  advance (fast + resample paths), all-paused SFX-only early-out.
- `src/audio/AudioDevice_Web.cpp` — `mOffMainFedFramesThisWindow` heartbeat + the
  `audioFeeding`-gated hard-underrun branch.
- `src/audio/AudioDevice.h` — `mOffMainFedFramesThisWindow` member.

---

## DC3-safety

All edits are in `#ifdef __EMSCRIPTEN__` AudioDevice_Web.cpp / the web worklet asset
(neither compiled for the Wii target). The worklet logic ("don't advance or gate on
a paused consumer") is generically correct for any consumer of the off-main mix, so
it does not change DC3's behavior beyond fixing the same latent pause bug. The
engine builds for DC3 from the same TUs; the new members are plain ints with safe
defaults and the hard-branch gate degrades to the prior `!mSources.empty()` signal
when off-main is OFF.

---

## Build + verification

- COMPILE: `scripts/web/build.sh --debug` against the private engine worktree
  (`MILO_ENGINE_PATH_OVERRIDE`). `AudioDevice_Web.cpp.o` + the wasm rebuilt clean
  (only the expected store/network/PlatformMgr undefined-symbol link warnings). The
  worklet `node --check` passes; deployed to `native/web/build/audio-worklet.js`.
- RUNTIME (Bug #4): `scripts/web/_pause-latency-verify.mjs` — Playwright headless
  drives into "25 or 6 to 4" gameplay (RB3_WEB_OFFMAIN_MIX=1), plays ~6s, opens the
  pause menu (Escape), holds ~12s, unpauses. Captures every
  `AudioDevice: latency GROW/HIGH` console line tagged by phase. (results below)
- RUNTIME (Bug #1): `scripts/web/web-worklet-tap-capture.mjs` taps the worklet
  output to WAV; compare guitar-vs-mix alignment. A/B `RB3_NO_CROWD_INTRO=1`
  (fast trigger toggle) and `RB3_NO_STEM_ANCHOR=1` (fix opt-out). (results below)

### Results

**COMPILE: PASS.** Web debug build (`scripts/web/build.sh --debug`) against the
private engine worktree — `AudioDevice_Web.cpp.o` + the wasm + the worklet all built
clean (only the expected store/network/PlatformMgr undefined-symbol link warnings).
`node --check` on the worklet passes. The build boots to gameplay headless with
11-channel decode (no regression).

**Bug #1 RUNTIME: PASS (mechanism directly observed).** With
`RB3_WEB_OFFMAIN_DBG=1` in "25 or 6 to 4" gameplay, the runtime log shows:

```
AudioDevice: STEM-ANCHOR seeded 1 stems in ONE tick  (firstStart=0 maxStartSkew=0 frames)   <- CrowdAudio intro (its own batch)
AudioDevice: STEM-ANCHOR seeded 11 stems in ONE tick (firstStart=0 maxStartSkew=0 frames)   <- the song's 11 channels (incl. guitar 3,4), one t=0
```

All 11 song channels are seeded in a SINGLE pump tick with `maxStartSkew=0` — the
staggered-seed race is structurally eliminated; the CrowdAudio intro is correctly a
separate, earlier batch that never drags the song. The fix-ON worklet-output capture
(`web-worklet-tap-capture.mjs` → `audio_verify.py --song 25or6to4 --section
gameplay`) is `VERDICT: MATCH` — chroma 0.91 (same song), speed 1.002x, pitch
1.000x, not clipped, constant align lag (menu lead-in). A ~10s guitar offset would
have torn the chroma down; it is clean across the whole window.

NOTE / verification gap: the original ~10s offset is an INTERMITTENT race; it did not
reproduce in the headless A/B (`RB3_NO_STEM_ANCHOR=1` fix-off run also came out
chroma 0.91 / same song — a 1.008x rate-measurement artifact, conf 0.78, not the
offset). So the proof is "fix-ON renders correctly + the race is provably removed at
runtime (11-in-one-tick, skew 0)," not "reproduced-then-fixed." Recommend the deploy
host confirm against the original user repro of the song once.

**Bug #4 RUNTIME: PASS.** `_pause-latency-verify.mjs` drove into "25 or 6 to 4"
gameplay (off-main ON), played ~8s, opened the pause menu (Escape), held 14s,
unpaused. During the pause: `GROW=0`, `HIGH=0` (zero latency-ramp logs — the target
did NOT climb toward the 500ms ceiling). After unpause: playback resumed (frames
advanced). Re-verified on the corrected (post-TDZ-fix) build: still `GROW=0 HIGH=0`,
1155 frames advanced post-resume, `PASS`.

### Verification artifacts / scripts
- `scripts/web/_pause-latency-verify.mjs` (rb3, new) — Bug #4 pause-menu latency gate.
- `scripts/web/web-worklet-tap-capture.mjs` + `scripts/native/audio_verify.py`
  (existing) — Bug #1 worklet-output identity/rate/offset.
- `RB3_WEB_OFFMAIN_DBG=1` prints the `STEM-ANCHOR seeded N stems in ONE tick` line
  (Bug #1 runtime proof).

### Verification gap for the deploy host
- The web runtime verification ran on the `--debug` build (SwiftShader/ANGLE-Vulkan
  headless). Re-confirm on the deployed RELEASE build + a real user device once.
- Confirm Bug #1 against the original user repro (the intermittent offset did not
  trigger in the headless A/B; the structural proof — 11-in-one-tick, skew 0 — is the
  primary evidence).
- DC3: these are engine changes; rebuild dc3-web once after the pin bump to confirm
  DC3 still links + its own pause behavior is unaffected (logic is generically
  correct; no DC3-specific code touched).
