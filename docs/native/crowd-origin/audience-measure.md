# AUDIENCE MEASURE — direct dump of the live WorldCrowd audience positions

Date: 2026-06-20. Agent B: measure-the-audience (Opus). Engine pin
`MILO_ENGINE_PIN = 884ab17d…` (matches engine HEAD; no engine edit this batch).
Native -O0 Debug build. Tool committed in `native/src/rb3_http_handlers.cpp`.

This closes the ONE evidentiary gap the verify agent flagged in
`verify-verdict.md` §1/§4: the prior `{rb3_pos_dump}` returned **`crowd=0`** —
the `WorldCrowd` audience container was never reached, so H4 ("crowd `unk0`
decodes to zero/origin") was refuted only INDIRECTLY (via the SHARD_GUARD bone0
fallback, which covers venue VIGNETTE extras, NOT the audience). The user's
literal report — *"the crowd is all congregating at origin … dump the locations
of crowd members so we can validate the distribution"* — demanded a direct
measurement of the audience. This is that measurement.

**Headline: the AUDIENCE is SPREAD, not at origin. H4 is now DIRECTLY REFUTED
for the audience.** 300 audience members across 6 `WorldCrowd` containers,
`crowd_at_origin = 0/300`, distributed across the full venue floor
(x∈[-161,161], y∈[-297.6,-22.1], z∈[68.6,74.5]). The audience was never the
visible "pile" — it is never SHARD-dropped (229 drops/run, all `male_extras*` /
`scrollbar`, ZERO `crowd_female*`/`crowd_male*`).

---

## 1. WHERE the live WorldCrowd actually lives (STEP 1)

The audience `WorldCrowd` objects live in the **per-song venue `WorldDir`**:
`world/venue/<class>/<name>/<name>.milo` (here `small_club_01`). This dir is a
SEPARATE `ObjectDir` from the resident `world/world.milo` and from the
`world_panel`'s `LoadedDir()` — the roots the prior tool walked. It is loaded
into and owned by `BandDirector`:

- `BandDirector::EnterVenue()` (BandDirector.cpp:614-…) force-loads the venue
  (`LoadVenue(venueSym, …)`) into `mVenue` (a `VenueLoader`, member 0xf0), then
  `TheBandWardrobe->SetVenueDir(mVenue.Dir())` and `mCurWorld = mVenue.Dir()`.
- `BandDirector::mVenue.Dir()` → the loaded venue `WorldDir*` (BandDirector.h:25).
- `BandDirector::mCurWorld` → `ObjPtr<WorldDir>`, the ENTERED venue (== mVenue
  after EnterVenue) (BandDirector.h:154).
- `TheBandDirector` is a global (`extern BandDirector *TheBandDirector`,
  BandDirector.h:186).

Why the prior recursive walk missed it: `ObjDirItr<T>(dir, true)` recurses only
through `dir->NextSubDir()` → `mSubDirs` (obj/Dir.cpp:657-674). The venue
`WorldDir` is NOT in the `mSubDirs` chain of `world/world.milo` /
`world_panel` — it is a top-level loaded dir held by `BandDirector`. So no
recursion from the old roots ever reached it (`crowd=0`).

**How positions are populated** (re-read of the actual code, not placeholders):

- `WorldDir::SyncObjects()` (Dir.cpp:342-356) clears + rebuilds
  `mCrowds` (`ObjPtrList<WorldCrowd>`, Dir.h:102) via
  `for (ObjDirItr<WorldCrowd> it(this, true); …) mCrowds.push_back(it)`. This is
  the LIVE, cached list of every `WorldCrowd` in the venue dir — reading it does
  NOT depend on the crowd being inside OUR walk's reachable subtree.
- `WorldCrowd::mCharacters` is an `ObjList<CharData>` (Crowd.h:113). Each
  `CharData` holds `mDef` (archetype Character/height/density), `mMMesh`
  (`RndMultiMesh*` of placement instances) and `m3DChars`
  (`std::vector<Char3D>`, Crowd.h:55).
