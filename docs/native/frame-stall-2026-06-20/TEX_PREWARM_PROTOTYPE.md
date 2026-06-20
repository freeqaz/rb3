# Venue-milo prewarm prototype — reveal-frame sync-drain elimination (2026-06-20)

Prototype for stall **#2** in [`FRAME_STALL_FINDINGS.md`](FRAME_STALL_FINDINGS.md):
the single worst web song-start engine frame, the `game_screen` reveal. Goal:
prove the ~600ms reveal stall is reducible by force-resolving the venue assets
through the budgeted background loader during the loading-vignette dwell, so the
synchronous texture/scene drain never fires on the audio-critical reveal frame.

**Bottom line.** The reveal stall is **not** a per-texture `Tex.cpp:181` drain as
the original finding framed it — it is a **13–19 MB random-venue `.milo` loaded
synchronously inside one rAF tick**. When the prewarm targets the *correct*
venue, it removes **~390 ms (−70%)** of the reveal-frame sync drain
(`lpu 561 → 169 ms`, `dt 672 → 254 ms`), reproducibly. The honest limit: in the
unforced **random**-venue quickplay path the venue Symbol re-rolls ~3 s before
the reveal and the 19 MB can't fully overlap the short dwell, so the win shrinks
into the run-to-run variance of *which* venue loads. The mechanism is proven; the
residual blocker is venue *prediction* + asset *size*, not the prewarm itself.

Worktree branch: **`wt-framestall-texprewarm`** (commit `7a6bb108`). Do not push /
do not bump `MILO_ENGINE_PIN` (engine untouched — all changes are rb3-side).

---

## Root cause (corrected attribution)

The worst frame in every capture is the `game_screen` reveal. FRAME_TRACE
(observer-free, wasm-side) for it, baseline (`RB3_TEX_PREWARM_OFF`):

```
f.. dt=671.8ms  lpu=560.99  obj=168.95(RndTex:floor_wood02_NORM.tex)  tex=91.16  pend=1
```

- `lpu` = ms in `LoadMgr::PollUntilLoaded` (the **synchronous drain**) — the metric.
- `obj` = slowest single object's PreLoad+PostLoad **inside** that drain (a venue
  normal-map `RndTex`: `floor_wood02_NORM` / `wall_wainscoat_plaster_norm`).
- `tex` = the separate GPU upload of those textures on the first draw.
- `fetch=0` → the venue bytes do **not** come via the WebAssets sync XHR; they
  stream through the venue `DirLoader`'s own chunked fetch, driven by the drain.

The chain (verified by reading the source + the server access log + a native
`RB3_TEX_PREWARM_DBG` run):

```
GamePanel goes live  ->  world_panel WorldDir syncs its venue WorldInstance
  ->  WorldInstance::SetProxyFile(fp)            (src/system/world/Instance.cpp:295)
  ->  ObjDirPtr::LoadFile(fp, share=true)        (src/system/obj/Dir.h:63)
  ->  ObjDirPtr::PostLoad                         (src/system/obj/Dir.h:123)
  ->  LoadMgr::PollUntilLoaded(...)   <-- 511–705 ms: fetch 19MB + inflate + parse
                                          every venue RndTex PostLoad inline
```

The venue milos are large: `small_club_01.milo_xbox` = **19,434,928 bytes**;
the small-club set is 13–19 MB each. The venue is **randomly selected**
(`MetaPerformer::SelectRandomVenue → SetVenue(venueCfg->Sym(RandomInt(...)))`),
and the server log shows it is not even *fetched* until the reveal frame — the
`tv3_*` loading vignette sits ~idle on-screen for ~5 s first.

Because `PumpAudio` runs once per `requestAnimationFrame` on the single-threaded
JSPI build, this one 500–700 ms tick starves the ~140 ms audio ring → under-run.
This is also the clean reproduction of the audio team's mid-play 114–127 ms
stalls (a venue/LOD re-sync mid-song hits the same `PollUntilLoaded` path).

