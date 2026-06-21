# Venue-aware prewarm — the reveal-frame win on the RANDOM quickplay path (2026-06-21)

Follow-up to [`TEX_PREWARM_PROTOTYPE.md`](TEX_PREWARM_PROTOTYPE.md). The prototype
proved the reveal-frame sync-drain (`game_screen` reveal, stall #2 in
[`FRAME_STALL_FINDINGS.md`](FRAME_STALL_FINDINGS.md)) is ~70% reducible **when the
prewarm targets the venue that actually loads**, but got **no separable win on the
default random quickplay path** because it prewarmed the *wrong* venue. This change
makes the prewarm target the **actually-committed venue** and lands the win on the
random path.

**Result (random/default quickplay, web debug, song `20thcenturyboy`, 2 runs):**

| metric | OFF (baseline) | ON (fix) |
|---|---|---|
| reveal frame `dt` | 878 ms / 711 ms | **411 ms / 292 ms** (−53% / −59%) |
| reveal `lpu` (sync drain) | 794 ms / 606 ms | **272 ms / 208 ms** (−66% / −66%) |
| longtasks >50 ms (count / sum) | 28 / 2140 ms | **13 / 1275 ms** |

The win is now **separable from venue-size variance** (the prewarm always hits the
loaded venue), unlike the prototype where ON sat inside the OFF run-to-run spread.

Branch **`wt-texwarm-venuepredict`** @ **`3686836e`** (rb3). Engine **untouched**
(`5cbe855`, no `MILO_ENGINE_PIN` bump). Do not push.

---

## Investigation — WHEN/WHERE the venue commits (the question the task asked)

Traced the quickplay venue-selection + load flow end-to-end and instrumented a live
native run (`scripts/native/_venue-commit-probe.py`: drives boot → song_select →
part_difficulty → game_screen, sampling `{meta_performer get_venue}` /
`get_venue_override` over `/api/dta/eval` at each phase, with `VENUE_DBG=1` logging
the venue `BandDirector::EnterVenue` actually force-loads).

### The venue commit point — `seldiff.dta:131`, with seconds of dwell

The quickplay venue is committed at the **`part_difficulty_screen`'s `load_panels`
handler**:

```
ui/seldiff/seldiff.dta:122  (load_panels
  ...
  :131       {meta_performer select_random_venue})
```

`select_random_venue` → `MetaPerformer::SelectRandomVenue()`
(`src/band3/meta_band/MetaPerformer.cpp:960`) → `SetVenue(venueCfg->Sym(RandomInt))`
(`:1038`), which sets `mVenue`. This fires when `part_difficulty` *loads its panels*
— **before** the `preloading_screen` → `tv3_*` → `game_screen` transition.

Measured dwell from this commit to the texture-reveal frame:

| run | part_difficulty(commit) → reveal | song-confirm(end_override) → reveal |
|---|---|---|
| 20thcenturyboy | **16.3 s** | 14.2 s |
| beastandtheharlot | **11.9 s** | 9.7 s |
| 20thcenturyboy (run 3) | **11.7 s** | 9.5 s |

So the venue is final **~12–16 s before reveal**, with **no re-roll** in between:
`get_venue` was stable from part_difficulty through tv3, game_screen, and reveal in
every run. (The prototype's "venue re-rolls ~3 s before reveal" was a
misattribution — see below.)

### The real bug — `EnterVenue` ignores `mVenue` (the prototype's "wrong venue")

The prototype's `ComputeVenueMiloPath()` read `MetaPerformer::GetVenue()`. But the
venue that actually loads is **not** `GetVenue()`. `BandDirector::EnterVenue`
(`src/system/bandobj/BandDirector.cpp:614`) resolves the venue, native path
(`:631-665`), as:

1. `MetaPerformer get_venue_override`  (if set, `!= no_venue_override`)
2. else `GetWorld()->Property("venue")`  (the gameplay `world.milo` instance prop)
3. else `"small_club_01"`

and **never consults `mVenue`** — its own comment says so (`:641-643`). Measured
directly: in two runs `get_venue` was `small_club_06` then `small_club_03`, but
`EnterVenue` force-loaded `small_club_01` **both times** (the world-prop fallback).
A 3rd run: `get_venue=small_club_10`, loaded `small_club_01`.

So in native quickplay the venue is effectively **pinned to `small_club_01`** (the
world-prop fallback; no override in this flow), while `select_random_venue` writes a
*phantom* random `mVenue` that is loaded by nothing. The prototype prewarmed that
phantom → downloaded a venue that never rendered (the "12.9 MB wasted download" it
reported as a "re-roll"). The only venue milo ever touched on disk is
`world/venue/small_club/small_club_01/small_club_01.milo`.

