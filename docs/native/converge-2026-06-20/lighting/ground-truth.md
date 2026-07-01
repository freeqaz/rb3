# Ground-truth: what arena + big_club gameplay SHOULD look like in original RB3

**Ground-truth agent (Opus). RESEARCH ONLY — no code/engine changes, no rebuild, no
commit.** Establishes the *intended* appearance for GAP 2 (arena_02 black band) and
GAP 3 (big_club white crowd) so the two fix agents target the right look and don't
"fix" an intentionally-dark arena. Evidence = (1) retail RB3 reference frames, (2) the
AUTHORED per-environ RndEnviron light data read out of the real assets via the engine's
own `RB3_VENUE_PROBE` (read-only run of the prebuilt `native/build-native/rb3-native`),
(3) a side-by-side of a venue that renders correctly (festival_01) vs the two broken
ones.

---

## TL;DR verdict (go/no-go for the fix agents)

| gap | native render | RB3 intent | divergence? | magnitude |
|---|---|---|---|---|
| **GAP 2 arena_02** | band = **pure-black silhouette** (mean frame luma ~40; performer surface luma ≈ ambient floor) | band **spotlit + visible** against a dark stage (skin/face/clothing readable); the *backdrop* is dark, the *performer* is NOT | **YES — genuine** | large. Festival band on the SAME engine/shot = luma 103. Arena performer is ~floor-black; should be a lit, readable figure. |
| **GAP 3 big_club_01** | audience = **flat stark-WHITE cut-out** figures lining the stage | audience **dim/dark + unobtrusive** (cf small_club retail) | **YES — genuine** | medium. Crowd figures are the brightest thing in frame; should be among the darkest. |

Both are real divergences, not intended looks. **The arena darkness is partly correct
(RB3 arenas ARE dark, spotlit stages) — but a PURE-BLACK performer with zero surface
detail is wrong; retail performers are always lit and readable.** The big_club white
crowd is unambiguously wrong (retail audiences are never bright-white).

---

## 1. Retail reference: how RB3 lights the band (universal — all venues)

RB3's stage look is consistent across venues: **moody/dark backdrop, but the band
performers are always richly lit and fully readable** (skin tone, hair, clothing, face).
This is the single most important ground-truth fact for GAP 2.

Evidence (saved under `docs/native/converge-2026-06-20/refs/`):

- **`refs/retail_rb3_vocalist_career_lit.jpg`** — RB3 career-mode vocalist closeup
  (warm-lit venue). Performer fully lit: skin, hair, glasses, white jacket all clearly
  visible with a warm key + ambient fill. Source:
  YouTube `auw78cT5J-g` thumbnail (`https://img.youtube.com/vi/auw78cT5J-g/maxresdefault.jpg`,
  video "Rock Band 3 - Career mode - walkthrough gameplay - Part 1",
  `https://www.youtube.com/watch?v=auw78cT5J-g`).