- `WorldCrowd::Set3DCharAll()` (Crowd.cpp:216-237) copies every
  `mMMesh->mInstances[i].mXfm` into a `Char3D(instIt->mXfm, idx)` pushed to
  `m3DChars`, THEN `mInstances.clear()`. So the authored per-member position is
  `m3DChars[i].unk0` (a `Transform`, Crowd.h:41), `.v` its translation.
- `WorldCrowd::Draw3DChars()` (Crowd.cpp:328-408) reads `m3DChars[i].unk0.v` and
  `curChar->SetWorldXfm(spXfm)` — ABSOLUTE world, so `unk0.v` IS the audience
  member's world position. **`unk0.v` is the right field** (confirms the
  verify-verdict §3 read; `unk0` is the real member name, not a placeholder).

## 2. How the tool was extended (STEP 2) — `native/src/rb3_http_handlers.cpp`

Native-only (no `src/system/*` edit). Three additions to `RB3DtaPosDump`:

1. **New root: the live venue dir.** Add `TheBandDirector->mVenue.Dir()` and
   `mCurWorld` to the walked root set (de-dup handles the overlap). Also keep
   `TheWorld` opportunistically (it is transiently NULL outside `DrawShowing`,
   so it is a bonus, not the primary handle).
2. **PRIMARY PATH: read each venue `WorldDir::mCrowds` DIRECTLY.** This is the
   path that closes the gap — it reads the cached pointer list `SyncObjects`
   already built, independent of `mSubDirs` reachability. All 300 members came
   via this path (`from=mCrowds`).
