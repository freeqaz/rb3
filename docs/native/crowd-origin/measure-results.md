# MEASURE RESULTS — crowd/band position dump (ground truth)

Date: 2026-06-20. Agent: tool-build + measure (Opus). Engine pin
`MILO_ENGINE_PIN = 884ab17d…` (matches engine HEAD). Native -O0 Debug build.

This is the GROUND-TRUTH measurement called for by `PLAN.md` §2/§3. It builds the
`{rb3_pos_dump}` debug DTA func, runs it in live gameplay (small_club_01), and
reports the per-object world positions that adjudicate H1 vs H2 vs H4.

**Headline verdict: H1, H2, and H4 are ALL REFUTED by the live data. The band and
the crowd are NOT at origin. They are placed correctly across the venue; the
visible "pile" is the V24 `[SHARD_GUARD]` DROPPING heavily-deformed skinned meshes
(worldExt/bindExt ratio 2.4–4.7) — a SKINNING problem, not a placement-at-origin
problem.**

---

## Tool + harness (deliverables)

- `native/src/rb3_http_handlers.cpp` — `RB3DtaPosDump` registered as
  `{rb3_pos_dump}` (next to `rb3_char_probe`). Walks `ObjectDir::sMainDir` PLUS
  the resident venue DirLoader dirs (`world/world.milo`,
  `world/shared/director.milo`, `world/shared/chars.milo`) and the `world_panel`
  `LoadedDir()` — because on native the venue/band/crowd are NOT merged into the
  walkable `sMainDir` tree; they live in those resident dirs (mirrors the gather
  in `rb3_gamewarm_native.cpp:402-427`). De-dups by object pointer. Cast order:
  `WorldCrowd` → `BandCharacter` → `RndTransformable`. Verbose `[POSDUMP]` stderr
  lines gated behind `POS_DUMP_VERBOSE`; the HTTP call always returns a summary.
- `scripts/native/crowd-origin-posdump.py` — boots `rb3-native` headless to
  gameplay (`song-end-test.py` nav), waits `songMs > 2000`, calls
  `{rb3_pos_dump}`, parses the `[POSDUMP]` + `[SHARD_GUARD]` lines, prints a
  verdict table mapping to H1/H2/H4. Sets `POS_DUMP_VERBOSE=1` + `SHARD_DBG=1`.

**Gotcha (durability):** the engine log is binary (NUL bytes from other output),
so plain `grep` silently skips the `[POSDUMP]`/`[SHARD_GUARD]` matches — use
`grep -a` or the harness's own Python parse. The harness snapshots the log to
`*.snapshot.log` BEFORE teardown so the verbose lines survive the SIGTERM.

---

## Raw summary line (verbatim, returned by `{rb3_pos_dump}`)

```
posdump roots=4 crowd=0 band=4 props=17 at_origin=13 band_at_origin=0/4 crowd_at_origin=0/0
```

Reproduced identically across 5 runs (band root coords vary ±2u frame-to-frame
because the song is playing; the distribution is stable).

---

## Band characters (the drum-kit half) — SPREAD, NOT at origin

| slot | root WorldXfm.v | \|root\| | parent | mInstDir (kit) world-sphere | \|inst\| |
|---|---|---|---|---|---|
| player0 | (88.9, 49.6, 13.2) | 102.7 | NULL | (96.6, 59.3, 46.6) | 122.5 |
| player1 | (-69.8, 81.2, 13.5) | 107.2 | NULL | (-72.0, 68.0, 50.0) | 111.0 |
| player2 | (-10.1, 31.4, 13.2) | 33.6 | NULL | (-9.9, 16.2, 49.8) | 53.3 |
| player3 | (14.4, 146.1, 13.2) | 147.4 | NULL | (5.8, 122.8, 28.2) | 126.1 |

- All 4 band roots are at **distinct, sane stage positions** (|pos| 34–147), NOT
  at (0,0,0). `band_at_origin=0/4`.
- Each member's `mInstDir` (the instrument geometry = the kit/guitar) sits
  **right next to its own root** (within ~10–25u), so the kit is NOT detached and
  NOT at origin. **H5 (merge mis-placement) is also refuted.**
