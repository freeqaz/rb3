# Audit MAP — venue / song / shot coverage plan (converge-2026-06-20)

**Map agent (research only).** Empirically derived the song→venue mapping, the
reachable venue surface, and the per-venue shot vocabulary so the 3 parallel audit
agents can maximize venue + wardrobe variety. Nothing built, nothing committed.

Harness: [`scripts/native/band-closeup-capture.py`](../../../../scripts/native/band-closeup-capture.py)
(prebuilt `native/build-native/rb3-native`). Venue-mapping probe used here:
`/tmp/venue_map_probe.py` (boots → gameplay → `{rb3_pos_dump}` + `VENUE_DBG=1` log
parse; sets `{meta_performer set_venue_override <v>}` early when forcing a venue).

---

## TL;DR — the single most important finding

**The native quickplay venue is FIXED at `small_club_01`, song-independent.** Every
`--song-downs` value (tested 0, 5, 10, 20, 40) loads `small_club_01`. This is NOT a
harness limitation — it is the source behavior: native quickplay never calls
`MetaPerformer::SelectRandomVenue` (`src/band3/meta_band/MetaPerformer.cpp:960`), so
`mVenue` stays empty and `BandDirector::EnterVenue`'s native bridge
(`src/system/bandobj/BandDirector.cpp:631-688`) force-loads the world's authored
`venue` prop, which defaults to `small_club_01`.

**To reach other venues you MUST set a venue override** — there is no env hook; use
the DTA bridge the EnterVenue code honors:
`{meta_performer set_venue_override <venue_sym>}` (set it EARLY, before gameplay
venue load, and re-assert across menu transitions — see the probe). This was
verified to load **arena_02, big_club_01, festival_01, video_01, small_club_03**
cleanly into gameplay (non-null `venueDir` + `wardrobe`, `OVERRIDE=<v>` echo).

**`arena_01` CRASHES natively** (SIGABRT, missing `rockarolla.1.mesh` in
`world/shared/props/arena/backwall_rockarolla_01.milo`) — DO NOT audit arena_01;
use **arena_02** as the arena representative.

**Wardrobe is randomized per boot, NOT per song** (no `srand`; libc default RNG
roll in `BandWardrobe` prefab selection). So footwear/outfit variety (incl. the
off-frame lowtopsneaks/saddleshoe chase) comes from **re-booting**, not from picking
a different `--song-downs`. Pool many boots; compare only boots where the target
mesh actually rolled. (Confirmed by `closeup-hunt.md`: drops_band swings 0↔4000
across boots of the same config.)

---

## 1. Venue families (asset surface)

`orig-assets/extracted/world/venue/<family>/<name>/gen/<name>.milo_xbox`

| Family | Sub-venues | Count | Pool (config/macros.dta) |
|---|---|---|---|
| **small_club** | 01,02,03,04,05,06,10,11,13,14,15 | 11 | SUBWAY_VENUES (default profile pool) |
| **big_club** | 01,02,04,05,06,07,08,09,10,11,12,13,14,15,17 | 15 | VAN_VENUES (+ subway) |
| **arena** | 01,02,03,04,06,07,10,11,12 | 9 | BUS_VENUES (+ van) — **arena_01 crashes natively** |
| **festival** | 01,02 | 2 | JET_VENUES (+ bus) |
| **video** | 01,02,03,04,05,06,07 | 7 | VENUES_VIDEO / video_venues (auto-vox / campaign key) |

44 sub-venues total. Default (no campaign progression) profile picks only from
`subway_venues` (small_club). Campaign progression (van/bus/jet) widens the random
pool; video venues need auto-vocals or the `video_venues` campaign key. The
override bypasses all of this.

---

## 2. Song → venue mapping (EMPIRICAL)

Venue does NOT vary by song in native quickplay. Confirmed boots:

| `--song-downs` | loaded venue (VENUE_DBG / POSDUMP) |
|---|---|
| 0 | small_club_01 |
| 5 | small_club_01 |
| 10 | small_club_01 |
| 20 | small_club_01 |
| 40 | small_club_01 |