3. **`DumpOneWorldCrowd()` helper.** Per `WorldCrowd`, per `CharData` archetype,
   emit each `m3DChars[i].unk0.v`. **Fallback:** if `m3DChars` is empty (i.e.
   `Set3DCharAll` hasn't run yet), read `mMMesh->mInstances[i].mXfm.v` instead —
   the same authored source — so we never report a spurious `crowd=0` just
   because the copy step hasn't fired. Each line tags `src=` (m3DChars/mInstances)
   and `crowd=`/`arch=`/`from=` for provenance.

The `[POSDUMP] kind=crowd` line now carries: `name`, `i`, `pos`, `src`,
`crowd`, `arch`, `from`. The summary line gained `crowd_containers=N`. The
existing harness regex (`scripts/native/crowd-origin-posdump.py` `CROWD_RE`)
matches the leading `name=…/i=…/pos=…` and is unchanged (the new trailing
fields are additive). Harness UNCHANGED this batch.

## 3. The measurement (STEP 3) — AUDIENCE IS SPREAD

Raw summary (verbatim, stable across runs):

```
posdump roots=5 crowd_containers=6 crowd=300 band=4 props=280 at_origin=42 \
        band_at_origin=0/4 crowd_at_origin=0/300
```

`venue_dir name=small_club_01 nCrowds=6`. Distribution of all 300 members:

```
count=300  at_origin=0  x=[-161.0,161.0] y=[-297.6,-22.1] z=[68.6,74.5]
distinct z = {68.6, 69.5, 73.6, 74.5}   # four standing/tier height bands
```

The 6 containers + per-archetype member counts (300 total):

| WorldCrowd container | archetypes | members |
|---|---|---|
| `WorldCrowd.crd`              | 8 | 70 |
| `WorldCrowd_2_ps3.crd`        | 2 | 70 |
| `WorldCrowd_4_ps3.crd`        | 4 | 70 |
| `WorldCrowd_frontrow.crd`     | 8 | 30 |
| `WorldCrowd_frontrow_2_ps3.crd` | 2 | 30 |
| `WorldCrowd_frontrow_4_ps3.crd` | 4 | 30 |

8–12 sample member world positions (`unk0.v`, across distinct containers):

```
crowd_female03 i=0  (-110.07, -203.02, 68.62)   WorldCrowd.crd
crowd_female03 i=1  (   1.92, -286.89, 68.62)   WorldCrowd.crd
crowd_female01 i=0  (  88.59,  -82.59, 68.62)   WorldCrowd_2_ps3.crd
crowd_female01 i=1  (  48.14,  -54.70, 68.62)   WorldCrowd_2_ps3.crd
crowd_female02 i=0  (-115.54, -138.11, 68.62)   WorldCrowd_4_ps3.crd
crowd_female02 i=1  ( -76.79,  -49.85, 68.62)   WorldCrowd_4_ps3.crd
crowd_female03 i=0  ( -46.67,  -51.87, 69.47)   WorldCrowd_frontrow.crd
crowd_female03 i=1  ( 132.50,  -58.57, 69.47)   WorldCrowd_frontrow.crd
crowd_female01 i=0  ( -31.95, -104.28, 69.47)   WorldCrowd_frontrow_2_ps3.crd
crowd_female01 i=1  (-128.09,  -86.61, 69.47)   WorldCrowd_frontrow_2_ps3.crd
crowd_female02 i=0  (-159.99, -108.74, 69.47)   WorldCrowd_frontrow_4_ps3.crd
crowd_female02 i=1  ( 106.89,  -72.52, 69.47)   WorldCrowd_frontrow_4_ps3.crd
```

These are real, structured venue-floor coordinates: full stage width in x,
audience depth in front of the stage in y (the venue camera sits at
y≈-474, so negative-y is the audience area), four discrete z height-bands
(seated/standing tiers). **NOT (0,0,0). NOT all-equal. `at_origin = 0/300`.**

## 4. Verdict — H4 REFUTED FOR THE AUDIENCE (the loose end is closed)

- **The audience is placed correctly (SPREAD), not at origin.** Direct
  `m3DChars[i].unk0.v` measurement, 0/300 at origin, stable across 2 runs.
  This upgrades the verify agent's "H4 — REFUTED FOR EXTRAS, UNPROVEN FOR THE
  AUDIENCE (medium)" to **H4 — REFUTED FOR THE AUDIENCE (high, direct)**.
- **The audience is NOT the reported visible "pile."** It is never
  SHARD-dropped (229 drops/run cross-checked: all `male_extras*` / `scrollbar`,
  zero `crowd_*` bodies). So even when the user perceives "crowd congregating at
  origin," the AUDIENCE bodies are not the geometry collapsing — consistent with
  the established cause: the V24 `[SHARD_GUARD]` dropping over-deformed
  band-instrument / vignette-extra skin (a SKINNING issue, not placement).
- **No new bug in the audience path.** Positions populated from `m3DChars`
  (`src=m3DChars` for all 300), not the pre-copy fallback — `Set3DCharAll` ran
  and the `RndMultiMesh::Instance.mXfm` venue load decoded to sane spread
  transforms (no endian/identity collapse on the native venue load, refuting
  scout-crowd §6 suspect #2 for this venue).

## 5. What this does NOT change (carry-overs for the fix agent)

The OPEN QUESTION from `verify-verdict.md` §4 fork #1 stands and is the
priority: is the band `instrument` skin genuinely exploding, or is the
SHARD_GUARD's bind-vs-world ratio a false positive on instrument geometry?
That is the `SHARD_GUARD_OFF=1` + screenshot A/B in
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5073-5108`. **This audience
measurement is the close-out of fork #2 (the audience loose end), not the cause
of the visible pile.** Do NOT spend a fix cycle on audience placement.

## 6. Reproduce

```bash
cmake --build native/build-native --target rb3-native -j16
python3 scripts/native/crowd-origin-posdump.py        # boot->gameplay->dump->verdict
# audience lines:  POS_DUMP_VERBOSE=1 ... grep -a 'kind=crowd' /tmp/rb3-posdump-*.snapshot.log
# live:            curl -s localhost:PORT/api/dta/eval -d '{rb3_pos_dump}'
```

LOG GOTCHA: the engine log is binary (NUL bytes) — use `grep -a` or the
harness's Python parse for `[POSDUMP]`/`[SHARD_GUARD]` lines.
