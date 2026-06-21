# Audit — Group A: small_club_01 (default venue) + footwear chase

**Audit agent 0 (Opus). Research only — measured, read, compared. ZERO code/engine
changes, nothing committed.** Used the prebuilt `native/build-native/rb3-native`
(115,998,568 bytes, built Jun 21 00:41) via
[`scripts/native/band-closeup-capture.py`](../../../../scripts/native/band-closeup-capture.py).

Group: `small_club_01` (native default, no venue override), guitarist closeups +
crowd + wide. `--song-downs` 0, 2, 4, 6, 8. Shots forced:
`coop_g_cg, coop_g_cg01, coop_g_n01, coop_d_cd, coop_dir_crowdg` (+ `coop_all_far`
which does NOT resolve — see §4). Env: `SHARD_DBG=1 SHARD_RATIO_DBG=1 MILO_HEADLESS=1`,
guard ON; one `SHARD_GUARD_OFF=1` A/B on sd8.

Frames: [`../shots/audit/`](../shots/audit/) (prefix
`Group A — small_club_01 default + footwear chase_*`).

---

## TL;DR

- **The camera pin is rock-solid: `pinned=K/N` held on EVERY frame of EVERY run**
  (10/10 × 5 runs, plus the A/B/probe runs). The harness is trustworthy.
- **Visible upper-body band geometry gets a CLEAN BILL.** Across all 5 songs ×
  5 shots, no band garment that is actually IN a guitarist/drummer/crowd framing
  (jackets, arms, gloves, heads, masks, instruments) was ever dropped or looked
  exploded/missing/mislit. The band renders coherently.
- **The ONE band-classified convergence gap is band FOOTWEAR** — thin `_skin.2`
  shoe submeshes fling past the band cap and the V24 guard drops them. I reproduced
  it on **3 of 5 song-downs** (sd4, sd6, sd8), each rolling a *different* shoe:
  `maleslipons2_skin.2` (sd4), `lowtopsneaks_skin.2` (sd6), `saddleshoe_skin.2` (sd8).
  Same family as the known gap; **`maleslipons2_skin.2` is NEW** (not in prior docs,
  which listed only saddleshoe + lowtopsneaks). All bind to foot/ankle/toe/knee bones,
  all `dir=''` (band character), all uncovered by any existing rebind.
- **ROI nuance, refined from closeup-hunt:** the footwear drop is OFF-FRAME in every
  guitarist/drummer/crowd CLOSEUP (highway fills the lower 2/3; feet below frame).
  BUT — contrary to closeup-hunt's "no wide/feet shot resolves" claim — the wide
  band framings **`coop_all_n00` / `coop_all_f00` / `coop_bftb_gf` DO resolve** and
  DO show full-body band incl. feet (see `_sd0_coop_all_n00_wide-fullband-feet.png`).
  In that wide the feet are small/dark/distant, so the dropped shoe is on-frame but
  **low-visibility**. Net: footwear is still LOW ROI, but slightly higher than
  "always off-frame" — it can appear in the wide shots the auto-director uses.
- **Non-band residuals reproduce exactly as scout-residual predicted** (all `other`,
  all guard-dropped, none a band-geometry bug): `scrollbar_bg.mesh` (UI scrollbar
  leak, dir=scrollbar, 497–1067/run — every boot, the dominant drop); crowd/extras
  servo shards `fist.mesh` / `clap.mesh` / **`horns.mesh` (NEW)** (dir=crowd_*),
  `male_extras_hair02` / `male_extras_eyebrows11` (dir=male_extras*). All distinct
  families, all already-rooted, out of scope for this group.
- **A harness/lighting caveat surfaced (NOT a render bug):** the wide multi-member
  shots captured late (~18–19 s) show a heavy RED/ORANGE wash + a "FAILED" overlay
  — that is the **song-failed state** (autohit doesn't perfectly sustain the song,
  so it drops to fail and the highway/stage turns the fail color). The clean
  closeups were captured pre-fail. Don't read the red wash as a venue-lighting gap.

---

## 1. Per-song verdict table (guard ON)