86 songs are in `orig-assets/extracted/songs/` (genres: classicrock 19, rock 16,
alternative 13, poprock 10, metal 9, new_wave 8, punk/indierock 6 each, …). Song
choice changes the **chart + tempo + which member is featured**, but NOT the venue.
Different `--song-downs` are still useful for **animation variety** (different tempo
/ part → different limb motion → different shard exposure) and for re-rolling the
random wardrobe, but for VENUE variety you must override.

### Venue override verification (the reachable surface)

| override | result |
|---|---|
| (none) | small_club_01 (force-loaded fallback) — DEFAULT |
| small_club_03 | OK — gameplay reached |
| big_club_01 | OK — gameplay reached, OVERRIDE echoed |
| arena_02 | OK — venueDir + wardrobe non-null |
| festival_01 | OK — venueDir + wardrobe non-null |
| video_01 | OK — venueDir + wardrobe non-null |
| **arena_01** | **CRASH (SIGABRT)** — missing `rockarolla.1.mesh` |

---

## 3. Per-venue shot vocabulary (candidate shots, WITH `.shot` suffix at runtime)

Enumerated by `grep -rao 'coop_[a-zA-Z0-9_]*' <venue.milo_xbox>`. **Shot names are
venue-specific.** Note the harness appends `.shot` and keeps whatever returns
`force_shot ok`; `not_found` is skipped + recorded. `_ps3` variants omitted.

> **CRITICAL vocabulary split:** club/video venues have the RICH per-member set
> (`coop_g_cg`, `coop_g_n0*`, `coop_d_c0*`/`coop_d_n0*`, `coop_k_cg`, etc.).
> **arena_02 + festival_01 have a SPARSE per-member set** — only
> `coop_<m>_near`, `coop_<m>_closeup_head`, `coop_<m>_closeup_hand`,
> `coop_<m>_behind` (NO `_cg`/`_n0*` numbered shots). Audit agents for arena/festival
> MUST pass `--shots` with the `_near`/`_closeup_*` names or the default club shots
> all `not_found`.

### small_club_01 (DEFAULT — no override needed)
- guitar: `coop_g_cg.shot`, `coop_g_cg01.shot`, `coop_g_n01.shot` (run live;
  numbered `coop_g_n0*` resolve), `coop_g_closeup_head.shot`, `coop_g_closeup_hand.shot`
- drums: `coop_d_cd.shot`, `coop_d_cd01.shot`, `coop_d_closeup_head.shot`
- keys: `coop_k_ck.shot`, `coop_k_closeup_head.shot`
- vocals/front: `coop_front_n00.shot`, `coop_front_n01.shot`
- crowd: `coop_dir_crowd.shot`, `coop_dir_crowdg.shot`, `coop_dir_crowdb.shot`
- wide/all: `coop_all_near.shot`, `coop_all_far.shot`, `coop_all_n00.shot`, `coop_all_behind.shot`

### big_club_01 (override: `big_club_01`)
- guitar: `coop_g_cg.shot`, `coop_g_cg01.shot`, `coop_g_n01.shot`, `coop_g_n02.shot`, `coop_g_closeup_head.shot`
- drums: `coop_d_c01.shot`, `coop_d_c02.shot`, `coop_d_ch00.shot`, `coop_d_closeup_head.shot`
- keys: `coop_k_cg.shot`, `coop_k_cg01.shot`, `coop_k_closeup_head.shot`
- vocals/front: `coop_front_n00.shot`, `coop_front_n01.shot`, `coop_front_bg_n00.shot`
- crowd: `coop_dir_crowd.shot`, `coop_dir_crowdg.shot`, `coop_dir_crowdg01.shot`, `coop_dir_crowdb.shot`
- wide/all: `coop_all_near.shot`, `coop_all_far.shot`, `coop_all_n00.shot`, `coop_all_behind.shot`