### Feasibility — YES

The venue is committed **~12–16 s before reveal**, the committed value (the
world-prop / override that `EnterVenue` reads) is **stable** and **knowable during
the dwell** (`TheBandDirector->GetWorld()` is the merged `world/world.milo` WorldDir,
loaded by `world_panel` early in the dwell). There is ample dwell to background-load
the ~19 MB venue before the synchronous reveal load. The only thing the prototype
got wrong was *which* venue to read.

---

## The fix — `ComputeVenueMiloPath()` mirrors EnterVenue's resolution

`native/src/rb3_gamewarm_native.cpp` — `ComputeVenueMiloPath()` now resolves the
venue exactly as `EnterVenue` will:

1. `MetaPerformer::Current()->GetVenueOverride()` (if set, `!= no_venue_override`),
2. else `TheBandDirector->GetWorld()->Property("venue")` (the world-prop;
   returns `""` if `GetWorld()` not yet merged → caller retries next frame, so a
   premature `small_club_01` guess can't waste a download if a late override
   differs),
3. else `small_club_01`,

then builds `world/venue/<class>/<venue>/<venue>.milo` (class = name with trailing
`_NN` stripped, via a local `VenueClassOf`, not `MetaPerformer::GetVenueClass` which
operates on the wrong `mVenue`). The kick path (`RB3VenuePrewarmPoll` → background
`DirLoader(kLoadFront)`) is unchanged.

**Hook point (unchanged from prototype):** `Game::IsLoaded()`
(`src/band3/game/Game.cpp:286`, inside `#ifdef HX_NATIVE`) calls
`RB3VenuePrewarmPoll()` every dwell frame before the `kReady` gate. Since the venue
is final the moment `GetWorld()` merges (early dwell), the kick now lands at the
**start** of the ~12–16 s dwell — full overlap. Measured: kick at frame ~1198,
`EnterVenue` load at frame ~2612 → **~1414 frames of background-load overlap**.

### Verification

- **Native:** prewarm kicks `small_club_01`, `EnterVenue` force-loads
  `small_club_01` (match), shares cleanly (no "Can't share unloaded dir" for the
  venue), venue renders with all textures + band + highway (`/tmp/twvp-gameplay.png`).
- **Web (the win that matters — native `-O2` is too fast to show the 600 ms stall):**
  the A/B table above, 2 reproducible runs on the random path.
- **No load-duration regression:** the prewarm only moves the venue fetch earlier
  into genuinely-idle dwell frames; web longtask sum *dropped* (2140 → 1275 ms).

### Residual (next lever, out of scope here)

`lpu` doesn't go to zero — ON still shows `lpu ~208–272 ms` / `obj ~172–213 ms`
(`floor_wood02_NORM` / `wall_wainscoat_plaster_norm`). That is the venue
**object-parse** still running on the reveal frame: `DirLoader::Find` shares the
*loaded* loader but the dir-object *construction* (the RndTex PostLoads) still
executes there. Eliminating it needs the parse spread across rAF ticks (let
`PollUntilLoaded` return to the scheduler so `PumpAudio` runs between slices) or a
pre-parsed/cached scene — a larger, venue-size-independent change.

---

## Match-neutrality

- `native/src/rb3_gamewarm_native.cpp` — native-only glue TU.
- `src/band3/game/Game.cpp` — the only shared-decomp edit is the
  `RB3VenuePrewarmPoll()` call + extern decl, both inside `#ifdef HX_NATIVE`. Wii
  build byte-identical.
- Engine (`milo-native-engine`) untouched.

## Artifacts / repro

- Fix: branch `wt-texwarm-venuepredict` @ `3686836e` (rb3),
  `native/src/rb3_gamewarm_native.cpp` (+ cherry-picked prototype `2d35f261`).
- Investigation tool: `scripts/native/_venue-commit-probe.py` (phase-by-phase venue
  state + dwell measurement; resolves the GetVenue-vs-loaded-venue divergence).
- Web A/B harness (prototype's): `scripts/web/_framestall-texprewarm-ab.mjs
  --prewarm on|off` (random venue). Captures: `cap/texprewarm-{on,off}/tickprobe.json`.
- Flags: `RB3_TEX_PREWARM_OFF=1` disables; `RB3_TEX_PREWARM_DBG=1` logs the kicked
  path; `VENUE_DBG=1` logs the venue EnterVenue force-loads.
