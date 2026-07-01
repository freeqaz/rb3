# Audit — Group B: big_club_01 + video_01 (rich closeups, new venues)

**Audit Agent 1 (Opus). Research only — nothing built, nothing committed.**
First-ever audit of the `big_club_01` and `video_01` venues (every prior batch
pinned the default `small_club_01`). Used the prebuilt
`native/build-native/rb3-native` (115,998,568 bytes) via a venue-override wrapper
around `scripts/native/band-closeup-capture.py` (the harness itself does NOT set a
venue override; see Method).

Screenshots: `docs/native/converge-2026-06-20/shots/audit/` (named
`Group B …_<venue>_<shot>[_GUARDON|_GUARDOFF].png`; full per-run sets in the
`run_*` subdirs).

---

## TL;DR

- **Both new venues load cleanly under the override and the camera pin is rock-solid
  (pinned=K/K on every run, 64/64 frames total).** Every Group-B shot resolves
  except a couple of wide-shot names (noted below).
- **Upper-body band closeups get a CLEAN BILL on both venues.** Across all 6
  guard-ON song runs, the ONLY band-classified mesh that ever exceeds the shard cap
  is **band FOOTWEAR** (`lowtopsneaks_skin.2` / `saddleshoe_skin.2`). No band
  jacket / arm / head / glove / torso mesh is ever dropped — they stay within the
  relaxed band caps. The jackets/chains/heads/hands render with good detail.
- **The band-footwear thin-skin drop (the prior batches' "highest real-bug value"
  residual) REPRODUCES on `big_club_01`** — and harder than on small_club:
  `lowtopsneaks_skin.2` (sd0, ratio 4.9, 265 drops) and `saddleshoe_skin.2` (sd25,
  ratio 4.41, 3458 drops). **`video_01` did NOT reproduce it** in 6 boots (thin-skin
  shoes rolled but stayed under cap, ratio ≤3.17) — a real venue/animation
  difference. Same uncovered gap: NO_SKEL/NO_INST do not fix it (confirmed).
- **Still off-frame.** Even in big_club's `coop_all_n00` wide and the crowd shots,
  the exploded shoe sits at foot height at the dim, partly-occluded stage base —
  not closeup-visible. The prior caveat holds for the new venues. **Low ROI.**
- **All non-band drops are the already-rooted families** (scrollbar UI leak +
  crowd/extras servo shards). big_club spawns NEW extras instances
  (`male_extras04` → `eyebrows.mesh`, `male_extras10` → `male_extras_eyebrows10_low.mesh`)
  — new mesh NAMES, but the SAME crowd-servo root cause, not a new bug.
- **One NEW observation worth flagging (not a shard drop):** the venue **crowd /
  audience renders as flat stark-WHITE silhouettes** in big_club_01 (visible guard
  ON *and* OFF, so it is NOT a masked shard — it is how the crowd is lit/shaded).
  vs retail (dim, dark audience). Hypothesis: crowd material/lighting, not the skin
  family. See Gap G3.

---

## Method (venue override + pin)

The map (`audit/map.md` §5.1) requires `{meta_performer set_venue_override <v>}` set
EARLY (before EnterVenue) and re-asserted across menu transitions; the bare harness
only reaches small_club_01. I wrapped the harness in `/tmp/groupB_capture.py`, which
reuses `keyboard-to-gameplay`'s nav with the override injection from
`/tmp/venue_map_probe.py`, then calls `band-closeup-capture`'s own
`force_shot`/`director_disable`/`cur_shot`/`parse_shard_log` functions directly — so
the verdict, binary-log shard parse, and PNG/manifest output are identical to the
real harness. Confirmed the override took: the engine log shows
`VENUE_DBG: EnterVenue honoring MetaPerformer venue override='big_club_01'` /
`venueName='big_club_01'` (and `video_01`). (The wrapper's `venue=UNKNOWN` in the
verdict line is a cosmetic regex miss on the VENUE_DBG format — the venue is correct
in the log.) All runs: `SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1`, guard ON
for the verdict + matched guard-OFF / NO_SKEL / NO_INST A/B passes.

---

## Per-song verdict table (guard ON)

| run | venue | song-downs | pinned | drops_band | drops_other | max_band_ratio | band-drop mesh (skin.2) | verdict |
|---|---|---|---|---|---|---|---|---|
| bigclub_sd0  | big_club_01 | 0  | 10/10 | **265**  | 1750 | 4.17 | **lowtopsneaks_skin.2** (ratio→4.9) | FAIL |
| bigclub_sd10 | big_club_01 | 10 | 12/12 | 0        | 2610 | 3.39 | none (rigid footwear rolled) | PASS |
| bigclub_sd25 | big_club_01 | 25 | 12/12 | **3458** | 3854 | 4.41 | **saddleshoe_skin.2** (ratio→4.9) | FAIL |
| video_sd0    | video_01    | 0  | 10/10 | 0        | 637  | 2.54 | none (rigid footwear rolled) | PASS |
| video_sd10   | video_01    | 10 | 10/10 | 0        | 1405 | 2.73 | none (rolledjeans under cap) | PASS |
| video_sd25   | video_01    | 25 | 10/10 | 0        | 2401 | 2.90 | none (rolledjeans under cap) | PASS |