- **`refs/retail_rb3_vocalist_ps3_lit.jpg`** — RB3 (PS3) vocalist closeup, magenta-lit
  venue. Again fully lit + readable. Source: YouTube `Kgu03I32IAQ` thumbnail
  (`https://img.youtube.com/vi/Kgu03I32IAQ/maxresdefault.jpg`, "Complete in Box Plays -
  Rock Band 3 (PS3) - Part 20", `https://www.youtube.com/watch?v=Kgu03I32IAQ`).
- **On-disk in-repo retail frames** (already in `images/retail-screenshots/`):
  - `fandom_gameplay_guitar.png` / `fandom_gameplay_drums.png` (360/PS3 club closeups) —
    guitarist (blonde hair, cream jacket, blue gloves) and drummer (pink hair, black top)
    **fully lit and readable** against a cool-blue dark backdrop.
  - `yt_qRagnZCIMzk_gameplay_guitar.png` (Wii club) — performer behind the highway is a
    **dark blue-lit silhouette but still distinguishable** (NOT pure black); cool-blue
    moody backdrop.
  - `yt_qRagnZCIMzk_gameplay_drums.png` (Wii wide club) — band members on the right are
    **visible lit figures** against a dim grey-blue slat backdrop.

**Caveat on the Wii look:** the Wii frames are the dimmest (it's a debug Wii build,
lower-fidelity), but even there the performer reads as a lit figure with edge/value
detail, not a featureless black cut-out. The 360/PS3 frames (the visual target the port
is converging toward) are clearly bright and detailed.

**No retail *arena* or *big_club* wide frame was locatable** (MobyGames/GiantBomb galleries
403 WebFetch; no press arena gameplay stills surfaced). But the band-lighting INTENT is
venue-independent in RB3 (same `*_silhouette.lit` + `char_bounce`/`rim` rig per venue —
see §3), so the closeup ground-truth transfers: an arena performer is lit, not black.

**Corroborating signal:** RB3 Deluxe ships a *separate* opt-in **"Black Venue"** feature
(`https://rb3dx.milohax.org/features/`). A dedicated all-black venue would be pointless
if the stock arena already rendered the band black — i.e. the community treats a black
stage as a special non-default look, confirming stock arenas are NOT black-band.

---

## 2. Native render (the gaps), measured

Captured read-only from the prebuilt binary via `{meta_performer set_venue_override}`
(`/tmp/bch_override.py`), shots from the venue's own `coop_fs_*` set, `RB3_VENUE_PROBE=1`.

- **`refs/native_arena02_black_band.png`** (arena_02, `coop_fs_v_n01`, mean luma **39.8**):
  the vocalist on the left is a **pure-black silhouette** — outline only, ZERO surface
  detail. Stage floor black. Crowd LED boards in the background are the only bright thing.
- **`refs/native_bigclub_white_crowd.png`** (big_club_01, `coop_all_n00`, guard ON):
  audience figures flanking the highway render as **flat stark-WHITE cut-outs**; the band
  on stage is lit normally. (= GAP 3.)
- **`refs/native_festival_band_correct.png`** (festival_01, same `coop_fs_v_n01` shot,
  mean luma **103.0**): band performer **well-lit and readable** — this is the same
  engine + same camera as the arena shot, proving the engine CAN light the band; the
  arena's specific environ data is what fails.

Luma summary (mean frame luma, same harness):
`arena_02 ≈ 40` (band black) · `festival_01 ≈ 78–103` (band lit) · matches the audit's
"26–56 arena vs 78–200 festival".

---

## 3. Asset intent: the AUTHORED per-environ lights (root-cause-grade evidence)

`RB3_VENUE_PROBE=1` dumps, once per distinct RndEnviron, the ambient + every light's
type/color/range/pos as the venue actually loads them. RB3 scopes lighting per
subsystem: **`char.env`/`RB3_chars.env` lights the BAND, `crowd.env`/`*_crowd*.env`
lights the AUDIENCE, `geom.env`/`*_geom.env` lights the STAGE.** Captured logs:
`/tmp/bch-ov-gt_arena02-*.log`, `/tmp/bch-ov-gt_bigclub-*.log`, `/tmp/bch-ov-gt_festival-*.log`.

Light `type`: **0 = point, 1 = directional, 2 = spot, 3 = projected/shadow.**

### 3a. The BAND environ — why arena is black but festival is lit

The arena's band IS authored to be lit — but almost entirely by **short-range white
POINT lights** (the per-instrument `*_silhouette.lit`, positioned right at each station),
with a **near-black directional fill**. Festival, which renders correctly, adds **bright
directional fill lights** that have no range falloff and blanket every performer.

**arena_02 `char.env`** (ambRaw 0,0,0 → ambAdj clamped to floor 0.01):
```
rim.lit               type=1 (dir)  color=(1.00,0.00,0.00)  range=800   ← red rim, low coverage
rim_underneath.lit    type=1 (dir)  color=(0.00,0.00,0.00)            ← OFF
vocals_silhouette.lit type=0 (pt)   color=(1,1,1) range=55  pos=(46.7,-532.1,313.5)
bass_silhouette.lit   type=0 (pt)   color=(1,1,1) range=55  pos=(-141.3,-508.3,311.0)
drums_silhouette.lit  type=0 (pt)   color=(1,1,1) range=55  pos=(-11.8,-5.0,380.5)
guitar_silhouette.lit type=0 (pt)   color=(1,1,1) range=55  pos=(102.5,-559.8,284.4)
keyboard_silhouette.lit type=0 (pt) color=(0,0,0) range=0.2           ← OFF
char_bounce.lit       type=1 (dir)  color=(0.00,0.03,0.24)  range=450  ← NEAR-BLACK fill
```

**festival_01 `RB3_chars.env`** (the venue that renders the band correctly):
```
rim.lit               type=1 (dir)  color=(0.78,1.00,0.23)  range=850  ← BRIGHT green rim
bass/drums/guitar/vocals_silhouette.lit  type=0 (pt) color=(1,1,1) range=40–50
char_bounce.lit       type=1 (dir)  color=(0.50,1.00,0.64)  range=450  ← BRIGHT green-white FILL
main_rear.lit         type=1 (dir)  color=(0.30,0.46,0.77)  range=650  ← BRIGHT blue fill
```
…and festival's `char.env` additionally carries `sun.lit` type=0 color=(2.0,1.41,0.74)
range=3000 — a huge bright key.

**big_club_01 `RB3_chars.env`**:
```
rim.lit               type=1 (dir)  color=(0.62,0.47,1.00)  range=850  ← moderate purple-blue
rim_underlight.lit    type=1 (dir)  color=(0.00,0.00,0.00)            ← OFF
bass/guitar/vocals_silhouette.lit type=0 (pt) color=(1,1,1) range=40
drums_silhouette.lit  type=0 (pt)   color=(1.5,1.5,1.5) range=40      ← brighter drums
bonus_01..04.lit      type=0 (pt)   color=(0,0,0) range=300           ← all OFF (script-driven)
```

**Conclusion (band):** the *authored* difference is the directional FILL. Festival's
band fill (`char_bounce` 0.5/1.0/0.64 + `main_rear` + `sun`) is bright and covers the
whole performer; arena's fill (`char_bounce` **0,0.03,0.24** — essentially black) is
authored OFF, and big_club has only a moderate single `rim`. Arena/big_club therefore
lean on the **short-range white silhouette POINT lights** for the band's frontal light.
Those are ~range 40–55 at the instrument stations; on real RB3 they evidently reach the
performer, but in the native render the band ends up near-black. So either the engine's
point-light path under-delivers these (range falloff `(1-d/range)^2`, the 4-point cap, or
the `×0.70` point-exposure clamp — see §4), or the silhouette lights' tiny range simply
doesn't envelop the band mesh as authored, and the missing bright fill that masked this
on festival is absent here. **The arena band is INTENDED to be lit (4 white per-station
lights + a red rim are authored on it) — it is not an authored-black stage.**

### 3b. The CROWD environ — why big_club crowd reads white

The audience environs carry **ambient only, NO real lights** (`numApprox=0`):
- big_club_01 `RB3_crowd_mesh.env` ambRaw=(0.18,0.18,0.18); `crowd.env` ambRaw=(0,0,0).
- festival_01 `RB3_crowd.env` ambRaw=(0.16,0.16,0.16); `RB3_crowd_detailed.env` (0,0,0).
- arena_02 `crowd_mesh.env` ambRaw=(0.00,0.11,0.17).

So the crowd is authored to be lit by a **low grey/cool ambient (~0.16–0.18) only**, with
its shading carried by the **baked per-vertex AO/colour** of the crowd character meshes.
Intent = dim, unobtrusive, low-value audience (matches the retail small-club crowd:
`refs/`-comparable `Group A …_sd0_coop_dir_crowdg.png` shows a dim purple club with no
bright figures).

The native white-out is therefore a **material/shading** failure, not a missing light:
the engine shader (`standard_wgsl.inc` fs_main, lines 703–734 + 819–837) keeps a
desaturated *skinned vertex tint* to avoid the old all-white look, but the RB3 crowd
**skin/cloth diffuse textures are absent from the extracted asset set** (documented in
that same comment). With no diffuse texture, `baseColor ≈ material.color × skinnedTint`,
and the crowd's baked vertex luma is high → modulated by even the ~0.18 ambient (plus
`softClipLighting`), the figures land near-white. (This is the exact failure mode the
shader comment describes; the desaturate-only fix tamed the *colour* but not the *value*
for the big_club crowd.) **Intent = dim/dark crowd; native = white. Genuine divergence.**

