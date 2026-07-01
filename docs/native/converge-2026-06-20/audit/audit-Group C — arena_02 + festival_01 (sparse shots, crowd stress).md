# Audit — Group C: arena_02 + festival_01 (sparse shots, crowd stress)

**Audit Agent 2 (Opus). Research only — no code/engine changes, nothing committed.**
First-ever audit of arena_02 + festival_01 (these venues were never reached by prior
batches, which pin small_club_01 on the default boot).

Harness: [`scripts/native/band-closeup-capture.py`](../../../../scripts/native/band-closeup-capture.py)
via a thin /tmp override wrapper (`/tmp/bch_override.py`) that injects
`{meta_performer set_venue_override <v>}` during nav — the harness itself does NOT
set an override and these venues require one. Binary: prebuilt
`native/build-native/rb3-native` (NOT rebuilt). All runs `SHARD_DBG=1
SHARD_RATIO_DBG=1 MILO_HEADLESS=1`, guard ON unless noted.

Frames: `docs/native/converge-2026-06-20/shots/audit/Group C — …_*.png`.

---

## TL;DR

- **Venue override works for both venues** (`EnterVenue venue='arena_02'` /
  `'festival_01'`, `honoring override`), `pinned=N/N` on every run — the pin is solid.
- **HARNESS/MAP GOTCHA (important, see §1):** the map.md §3 shot lists for
  arena_02/festival_01 (`coop_g_near`, `coop_v_near`, `coop_d_near`,
  `coop_all_near`, `coop_all_far`) are **WRONG** — they grep as substrings in the
  milo but do NOT resolve as `BandCamShot` objects (`force_shot not_found`). The
  REAL resolvable names use a `coop_fs_<member>_*` prefix (`coop_fs_g_n01`,
  `coop_fs_v_n01`, `coop_fs_g_c01`, `coop_fs_all_n00`) and `coop_bs_d_*` for the
  drummer. I corrected the shot list and got 6/6 (arena) and 5/5 (festival) shots
  to resolve + pin.
- **Band CLOSEUPS are CLEAN in both venues.** Every upper-body band garment that is
  actually IN a closeup framing (jackets, arms, heads, gloves, guitar) renders
  coherent; no shards. Guard ON vs OFF guitar closeup is visually identical
  (drops were all off-frame).
- **ONE band-classified convergence gap reproduces here = the FOOTWEAR thin-skin
  family** (`saddleshoe_skin.2`, `lowtopsneaks_skin.2`, `maleslipons2_skin.2`).
  This is the SAME gap closeup-hunt already rooted — NOT new, NOT venue-specific,
  wardrobe-gated (random per boot), and OFF-FRAME in every shot these venues cut to.