The FAIL verdicts are the footwear drop (Gap G1), wardrobe-roll-gated. The PASS runs
that show non-zero `drops_other` are all the rooted scrollbar/extras families (Gap G2).

### Shot resolution notes (skipped → venue mismatch)
- big_club_01: my listed `coop_all_near.shot` returns **not_found** (the `coop_all_*`
  *all-band* framings are not resolvable `BandCamShot` objects in this venue);
  **`coop_all_n00.shot` DOES resolve** — use it as the wide shot. Everything else in
  the Group-B big_club list resolves: `coop_g_cg/g_n01/k_cg/d_c01/dir_crowdg`.
- video_01: `coop_all_n00.shot` AND `coop_all_far.shot` both **not_found**; video_01's
  resolvable wide framings are the `coop_bdgv_*` / `coop_all_behind` family (the
  multi-member combos the map flagged). The rich per-member set all resolves:
  `coop_g_cg/g_n04/k_cg00/v_c/dir_crowdb`.

---

## Gaps found

### G1 — Band footwear thin-skin shard (REPRODUCES on big_club_01; off-frame)
- **What:** the thin `_skin.2` footwear submesh explodes under the foot/ankle/knee
  rotation-basis fling and the V24 guard band-drops it. bone0 z≈64–73 = foot height.
- **Repro:** big_club_01 — sd0 → `lowtopsneaks_skin.2.mesh` (265 drops, ratio peaks
  4.9); sd25 → `saddleshoe_skin.2.mesh` (3458 drops, ratio peaks 4.9). Drives over
  the 4.0 band cap. **Wardrobe-roll-gated** (random per boot) — sd10 rolled
  rigid-only footwear and PASSed.
- **Objective signal:** `[SHARD_GUARD] dropped … 'saddleshoe_skin.2.mesh' bindExt=11.21
  worldExt=55.46 ratio=4.9 … bone0=(-12.2,-19.0,63.9)`.
- **Visible?** **No** — off-frame. In the `coop_all_n00` wide
  (`…_bigclub_sd25_coop_all_n00_GUARDON.png`) the band feet are at the dim, partly
  highway-occluded stage base; in all per-member closeups feet are below frame. Same
  off-frame caveat the prior batch documented, now confirmed on big_club too.
- **Family:** char-skinning-deform (foot bones). **Uncovered by existing fixes** —
  `RB3_NO_SKEL_REBIND=1` and `RB3_NO_INST_REBIND=1` both leave the drop intact
  (verified: lowtopsneaks still 1989–2465 band-drops at ratio 4.27 with NO_SKEL).
  The torso rebind is hard-gated to trackjacket/vestdenim/plaidshirt/shred; InstStrings
  is guitar-only; nothing rebinds feet.
- **NEW datum vs prior research:** `video_01` does NOT reproduce the over-cap drop
  (6 boots, thin-skin shoes rolled at ratio ≤3.17, 0 drops). So the footwear fling is
  **venue/animation-dependent**, not universal — big_club's poses curl the shod limb
  harder than video_01's. **Verdict: real band geometry, fix-eligible, but LOW ROI
  (off-frame).** Bundle into a general foot-rebind, not a standalone fix.

### G2 — Non-band drops: scrollbar UI leak + crowd/extras servo shards (already rooted)
- **scrollbar_bg.mesh** — dominant non-band drop (383–2582 per run, both venues). The
  UI list-scrollbar widget leaking into the gameplay 3D draw (scout-residual (a)).
  Drop is accidentally-correct (retail draws no scrollbar in-venue). Confirmed
  present on the new venues; **not a new bug**.
- **Crowd/extras servo shards** — big_club spawns DIFFERENT extras than small_club,
  so NEW mesh names appear: `eyebrows.mesh` (dir `male_extras04`, ratio 4.7, 607
  drops) and `male_extras_eyebrows10_low.mesh` (dir `male_extras10`, ratio 4.3, 607
  drops), plus the familiar `male_extras_hair02`/`_eyebrows11`/`clap.mesh`. These are
  the **same non-band `Character`/servo-skeleton family** (scout-residual b/c) — no
  rebind exists for non-BandCharacters. New instances, same root cause. Visible as
  dark angular shards radiating from the crowd in the guard-OFF wide
  (`…_bigclub_sd25_coop_all_n00_GUARDOFF.png`); the guard suppresses them.
- **Family:** UI-draw-tree leak (scrollbar) + crowd-servo skin (extras). **Defer** —
  out of band-convergence scope, high blast-radius, low payoff. Confirmed on the new
  venues, no new root cause.