### arena_02 (override: `arena_02`) — SPARSE per-member set
- guitar: `coop_g_near.shot`, `coop_g_closeup_head.shot`, `coop_g_closeup_hand.shot`, `coop_g_behind.shot`
- drums: `coop_d_near.shot`, `coop_d_closeup_head.shot`, `coop_d_closeup_hand.shot`
- keys: `coop_k_near.shot`, `coop_k_closeup_head.shot`
- vocals: `coop_v_near.shot`, `coop_v_closeup.shot`, `coop_front_near.shot`
- crowd: `coop_dir_crowd.shot`, `coop_dir_crowd01.shot`, `coop_dir_crowdg.shot`, `coop_dir_crowdb.shot`
- wide/all: `coop_all_near.shot`, `coop_all_far.shot`, `coop_all_behind.shot`
- (extra band-wide framings unique to arena: `coop_bk_fs_all_n00.shot`, `coop_arena_bdgv.shot`)

### festival_01 (override: `festival_01`) — SPARSE per-member, MASS crowd
- guitar: `coop_g_near.shot`, `coop_g_closeup_head.shot`, `coop_g_closeup_hand.shot`
- drums: `coop_d_near.shot`, `coop_d_closeup_head.shot`
- keys: `coop_k_near.shot`, `coop_k_closeup_head.shot`
- vocals/front: `coop_front_n00.shot`, `coop_front_n01.shot`, `coop_front_b00.shot`
- crowd: `coop_dir_crowd00.shot`, `coop_dir_crowdb.shot`, `coop_crowd_mass01_screenmask.shot` (huge festival audience — crowd-shard stress)
- wide/all: `coop_all_near.shot`, `coop_all_far.shot`, `coop_all_n00.shot`, `coop_all_behind.shot`

### video_01 (override: `video_01`) — RICH set + multi-member combos
- guitar: `coop_g_cg.shot`, `coop_g_cg01.shot`, `coop_g_n01.shot`, `coop_g_n04.shot`, `coop_g_closeup_head.shot`
- drums: `coop_d_c.shot`, `coop_d_c01.shot`, `coop_d_c02.shot`, `coop_d_closeup_head.shot`
- keys: `coop_k_cg00.shot`, `coop_k_cg01.shot`, `coop_k_closeup_head.shot`
- vocals: `coop_v_c.shot`, `coop_v_c01.shot`, `coop_front_near.shot`
- crowd: `coop_dir_crowd.shot`, `coop_dir_crowdg.shot`, `coop_dir_crowdb.shot`
- wide/all: `coop_all_near.shot`, `coop_all_far.shot`, `coop_all_behind.shot`

---

## 4. Three audit groups (maximize venue + wardrobe + footwear variety)

All groups: `MILO_HEADLESS=1 SHARD_DBG=1 SHARD_RATIO_DBG=1`, guard ON for the
verdict + a guard-OFF pass at a matched `--anchor-ms` for the visible-residual A/B.
Non-default venues require the `set_venue_override` step (copy from
`/tmp/venue_map_probe.py` — set early, reassert in song_select). small_club is
reachable with the bare harness (no override). **Re-boot each (song-downs) 3-5×**
to sweep the random wardrobe; record which boots rolled the target mesh.

### Group A — DEFAULT small_club_01 + FOOTWEAR chase (no override; rich club shots)
- `--song-downs`: **0, 2, 4, 6, 8** (the closeup-hunt sweep — sd4/sd8 are where
  thin-skin footwear was seen to roll + drop).
- Venue: small_club_01 (native default — no override).
- Members: guitar (primary, `_cg` set), drums, bass.
- Shots: `coop_g_cg.shot,coop_g_cg01.shot,coop_g_n01.shot,coop_d_cd.shot,coop_dir_crowdg.shot,coop_all_far.shot`
- Notes: chase lowtopsneaks_skin.2 / saddleshoe_skin.2 (off-frame footwear shard) —
  re-boot until a thin-skin shoe rolls (random wardrobe). This is the only group
  needing no override; spend the budget on boot-count for the wardrobe roll. Also
  re-confirm the clean bill on visible upper-body band closeups.