- **NEW venue-specific observations (not band-skin):**
  1. **arena_02 is severely UNDERLIT** — band members render as near-black
     silhouettes (mean luma 26–56 vs festival's 78–200); this is NOT a guard artifact
     (guard ON/OFF luma 45.7 vs 49.1). Candidate venue-lighting gap. (§4)
  2. **festival `coop_crowd_mass*_screenmask` shots render a near-WHITE blank
     frame** (mean luma 200) — the mass-crowd that the shot frames is replaced by a
     flat white field. Candidate `screenmask` overlay / crowd-render gap. (§5)
- Crowd/extras servo shards (`male_extras_*`, `eyebrows.mesh`, `*_head`, `fist`,
  `clap`-family) are present and dropped, exactly as scout-residual (b)/(c)
  predicted — festival's mass crowd amplifies the count. All correctly classified
  `other`, off in the background vignette. Not new. (§6)

---

## 1. Shot-vocabulary correction (carry forward to map.md)

The map's claimed sparse `_near`/`_closeup_head`/`_near` set does NOT resolve.
On-device `force_shot` results (override=arena_02, live):

```
coop_g_near.shot        -> not_found      coop_fs_g_n01.shot   -> OK
coop_g_closeup_head.shot-> not_found      coop_fs_g_c01.shot   -> OK
coop_v_near.shot        -> not_found      coop_fs_v_n01.shot   -> OK
coop_d_near.shot        -> not_found      coop_bs_d_n01.shot   -> OK
coop_all_near.shot      -> not_found      coop_fs_all_n00.shot -> OK
coop_dir_crowd.shot     -> OK (the one map name that resolved)
```

The grep-token vs resolvable-object mismatch is the same trap the impl doc hit with
the arena_01 `coop_dir_*_cls` selector names. **The resolvable per-member closeup
family in both venues is `coop_fs_<member>_<n/c><NN>`** (`fs` = "full stage"); the
drummer is `coop_bs_d_*` (behind-set). festival_01 additionally has NO
`coop_fs_all_n00` (its wide is different) and `coop_all_far`/`coop_all_near` are not
Findable; use `coop_dir_crowd00`/`coop_dir_crowdb` for crowd and `coop_fs_v_*` etc.

---

## 2. Per-run verdict table

All runs guard ON, `--frames 2 --frame-dt 600` unless noted. Override set + reasserted
across menu transitions.

| run | venue | song-downs | shots res. | pinned | verdict | drops_band | drops_other | max_band_ratio | closest_band_to_cap |
|---|---|---|---|---|---|---|---|---|---|
| arena02_sd0  | arena_02    | 0  | 6/6 | 12/12 | **PASS** | 0    | 1065 | 2.99 | maleslipons2_skin.2:2.99 |
| arena02_sd15 | arena_02    | 15 | 6/6 | 12/12 | **PASS** | 0    | 2213 | 6.08 | gloves_resource.1:2.74 |
| arena02_sd30 | arena_02    | 30 | 6/6 | 12/12 | **FAIL** | 5931 | 3346 | 4.85 | **lowtopsneaks_skin.2:4.00** |
| fest01_sd0   | festival_01 | 0  | 5/6¹| 10/10 | **PASS** | 0    | 1688 | 5.81 | rolledjeans_skin.2:3.19 |
| fest01_sd15  | festival_01 | 15 | 5/6¹| 10/10 | **FAIL** | 3852 | 3009 | 5.20 | **lowtopsneaks_skin.2:3.97** |
| fest01_sd30  | festival_01 | 30 | —²  | —     | (see note) | — | — | — | — |
| festcrowd    | festival_01 | 0  | 3/4 | 3/3   | **FAIL** | 30   | 1901 | 4.61 | **maleslipons2_skin.2:3.83** |

¹ `coop_all_far`/`coop_fs_all_n00` not_found in festival_01 → wide shot skipped.
² fest01_sd30 was folded into the festcrowd boot (a sd0 boot that rolled a thin-skin
  shoe) — both reproduce the same footwear FAIL; re-running sd30 explicitly would
  only re-roll the random wardrobe (closeup-hunt §4 confound).

**The drops_band swings 0 → 5931 across boots of the SAME venue purely on whether the
random wardrobe rolled a thin-skin `_skin.2` shoe** — exactly the wardrobe confound
the map + closeup-hunt §4 warned about. PASS boots rolled rigid `_resource`-only
footwear (modavengerboots, talldocs, nailboots, kissboots — ratio ≤2.83, no drops).

---

## 3. Band-classified drop = FOOTWEAR thin-skin (not new; reproduces here)

The ONLY band-classified drops in the whole group are the thin-skin footwear family.
From the binary SHARD_DBG log (arena02_sd30, band-set classified):

```
 5570  saddleshoe_skin.2.mesh        (bindExt 11.2, worldExt 44-60, ratio 4.0-4.85)
  378  lowtopsneaks_skin.2.mesh      (ratio 4.0, at the band cap)
    6  saddleshoe_resource.mesh      (the rigid companion, mostly under cap)
```
festival adds `maleslipons2_skin.2.mesh` (ratio 3.83, 30 drops) — same rig.

This is **identical to closeup-hunt §2/§7**: `_skin.2` shoe submeshes bound to
`bone_{L,R}-ankle/toe/knee` fling under the rotation-basis error; no rebind covers
feet (`RebindOutfitBonesToOwnSkeleton` is torso-only;
`RebindInstStringsToRestBasis` is guitar-only). **Confirmed it is NOT venue-specific:
it reproduces in arena_02 and festival_01 exactly as in small_club, gated only by the
random wardrobe roll.**

### Visible? NO — off-frame in every shot these venues cut to.
- Guard ON vs OFF guitar closeup (`coop_fs_g_c01`, matched `--anchor-ms 22000`) is
  visually identical (`_arena02_g_closeup_guardON.png` / `_guardOFF.png`); the
  guitarist (jacket/head/arm/guitar) is coherent, feet below frame.
- The widest framing that resolves (`coop_fs_all_n00`, arena) puts the band tiny +
  distant on a dark stage; feet are at the highway base / below frame
  (`_arena02_wide_guardOFF.png`).
- This matches closeup-hunt's honest caveat: **real geometry, but rarely
  closeup-visible** in the auto-director's vocabulary. **Fix-eligible but low ROI**
  — fold into a general foot-rebind / C8 pose-basis batch, not a standalone fix.

---

## 4. NEW: arena_02 is severely UNDERLIT (band = black silhouettes) — candidate venue-lighting gap

Mean-luma of arena_02 frames is 26–56 (`_arena02_sd0_v_near.png` 25.9,
`_arena02_sd0_all_wide.png` 31.7, `_crowd.png` 38.2, `_g_closeup.png` 47.4) vs
festival_01's 78–200. In the arena vocals-near + crowd + wide frames the band members
render as **near-black silhouettes** with essentially no fill/stage light; the crowd
shows as bright dotted LED-board bands but the stage floor and performers are black.

- **It is NOT the guard.** Guard ON vs OFF guitar closeup luma is 45.7 vs 49.1 (≈equal)
  — the darkness is the venue lighting itself, not dropped geometry.
- **Repro:** `--override arena_02 --song-downs 0` (or any), shots `coop_fs_v_n01`,
  `coop_dir_crowd`, `coop_fs_all_n00`. Deterministic (no wardrobe dependence).
- **Hypothesized family:** venue-lighting / RndEnviron under the venue `world.cam`
  (cf MEMORY A4 "venue-env" + "P4 venue lighting NOW DEFAULT-ON" — that work added a
  grey fallback only when an env is *unlit*; arena_02 may have an env that loads but
  whose lighting/exposure doesn't reach the band, or whose key/spot lights aren't
  being applied to the character draw under `RB3_VENUE_LIGHT` path). NOT a skin-deform
  bug. **Uncertain whether this diverges from retail** — RB3 arenas ARE dark with
  spotlit bands, so the *darkness* may be correct; the open question is whether the
  band should be receiving a stage spot/key that it currently isn't (pure-black
  silhouette is the suspicious part vs a spotlit-but-dim performer).