### G3 — NEW observation: crowd renders as flat WHITE silhouettes (big_club_01)
- **What:** the venue audience renders as stark, flat **white** cut-out silhouettes
  lining the stage (`…_bigclub_sd0_coop_dir_crowdg.png`, `…_bigclub_sd25_coop_all_n00_GUARDON.png`).
- **Visible?** **YES** — and it is present with the guard **ON** as well as OFF, so it
  is **NOT a masked shard** — it is how the crowd is shaded/lit. vs retail
  (`yt_qRagnZCIMzk_gameplay_guitar.png`): the small-club audience is dim/dark and
  unobtrusive. Native big_club crowd is conspicuously over-bright/flat-white.
- **Repro:** big_club_01 any sd, `coop_dir_crowdg` / `coop_all_n00` (crowd in frame).
  video_01's white *studio backdrop* is authored (the "video" venue look) and is
  correct — G3 is specifically the big_club *audience figures* reading as white.
- **Objective signal:** none from the shard log (not a drop) — this is a
  pixel/material observation, would need a lighting/material probe to quantify.
- **Family hypothesis:** crowd **lighting/material**, not the skin-deform or shard
  family. Possibly the crowd characters missing their lit material / falling back to
  an unlit/white shader under big_club's environ, or a crowd-LOD billboard rendering
  white. NOT a band mesh. **Flagged for a separate crowd-lighting investigation** —
  it is the most visually-divergent thing in the new-venue band/crowd framings, but
  it is crowd, not band geometry. Medium visible-impact, unknown tractability.

---

## Clean bill (what is GOOD on the new venues)

- **Band upper-body closeups converge.** Jackets (denim/chains), arms, heads/beanies,
  hands, gloves all render with detail and stay within shard caps — no band garment
  that is actually IN a closeup framing is ever dropped. The green-costume guitarist,
  the chain-denim keys member, the bikini/skirt vocalist all look right.
- **video_01 is the cleanest:** drops_band=0 in all 6 boots tested; the white-studio
  lighting gives the best-lit band characters of the batch
  (`…_video_sd0_coop_k_cg00.png`, `…_video_sd0_coop_g_cg.png`).
- **Camera pin is fully deterministic** on both new venues (64/64 frames pinned).
- **No missing band member, no missing instrument, no black/unlit band character, no
  garment holes** observed in any closeup on either venue.

---

## Reproduce

```bash
cd /home/free/code/milohax/rb3
# big_club_01 footwear repro (re-boot a few times — wardrobe is random per boot):
SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 /tmp/groupB_capture.py \
  --override big_club_01 --song-downs 25 \
  --shots "coop_g_cg.shot,coop_g_n01.shot,coop_k_cg.shot,coop_d_c01.shot,coop_dir_crowdg.shot,coop_all_n00.shot" \
  --frames 2 --out /tmp/bc_repro --tag repro
#  -> FAIL drops_band>0 closest=saddleshoe_skin.2 / lowtopsneaks_skin.2 when a thin shoe rolls; PASS otherwise.
# video_01 (clean band, drops_band=0 in tested boots):
SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 /tmp/groupB_capture.py \
  --override video_01 --song-downs 0 \
  --shots "coop_g_cg.shot,coop_g_n04.shot,coop_k_cg00.shot,coop_v_c.shot,coop_dir_crowdb.shot" \
  --frames 2 --out /tmp/vid_repro --tag repro
```
The wrapper is `/tmp/groupB_capture.py` (throwaway; reuses the prebuilt binary +
the tracked harness functions, no tracked code modified). Per-run PNGs + verdict.json
live in `docs/native/converge-2026-06-20/shots/audit/run_*`.

---

## Ranked findings for the impl batch

| rank | gap | venue/repro | signal | visible? | family | recommendation |
|---|---|---|---|---|---|---|
| 1 | **G3 crowd white silhouettes** | big_club_01, crowd shots | pixel (guard-independent) | **YES** | crowd lighting/material | **Investigate** — most visible new-venue divergence; needs a crowd-lighting probe to root-cause. Not band geometry. |
| 2 | **G1 band footwear shard** | big_club_01 sd0/sd25 | ratio 4.4–4.9, 265–3458 drops | no (off-frame) | char-skin (feet) | fix-eligible but **LOW ROI** (off-frame); bundle into a general foot-rebind. video_01 does NOT repro. |
| 3 | G2 scrollbar UI leak | both venues | 383–2582 drops | no (drop ≈ retail) | UI-draw-tree leak | defer (rooted; accidentally-correct) |
| 4 | G2 crowd/extras servo shards | both venues | ratio 4.3–4.7 | no (guard masks) | crowd-servo skin | defer (rooted; high blast-radius) |

**Bottom line:** Group B is largely a clean group for *band* convergence on the two
new venues — upper-body closeups converge, the only band residual is the off-frame
footwear (which reproduces on big_club but not video_01). The one genuinely NEW,
visible item the new venues surface is the **big_club crowd rendering as flat white
silhouettes (G3)** — a crowd lighting/material question, distinct from the shard
family, worth a dedicated probe.
