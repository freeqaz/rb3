# Scout: Ground-Truth — what CONVERGENCE means for the V24 shard residuals

**Research only.** Read the probe data (`probe-data.md`), read the A/B screenshots
under `shots/`, read the retail references in `images/retail-screenshots/`, read the
SHARD_GUARD engine code + the scrollbar/extras assets. Nothing built, nothing run,
nothing modified.

This doc answers: for each residual the V24 guard silently drops — **what does the
real game show, and is the drop actually VISIBLE (a bug) or invisible (accept it)?**

---

## TL;DR ranking (detail + evidence below)

| Rank | Residual | Visible vs retail? | Verdict |
|---|---|---|---|
| — | **Deterministic band-closeup harness** | n/a (tooling) | **DO FIRST** — every visibility claim here is "unmatched-frame" soft until force_shot pins a frame. This is the gate. |
| 1 | **`lowtopsneaks_skin` / band footwear** | potentially visible in band closeups | **HIGHEST real-bug value** — but NOT reproduced this song (0 drops). Needs a song that drives the shoe's limb into a deep curl. Same skin-deform family as the strings fix → a genuine convergence target IF it manifests. |
| 2 | **`male_extras*` (vignette extras)** | marginal — background filler, intermittent | **LOW–MED.** Real geometry, but it's distant background "vignette" people that only the guard-OFF frames show poking in as pale blobs. Worth fixing only after (1). |
| 3 | **`clap.mesh` (crowd hand-prop)** | marginal — tiny held prop, flickers | **LOW.** ratio ~2.1, just over the 2.0x cap; a clapping-hand sliver. Cosmetic at best. |
| 4 | **`scrollbar_bg.mesh`** | **NOT visible — wrong scene entirely** | **ACCEPT / out-of-scope as a "deform" bug.** It is a *UI* scrollbar widget, not venue geometry. ratio is a rock-steady 4.0 (a stretched bar, not an explosion). Dropping it is harmless; the real question is why a UI mesh is submitted during the venue render at all (separate issue). |

The single most important deliverable for convergence is the **deterministic
harness** — without it, none of items 1–4 can be A/B'd honestly (see §0).

---

## 0. The harness is the real blocker (and the probe proved it)

Per `probe-data.md §1`: `{band_director force_shot …}` and
`{$band_director set disabled 1}` are **silent no-ops** in native (no DTA accessor;
`TheBandDirector` is a C++ global, not a name-resolvable DTA object). So the
auto-director keeps cutting every frame and **all A/B pairs are at DIFFERENT camera
angles** — file sizes all differ, and e.g. `guardon_coop_dir_g_cls00_A` vs `_B` are
two different shots.

**Why this poisons every visibility claim below:** the shard explosions are
*per-frame / pose-dependent and intermittent*. I directly observed this:
- `guardoff_coop_dir_g00_A.png` (guard OFF) and `guardon_coop_dir_g00_A.png` (guard
  ON) look essentially the **same** — both have the blue filigree highway, band at
  the sides, no obvious artifact. If the guard were suppressing something there,
  guard-OFF would show it. It doesn't, because the explosion didn't happen on
  *that* frame.
- `guardoff_coop_dir_g_cls00_A.png` (guard OFF) **does** show pale blob/shard
  intrusions at the left and right edges; `guardon_coop_dir_g_cls00_A.png` is a
  different, cleaner frame. Unmatched → can't attribute the difference to the guard
  vs to the camera move.

**Conclusion:** to measure any residual honestly you must pin one band-closeup shot
across the guard-ON/guard-OFF builds. The downstream harness builder needs the two
native-only DTA accessors the probe spec'd (`rb3_force_shot` + `rb3_director_disable`,
both required — `ForceShot` alone is re-overridden by `OnSelectCamera` unless
`mDisabled` is also set). **This is rank-0 work; do it before touching the meshes.**

---

## 1. Are the highway "filigree" frames the scrollbar? NO — that's the normal track.

The probe doc (`§6`) reads `guardoff_coop_dir_g00_A` as "the exploded scrollbar mesh
sprawled across the highway." **I believe that's a misread.** The large ornate
blue/teal filigree on the play surface is the **note highway's own decorative
texture / star-power art**, present in BOTH guard-ON and guard-OFF frames AND in the
retail references:

- Retail `gameplay_highway_wikipedia.jpg` — the 4-lane composed shot — shows the
  identical ornate blue filigree running down every lane. It is the track skin.
- Retail `yt_qRagnZCIMzk_gameplay_drums_starpower.png` — the same filigree, brighter
  (star power).
- In the probe shots it is present whether the guard drops `scrollbar_bg` or not, so
  it cannot BE `scrollbar_bg` (which the guard removes 607×).

So "filigree on the highway" is **correct rendering, not a residual.** Don't chase it.

