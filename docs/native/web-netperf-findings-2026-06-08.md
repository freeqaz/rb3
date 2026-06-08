# Web load/transition performance under emulated networks (2026-06-08)

Hard data on rb3-web boot **and menu-transition** stalls measured under realistic
network throughput — not just on unbounded loopback, which hides the dominant cost.

**Headline:** the port fetches every on-demand asset with a **synchronous XHR that
blocks the wasm main thread for the full transfer**. On loopback the bytes arrive
instantly so the block is invisible; under real bandwidth the freeze time is
literally `bytes ÷ throughput`. This is why menu transitions hitch in production.

## Tooling

`scripts/web/netperf-suite.mjs` — network-conditioned suite (reuses the Playwright
+ CDP harness in `scripts/web/lib/core.mjs`).

- Throttles via CDP `Network.emulateNetworkConditions` (confirmed to apply to
  localhost). Profiles (edit `PROFILES` at the top):
  - **low** = 50 Mbit/s, 30 ms RTT  ("low end")
  - **normal** = 200 Mbit/s, 15 ms RTT  ("normal")
  - **local** = unbounded, 0 ms RTT  ("high watermark" — loopback)
- Scenarios: `boot` (cold → first interactive screen) and `nav` (boot, skip intro,
  then drive `main_hub → song_select → part_difficulty → game`, measuring **each
  transition** independently).
- Per run captures: boot milestones; per-transition **wall time / bytes / request
  count / main-thread-blocked ms / worst RAF gap**; a full CDP network waterfall
  (every request, type, bytes, duration); long tasks; RAF gaps; and (opt-in
  `--trace` / `--cpuprofile`) a DevTools `trace.json` + V8 `.cpuprofile` for deep
  inspection in the Performance panel.
- Emits a comparison matrix + `REPORT.md` + `summary.json` + per-run dirs.

```bash
python3 native/web/server.py &                       # serve the build
node scripts/web/netperf-suite.mjs                   # all profiles, boot + nav
node scripts/web/netperf-suite.mjs --scenario nav --profiles low --trace --cpuprofile
node scripts/web/netperf-suite.mjs --runs 3          # median over N runs
```

Each run uses a fresh browser context ⇒ **cold** IndexedDB cache (worst case;
warm/returning-user boots skip the network — see the W4b IDB cache).

## Boot (cold, to first interactive screen)

| profile | first screen | app booted | network | main-thread **blocked** |
|---|---|---|---|---|
| local (unbounded)   | 13.1 s | 13.0 s | 70.2 MB / 104 req (98 xhr) | **0.9 s** |
| normal (200 Mbit/s) | 17.8 s | 17.7 s | 71.0 MB / 104 req (98 xhr) | **2.9 s** |
| low (50 Mbit/s)     | 27.2 s | 27.0 s | 71.1 MB / 104 req (98 xhr) | **10.0 s** |

Same ~71 MB / 98 sync XHRs every time; only the throttle changes. The frozen-tab
time scales straight with it: **0.9 → 2.9 → 10.0 s**. The CPU floor (~4.5 s real
work) is constant; everything above it is sync-fetch wait.

## Menu transitions (per-transition stall) — the mid-session jank

Main-thread **blocked** ms = tab frozen/unresponsive; **max RAF gap** = worst single
user-visible hitch.

| transition | local blocked / gap | normal blocked / gap | **low blocked / gap** |
|---|---|---|---|
| boot → main_hub               | 2.8 s / 0.35 s | 6.2 s / 1.2 s | **15.5 s / 3.7 s** |
| main_hub → song_select        | 0.1 s / 0.15 s | 1.7 s / 0.75 s | **5.9 s / 2.2 s** |
| song_select → part_difficulty | 0.0 s / 0.05 s | 0.6 s / 0.5 s | **1.7 s / 1.3 s** |
| part_difficulty → game        | 0.8 s / 0.55 s | 3.6 s / 2.3 s | **11.4 s / 6.7 s** |

At 200 Mbit/s — a perfectly normal connection — **entering a song freezes the tab
for 3.6 s with a 2.3 s single hitch**, and reaching the main hub costs 6.2 s frozen.
At 50 Mbit/s the song-load freeze is **11.4 s with a 6.7 s dead frame**.

## The named offenders (low/nav waterfall, per-request block time)