NOTE — distinguish from authored-white surfaces (do NOT "fix" these):
- festival_01's crowd backdrop renders as an INTENDED comic/poster **B&W** style
  (`refs/native_festival_band_correct.png` background, and the festival `coop_dir_crowd00`
  shot) — authored, correct.
- video_01's white **studio backdrop** is the authored "video" venue look — correct.
- GAP 3 is specifically the **big_club *audience figures*** reading flat-white.

---

## 4. Engine path the fix agents will touch (pointers, not a fix)

Venue lighting is read in `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
`BandRnd::WriteSceneUniforms` (lines ~1265–1358), gated to `world.cam` + DEFAULT-ON
(`RB3_VENUE_LIGHT_OFF=1` opts out), re-written per-environ in `DrawMesh` (~3505–3511).
Things that matter for these two gaps (verified by reading the source at pin `5cbe855`):

1. **It only uploads light type 0 (point) and type 1 (directional); type 2 (spot) and
   type 3 are silently skipped** (`if (ty==1...) else if (ty==0...)`, line ~1315–1335).
   All the band silhouette lights are type 0, so they ARE uploaded — but if any band key
   were authored as a spot it would be dropped. (Worth a quick spot-type audit of
   `char.env`; in the three venues probed the band lights were all type 0/1.)
2. **Caps at 4 directional + 4 point per env** (`dl < 4`, `pl < 4`). arena_02 `char.env`
   has 4 *non-black* silhouette point lights (keyboard is 0,0,0) so it fits, but venues
   with ≥5 live point lights drop the overflow.
3. **Point-exposure clamp `×0.70`** (`sVenuePointExposure`) and **dir `×0.80`** scale the
   light colours DOWN (a menu/disco-overbright tuning). On a venue that depends entirely
   on those point lights for the band (arena), this directly dims the only key the band
   has.
4. **Ambient is clamped hard**: ambRaw≥0.85 → ×0.09; floored at 0.008. arena/big_club
   band envs are ambRaw=(0,0,0) → band ambient ≈ 0.008. With no directional fill and the
   point lights not landing, `litTerm ≈ 0.008` → near-black band. (`baseColor × litTerm`,
   shader line ~833.)
5. **Grey-key fallback fires ONLY when `dl==0 && pl==0`** (line ~1337). arena/big_club
   `char.env` HAS point lights (the silhouettes), so the fallback does NOT fire — the
   band is left dark even though the point lights effectively don't illuminate it.
6. **Crowd white** is the shader `skinnedTint` path (no diffuse texture → high vertex
   luma) modulated by the ~0.18 crowd ambient — see §3b. Fix candidate is the crowd
   character material/shading path, not a missing light.

These are *diagnostic pointers for the fix agents*, not a committed root cause. The
go/no-go ground-truth this doc establishes is in §0/§1/§3: **both gaps are genuine
divergences from RB3's intent — band must be lit (not black), crowd must be dim (not
white).**

---

## 5. How to reproduce (for the fix agents)

```bash
# venue override + probe (read-only; prebuilt binary; NO rebuild)
RB3_VENUE_PROBE=1 python3 /tmp/bch_override.py --override arena_02   --song-downs 0 \
    --shots "coop_fs_v_n01.shot" --frames 1 --tag a --out /tmp/x/a
