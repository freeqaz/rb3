# research/14 integrator handoff — pin bump + full gate suite

**Integrator run 2026-07-02.** Inputs: spec `../14-sharpen-fetch-hardening-plan.md`,
Lane A handoff `14-laneA-dta.md`, Lane B handoff `14-laneB-throttle.md`, adversarial
review `14-laneB-review.md` (verdict FAIL -> B1 fixed in rb3 `ecb1c8f7`; advisories
A1-A7 remain open, none blocking).

## What was integrated

- Lane A (rb3 `0ea167ec`): rb3-dta link fix — real telemetry TUs + one weak stub.
- Lane B (engine `3e02cea` + rb3 `9df9fd9b` + fix `ecb1c8f7`): chunked,
  mogg-yielding sharpen-sidecar fetch (256 KB default, `RB3_SHARPEN_CHUNK_KB`,
  0 = legacy) + `RB3SharpenStep` retry-on-not-ready + Range-ignoring-server
  detection.
- **Pin:** the requirement (pin must include Lane B's engine `3e02cea`) is satisfied:
  a concurrent commit (`fadd179a`, C8 eye-fix, 19:05) moved `MILO_ENGINE_PIN`
  77eb428b -> `04c8e1c`, and `3e02cea` is a verified ancestor of `04c8e1c`
  (engine main: 77eb428 -> 3e02cea -> 04c8e1c). My in-flight 3e02cea pin edit was
  superseded; no separate bump needed — same "already pinned by a concurrent
  commit" pattern research/13 hit.

## Build gates (all green)

- `rb3-dta`: links; `rb3-dta orig-assets/extracted/songs/songs.dta 138` ->
  "Done. Showed 138 song(s).", exit 0.
- `rb3-native`: builds clean (native/build-native, Clang, -j).
- `rb3-tests`: **53/53 PASS** incl. the two new Lane B retry tests
  (`RetriesWhenGpuNotReady`, `RetryCapMarksDoneEventually`) and 5/5
  `TexSharpenManagerTest`.
- `rb3-web`: `scripts/web/build.sh` full dual build (release 6.3M wasm /
  1.6M br + debug 30M wasm), deployed to `native/web/build/{release,debug}/`.
  Only the expected store/network/PlatformMgr link-stub warnings.

## Throttled web A/B gate (release build, cold cache, CDP throttle, RB3_WEB_DOWNSCALE=1 server, port 8623)

Harness: NEW `scripts/web/_sharpen_gate14.mjs` — the underrun window mirrors
`_sharpen_audio_throttle.mjs`'s T2 protocol EXACTLY (u0 at game_screen+2.5 s,
20 s window, 2 s polls) for baseline comparability, plus flow-gate
nofail+autohit so gameplay survives the long chunked window, a `--chunkkb`
lever, `--mbps 0` unthrottled mode, and a COMPLETE-wait phase that logs the
sharpen-window duration. Runs serialized under `flock /tmp/rb3-native-run.lock`.
Song/venue: 20thcenturyboy / small_club_01 (same as all baselines).

| run | window underrun delta | baseline | sharpen | transfer phase |
|---|---|---|---|---|
| 1.5 Mbps chunked (256 KB) | **1375 ev / 6920 q (19.87%)** | <= 635 ev (T2 ON) / 861 (OFF) — **FAIL** | COMPLETE 15/15, 21 chunks 5,374,546 B exact | 206.7 s; full-phase 20,657 ev / 71,795 q = 28.8% |
| 1.5 Mbps legacy `=0` (same-day control) | **669 ev / 6920 q (9.67%)** | in T2 band (635/7.06%) — conditions NOT drifted | COMPLETE 15/15, single fetch | 92.9 s; full-phase 13,023 ev / 32,524 q = 40.0% |
| 4 Mbps chunked | **0 ev / 6920 q (0.000%)** | ~3 ev — PASS | COMPLETE 15/15, 5,177,344 B | +48.7 s (vs ~11 s single) |
| unthrottled, `=0` (flag regression) | 4 ev / 6920 q (localhost noise) | n/a | COMPLETE 15/15 via single fetch (0 chunk lines — legacy arm confirmed exercised) | +4.2 s |

**Gate verdict: chunked FAILS the 1.5 Mbps <=635 rule** (2.05x the same-day legacy
control on the window; ~1.6x total padded quanta over a 2.2x longer transfer — the
strict yield halves PEAK starvation, 28.8% vs 40.0%, but duration wins). Per the
integration decision rule the shipped default flipped to
**`RB3_SHARPEN_CHUNK_KB=0` (legacy single fetch)** in
`native/src/rb3_texsharpen_native.cpp` (rb3-only, no engine change); chunked stays
opt-in (`=256`) for Range-fragile topologies. All targets rebuilt after the flip
(rb3-tests 53/53 again; web redeployed) + an unthrottled DEFAULT-env smoke confirmed
the default now takes the legacy arm (0 chunk lines, COMPLETE 15/15 at +3.9 s, 0
underruns, exit 0). 4 Mbps: chunked measured clean today (0 ev); the shipped legacy
default at 4 Mbps is exactly the C-wave-shipped behavior (T2: 3 ev; flow gate: 0 ev). Re-open trigger for a chunked default: idle-gate the pump on
audio-ring depth (cuts duration cost) or a real fetch-priority lane.

- 4 Mbps chunked: `chunk pump start ... (manifest size -1, chunk 256 KB)` — the
  manifest still doesn't list sidecars (expected, Lane B empirical finding #1);
  short-read EOF terminated at 21 chunks = 5,374,546 B exactly; full-phase
  underrun delta (window start -> COMPLETE) also **0 ev / 17300 q**. The 1.5 Mbps
  regression is specific to links where the mogg has no slack.
- n=1 per arm (each 1.5 Mbps run costs ~15 min); the two 1.5 Mbps arms were run
  back-to-back on the same build/server and the legacy control landing inside the
  T2-era band is the drift check.

## Honest notes / residual state

- Concurrent-agent activity during the wave (all left untouched per git rules):
  engine `src/platform/FxSendNative.cpp` uncommitted WIP (1/2-line, audio
  FX-send) was in-tree for every build; engine `Rnd_Wgpu_RB3.cpp` WIP came and
  went (committed as the eye fix `04c8e1c` mid-wave); rb3 `native/src/main_web.cpp`
  gained uncommitted shell-SFX WIP mid-session. The two 1.5 Mbps A/B arms were
  measured back-to-back on the SAME pre-flip build (engine at 3e02cea+FxSend WIP),
  so the A/B comparison is internally consistent; the post-flip
  rebuild+redeploy+smoke picked up whatever the engine tree held at 04c8e1c. None
  of these touch the fetch/underrun path.
- Review advisories A1-A7 (14-laneB-review.md) remain OPEN as non-blocking
  follow-ups; most valuable: A1 (truncated sidecar persists resident in MEMFS
  for the session -> silently disables sharpening for later songs on that
  venue), A2 (ParseSidecar nameLen u32-wrap), A3 (truncation-sweep test gap),
  A7 (non-numeric RB3_SHARPEN_CHUNK_KB atoi's to 0 = silently legacy).
- The harness's COMPLETE-wait phase keys off screen + console lines (not
  songMs); the underrun window itself is time-based and protocol-identical to
  the T2 baseline script.