Every one of these is a single **synchronous** XHR that freezes the main thread for
the listed time at 50 Mbit/s:

**part_difficulty → game** (21.2 s wall, 11.4 s blocked, **6.7 s worst freeze**):
- `songs/.../20thcenturyboy.mogg` — **37.4 MB → 6.76 s frozen.** A 37 MB audio
  file fetched as a blocking main-thread XHR is the single worst hitch in the game.
- `world/venue/small_club/.../small_club_01.milo_xbox` — 19.4 MB → 3.5 s
- `sfx/gen/kit03_bank.milo_xbox` — 4.0 MB → 0.8 s

**boot → main_hub** (19.7 s wall, 15.5 s blocked):
- `char/main/shared/gen/colorpalettes.milo_xbox` — **20.8 MB → 3.7 s**
- `world/vignette/shell/gen/sv8_a.milo_xbox` — 11.8 MB → 2.4 s
- `world/vignette/shell/gen/sv3_a.milo_xbox` — 11.2 MB → 2.3 s
- `world/vignette/transition/gen/tv6_a.milo_xbox` — 6.3 MB → 1.2 s
- `ui/main/gen/main_hub.milo_xbox` — 5.3 MB → 1.1 s

**main_hub → song_select** (9.2 s wall, 5.9 s blocked): `sv4_d` (12 MB), `tv1_a`
(5.9 MB), `tv11_a` (5.7 MB) vignettes + `song_select` milos.

No redundant re-fetching: 188 file requests across the session, all unique (MEMFS
dedups within a session). The cost is purely the synchronous, throughput-bound
delivery of large binaries on demand.

## Where to focus (ranked, tied to the data)

1. **Eliminate the synchronous on-demand fetch — the root cause.** Every miss goes
   through `WebAssetsFetchSync()` (blocking `xhr.open(..., false)`), so each large
   asset freezes the wasm thread for its whole transfer. The engine **already has**
   an async path (`WebAssetsFetchBundle` / `emscripten_fetch`); the fix is to feed
   the per-screen working set through it **ahead of** the screen that needs it, so
   the synchronous engine reads land in MEMFS and never touch the network. Two
   layers:
   - **Per-screen async prefetch / bundle.** Extend the boot bundle concept
     (currently `.dta/.dtb` only — see `server.py _serve_bundle`) to a per-screen
     manifest of `.milo_xbox` (+ the song mogg) fetched async into MEMFS during the
     *previous* screen / on selection. Converts N blocking XHRs per transition into
     one overlapped background download.
   - **Special-case the giant files.** The mogg (37 MB), `colorpalettes` (21 MB),
     and venue milos (12–19 MB) dominate every freeze. At minimum fetch these async
     with priority; the mogg is audio and can be **streamed/decoded progressively**
     rather than fully resident before play.
2. **Prefetch the next screen's assets during idle.** The working set per screen is
   small and known (the waterfalls above are the manifest). Kick off async fetches
   for the likely-next screen while the current one is interactive, so transitions
   hit warm MEMFS.
3. **Compress `/api/file` on the wire (secondary).** `_maybe_serve_precompressed`
   only serves `.br`/`.gz` siblings, which exist for `.wasm/.js` only — `.milo_xbox`
   are sent **raw**. The measured byte counts are uncompressed; gzipping milos would
   cut transfer (moggs are already-compressed Ogg → ~0 gain, so this doesn't fix the
   worst case, but helps the venue/palette milos). Pre-generate `.gz` siblings in
   `build.sh`/asset prep, or compress on demand with a cache.
4. **Warm-boot IDB cache already helps returning users** (cold runs here bypass it);
   it does nothing for first-time loads or the first song of a new venue, which is
   what hurts. #1–#2 fix the cold path.

The unifying point: **bandwidth is fine; synchrony is the bug.** 71 MB at 200 Mbit/s
is ~3 s of actual transfer — but because it's delivered as serial blocking XHRs
interleaved with the boot, it costs far more in frozen frames. Make the fetches
async + ahead-of-need and the throughput tiers collapse toward the loopback numbers.

## Caveats

- Throttling is applied at the renderer (CDP), so it models bandwidth + latency but
  not real-world packet loss / variance.
- Cold IDB every run (fresh context). Warm boots are faster but not the complaint.
- `nav` drives one representative path (20thcenturyboy / small_club); other
  songs/venues differ in asset size but not in the synchronous-fetch shape.