### Group B — big_club_01 + video_01 (override; rich `_cg`/`_n0*` shots, new wardrobes)
- `--song-downs`: **0, 10, 25** (vary animation/featured-member).
- Venues: big_club_01, video_01 (two distinct visual environments + lighting).
- Members: guitar, keys, drums, vocals.
- Shots (big_club): `coop_g_cg.shot,coop_g_n01.shot,coop_k_cg.shot,coop_d_c01.shot,coop_dir_crowdg.shot,coop_all_near.shot`
- Shots (video): `coop_g_cg.shot,coop_g_n04.shot,coop_k_cg00.shot,coop_v_c.shot,coop_dir_crowdb.shot,coop_all_far.shot`
- Notes: these venues have the richest closeup vocabulary → best for VISIBLE
  upper-body band-garment convergence (jackets/arms/gloves/heads). video_01 has
  multi-member combo framings (`coop_bdgv_*`, `coop_bgv_*`) worth a wide pass for
  multi-character interaction shards.

### Group C — arena_02 + festival_01 (override; SPARSE `_near`/`_closeup` shots, crowd stress)
- `--song-downs`: **0, 15, 30**.
- Venues: arena_02 (NOT arena_01 — crashes), festival_01.
- Members: guitar, vocals, drums.
- Shots (arena_02): `coop_g_near.shot,coop_g_closeup_head.shot,coop_v_near.shot,coop_d_near.shot,coop_dir_crowd.shot,coop_all_near.shot`
- Shots (festival_01): `coop_g_near.shot,coop_g_closeup_head.shot,coop_front_n00.shot,coop_d_near.shot,coop_crowd_mass01_screenmask.shot,coop_all_far.shot`
- Notes: MUST use the sparse `_near`/`_closeup_*` shot names (no `_cg`/`_n0*` in
  these venues — the club defaults all `not_found`). Festival's `coop_crowd_mass*`
  shots frame a huge audience → best venue to stress the crowd/extras-servo shard
  family (`male_extras_*`, `clap.mesh`, `fist.mesh` per scout-residual b/c). Arena
  is the largest-stage / most-distant-band framing → checks band-at-distance + the
  arena backwall props.

---

## 5. Gotchas for the audit agents (carry forward)

1. **Venue override is mandatory for non-small_club venues** and there is NO env
   hook — use `{meta_performer set_venue_override <v>}` set EARLY (before EnterVenue)
   and RE-ASSERTED after each menu transition (see `/tmp/venue_map_probe.py`
   `to_gameplay`). The bare `band-closeup-capture.py` does not set it.
2. **arena_01 is a hard crash** (missing prop mesh) — use arena_02.
3. **Wardrobe is random per boot** — sweep by re-booting, not by `--song-downs`;
   pool boots and only compare ones where the target mesh rolled. Single-boot env
   A/B is unreliable (closeup-hunt §4 methodology note).
4. **Shot vocabulary is venue-specific** — club/video = rich `_cg`/`_n0*`;
   arena/festival = sparse `_near`/`_closeup_*`. Rely on the harness `force_shot ok`
   / `not_found:<name>` return; pass `--shots` overrides per the §3 lists.
5. **Engine log is BINARY** — parse `[SHARD_GUARD]`/`[SHARD_RATIO]`/`venue_dir`
   with `grep -a` or python binary-read; plain grep silently skips NUL-containing
   lines.
6. **Known residuals already rooted** (scout-residual): `scrollbar_bg.mesh` = UI
   scrollbar leaking into the 3D draw (71% of drops; guarded render already ≈
   retail); `male_extras_*`/`clap`/`fist` = crowd/extras servo-skeleton shards (no
   rebind exists for non-BandCharacters); band footwear `lowtopsneaks/saddleshoe
   _skin.2` = uncovered foot-rebind gap, off-frame in club closeups. Audit agents
   should confirm/deny these per venue and surface anything NEW (esp. in the
   never-audited big_club/arena/festival/video venues, which prior batches did not
   reach because the default boot pins small_club_01).
