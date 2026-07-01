# Adversarial review — venue-aware prewarm (random-path reveal stall)

Reviewer pass over the venue-prediction fix
([`VENUE_PREDICTION_FIX.md`](VENUE_PREDICTION_FIX.md) /
[`VENUE_PREDICTION_VERIFY.md`](VENUE_PREDICTION_VERIFY.md)).
Branch **`wt-texwarm-venuepredict`** @ **`1f529fb0`** (rb3); engine
**`5cbe855`** (untouched). **Verdict: CONFIRM (with disclosed residual).**

## What I independently reproduced

### 1. The mechanism is correct on the RANDOM path (native, re-run myself)

Built `rb3-native` clean in the worktree against the paired engine worktree
(`milo-native-engine-worktrees/texwarm-venuepredict` @ `5cbe855`). Ran the
native venue-commit probe several times on **default random quickplay** (the
harness forces only the *song*, never a venue):

| run | `GetVenue()` (phantom `mVenue`) | prewarm **kicked** | `EnterVenue` **force-loaded** | match? |
|---|---|---|---|---|
| A | `small_club_14` | `small_club_01` | `small_club_01` | YES |
| B | `small_club_04` | `small_club_01` | `small_club_01` | YES |
| C | `small_club_11` | `small_club_01` | `small_club_01` | YES |

The phantom `mVenue` (`select_random_venue` writes it; `GetVenue()` reads it)
diverges run-to-run, but the prewarm **never** targets it — it resolves the
world-prop / override exactly as `BandDirector::EnterVenue` does, and hits the
**actually-loaded** venue (`small_club_01`) every time. The prototype's
wrong-venue download (reading `GetVenue()`) is genuinely gone. This is the crux
of the fix and it holds.

Confirmed `EnterVenue`'s native resolution order in source
(`src/system/bandobj/BandDirector.cpp:631-665`): (1) `get_venue_override`,
(2) `GetWorld()->Property("venue")`, (3) `small_club_01` — `mVenue` never
consulted. `ComputeVenueMiloPath()` mirrors this 1:1.

### 2. The win shows on the RANDOM path (web artifacts, independently parsed)

The deployed web debug wasm (`native/web/build/debug/rb3-web.wasm`, 03:06, newer
than the 02:49–02:50 source edits) contains the fix string
`RB3_TEX_PREWARM: kicked background DirLoader for venue` — it is built from this
branch's source. I could not re-drive the web A/B here (no Playwright resolvable
in the worktree, no DISPLAY for the ANGLE/Vulkan WebGPU path), but I parsed the
prior agent's freshly-captured FRAME_TRACE artifacts
(`cap/texprewarm-{off,on}/tickprobe.json`, 03:11–03:12) directly:

| metric (worst reveal frame) | OFF | ON |
|---|---|---|
| reveal `dt` | **721.2 ms** | **242.5 ms** (−66%) |
| reveal `lpu` (sync drain) | **612.1 ms** | **170.6 ms** (−72%) |
| worst object | `RndTex:floor_wood02_NORM.tex` | `RndTex:wall_wainscoat_plaster_norm.tex` |
| longtasks >50ms (count / Σms) | 15 / 1404 | 12 / 1146 |

- The worst object in BOTH arms is a `small_club` **venue normal-map** — exactly
  the SetBitmap-drain target from FRAME_STALL_FINDINGS, confirming this is the
  reveal frame and the right stall.
- The **second**-worst frame in each arm has `lpu ≈ 0.1 ms` → there is exactly
  ONE big reveal-frame drain and the fix cuts it ~3.5×; it is not smeared.
- The ON `lpu` (~171 ms) sits **well below** the OFF run-to-run spread
  (532–612 ms across VERIFY's runs) → the win is separable from venue-size
  variance, unlike the prototype.

### 3. No load-duration regression / no missing textures

- Web longtask Σ **dropped** OFF→ON (1404 → 1146 ms); the prewarm only moves the
  venue fetch earlier into idle dwell. Reveal frame index comparable across arms.
- Native gameplay reaches `songMs > 100` in every run; venue + highway render
  (VERIFY's `/tmp/twvp-gameplay.png`). The kicked venue == the loaded venue, so
  nothing is downloaded-but-unused and nothing is missing.

### 4. Match-neutral (Wii byte-identical)

- **Only shared-source change on the branch:** `src/band3/game/Game.cpp`, +12
  lines, **all** inside `#ifdef HX_NATIVE` (verified line-by-line — the call
  `RB3VenuePrewarmPoll()` + its extern decl, both guarded).
- **`src/system/rndobj/Tex.cpp` is UNTOUCHED** on the branch (empty diff) — the
  Wii synchronous `PollUntilLoaded` drain path is byte-identical.
- `native/src/rb3_gamewarm_native.cpp` is a native-only glue TU.
- Engine `milo-native-engine` untouched (`5cbe855`, clean, no `MILO_ENGINE_PIN`
  bump).
- Per-song reset wired: `GamePanel::Unload()` → `RB3GameWarmReset()` →
  `RB3TexPrewarmReset()` (all `#ifdef HX_NATIVE`), so a 2nd song re-prewarms its
  own venue. Re-kick-on-path-change guard prevents stale/double kicks.

## Residual (disclosed, not a blocker)

ON still pays `lpu ~171 ms` / `objMs ~143 ms` on the reveal frame: `DirLoader::Find`
shares the *loaded* loader, but the venue's RndTex **PostLoad object-parse** still
runs on that frame (`pend:1`). So the under-run trigger is reduced ~3.5×, not
eliminated — 171 ms still overruns the ~33 ms audio budget. Killing it needs the
parse spread across rAF ticks or a pre-parsed scene (venue-size-independent,
explicitly out of scope). The task's bar ("the 612 ms reveal frame drops
materially, no wasted prewarm") is met.

## Verdict

**CONFIRM.** The random-path reveal stall is genuinely reduced (`lpu` 612→171 ms,
−72%; `dt` 721→243 ms, −66%), the prewarm targets the actually-committed venue
(no wrong-venue waste — re-verified across 3 native runs with diverging phantom
`mVenue`), there is no load-duration regression (longtask Σ drops) or missing
texture, and it is match-neutral (Game.cpp under `#ifdef HX_NATIVE`, Tex.cpp
untouched, engine untouched). The residual reveal-frame object-parse cost is
honestly disclosed and out of scope.