- `parent=NULL` for all 4: the root WorldXfm is set ABSOLUTELY (not inherited
  through a venue parent). This is the "parent NULL" the H2/H3 fork warned about —
  BUT because the root WorldXfm IS a real spread position, the NULL parent is
  benign here (the placement mechanism just writes WorldXfm directly rather than
  re-parenting). **H2 ("root never placed → at origin") is REFUTED at the data
  layer: the root IS placed.**

## Crowd — the WorldCrowd container is not enumerable, BUT the crowd renders SPREAD

- `crowd=0`: `dynamic_cast<WorldCrowd*>` matched ZERO objects across all 4 walked
  roots. The `WorldCrowd` container lives under a deeper per-song venue proxy
  subdir that is synced at Enter and is NOT reached by these roots, so
  `m3DChars[i].unk0.v` could not be read directly.
- The crowd ARCHETYPE Characters ARE found (as `RndTransformable` props):
  `crowd_male01..04`, `crowd_female01..04`, all at `world=(0,0,0)`. This is
  EXPECTED — they are the shared archetypes the draw loop re-`SetWorldXfm`s
  per-instance each frame; between draws they sit at origin. It does NOT mean the
  drawn crowd is at origin.
- **Decisive crowd evidence — the `[SHARD_GUARD]` cross-check:** 1287 crowd/band
  skinned-mesh drops this run, with bone0 world positions **fully SPREAD and
  none at origin**:
  - `\|bone0\|` range = [40.0, 622.2], `at_origin(<1) = 0/1287`
  - x ∈ [-226.7, 0.5], y ∈ [-0.0, 598.6], z ∈ [-42.7, 184.4]
  - e.g. `male_extras_hair02.mesh dir='male_extras02' bone0=(-226.7, 71.6, 183.7)`,
    `male_extras_eyebrows11.mesh dir='male_extras11' bone0=(-164.2, 598.2, -42.6)`.
  - The crowd extras therefore render at real, spread venue positions. **H4
    (crowd `unk0` decodes to zero/origin) is REFUTED.**

## Static props (control) — venue placed correctly

`[POSDUMP]` props dump (verbatim, the load-bearing ones):

```
[POSDUMP] kind=prop name=world         class=WorldDir world=0.00,0.00,0.00
[POSDUMP] kind=prop name=small_club_01 class=WorldDir world=0.00,0.00,0.00
[POSDUMP] kind=prop name=world.cam     class=Cam      world=-267.04,-431.50,131.15
[POSDUMP] kind=prop name=WorldCamInterest.intr class=CharInterest world=-267.04,-431.50,131.15
[POSDUMP] kind=prop name=crowd_male01  class=Character world=0.00,0.00,0.00   # archetype (expected origin)
... (crowd_male02-04, crowd_female01-04 likewise — shared archetypes)
```

- The venue WorldDirs (`world`, `small_club_01`) are at their own origin (0,0,0) —
  correct: the venue root is at origin and its CONTENTS carry their own xfms.
- `world.cam` / its interest are at the real camera position (-267, -431, 131).
- The 11 "at origin" props are cameras, the default light, the venue WorldDirs,
  the crowd-archetype Characters, and `crowd_chars.grp` — ALL legitimately at
  origin. There is no broadly-collapsed prop. **The dartboard-control premise
  holds: static placement is fine.**

## `[SHARD_GUARD]` cross-check — the ACTUAL cause

`SHARD_DBG=1` drops (this run; varies run-to-run with frame timing, conclusion
invariant):

| dir | mesh (example) | bone0 (example) | bindExt | worldExt | ratio |
|---|---|---|---|---|---|
| `instrument` (kit) | `guitar_brain_strings.mesh` | (60.9, 44.0, 49.7) | 29.35 | 137.89 | 4.7 |
| `male_extras02` | `male_extras_hair02.mesh` | (-226.7, 71.6, 183.7) | 14.62 | 35.27 | 2.4 |
| `male_extras11` | `male_extras_eyebrows11.mesh` | (-164.2, 598.2, -42.6) | … | … | … |
| `scrollbar` | `scrollbar_bg.mesh` | … | … | … | … |
| (band outfit) `''` | `lowtopsneaks_skin.2.mesh` | (-86.4, 161.9, -4.1) | 11.20 | 46.57 | 4.2 |

