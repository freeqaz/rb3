# W22-SWEEP — STATUS (Wave-23 ranked visual menu)

**Lane:** SWEEP (Wave 22). **Verdict:** COMPLETE — fresh native-vs-retail sweep captured on
build `81a81de5` (rb3-native Jul 8 12:27), all four canonical screens + ROI provenance. NO
source edits, NO fixes (scripts + docs only, per charter). This file IS the Wave-23 candidate
list.

## Method recap
- Captured native at 1280×720 (matches Wii GT resolution → direct overlay): main_hub,
  song_select (depths 0/8/16), part/diff select, gameplay (8-frame burst). Harnesses:
  `song-select-capture.py`, `keyboard-to-gameplay.py --diff hard --game-burst 8`, plus lane
  helpers (`evidence/scripts/`). All `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, pgid cleanup.
- Named world/skinned gaps via `uidump_query.py --roi` with `RB3_DRAWLOG_PROV=1`
  (`evidence/roi_song_select.txt`, `evidence/roi_main_hub.txt`).
- Exclusions applied (A7): ledger 72 default-ON rows, fixed `512a1bde`/`9a7c40eb`, memory
  don't-re-find (C8 skin / venue exposure / offline chrome), CLOSED hands-finger family,
  other-lane FOREARM-FLOAT + score-HUD/star-meter.

## Confirmation of prior-baseline fixes (2026-07-02 memory doc)
- **#1 hub grey ticker quad** — FIXED. Native main_hub shows no mid-screen opaque ticker panel;
  "NEXT MESSAGE (1/1)" text renders bottom-left as retail does. (`512a1bde`.)
- **#4/#5 song_select album-art overlap / header clip** — FIXED. Album-art panel sits top-right,
  list header clean, focused row yellow — matches retail `song_select_list`. (`9a7c40eb`/`512a1bde`.)
- **#2 score-HUD mid-screen / #3 star-meter single-pip** — STILL OPEN but OWNED BY LANE HUD;
  excluded here (native gameplay shows a plain white pill top-center, no filled star row).
- **#6 stray red vertical bar** — REFUTED as a discrete bug: ROI at [845,120,30,240] resolves to
  the `subway_station` backdrop / `back_border` mesh, not a floating quad. Dropped.

---

## RANKED WAVE-23 VISUAL MENU

Severity: HIGH = large/central/systematic · MED = noticeable localized · LOW = minor/edge.
Confidence: HIGH = clear native gap vs trustworthy Wii GT · MED = plausible, some GT/exposure-family
adjacency · LOW = no GT (internal-consistency judgment only).

### S1 — main_hub: mid-street animated pedestrian/crowd figures ABSENT
- **Severity: MED · Confidence: MED-HIGH**
- Retail hub (`yt_mhKNp9uAT48_menu_hub`) shows 3 silhouetted walking figures in the mid-street
  distance (center, ~x560-800/y470-620). Native renders ZERO skinned draws in that region — ROI
  `center_walkers`[560,460,260,180] is 100% static architecture (`city_backdrop`, `sidewalk_*`,
  `traffic_streak_*`, storefronts). The player band chars (player0-3) ARE present but in the
  FOREGROUND bottom band (y≈426-720), largely occluded by the menu/gamertag chrome bar.
- **Crop:** `evidence/compare/main_hub_native_vs_retail.png` (L=native, R=retail).
- **ROI provenance:** `evidence/roi_main_hub.txt` — no `owner=` in center-street; foreground
  chars = player0 greaserjacket / player1 hippyfringe / player2 escapeartist / player3 gloves.
- **Suggested first discriminator:** is the hub scene authored to include street-crowd char
  actors (Crowd/BandCrowd owner in the rockcity `.milo`)? Boot with `RB3_DRAWLOG_PROV=1`, dump
  ALL owners across the full hub frame; if no crowd actor exists in the native hub dir, it is a
  MISSING-ACTOR (scene-population) gap, not a placement one. NOTE possible overlap with the
  crowd-rebind family (`RB3_NO_CROWD_REBIND`) — check that first.

### S2 — main_hub: scene reads over-bright / washed vs retail's dark-night grade
- **Severity: MED · Confidence: LOW-MED (exposure-family adjacent)**
- Native hub backdrop is bright grey-green (daytime feel); neon signs (`neonsigns_tiger_tattoo`,
  `neonsigns_palace`, `red_neon`, `neon_bar` — all present, correct mats) read as OPAQUE bright
  cutouts rather than glow-on-dark. Retail is a dark night street where neon is the light source.
- **Caveat / honesty:** this shares the "over-bright venue" signature with the EXCLUDED venue-
  exposure family — but the hub is a distinct MENU scene (rockcity street), not the gameplay/
  song-select venue backdrop those flags target, so it is reported as a possibly-new instance.
  Coordinator should confirm whether the menu-hub grade is already covered by an existing default
  before charter.
- **Crop:** `evidence/compare/main_hub_native_vs_retail.png`.
- **Suggested first discriminator:** A/B the hub under `RB3_PP_OFF=1` and `RB3_UI_POST_GRADE_OFF=1`
  — if either restores a darker hub, it is grade-path, not a new bug.

### S3 — part/difficulty select: picker panel cramped bottom-left, runs off bottom edge
- **Severity: MED · Confidence: LOW (no retail GT for this screen)**
- Native `02_part_difficulty` / `04_choose_diff`: the "CHOOSE INSTRUMENT" / "CHOOSE DIFFICULTY"
  overshell panel is anchored bottom-left and the list ("...OOSE INSTRUMENT" header clipped;
  GUITAR/BASS/EASY/MEDIUM/HARD/EXPERT) runs off the bottom frame edge. The upper ~60% of the
  screen is empty washed backdrop. Layout looks unbalanced/mis-anchored.
- **No retail GT** exists (README "Still missing: difficulty/instrument-select screen") →
  confidence capped LOW; triaged on internal-consistency only.
- **Crop:** `evidence/native/gameplay/02_part_difficulty.png`, `04_choose_diff.png`.
- **Suggested first discriminator:** is the panel clipped by a resolution/safe-frame anchor
  (cf the RndFont CellDiff wide-atlas / UIList slot family), or is this the authored Wii layout?
  Compare the panel's authored `.milo` anchor vs the panel screen rect via `uidump`; if the
  authored anchor is centered but the drawn rect is bottom-left, it is an anchor-math gap.

### S4 — song_select/part-diff: player1 avatar preview crops off the right frame edge
- **Severity: LOW-MED · Confidence: LOW (likely authored + skin is C8-excluded)**
- ROI `right_edge_avatar`[1080,360,200,300] on song_select names the right-edge figure as
  **player1's full skinned body** in the `subwaystation` preview env: `head.mesh`,
  `hippyfringe_skin.2.mesh` (torso), `jumpsuitshorts_skin.2.mesh` (legs), `hands_naked.mesh`
  (R-hand/arm jutting to x≈1250), `modavengerboots_resource.mesh`, owner=`player1`. Head rect
  [1022.9, 231.3, 87.7, 145.6]. The avatar sits at the far-right edge, torso+hand cropped by the
  frame. On `part_difficulty` the same avatar appears headless-cropped at right.
- **Two overlapping exclusions:** (a) the pale skin tone (matColor 0.91/0.87/0.70) = C8 skin
  family — EXCLUDED; (b) `hands_naked.mesh` bones = the CLOSED hands-finger family — do NOT
  charter a hands fix. The only potentially-new axis is PLACEMENT (avatar too far right / cropped),
  but retail song_select also places the avatar on the right (dark), so this is likely authored.
- **Crop:** `evidence/compare/song_select_native_vs_retail.png`; `evidence/native/gameplay/04_choose_diff.png`.
- **ROI provenance:** `evidence/roi_song_select.txt` (draws #75-#145).
- **Suggested first discriminator:** compare the player1 preview root world-x on native vs the
  authored preview-cam framing; only pursue if the avatar root is measurably off-center vs the
  authored preview anchor AND not merely the dim-retail avatar made visible by the S2 over-bright
  grade. LOW priority — probably a non-bug once S2 grade is addressed.

### S5 — gameplay: now-bar / combo-multiplier ring renders plain, no lit "Nx" glow
- **Severity: LOW-MED · Confidence: LOW-MED**
- Native gameplay strikeline shows a plain silver/blue circular element at the now-bar center;
  retail shows a lit combo-multiplier ring with an "Nx" label and a bright glow (e.g. "4x" in
  `gameplay_guitar`). Native "4x"-style multiplier text/glow not observed in the burst.
- **Caveat:** captured at low score/early combo, so the multiplier may simply not have ramped;
  and highway/gem glow overlaps the shipped bloom/track-light defaults. Needs a driven-combo
  capture to confirm presence/absence.
- **Crop:** `evidence/compare/gameplay_native_vs_retail.png`.
- **Suggested first discriminator:** drive autohit to a sustained combo (≥4x) via the game-burst
  harness, ROI the now-bar center, and check for the multiplier-ring mesh/label draw; absence
  under a confirmed ≥4x multiplier = a real HUD gap (hand to Lane HUD, adjacent to their family).

---

## Items INSPECTED and EXCLUDED (not re-reported, for auditor completeness)
- Gameplay venue backdrop over-bright + pink/magenta wash (burst_06): venue exposure / PP grade
  family (memory don't-re-find; `RB3_VENUE_LIGHT`/`RB3_PP_CHROMA_PRESERVE`/`RB3_TRACK_LIGHT`).
- Band-char pale/washed skin + torn hand geometry (gameplay + menu avatars): C8 skin family +
  CLOSED hands-finger family.
- Score-HUD mid-screen + single-outline star meter: Lane HUD owns (Wave-22).
- Player3 right-forearm float: Lane FOREARM owns (Wave-22).
- Missing top-right friends/leaderboard panel + gamertag ("Player 1" vs a real name): offline
  chrome (intentional).
- Hub menu list showing PLAY NOW submenu expanded (QUICKPLAY/START A ROAD CHALLENGE) vs retail's
  collapsed list: different menu STATE, not a bug.

## Evidence index
- `evidence/native/` — raw native captures (main_hub, song_select ×3, gameplay ×16).
- `evidence/compare/` — retail-vs-native side-by-side crops (L=native, R=retail).
- `evidence/roi_song_select.txt`, `evidence/roi_main_hub.txt` — uidump ROI provenance.
- `evidence/scripts/` — lane capture/ROI helpers.
- Checkpoint: `/tmp/wave22-checkpoints/SWEEP.json`.

## Honest confidence note
The two highest-value NEW candidates (S1 hub crowd, S3 part-diff panel) are both MED-or-lower
confidence: S1 because the retail hub is a 360/PS3 shot (layout-valid but the mid-street crowd
could be an intro-anim state), S3 because no retail GT exists for the difficulty screen. S2/S4
carry real exclusion-family adjacency and should be grade-checked before any charter. This is a
thinner menu than Wave-22's FOREARM/HUD because the 2026-07-02 backlog's top items either
shipped (`512a1bde`/`9a7c40eb`) or are owned by sibling lanes — the remaining surface is genuinely
smaller.