| song-downs | verdict | pinned | drops_band | band drop mesh (dir='') | max_band_ratio | closest band-to-cap | drops_other (by mesh) |
|---|---|---|---|---|---|---|---|
| **0** | PASS | 10/10 | **0** | — | 3.72 | wovensteppers_skin.2:3.72 | fist 711, scrollbar_bg 497, male_extras_hair02 90, male_extras_eyebrows11 90, clap 84 |
| **2** | PASS | 10/10 | **0** | — | 2.70 | rolledjeans_skin.2:2.70 | scrollbar_bg 603, male_extras_hair02 90, male_extras_eyebrows11 90 |
| **4** | **FAIL** | 10/10 | **107** | `maleslipons2_skin.2` (NEW) | 4.05 | maleslipons2_skin.2:3.99 | scrollbar_bg 836, male_extras_hair02 90, male_extras_eyebrows11 90 |
| **6** | **FAIL** | 10/10 | **721** | `lowtopsneaks_skin.2` | 5.07 | lowtopsneaks_skin.2:3.93 | scrollbar_bg 848, male_extras_hair02 91, male_extras_eyebrows11 91, horns 48 (NEW crowd) |
| **8** | **FAIL** | 10/10 | **2474** | `saddleshoe_skin.2` | 5.54 | saddleshoe_resource:3.97 | scrollbar_bg 1067, male_extras_hair02 92, male_extras_eyebrows11 92, clap 84 |

Note: `drops_band` is dominated by the random per-boot wardrobe roll. sd0/sd2 rolled
rigid-only / under-cap footwear (wovensteppers, rolledjeans) → 0 band drops. sd4/sd6/sd8
each rolled a different thin-`_skin.2` shoe → drop. The pin held on all of them.

### Drop-line evidence (guard ON, from the binary log)
```
[SHARD_GUARD] dropped ... mesh='saddleshoe_skin.2.mesh'  bindExt=11.21 worldExt=48.27 ratio=4.3 dir='' bone0=(-78.6,173.9,7.1)   # sd8, ankle/foot height
[SHARD_GUARD] dropped ... mesh='lowtopsneaks_skin.2.mesh' bindExt=11.20 worldExt=45.50 ratio=4.1 dir='' bone0=(-72.1,60.5,33.1)   # sd6
[SHARD_GUARD] dropped ... mesh='maleslipons2_skin.2.mesh' bindExt=13.70 worldExt=55.03 ratio=4.0 dir='' bone0=(-36.1,21.6,30.2)   # sd4  (NEW shoe)
[SHARD_GUARD] dropped ... mesh='horns.mesh'  bindExt=49.99 worldExt=101.11 ratio=2.0 dir='crowd_male03' bone0=(14.4,-2.8,43.3)    # sd6  (NEW crowd prop)
```
`dir=''` = band character; `bone0` y at foot/ankle height. The thin `_skin.2`
submesh explodes; its rigid `_resource` companion stays under cap (saddleshoe_resource
peaked 3.97, just under). This is the rotation-basis fling family (NOT a missing
asset, NOT a load failure) — identical signature to the landed strings/outfit fixes.

---

## 2. Gaps found

### GAP A1 — band footwear `_skin.2` thin-skin shard (the headline)
- **What's wrong:** thin foot/shoe submeshes bound to `bone_{L,R}-ankle/toe/knee`
  blow their blended-world/bind AABB past the relaxed band cap (4.0×); the V24 guard
  drops them, so the shoe is missing for those frames (geometry-vs-retail gap).
- **Repro:** `--song-downs 4` (maleslipons2), `6` (lowtopsneaks), `8` (saddleshoe),
  guitarist closeups; random per-boot wardrobe gates WHICH shoe rolls.
- **Objective signal:** ratio 4.0–5.54; drops_band 107 / 721 / 2474; `dir=''`.
- **Visible?** OFF-FRAME in all club closeups (feet below the highway-dominated
  frame — see `_sd6_..._lowtops-dropped.png`, `_sd8_..._saddleshoe-dropped.png`,
  `_sd8_coop_d_cd.png`). On-frame but small/dark/distant in the wide
  `coop_all_n00`/`coop_all_f00` framings (`_sd0_coop_all_n00_wide-fullband-feet.png`).
  So: real geometry, low closeup-visibility, marginal wide-visibility.
- **Family:** char-skinning-deform (rotation-basis fling) on FEET bones.
- **Existing-fix coverage:** NONE. `RebindOutfitBonesToOwnSkeleton` is hard-gated to
  torso garments; `RebindInstStringsToRestBasis` is guitar-strings-only;
  `RebindHeadHandsAtRest` is head/hair. Neither `RB3_NO_SKEL_REBIND` nor
  `RB3_NO_INST_REBIND` touches feet (closeup-hunt §4 proved this exhaustively). This
  is a genuinely uncovered band-rebind gap.
- **New vs prior docs:** `maleslipons2_skin.2` is a THIRD footwear mesh in this
  family not previously reported. Watch-list (under cap this batch):
  `wovensteppers_skin.2` (3.32–3.72), `rolledjeans_skin.2` (2.70),
  `tightdistressedpants_skin.2`, `femaledestroyedchucks_resource` (3.46) — same rig,
  would join A1 on a higher-motion song.