RB3_VENUE_PROBE=1 python3 /tmp/bch_override.py --override big_club_01 --song-downs 0 \
    --shots "coop_all_n00.shot,coop_dir_crowdg.shot" --frames 1 --tag b --out /tmp/x/b
RB3_VENUE_PROBE=1 python3 /tmp/bch_override.py --override festival_01 --song-downs 0 \
    --shots "coop_fs_v_n01.shot" --frames 1 --tag f --out /tmp/x/f   # control: renders CORRECTLY
# probe lines are in the engine log: grep -a 'VENUE_PROBE' /tmp/bch-ov-<tag>-*.log
# (engine log is binary/NUL — always grep -a)
# arena_01 CRASHES — use arena_02. Shot names are venue-specific (coop_fs_* prefix).
# A/B the band-light path: re-run with RB3_VENUE_LIGHT_OFF=1 and compare band-region luma.
```

Asset intent can also be read statically (light NAMES, not types):
`strings -n4 orig-assets/extracted/world/venue/arena/arena_02/gen/arena_02.milo_xbox | grep -iE '\.lit$|\.env$'`
— but `RB3_VENUE_PROBE` is authoritative for the type/color/range/pos actually loaded.

---

## Sources

- Retail closeups (band fully lit): YouTube `auw78cT5J-g`
  (`https://www.youtube.com/watch?v=auw78cT5J-g`) + `Kgu03I32IAQ`
  (`https://www.youtube.com/watch?v=Kgu03I32IAQ`) thumbnails, saved to `refs/`.
- In-repo retail club frames: `images/retail-screenshots/{fandom_gameplay_guitar,
  fandom_gameplay_drums,yt_qRagnZCIMzk_gameplay_guitar,yt_qRagnZCIMzk_gameplay_drums}.png`.
- Venue progression (small club → big club → theater → arena): RB Wiki Solo Tour
  (`https://rockband.fandom.com/wiki/Solo_Tour`), via web search.
- "Black Venue" is a separate opt-in feature: RB3 Deluxe features
  (`https://rb3dx.milohax.org/features/`).
- Authored light data: `RB3_VENUE_PROBE` read-only run of `native/build-native/rb3-native`,
  logs `/tmp/bch-ov-gt_{arena02,bigclub,festival}-*.log`; assets at
  `orig-assets/extracted/world/venue/{arena/arena_02,big_club/big_club_01,festival/festival_01}/gen/*.milo_xbox`.
- Engine lighting path: `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
  (`WriteSceneUniforms` ~1139–1381, `DrawMesh` env re-write ~3505) +
  `src/gfx/standard_wgsl.inc` (fs_main lighting ~692–873, crowd skinnedTint ~703–734).
- Native gap + control frames: `refs/native_{arena02_black_band,bigclub_white_crowd,
  festival_band_correct}.png`.