- **Signal:** luma 26–56 (arena) vs 78–200 (festival), guard-independent.
- **Visible:** YES (dominant — the whole stage/band is black in the closer shots).

> A/B to localize next: capture the same arena_02 shot with `RB3_VENUE_LIGHT_OFF=1`
> vs default and compare band-region luma; and compare against retail arena footage
> (no retail arena_02 screenshot is in `images/retail-screenshots/` — only
> club/generic gameplay — so ground-truth for arena lighting is currently missing).

---

## 5. NEW: festival `coop_crowd_mass*_screenmask` → near-WHITE blank frame — candidate screenmask/crowd-render gap

The festival shot the map called out as the crowd-stress framing
(`coop_crowd_mass01_screenmask`, and `coop_crowd_mass_screenmask`) renders as a
**flat near-white field** (mean luma 200.8) with only the HUD bar, star-power meter,
guitar neck + a hand visible — **the huge festival audience the shot is named for is
NOT visible**, replaced by white. (`_fest01_sd0_crowd_mass_screenmask.png`,
`_fest01_crowd_mass_screenmask.png`.)

By contrast the festival's DIRECT crowd shots `coop_dir_crowd00` / `coop_dir_crowdb`
DO render backdrop content — a stylized high-contrast B/W poster backdrop (band/face
graphics, sunburst rays, amp stacks), which is festival_01's intended comic/poster
aesthetic (`_fest01_dir_crowd00.png`, `_fest01_dir_crowdb.png`). So the venue's crowd
backdrop CAN render; only the `*_screenmask` framing goes white.