**Baseline variance is large** — the reveal `dt` ranges **601–792 ms** run to
run, entirely because a *different* random venue (different size) loads each time.
This matters for interpreting the random-path A/B below.

---

## The fix (native-only, match-neutral)

`RB3VenuePrewarmPoll()` (in `native/src/rb3_gamewarm_native.cpp`, the existing
native prewarm TU) is called every loading-dwell frame from `Game::IsLoaded()`
(before the `kReady` gate, so it runs from the first dwell frame). It:

1. computes the venue milo `FilePath` from `MetaPerformer::Current()->GetVenue()`
   + `GetVenueClass()` → `world/venue/<class>/<venue>/<venue>.milo` (this resolves
   to the served `gen/<...>_xbox` path exactly);
2. if no `DirLoader` exists for that path yet, creates a background one
   (`new DirLoader(fp, kLoadFront, 0,0,0,false)` — the same self-owned, queued,
   budget-drained loader `ObjDirPtr::LoadFile`'s non-shared branch makes);
3. re-kicks if the committed venue **re-rolls** (random path), so we always
   target the *current* venue, never waste bandwidth on a stale one.

When the reveal's `WorldInstance::SetProxyFile → LoadFile(fp, share=true)` runs,
`DirLoader::Find(fp)` finds the now-**loaded** loader and shares it — the
`PollUntilLoaded` drain no-ops. (`LoadFile` only shares an *already-loaded*
DirLoader; an in-flight one warns "Can't share unloaded dir" and duplicates — so
the win requires the prewarm to *finish* during the dwell.)

Gated `RB3_TEX_PREWARM_OFF=1` (default ON), `RB3_TEX_PREWARM_DBG=1` for logs.
State reset by `RB3GameWarmReset()` on `GamePanel::Unload`.

**Match-neutrality.** `native/src/rb3_gamewarm_native.cpp` is native-only glue.
The one shared-decomp edit, the `RB3VenuePrewarmPoll()` call + extern decl in
`src/band3/game/Game.cpp`, is inside `#ifdef HX_NATIVE` → the Wii build is
byte-identical. The engine (`milo-native-engine`) is untouched. Verified to
compile clean in both `rb3-native` (clang) and the web bundle.

---

## Measurement (before / after)

Harness: `scripts/web/_framestall-texprewarm-ab.mjs` / `_framestall-texprewarm-det.mjs`
(Playwright + the FRAME_TRACE recorder — **no** CPU-profiler observer effect).
Web debug bundle built in the worktree, served by `native/web/server.py --port 8832`.
Song `20thcenturyboy`.

### Correct-venue case (deterministic — the proof the mechanism works)

Forcing a fixed venue via a `ui/dev_only/selvenue.dta` overlay
(`{meta_performer set_venue_override small_club_01}`) removes the random-venue
confound, so the prewarm always targets the venue that actually loads:

| run | reveal `dt` | reveal `lpu` (sync drain) | `obj` parse | `tex` upload |
|---|---|---|---|---|
| **DET-OFF** (baseline) | **671.8 ms** | **561.0 ms** | 169.0 ms | 91.2 ms |
| **DET-ON** | **253.9 ms** | **168.7 ms** | 141.0 ms | 76.5 ms |
| DET-ON (run 2) | 260.4 ms | 171.2 ms | 142.2 ms | 78.2 ms |
| DET-ON (run 3, rebuilt) | 266.5 ms | 172.7 ms | 144.0 ms | 77.4 ms |

→ **reveal `dt` −418 ms (−62%); sync drain `lpu` −392 ms (−70%)**, reproducible
across 3 runs (±7 ms). The venue **fetch+inflate** moves into the idle dwell; the
share is clean (no "Can't share unloaded dir", only `small_club_01` touched).
Steady-state longtasks are unchanged / slightly better (DET-OFF 26 → DET-ON 18–20).

The residual `lpu ≈ 169 ms` is the venue **object-parse** (`obj ≈ 141 ms` of
RndTex PostLoads): `DirLoader::Find` shares a loaded loader, but the dir-object
*construction* still runs on the reveal frame. Eliminating that needs the parse
itself spread across dwell frames (a larger change — the loader API parses on
drain), or a pre-parsed/cached scene.

### Random-venue case (the honest limit)

Without the override, the quickplay venue is random and re-rolls ~3 s before the
reveal (`RB3_TEX_PREWARM_DBG` showed the prewarm see `small_club_13` then the load
use `small_club_01`):

| run | reveal `dt` | reveal `lpu` |
|---|---|---|
| rand-OFF (run 1) | 791.9 ms | 705.1 ms |
| rand-OFF (run 2) | 601.2 ms | 511.5 ms |
| rand-ON | 603.3 ms | 494.4 ms |

The rand-ON number sits **inside the rand-OFF run-to-run spread** (501–705 ms lpu)
— i.e. with the pre-refinement code it picked the *wrong* venue, wasted a 12.9 MB
download, and the apparent improvement was not separable from venue-size variance
(it also bumped steady-state longtasks 19 → 112 from the wrong-venue download
contending for the pipe). The committed code's re-kick-on-re-roll fixes the
*wasted-bandwidth* half (it now fetches the correct venue), but ~3 s of dwell
cannot fully overlap a 19 MB download, so the random-path win is partial and
variance-dominated. **The deterministic table is the true ceiling of this lever.**

---

## Verdict & what would make it production-real

The prewarm **works** — proven to cut the reveal sync drain ~70% when it targets
the correct venue, with clean sharing and no steady-state regression. The fix did
**not** relocate cost onto another frame in the deterministic case (the fetch went
to genuinely idle dwell frames; only the ~140 ms parse remains on the reveal). The
two things standing between this prototype and a shipped default-on win:

1. **Venue prediction in the random path.** Hook the prewarm to the point the
   quickplay venue is *committed* (after `TourPerformerLocal::SelectVenue` /
   `SelectRandomVenue`, before the reveal) rather than re-reading a possibly-stale
   `mVenue` during the dwell — so the correct 19 MB starts downloading at the
   *start* of the ~5 s dwell, not 3 s before reveal. (Tour mode and any
   `set_venue_override` flow already commit early and would get the full win
   today.)
2. **Venue asset size.** 13–19 MB `.milo_xbox` per venue is the real wall. Even a
   perfectly-predicted prewarm needs the dwell ≥ the download time. Shipping
   smaller / little-endian / split venue milos (overlaps the incremental-load
   workflow) shrinks both the prewarm window *and* the residual parse.

A complementary lever for the residual `~140 ms` object-parse and the
random-path tail: spread the venue `PollUntilLoaded` across rAF ticks (let it
return to the rAF scheduler so `PumpAudio` runs between slices) instead of
draining the whole dir inside one tick — this attacks the *frame-budget* directly
rather than the *fetch overlap*, and is venue-size-independent.

## Artifacts / repro

- Prototype: branch `wt-framestall-texprewarm` @ `7a6bb108`
  (`native/src/rb3_gamewarm_native.cpp`, `src/band3/game/Game.cpp`).
- Harnesses (main repo `scripts/web/`): `_framestall-texprewarm-ab.mjs`
  (`--prewarm on|off`, random venue), `_framestall-texprewarm-det.mjs`
  (deterministic via the overlay).
- Deterministic-venue scaffold (NOT committed — it changes default venue):
  `native/dta/ui/dev_only/selvenue.dta` = `{meta_performer set_venue_override small_club_01}`,
  served by the dev server's DTA overlay.
- Per-run captures: `cap/texprewarm-{on,off}/tickprobe.json`; run logs under
  `/tmp/texprewarm-*.log`.
