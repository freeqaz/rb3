# Native-Probe Data — converge-2026-06-20

On-device measurements for the convergence batch. **Research only** — nothing in
`src/`/engine was modified. Downstream tasks (deterministic harness builder +
residual root-causer) read this doc.

- Binary: `native/build-native/rb3-native` (build: `ninja: no work to do`, up to date)
- Boot song: default first song reached by `keyboard-to-gameplay.py` nav
  (`--song-downs 4`), hard difficulty, headless `MILO_HEADLESS=1 RB3_HTTP=1`.
- Probe harness written: `docs/native/converge-2026-06-20/band-closeup-probe.py`
- Engine logs (binary, NUL-laden — parse with `-a`/python):
  - guard ON:  `/tmp/rb3-bandcloseup-guardon-8755.log`  (859 `[SHARD_GUARD]`, 23335 `[SHARD_RATIO]`)
  - guard OFF: `/tmp/rb3-bandcloseup-guardoff-8756.log` (0 `[SHARD_GUARD]`, 23290 `[SHARD_RATIO]`)

---

## 1. BLOCKER: `force_shot` is NOT reachable via DTA in the native build

The existing harness (`scripts/native/crowd-shot-capture.py`) drives the camera with
`{band_director force_shot "..."}` and `{$band_director set disabled 1}`. **Both are
silent no-ops in native.** Measured live at gameplay:

| DTA expr | result |
|---|---|
| `{band_director camera_source}` | `0` |
| `{$band_director get disabled}` | `0` |
| `{$band_director set disabled 1}` | `0` |
| `{$band_director get disabled}` (after set) | `0` (unchanged) |
| `{band_director force_shot "coop_dir_g_cls00"}` | `0` |
| `{band_director force_shot "coop_dir_g_cls00.shot"}` | `0` |
| `{director iterate}` / `{arena_01 iterate}` / `{coop_dir_g_cls00 iterate}` | `0` (object not resolvable) |

Root cause: the native build registers only `rb3_set`, `rb3_overshell`,
`rb3_char_probe`, `rb3_pos_dump` as DTA funcs
(`native/src/rb3_http_handlers.cpp:772-777`). There is **no** `band_director` DTA
accessor and the live `BandDirector` is not a name-resolvable object in the DTA
tree (`TheBandDirector` is a C++ global only — `BandDirector.cpp:48,110`). So the
DTA evaluator treats `band_director`/`$band_director` as an unknown symbol and
returns 0 without ever reaching `BandDirector::OnForceShot`
(`BandDirector.cpp:1216`) or the `disabled` SYNC_PROP (`BandDirector.cpp:2070`).

**Consequence for determinism:** the captured A/B frame pairs are at DIFFERENT
camera angles (the auto-director keeps cutting every frame via `OnSelectCamera`,
`BandDirector.cpp:1446` — only gated by `mDisabled`, which we cannot set). So the
shots in §3 were NOT pinned. Verified visually: e.g. `guardon_coop_dir_g_cls00_A`
vs `_B` are two different shots, and A/B file sizes all differ (the scene is live).

**What the deterministic harness needs (downstream impl, native-only, Wii-neutral):**
a tiny `RB3HttpRegisterDtaFuncs` accessor pair, e.g.
`{rb3_force_shot "coop_dir_g_cls00"}` → `TheBandDirector->ForceShot(TheBandDirector->mVenue.Dir()->Find<BandCamShot>(name,false))`
and `{rb3_director_disable 1}` → set `mDisabled`. `mVenue.Dir()` is already used by
`rb3_pos_dump` (`rb3_http_handlers.cpp:632-633`), so the plumbing exists.
`BandDirector::ForceShot` sets `mNextShot`+`mDisablePicking` (`BandDirector.cpp:900`)
but `OnSelectCamera` still re-picks unless `mDisabled` is also set — so BOTH verbs
are required to pin a shot.

---

## 2. Venue director shot vocabulary (arena_01)

Shot objects live INSIDE the venue milo, not as loose `.shot` files. Source:
`orig-assets/extracted/world/venue/arena/arena_01/gen/arena_01.milo_xbox`
(653 distinct `coop_dir_*` object names). Role-letter prefix decoded from the
`PickDist`/category code in `BandDirector.cpp:1116-1128` (`'v'` special-cased =
vocals) and the histogram:

| prefix | role | count | notes |
|---|---|---|---|
| `g…` | **guitar** | 84 | incl. `g_cls00..03` (explicit CLOSEUP) |
| `b…` | **bass** | ~30 | incl. `b_cls00..01` (explicit CLOSEUP) |
| `d…` | **drums** | ~70 | `d_pnt*` (point), `d_lt*` (look-thru), `d_kp*` |
| `k…` | **keys** | ~50 | `k_cls`? none; `k_bre*`/`k_brej*` |
| `v…` | **vocals** | ~280 | `v_scream*`, `v_dive*`, `v_surf*`, `v_cam_*` |
| `all…`/`*_duo`/`crowd*`/`*_cam*` | group/crowd/wide | rest | `coop_dir_crowd*` = crowd cams |

**Explicit single-member CLOSEUP shots (`_cls`):**
`coop_dir_g_cls00`, `coop_dir_g_cls01`, `coop_dir_g_cls02`, `coop_dir_g_cls03`
(guitarist), `coop_dir_b_cls00`, `coop_dir_b_cls01` (bassist). No `_cls` exists for
drums/keys/vocals; for those the bare `d00..d04`, `k00..k07`, `v…` are the
per-member framings and `d_pnt_*`/`d_lt_*` frame the drummer/kit (incl. feet/legs).

Full dump: `/tmp/allshots.txt` (regenerate:
`grep -rao 'coop_dir_[a-zA-Z0-9_]*' <arena_01.milo_xbox> | sort -u`).

---

## 3. force_shot determinism — NOT verified (blocked by §1)

8 shots forced per pass, A frame + B frame ~2.5s later, both passes. Because §1
makes `force_shot` a no-op, the camera was NOT pinned; A and B are different
auto-director shots. **Determinism is currently UNACHIEVABLE via DTA** and will be
once the §1 accessors land. The shots themselves DO exist and DO render (the
auto-director naturally cuts to band-member framings — see screenshots in §6 that
show band members near-camera).

File-size table (proves every frame is a distinct live render, none identical):

| shot | guardon A/B | guardoff A/B |
|---|---|---|
| coop_dir_g_cls00 | 1699730/1329834 | 1873033/1302360 |
| coop_dir_g_cls01 | 1588005/1996736 | 1313230/2067605 |
| coop_dir_b_cls00 | 1650106/1822221 | 2000226/1989864 |
| coop_dir_g_np_m00 | 1713081/1947605 | 1634905/2128083 |
| coop_dir_d_pnt_m00 | 1585418/1918685 | 1858869/1671209 |
| coop_dir_d_lt00 | 1941974/2046926 | 1666902/2104614 |
| coop_dir_g00 | 2059083/1858747 | 2116570/1878932 |
| coop_dir_b00 | 2048873/1426618 | 2105608/1433359 |

---

## 4. The residual drops — dropped-mesh table (guard ON, full run)

Parsed from 859 `[SHARD_GUARD]` lines. **All 4 dropped meshes classify as
`other` (NON-band).** NO band-classified mesh was dropped this run.

| mesh | drops | bind~ | world~ | ratio (med[min-max]) | owning dir | bone0 | class |
|---|---|---|---|---|---|---|---|
| `scrollbar_bg.mesh` | **607** | 80.8 | 324.1 | 4.0 [4.0-4.0] | `scrollbar` | (0,-0,40) | other (UI) |
| `clap.mesh` | 128 | 51.3 | 109.4 | 2.1 [2.0-2.2] | `crowd_male03` | (22,-3,57) | other (crowd prop) |
| `male_extras_hair02.mesh` | 62 | 14.6 | 36.6 | 2.5 [2.4-2.6] | `male_extras02` | (-227,72,184) | other (vignette extra) |
| `male_extras_eyebrows11.mesh` | 62 | 4.9 | 23.2 | 4.7 [4.7-4.7] | `male_extras11` | (-164,598,-43) | other (vignette extra) |

Asset attribution:
- `scrollbar_bg.mesh` → **UI** resource: `ui/resource/gen/scrollbar_accomplishments.milo_xbox`,
  `scrollbar_display.milo_xbox`, `ui/ui_objects.dta`. This is a UI scrollbar
  background, skinned, exploding ~80u→324u (4.0x) across the play area — NOT a
  character mesh. **Dominant residual (71% of all drops).**