- **Hypothesized family:** the `screenmask` overlay. The asset has
  `screenmask.shot` + `screenmask.trig` and 10+ `coop_crowd_mass##_screenmask.shot`
  variants — a designed fullscreen masked overlay. The near-white result suggests the
  screenmask quad is drawing **opaque white** (mask texture / alpha / blend mode not
  applied on native) instead of acting as a cutout that reveals the mass crowd. Could
  alternatively be an intended white screen-flash, but given it persists across both
  captured frames (not a 1-frame flash) and totally hides the crowd, an unmasked
  overlay is the more likely read. UI/material/overlay family, NOT skin-deform.
- **Repro:** `--override festival_01`, shots `coop_crowd_mass01_screenmask` or
  `coop_crowd_mass_screenmask`. Deterministic.
- **Signal:** mean luma ~200 (vs ~78 for the same venue's band/direct-crowd shots).
- **Visible:** YES (the entire crowd framing is white).
- **Uncertain:** whether retail shows the masked crowd or a white flash here — needs
  retail festival footage (not in `images/retail-screenshots/`).

---

## 6. Crowd / extras servo shards — confirmed, not new (scout-residual b/c)

festival_01's mass crowd amplifies the non-band servo-skeleton shard count. Dropped
`other`-class meshes (all correctly classified, off in the background vignette):

| mesh | owner (SHARD_DBG dir) | family |
|---|---|---|
| `eyebrows.mesh` (798) | `male_extras04` | extras servo (same family as `male_extras_eyebrows11`, different instance) |
| `male_extras_hair02.mesh` / `_eyebrows11/12/10_low` | male_extras02/11/12 | extras servo face/hair |
| `male_extra_head01/03.mesh`, `female_extra_head.mesh` | male/female extras | extras servo head |
| `male_extras_body02_medium`, `male/female_extras_skin0*` | extras | extras servo body |
| `fist.mesh` | crowd | crowd hand-prop (scout-residual c, fist sibling of clap) |
| `scrollbar_bg.mesh` (400–2788) | `scrollbar` (UI) | UI-in-3D leak (scout-residual a) — dominant `other` count, accidentally-correct drop |

`eyebrows.mesh` (the one not in prior tables) is **NOT a new band gap** — binary
SHARD_DBG confirms `dir='male_extras04'`, bone0 far from band (e.g. -256,312,67) =
background extra. It's the same crowd/extras servo family scout-residual (b) ranked
ACCEPT-for-now (high blast-radius, low payoff). festival just shows more extras so
more variants drop.

---

## 7. Gap list (every gap found)

| # | gap | repro (override/song-downs/shot) | objective signal | visible? | family | new? |
|---|---|---|---|---|---|---|
| C1 | **arena_02 underlit — band = black silhouettes** | arena_02 / any sd / `coop_fs_v_n01`,`coop_dir_crowd`,`coop_fs_all_n00` | mean luma 26–56 vs festival 78–200; guard-independent (45.7 vs 49.1 ON/OFF) | **YES (dominant)** | venue-lighting (RndEnviron/world.cam), NOT skin | **NEW** (venue never audited) |
| C2 | **festival `*_screenmask` crowd shot → white blank** | festival_01 / any sd / `coop_crowd_mass01_screenmask` | mean luma ~200 vs ~78 same venue; crowd absent | **YES** | screenmask overlay / crowd-render (UI/material), NOT skin | **NEW** |
| C3 | band FOOTWEAR thin-skin drop | arena_02 sd30 / festival sd15 / any shot (wardrobe-gated) | `saddleshoe_skin.2` ratio 4.0–4.85, `lowtopsneaks_skin.2` 4.0, `maleslipons2_skin.2` 3.83; drops_band up to 5931 | NO (off-frame; guard ON/OFF closeup identical) | skin-deform (foot rebind gap, C8) | not new (closeup-hunt §2) — but newly confirmed in arena+festival |
| C4 | crowd/extras servo face/head/hand shards (`eyebrows.mesh`, `male_extras_*`, `*_head`, `fist`) | festival_01 (mass crowd amplifies) | ratios 2.1–4.7, hundreds of drops; all `other`-class | barely (tiny, far, background) | crowd/extras servo skeleton (no rebind exists) | not new (scout-residual b/c) |
| C5 | `scrollbar_bg.mesh` UI leak | both venues, all runs | 400–2788 drops, ratio 4.0, dir=`scrollbar` | not visible WITH guard (drop ≈ retail) | UI-in-3D-scene leak | not new (scout-residual a) |

---

## 8. Clean-bill statement + priorities

**The band CLOSEUP render is CLEAN in both never-before-audited venues.** Across
arena_02 + festival_01, every band garment that is actually *in* a closeup framing
(jackets, arms, heads, gloves, guitar) renders coherent, and the V24 guard never
drops on-frame band geometry — guard ON vs OFF closeups are visually identical. The
only band-classified drops are the off-frame footwear family (C3) already rooted by
closeup-hunt.

The two NEW, genuinely venue-specific, **visible** gaps are NOT skin-deform:
1. **C1 — arena_02 underlighting** (band as black silhouettes; venue-lighting family).
   Highest visible impact in this group. Needs an `RB3_VENUE_LIGHT_OFF` A/B + retail
   arena ground-truth (currently missing from `images/retail-screenshots/`) to
   confirm divergence vs the intended dark-arena look.
2. **C2 — festival screenmask crowd shot rendering white** (screenmask overlay /
   crowd-render family). Hides the mass crowd the shot is built to show.

Both warrant follow-up by a render/lighting agent (not a skin-rebind agent). C3/C4/C5
are already-rooted, deferred families.

---

## 9. Reproduce

```bash
# wrapper (research-only) lives at /tmp/bch_override.py; it = band-closeup-capture.py
# + {meta_performer set_venue_override <v>} injected during nav.
cd /home/free/code/milohax/rb3

# arena_02 band closeups + crowd + wide (REAL resolvable shot names):
SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1 python3 /tmp/bch_override.py \
  --override arena_02 --song-downs 0 \
  --shots "coop_fs_g_n01,coop_fs_g_c01,coop_fs_v_n01,coop_bs_d_n01,coop_dir_crowd,coop_fs_all_n00" \
  --frames 2 --frame-dt 600 --out /tmp/bch_arena02 --tag arena02

# festival_01 (note: no coop_all_far / coop_fs_all_n00; use dir_crowd00 for crowd):
SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1 python3 /tmp/bch_override.py \
  --override festival_01 --song-downs 0 \
  --shots "coop_fs_g_n01,coop_fs_g_c01,coop_fs_v_n01,coop_bs_d_n01,coop_crowd_mass01_screenmask,coop_dir_crowd00" \
  --frames 2 --frame-dt 600 --out /tmp/bch_fest01 --tag fest01

# footwear FAIL is wardrobe-random per boot — re-run sd30 a few times to catch a
# thin-skin shoe (saddleshoe/lowtopsneaks/maleslipons2); PASS otherwise.
```