### Where `scrollbar_bg.mesh` actually lives — it is UI, not venue
- `src/system/bandobj/ScrollbarDisplay.cpp` + `orig-assets/extracted/ui/ui_objects.dta:568-598`:
  `ScrollbarDisplay` (type `default` / `accomplishments`) loads
  `resource/scrollbar_display.milo` / `scrollbar_accomplishments.milo` and skins a
  bar between `scrollbar_bg_bone_top.mesh` / `scrollbar_bg_bone_bottom.mesh`.
- Asset roots: `ui/resource/gen/scrollbar_{display,accomplishments}.milo_xbox` (UI),
  referenced from `ui/ui_objects.dta`. **`allowed_dirs RndDir`** — it is a UI list
  widget used in menus / song-select / accomplishments panels, NOT a venue object.
- The mesh is a skinned bar **designed to stretch** between two bones to fill a list
  height. The probe measured ratio **4.0, rock-steady `[4.0-4.0]`** across 607
  frames — that is a *constant, deliberate* stretch, the opposite signature of a
  chaotic per-frame pose explosion (compare `male_extras` 2.4–2.6, footwear
  0.9–3.5). It is behaving exactly as a scrollbar should; the 2.0x guard simply has
  no concept of "a legitimately long UI bar."

**So `scrollbar_bg` is not a deform bug at all.** It is a UI mesh leaking into the
gameplay/venue draw pass (likely a residual song-select/HUD panel still in the dir
tree). It is **not visible** in any probe screenshot (no vertical bar appears in any
of the 12 frames I read, guard ON or OFF). Dropping it is harmless. CONVERGENCE here
means *don't draw the UI scrollbar during gameplay* — which it already (accidentally)
achieves. The proper fix is upstream (don't submit it), not in the shard guard.

---

## 2. `male_extras*` (venue "vignette" extras) — real, but background filler

- Assets: `char/extras/gen/male_extras*.milo_xbox` — background "extra" people the
  venue's **vignette** intro/outro sequence + ambient crowd uses
  (`world/camera_cats.dta:7` `vignette`; `world_objects.dta` vignette triggers).
- Probe drops: `male_extras_hair02` (62×, ratio 2.5, bone0 (-227,72,184)) and
  `male_extras_eyebrows11` (62×, ratio 4.7, bone0 (-164,**598**,-43)). The y=598
  height on eyebrows11 is far off the floor → that extra is genuinely mis-posed /
  flung (same skin-deform family). hair02 at ratio 2.5 is a milder over-cap.
- Visibility: I see **pale blob intrusions** consistent with flung extras in the
  guard-OFF closeups (`guardoff_coop_dir_g_cls00_A`, `guardoff_coop_dir_d_pnt_m00_A`)
  — and the matching guard-ON frames (`guardon_coop_dir_d_pnt_m00_A`) are clean. So
  the guard *is* suppressing exploded extras, and without it they'd be visible
  pale-shard garbage.
- Retail ground truth: in `gameplay_highway_wikipedia.jpg` the band is on stage with
  a dim venue behind; background "people" are not a prominent feature of the
  small-club gameplay framing. Extras are mostly a vignette/intro flourish, dimly
  lit, distant.

**Verdict:** real geometry that *should* render (an extra person's hair/eyebrows),
currently dropped because it's flung — but it's distant, dim, background filler that
only matters in occasional closeups. CONVERGENCE = rebind the extra's hair/eyebrows
to its own skeleton basis (the `RebindOutfitBonesToOwnSkeleton` pattern) so it poses
correctly instead of being dropped. **Medium-low priority** — fix after footwear.

---

## 3. `clap.mesh` (crowd hand-prop) — tiny sliver, lowest geometry value

- Asset: crowd character hand prop, owner `crowd_male03`
  (`char/crowd/gen/crowd_male03.milo_xbox`), bone0 (22,-3,57).
- Drops 128×, ratio ~2.1 [2.0–2.2] — *barely* over the strict 2.0x cap. Bind 51u →
  world 109u. This is a clapping-hands prop whose held pose just crosses the line.
- Visibility: a small held prop on a distant crowd member; even un-dropped it's a
  few pixels in the audience. The engine comment (`Rnd_Wgpu_RB3.cpp:5049`) already
  calls out "a few small held-prop slivers (lighter/clap, ratio <2.0) can still
  flicker for a frame" as the known residual.

**Verdict:** cosmetic. The convergence win is negligible (a crowd hand prop). Could
be addressed by a per-crowd-prop cap tweak, but **lowest priority** — accept for now.

---

## 4. `lowtopsneaks_skin` / band footwear — the genuine convergence target (when it fires)

- This is the only residual in the **band** skin-deform family (the same family as
  the landed strings fix `2f393eaa` and the outfit-bone rebind). Per `probe-data.md §5`
  it was **NOT dropped on the boot song** — `lowtopsneaks_skin.2.mesh` measured
  ratio 1.7 [0.9–3.5], 0 drops; every band garment stayed within the relaxed band
  caps (4.0x ratio / 110u world / 40u floor).
- So on this song there is **nothing to converge** — the band's shoes render fine.
  The prior batch's "shoe-skin dropped" report is **song/outfit dependent**: it needs
  a song whose wardrobe assigns the low-top sneakers AND an animation that curls the
  shod limb hard enough (or binds it to a wrong basis) to exceed the band caps.
- Retail ground truth on band footwear: the retail closeups
  (`fandom_gameplay_guitar.png`, `fandom_gameplay_drums.png`) frame the band member
  at **upper body** — gloves/arms/face are in shot, **feet/shoes are not** in the
  per-instrument closeups. The full-venue wide (`gameplay_highway_wikipedia.jpg`)
  shows full-body band but is distant/dim; shoes are present but tiny. So even when
  it fires, a dropped shoe is **only visible in a full-body or feet framing** (the
  drummer kick-pedal / `d_pnt`/`d_lt` shots, or a wide), not in the common
  upper-body member closeups.

**Verdict:** **highest *real-bug* value** of the four (band member, not background)
— a missing shoe on a band character is the kind of thing a player notices. But it
is **latent here**: convergence work requires first reproducing it (pick the right
song + a feet-revealing shot via the new harness), then applying the rebind/rebake
pattern. Until reproduced it cannot be A/B'd. **Prioritize the reproduction over the
fix.**

---

## 5. Direct screenshot evidence index (files I actually read)

Retail (`images/retail-screenshots/`):
- `gameplay_highway_wikipedia.jpg` — full venue, band on stage, **normal blue
  filigree on lanes** (proves filigree ≠ scrollbar), no scrollbar anywhere, bg
  people not prominent.
- `yt_qRagnZCIMzk_gameplay_{guitar,drums,drums_starpower}.png` — Wii highway; same
  filigree; small-club venue with band member visible at edge of drums shot.
- `fandom_gameplay_{guitar,drums}.png` — the **band-member closeups** the auto
  director cuts to (upper-body framing; gloves visible, no shoes); confirms band
  closeups are normal retail gameplay.

Probe A/B (`docs/native/converge-2026-06-20/shots/`):
- `guardoff_coop_dir_g00_A.png` vs `guardon_coop_dir_g00_A.png` — both clean +
  filigree (no scrollbar) → the §1 "scrollbar on highway" read is the normal track.
- `guardoff_coop_dir_g00_B.png` vs `guardon_coop_dir_g00_B.png` — near-identical,
  clean → scrollbar drop is invisible.
- `guardoff_coop_dir_g_cls00_A.png` — pale shard intrusions L+R edges (flung
  extras); `guardon_coop_dir_g_cls00_A.png` — different, cleaner frame (unmatched).
- `guardoff_coop_dir_d_pnt_m00_A.png` — pale blob bottom-center (flung extra);
  `guardon_coop_dir_d_pnt_m00_A.png` — clean band closeup.
- `guardon_coop_dir_b_cls00_A.png`, `guardoff_coop_dir_b_cls00_A.png` — band members
  near-camera; normal filigree; no scrollbar bar.
- `guardoff_coop_dir_{g_cls01,g_np_m00,b00,d_lt00}_A/B.png` — band at sides/closeup,
  no scrollbar, intermittent (mostly absent) shard artifacts.

Venue identified from the imagery: **small_club** (brick walls, dartboard, "COBY"/
"SMOOVE"/"SMOOVE" club signage, wooden chairs) — NOT arena_01 as the probe's shot
vocabulary assumed (the shot *names* `coop_dir_*` come from arena_01.milo but the
boot song loads the small club; the auto-director shot set is venue-driven).

---

## 6. What "CONVERGENCE" means, per residual (the contract for the impl batch)

- **scrollbar_bg** → converge = *do not draw a UI scrollbar in the gameplay venue*.
  Already achieved by the drop; the clean fix is upstream (stop submitting the UI
  mesh), not a shard-guard change. **No visible bug; accept the drop / fix upstream
  later.**
- **clap.mesh** → converge = the clapping crowd hand renders. Tiny/distant.
  **Accept for now.**
- **male_extras** → converge = the background extra poses correctly (rebind to own
  skeleton) instead of exploding+dropping. **Medium-low.**
- **lowtopsneaks (band footwear)** → converge = the band member's shoe renders
  correctly in a feet/full-body shot. **Highest real value, but latent — must
  reproduce first.**
- **harness** → converge-enabler = pin a band-closeup shot deterministically so all
  of the above can be honestly A/B'd. **Do this first.**