- `clap.mesh` → crowd character hand-prop: `char/crowd/gen/crowd_*.milo_xbox`.
- `male_extras_hair02/eyebrows11.mesh` → venue "vignette" extra people:
  `char/extras/gen/male_extras*.milo_xbox`.

Guard-OFF pass = 0 drops (confirms the env gate works; only the ratio is logged).

---

## 5. `lowtopsneaks_skin` / band footwear — NOT reproduced this song

The prior batch listed `lowtopsneaks_skin` (band-outfit shoe) as a masked
residual drop. **On the default boot song it is present and is NOT dropped.**
From `[SHARD_RATIO]` (band-classified):

| mesh | n | bind~ | world~ | ratio (med[min-max]) | drops |
|---|---|---|---|---|---|
| `lowtopsneaks_skin.2.mesh` | 349 | 11.2 | 19.1 | 1.7 [0.9-3.5] | **0** |
| `lowtopsneaks_resource.mesh` | 349 | 18.4 | 26.1 | 1.4 [0.9-2.7] | 0 |
| `gloves_resource.1.mesh` | 112 | 3.8 | 12.5 | 3.3 [2.4-3.7] | 0 (saved by 40u world floor) |
| `wovensteppers_skin.2.mesh` | 328 | 14.2 | 25.9 | 1.8 [1.0-3.7] | 0 |
| `thighboots_resource.mesh` | 352 | 36.9 | 53.6 | 1.4 [1.1-1.9] | 0 |
| `kissboots_resource.mesh` | 346 | 33.0 | 43.7 | 1.3 [1.0-1.9] | 0 |
| (all other band garments) | — | — | — | ≤1.8 | 0 |

So the band-relaxed caps (4.0x ratio + 110u world + 40u world-floor,
`Rnd_Wgpu_RB3.cpp:5098-5106`) ALREADY protect every band garment/footwear on this
song. The closest band-edge case is `gloves_resource.1.mesh` (a tiny 3.8u-bind
submesh hitting ratio 3.7), saved only by the 40u world-floor.

**Implication:** the `lowtopsneaks_skin` drop is song/outfit-dependent and did NOT
manifest on the boot song — it would require a song whose band outfit drives that
shoe's bound limb into a deeper curl / different skeleton basis than this song. To
reproduce it, downstream should pick a song that assigns the low-top-sneaker
outfit AND a high-motion drummer/guitarist animation. Asset roots:
`orig-assets/extracted/char/` (outfit meshes) — `lowtopsneaks_*` is a real
outfit-footwear pair present in this song's wardrobe but well within caps here.

---

## 6. Screenshot index (`docs/native/converge-2026-06-20/shots/`)

32 PNGs: `{guardon|guardoff}_{shot}_{A|B}.png`. Caveat (§1): A/B are NOT matched
frames — the camera was not pinned. Use them to SEE the residual geometry, not for
pixel A/B (which needs the §1 fix). Key frames inspected:

- `guardoff_coop_dir_g00_A.png` — **shows the `scrollbar_bg` residual**: large
  ornate blue/teal filigree pattern sprawled across the whole highway/track
  surface = the exploded UI scrollbar mesh (80u→324u). This is what the guard
  drops; without the guard it corrupts the play area.
- `guardon_coop_dir_g00_A.png` — guard ON but filigree still visible (frame NOT
  matched to the guardoff frame; explosion is intermittent/per-frame) — concrete
  proof that unmatched venue A/B is a false-positive trap (§1).
- `guardoff_coop_dir_d_lt00_B.png` — scrollbar filigree clearly across highway.
- `guardon_coop_dir_b_cls00_A.png` — band members framed near-camera (left) +
  scrollbar filigree on track; shows the auto-director does swing to band.
- `guardon_coop_dir_g_cls00_A.png` / `_B.png` — two DIFFERENT angles for the same
  forced name = determinism NOT achieved (§3).

All 32 files present (verified). Raw shot list: `/tmp/allshots.txt`.

---

## 7. Sanity: placement is correct (not the issue)

`{rb3_pos_dump}` at gameplay: `roots=5 crowd_containers=6 crowd=292 band=4
props=280 at_origin=42 band_at_origin=0/4 crowd_at_origin=0/292`. Band (4) and
crowd (292) are all placed; none at origin. Consistent with the prior
crowd/drum-kit "at origin" misattribution being resolved. The residual is purely
the §4 skinned-mesh shard drops.