- The DROPPED meshes are the band INSTRUMENT geometry (`dir='instrument'`, 1161
  drops in one run) + band outfit skin + crowd extras — i.e. exactly the
  "jumbled pile" the screenshot showed.
- Their bone0 positions are at the CORRECT staged location (the `instrument`
  drops cluster at (60–115, 35–59, 48–50), right at player0's staged kit), NOT
  origin. So placement is correct.
- The DROP fires because the skinned WORLD bounding box is 2.4–4.7× the bind-pose
  box (`worldExt/bindExt`): the V24 shard guard treats that as a runaway skin and
  drops the mesh. The engine even logs the upstream cause:
  `NOTIFY: --->Arvin/Diana: Skinned mesh needs to be re-exported: guitar_brain_strings.mesh`.
- Net visual effect: the meshes that should render at the band/crowd positions
  get DROPPED, so what remains looks like a sparse/jumbled cluster. The
  "everything at origin" reading from the 2D screenshot was a misattribution.

---

## Verdict (PLAN.md §3 forks)

```
REFUTES H1 / H2 / H4 — skinning-guard drop, not placement-at-origin
```

Reasoning chain:
- **H2 (band root never placed → at origin):** REFUTED. All 4 band roots are at
  distinct spread stage positions; the kit (`mInstDir`) rides right next to each
  root. The root WorldXfm IS being set (absolutely; parent NULL is benign).
- **H5 (instrument merge mis-place):** REFUTED. The kit is co-located with its
  drummer, not floating.
- **H4 (crowd `unk0` decodes to zero):** REFUTED. The crowd extras render at
  spread, sane bone0 positions ([40, 622] magnitude, none at origin).
- **H1 (shared `WorldInstance::SyncDir` reparent collapse):** REFUTED. Neither
  half is at origin and the venue WorldDirs are correctly at their own origin —
  there is no collapse to reparent away.
- **ACTUAL cause:** the V24 `[SHARD_GUARD]` (engine
  `Rnd_Wgpu_RB3.cpp:4924-5141`) DROPPING over-deformed skinned meshes
  (worldExt/bindExt 2.4–4.7) for the band instrument geometry, band outfit skin,
  and crowd extras. This is the SAME skinning-deform family as the prior
  char-skinning / shard-guard work — the bones place correctly but the skin
  composes to a runaway AABB, tripping the guard. The fix belongs in the
  skin-deform / inverse-bind / shard-guard path (engine + the `Crowd.cpp` /
  `BandCharacter.cpp` rebake), NOT in venue placement (`world/Instance.cpp`,
  `BandWardrobe.cpp`).

### Follow-on recommendation
1. Do NOT pursue the H1/H2/H4 placement fixes — the dump proves placement is
   correct. Re-pointing the leading hypothesis to skinning will save a wasted
   fix cycle.
2. Investigate WHY the band instrument + crowd-extra skinned meshes compose to a
   2.4–4.7× world AABB (the `guitar_brain_strings.mesh` "needs re-export" notify
   is a strong lead — likely a bind-pose / inverse-bind mismatch on those
   specific meshes, same class as the char-skinning-deform saga).
3. A/B with `SHARD_GUARD_OFF=1` to see the un-dropped (but possibly flung) meshes
   and confirm the guard is the visible-pile gate, then fix the deform so the
   guard stops firing (rather than relaxing the guard, per PLAN.md 0.4).
4. To read `m3DChars[i].unk0.v` directly (currently `crowd=0`), extend the tool's
   root set to descend the per-song venue proxy subdir where `WorldCrowd` lives
   (it is synced at Enter, below the resident `world/world.milo` dir) — the
   shard cross-check already settles the spread-vs-origin question, so this is a
   nice-to-have, not required for the verdict.

---

## Reproduce

```bash
cmake --build native/build-native --target rb3-native -j16
python3 scripts/native/crowd-origin-posdump.py            # boot→gameplay→dump→verdict
# or live:  curl -s localhost:PORT/api/dta/eval -d '{rb3_pos_dump}'
# verbose:  POS_DUMP_VERBOSE=1 SHARD_DBG=1 ... (the harness sets these)
```