- **Recommended fix family (for a LATER impl batch, not this research):** extend the
  band rebind to footwear, mirroring `RebindInstStringsToRestBasis` — a single-anchor
  rigid rebake of the `_skin.2` shoe to its least-moving foot bone (likely the ankle),
  baking `offset = meshWorld·inv(anchorWorld)` so the shoe rides the ankle rigidly
  (ratio→~1.0). MUST be gated behind a strict same-boot A/B: the engine's
  `RB3_GUARD_EXEMPT_REBOUND` note warns a translation-only anchor leaves the
  rotation-basis fling and can draw a worse slab — so the fix must demonstrably bring
  `max_band_ratio` under cap, not just exempt the mesh. **ROI: LOW** — defer, fold
  into a general foot / C8 pose-basis batch.

### Non-gaps confirmed (NOT band-geometry bugs — already rooted, out of scope)
- **`scrollbar_bg.mesh`** (dir=scrollbar): the UI list-scrollbar leaking into the
  gameplay 3D draw; every boot, 497–1067 drops (the dominant residual). NOT visible
  on any frame I captured (the guard drop ≈ retail, which draws no scrollbar in-venue).
  scout-residual (a): a UI-in-3D-scene draw-tree leak, not a deform bug.
- **Crowd/extras servo shards** (dir=crowd_* / male_extras*): `fist.mesh`,
  `clap.mesh`, **`horns.mesh` (NEW devil-horns hand-prop on crowd_male03, ratio 2.0)**,
  `male_extras_hair02`, `male_extras_eyebrows11`. Background audience servo-skeleton
  basis divergence; no rebind exists for non-`BandCharacter`s. scout-residual (b)+(c).
  Tiny, distant, dim — accept for now.

---

## 3. Footwear chase result (the assigned objective)
- **Reproduced cleanly on sd4/sd6/sd8** (3 distinct shoes), absent on sd0/sd2 (the
  boot rolled rigid/under-cap footwear). Confirms wardrobe is per-boot random, not
  per-song.
- **On-frame check:** NOT visible in any guitarist/drummer/crowd CLOSEUP (off-frame
  below highway). MARGINALLY on-frame (small/dark/distant) in the wide `coop_all_n00`
  — which DOES resolve (closeup-hunt under-reported this). So the gap can surface in
  the auto-director's wide cuts, but is never a prominent artifact.

## 4. Skipped / harness notes (carry forward)
- **`coop_all_far.shot` does NOT resolve** in the loaded small_club_01 scene
  (`force_shot not_found`), even though the name IS present in the venue milo
  string-table (grep confirms `coop_all_far`). The harness tried it (sent WITH the
  `.shot` suffix). So a milo string-table name ≠ a resolvable runtime `BandCamShot`
  via `wdir->Find` — likely lives under a sub-dir not searched, or no instantiated
  object. **Use `coop_all_n00` / `coop_all_f00` / `coop_bftb_gf` as the resolvable
  wide framings instead.** (Map agent's small_club_01 shot list lists `coop_all_far`
  as available — that is a milo-grep artifact; it does not resolve at runtime.)
- Also `not_found` at runtime: `coop_all_near`, `coop_all_behind` (despite being in
  the milo string-table). `coop_all_n00`, `coop_all_f00`, `coop_bftb_gf` DO resolve.
- **Song-fail wash:** wide shots captured late (~18–19 s) show a red/orange wash +
  "FAILED" overlay = the song-failed state (autohit lapse), NOT a lighting bug.
  Capture wide frames EARLY (low songMs, before fail) for clean lighting; the clock
  can't rewind below the arrival songMs (~16.6 s), so a pre-fail wide window is tight.
- **Lighting note (NOT a gap):** the small_club_01 gameplay lighting is dark/purple
  in closeups — this matches the Wii retail ground truth
  (`yt_qRagnZCIMzk_gameplay_guitar.png` is also a dark venue). The bright
  `fandom_gameplay_guitar.png` is the HD console build. Native ≈ Wii here; no
  convergence gap.

## 5. Bottom line
Group A is **CLEAN for all visible band closeup geometry** (upper body / instruments
/ heads / garments) across 5 songs × 5 shots, with a fully deterministic camera pin.
The single band-classified gap is **footwear `_skin.2` thin-skin fling** (3 distinct
shoes incl. one NEW, `maleslipons2_skin.2`), which is real geometry but LOW ROI
(off-frame in closeups, small/dark/distant in the resolvable wides). All other drops
are the already-rooted non-band families (UI scrollbar leak + crowd/extras servo
shards, incl. one NEW crowd prop `horns.mesh`). **No new band convergence bug; the
footwear gap is fix-eligible-but-defer, best folded into a general foot/C8 rebind.**
</content>
</invoke>
