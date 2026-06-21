# RANKED CONVERGENCE GAPS — master synthesis (converge-2026-06-20)

**Synthesis agent (Opus). Research only — no native runs, no code/engine changes,
nothing committed.** Merges + de-dupes every gap from the three audit groups
(`audit-Group A/B/C`), the venue/shot map (`map.md`), the scrollbar root-cause
(`scrollbar-rootcause.md`), and the prior scout docs into ONE ranked list. ROI =
user-visible severity × tractability.

Sources merged:
- `audit/audit-Group A — small_club_01 default + footwear chase.md` (sd0-8, default venue)
- `audit/audit-Group B — big_club_01 + video_01 (rich closeups, new venues).md`
- `audit/audit-Group C — arena_02 + festival_01 (sparse shots, crowd stress).md`
- `audit/scrollbar-rootcause.md` (root-caused + greenlit fix plan)
- `audit/map.md` (venue/song/shot coverage) + `scout-residual.md` (crowd/extras family)

---

## TL;DR — what to fix, what to defer

The audit gave **band closeup geometry a CLEAN BILL across all 5 venues** (small_club_01,
big_club_01, video_01, arena_02, festival_01) — no jacket/arm/head/glove/instrument
drop in any on-frame closeup, pin deterministic on 100% of frames. So there is NO new
band-skin convergence bug. The convergence frontier has moved OFF the skin-deform
family onto **two new venue-render gaps** (arena lighting, festival screenmask) plus a
**crowd lighting/material gap** — and the already-greenlit **scrollbar UI-leak cleanup**.

**Implement next (the batch):** #1 scrollbar UI leak (greenlit, root-caused, easy) +
#2 arena_02 underlighting (most visible, dominant whole-frame) + #3 big_club crowd
white-silhouette (visible, same crowd-lighting family as #2 — likely one investigation).
**Defer:** footwear thin-skin (real but off-frame), crowd/extras servo shards (masked,
high blast-radius), festival screenmask (needs retail ground-truth first).

---

## Ranked gap list

| rank | gap | severity | tractability | family | status |
|---|---|---|---|---|---|
| **1** | scrollbar_bg UI leak into 3D world | low (masked) | easy | UI-draw-tree | **GREENLIT — do it** |
| **2** | arena_02 band underlit → black silhouettes | high | hard | lighting | investigate→fix |
| **3** | big_club_01 crowd renders flat white | medium | medium | crowd/lighting | investigate→fix |
| **4** | festival `*_screenmask` shot → white blank | medium | medium | other (overlay) | needs ground-truth |
| **5** | band footwear `_skin.2` thin-skin fling | low | medium | skin-deform | defer |
| **6** | crowd/extras servo shards | low | hard | crowd | accept/defer |

---

### GAP 1 — scrollbar_bg / scrollbar UI leak into the gameplay 3D world  (GREENLIT)
- **Family:** UI-draw-tree.  **Severity: LOW** (the V24 guard already masks it — the
  highway matches retail TODAY).  **Tractability: EASY** (root-caused, fix sketched,
  name-scoped).
- **What:** the shared preloaded `ui/resource/scrollbar_display.milo` scrollbar dir
  (`scrollbar_bg.mesh` + `scrollbar.mesh`) is drawn in the venue **world camera pass**,
  interleaved with the gameplay band, sprawled as a 200u teal filigree across the note
  highway. Owner is an overshell-slot `ScrollbarDisplay` (`chars.sbd`) whose
  `m_fSavedScale` is at its ctor default `0`, so the draw gate `m_fSavedScale < 1.0f`
  (`ScrollbarDisplay.cpp:179`) is always true. Wii frustum-culls the off-frame widget;
  **native disables culling** (`Draw.cpp:199-206`) so it draws every frame.
- **Why it dominates the metric:** 71–74% of ALL shard-guard drops (834/1123 in the
  root-cause run); present every boot in EVERY venue (A: 497–1067, B: 383–2582,
  C: 400–2788). Collapsing it makes the genuinely-uncovered crowd/extras residue the
  visible signal.
- **Repro:** ANY venue, any `--song-downs`, any gameplay shot (it leaks regardless of
  framing). e.g. small_club_01 default boot, `coop_g_cg.shot`; A/B with
  `SHARD_GUARD_OFF=1` to see the teal ribbon on the highway.
- **Recommended fix (HX_NATIVE, src/):** **Option A** from `scrollbar-rootcause.md §5`
  — extend the existing native `MenuVoidDrawHook` (`src/system/rndobj/Draw.cpp:63`,
  already wired into `RndDrawable::Draw`/`DrawBudget`) with a default-ON skip of
  `scrollbar_bg.mesh`/`scrollbar.mesh` **only under the world/venue cam** (reuse the
  `RB3VenueFrustumCull` world-cam discriminator so menu scrollbars are untouched),
  opt-out `RB3_SCROLLBAR_FIX_OFF`. Falls back to Option B (gate
  `ScrollbarDisplay::DrawShowing` on real scrollable content,
  `ScrollbarDisplay.cpp:179`) if a clean world-cam test isn't reachable in Draw.cpp.
  Pure src/ change — no engine pin bump. Must-not-break gate: song_select +
  accomplishments still show a scrollbar.
- **Value:** correctness/cleanup, not a new visible win (net visual gain over the
  guarded render ≈ zero). But it removes the band-aid, kills 834 wasted skinned-mesh
  draws/drops per ~30 frames + a per-frame re-export warning, and unmasks the real
  residue. Greenlit by the user.

### GAP 2 — arena_02 band severely underlit → black silhouettes  (most visible)
- **Family:** lighting (RndEnviron under venue world.cam).  **Severity: HIGH**
  (whole-frame, dominant — the band + stage floor render near-black).
  **Tractability: HARD** (lighting path, ground-truth missing).
- **What:** arena_02 band members render as near-black silhouettes with no fill/stage
  key light; crowd LED boards are bright but performers + stage floor are black. Mean
  frame luma 26–56 (arena) vs 78–200 (festival). **Guard-independent** (guard ON/OFF
  closeup luma 45.7 vs 49.1) — it is the venue lighting, not dropped geometry.
- **Repro:** `set_venue_override arena_02` (Group C wrapper / `/tmp/bch_override.py`),
  any `--song-downs`; shots `coop_fs_v_n01`, `coop_dir_crowd`, `coop_fs_all_n00`
  (NOTE: map.md's `coop_*_near` names are WRONG — arena/festival use the `coop_fs_*` /
  `coop_bs_d_*` prefix; see Group C §1). Deterministic, no wardrobe dependence.
- **Open question (honesty):** RB3 arenas ARE intentionally dark with spotlit bands, so
  the *darkness* may be partly correct — the suspicious part is **pure-black band**
  (no spot/key reaching the performer) vs a spotlit-but-dim performer. **No retail
  arena_02 screenshot exists** in `images/retail-screenshots/` (only club/generic
  gameplay), so ground-truth for arena lighting is currently missing.
- **Recommended approach:** FIRST localize — A/B `RB3_VENUE_LIGHT_OFF=1` vs default on
  the same arena_02 shot, compare band-region luma; capture retail arena footage for
  ground-truth before committing a fix. Likely engine-side (the per-environ
  SceneUniforms / `RB3_VENUE_LIGHT` path in `../milo-native-engine`, cf MEMORY A4 "P4
  venue lighting" which only added a grey fallback when an env is *unlit* — arena_02
  may load a lit env whose key/spot doesn't reach the character draw). Engine fix →
  pin bump if confirmed.

### GAP 3 — big_club_01 crowd renders as flat stark-WHITE silhouettes  (visible)
- **Family:** crowd lighting/material.  **Severity: MEDIUM** (conspicuous in crowd +
  wide framings, present whenever the audience is in frame).  **Tractability: MEDIUM**
  (material/lighting probe, not a deform fix).
- **What:** the big_club_01 audience renders as stark flat white cut-out silhouettes
  lining the stage, **present guard ON and OFF** → NOT a masked shard; it is how the
  crowd is shaded/lit. vs retail (dim/dark unobtrusive audience). Hypothesis: crowd
  characters missing their lit material / falling back to an unlit/white shader under
  big_club's environ, or a crowd-LOD billboard rendering white.
- **Repro:** `set_venue_override big_club_01`, any sd, `coop_dir_crowdg.shot` /
  `coop_all_n00.shot` (crowd in frame). NOTE: video_01's white *studio backdrop* is
  authored + correct — this gap is specifically the big_club *audience figures*.
- **Relationship to GAP 2:** both are venue crowd/character LIGHTING (arena = too dark,
  big_club = too white) — plausibly the same crowd/venue lighting-material path failing
  differently per environ. **Likely one combined crowd-lighting investigation** that
  also informs GAP 2.
- **Recommended approach:** crowd lighting/material probe (no shard signal — pixel
  observation). Determine if crowd chars get their lit material vs an unlit fallback
  under each environ. Likely engine-side → pin bump.

### GAP 4 — festival `coop_crowd_mass*_screenmask` shot → near-white blank frame
- **Family:** other (screenmask overlay / crowd-render).  **Severity: MEDIUM** (the
  entire crowd framing the shot is built for goes white).  **Tractability: MEDIUM**
  (overlay blend/alpha, but ground-truth unknown).
- **What:** festival_01's `coop_crowd_mass01_screenmask` / `coop_crowd_mass_screenmask`
  shots render a flat near-white field (mean luma ~200) with only HUD + guitar neck/hand
  visible — the mass audience the shot is named for is replaced by white. The festival's
  DIRECT crowd shots (`coop_dir_crowd00`/`coop_dir_crowdb`) DO render the intended
  comic/poster B/W backdrop, so the venue crowd backdrop CAN render — only `*_screenmask`
  goes white. Likely the `screenmask` overlay quad drawing **opaque white** (mask
  texture/alpha/blend not applied on native) instead of revealing the crowd.
- **Repro:** `set_venue_override festival_01`, any sd, shot
  `coop_crowd_mass01_screenmask` / `coop_crowd_mass_screenmask`. Deterministic.
- **Open question (honesty):** could be an *intended* white screen-flash — but it
  persists across both captured frames (not a 1-frame flash) and totally hides the
  crowd, so an unmasked overlay is the likelier read. **No retail festival footage** in
  `images/retail-screenshots/` to confirm. **Get ground-truth before fixing.** Defer
  pending that.

### GAP 5 — band footwear `_skin.2` thin-skin fling (foot-bone rebind gap)
- **Family:** skin-deform (foot bones).  **Severity: LOW** (off-frame in every
  closeup; small/dark/distant in the resolvable wides).  **Tractability: MEDIUM**
  (rigid single-anchor rebake pattern exists, but must be gated to genuinely drop ratio).
- **What:** thin `_skin.2` shoe submeshes bound to `bone_{L,R}-ankle/toe/knee` blow
  the blended-world/bind AABB past the relaxed band cap (4.0×); V24 guard drops them →
  shoe missing. `dir=''` (band character), bone0 at foot/ankle height. The rigid
  `_resource` companion stays under cap. This is the rotation-basis fling family,
  identical signature to the landed strings/outfit fixes — but **NO existing rebind
  covers feet** (`RebindOutfitBonesToOwnSkeleton`=torso, `RebindInstStringsToRestBasis`
  =guitar-strings, `RebindHeadHandsAtRest`=head). `RB3_NO_SKEL_REBIND` /
  `RB3_NO_INST_REBIND` both leave it intact (verified Groups A+B).
- **De-dup across audits (ONE gap, many songs/venues/shoes):**
  - Group A small_club_01: sd4 `maleslipons2_skin.2` (NEW shoe), sd6 `lowtopsneaks_skin.2`,
    sd8 `saddleshoe_skin.2` (drops 107/721/2474).
  - Group B big_club_01: sd0 `lowtopsneaks_skin.2`, sd25 `saddleshoe_skin.2`
    (265/3458). **video_01 did NOT reproduce** (thin shoes rolled ≤3.17) → footwear
    fling is **venue/animation-dependent**.
  - Group C arena_02 sd30 `saddleshoe_skin.2`+`lowtopsneaks_skin.2` (up to 5931),
    festival_01 `lowtopsneaks_skin.2`/`maleslipons2_skin.2`.
  - **Wardrobe is RANDOM PER BOOT** (not per song) — `drops_band` swings 0↔5931 across
    boots of the same config purely on whether a thin-`_skin.2` shoe rolled. Watch-list
    (under cap this batch, same rig): `wovensteppers_skin.2`, `rolledjeans_skin.2`,
    `tightdistressedpants_skin.2`, `femaledestroyedchucks_resource`.
- **Repro:** any venue, re-boot 3–5× until a thin-skin shoe rolls (Group A sd4/6/8
  reliably; big_club sd0/25; arena sd30). Off-frame: highway fills the lower frame in
  every closeup; feet only appear small/dark/distant in `coop_all_n00`/`coop_fs_all_n00`.
- **Recommended fix (DEFER, fold into a general foot/C8 pose-basis batch):** extend the
  band rebind to footwear, mirroring `RebindInstStringsToRestBasis` in
  `src/system/bandobj/BandCharacter.cpp` — single-anchor rigid rebake of the `_skin.2`
  shoe to its least-moving foot bone (ankle), baking
  `offset = meshWorld·inv(anchorWorld)` so the shoe rides the ankle rigidly (ratio→~1).
  HX_NATIVE, Wii-neutral via Poll. **MUST be gated behind a strict same-boot A/B that
  demonstrably brings `max_band_ratio` under cap** (the engine `RB3_GUARD_EXEMPT_REBOUND`
  note warns a translation-only anchor leaves the rotation fling and can draw a worse
  slab). **LOW ROI — not standalone; do it last with the crowd/servo rebind batch.**

### GAP 6 — crowd/extras servo-skeleton shards (`male_extras_*`, `eyebrows`, `clap`, `fist`, `horns`)
- **Family:** crowd (non-BandCharacter servo skeleton).  **Severity: LOW** (tiny, far,
  background; guard masks them).  **Tractability: HARD** (no rebind exists for
  non-BandCharacters; whole-292-crowd blast radius).
- **What:** background audience/extras characters' servo skeletons momentarily produce
  finite-but-wrong AABBs → guard drops face/hair/head/hand shards. All `other`-class,
  all off in the background vignette.
- **De-dup across audits (ONE family, many instances):** `fist.mesh`, `clap.mesh`,
  `horns.mesh` (NEW crowd hand-prop, Group A sd6, `dir=crowd_male03`),
  `male_extras_hair02`, `male_extras_eyebrows11`, and big_club/festival NEW instances
  `eyebrows.mesh` (`male_extras04`), `male_extras_eyebrows10_low`, `male_extra_head01/03`,
  `female_extra_head`, `male/female_extras_skin0*`, `male_extras_body02_medium`. These
  are NEW mesh NAMES across venues but the SAME crowd-servo root cause (scout-residual
  b/c) — festival's mass crowd just amplifies the count.
- **Repro:** festival_01 mass-crowd shots / big_club_01 crowd shots; guard-OFF shows the
  dark angular shards radiating from the crowd.
- **Recommended approach (ACCEPT/DEFER):** would need a NEW `Character`/servo
  rest-rebake hook that walks the venue's extras/crowd dirs (none exists — all three
  rebinds are BandCharacter-only). HIGH blast-radius (a basis mistake regresses the
  whole audience), LOWEST payoff. **Accept-as-droppable for now**; fix only alongside a
  built-and-adversarially-gated crowd/servo rebind hook (bundle with GAP 5's foot batch).

---

## Genuinely-visible vs masked/off-frame (honest split)

**Genuinely visible (worth implementing for convergence):**
- GAP 2 arena_02 underlighting — whole-frame, dominant. (pending retail ground-truth)
- GAP 3 big_club crowd white silhouettes — conspicuous in crowd/wide framings.
- GAP 4 festival screenmask white — entire crowd framing white. (pending ground-truth)

**Masked / off-frame / accidentally-correct (accept or defer):**
- GAP 1 scrollbar — masked by the guard today (highway already matches retail); fix is
  cleanup/correctness, greenlit anyway.
- GAP 5 footwear — real geometry, but off-frame in every closeup, marginal in wides.
- GAP 6 crowd/extras servo shards — guard-masked, tiny, distant.

**NON-gaps re-confirmed (NOT bugs):** all band upper-body closeups (5 venues) — clean
bill. small_club_01 dark/purple closeup lighting MATCHES Wii retail
(`yt_qRagnZCIMzk_gameplay_guitar.png`). video_01 white studio backdrop is authored +
correct. Group A's "red/orange FAILED wash" on late wide shots is the song-fail state,
not a lighting bug.

---

## RECOMMENDED IMPLEMENTATION BATCH (next 1–4)

1. **GAP 1 — scrollbar UI-leak skip (DO FIRST).** Greenlit, root-caused, easy, pure
   src/ HX_NATIVE (Option A: extend `MenuVoidDrawHook` world-cam-scoped, opt-out
   `RB3_SCROLLBAR_FIX_OFF`). Collapses 71–74% of all residual drops + the band-aid.
   Verify: drop table no longer contains `scrollbar_bg.mesh`; drops_band stays 0;
   guard-OFF highway clean; no menu-scrollbar regression (song_select/accomplishments).
2. **GAP 2 + GAP 3 — venue/crowd LIGHTING investigation (combined).** These are the
   highest genuinely-visible gaps and likely the same crowd/venue lighting-material
   path failing differently per environ (arena too dark, big_club too white). Start
   research-only: `RB3_VENUE_LIGHT_OFF` A/B + a crowd-material probe + capture/locate
   retail arena+club ground-truth. Then an engine fix → pin bump. This is the real
   convergence frontier now that band skin is clean.

**DEFER from this batch:**
- GAP 4 (festival screenmask) — get retail festival ground-truth FIRST; could be an
  intended white flash. Then a small overlay blend/alpha fix.
- GAP 5 (footwear) + GAP 6 (crowd/extras servo) — fold together into a LATER general
  rest-rebake batch (foot rebind + a new crowd/servo `Character` hook), strictly
  A/B-gated. Off-frame / masked → low ROI, not worth this batch's surface.

---

## Anchors (key source/asset pointers carried from the audits)

| claim | anchor |
|---|---|
| scrollbar draw gate / fix site | `ScrollbarDisplay.cpp:179`; `src/system/rndobj/Draw.cpp:63,199-206` |
| native culling OFF (Wii cull) | `src/system/rndobj/Draw.cpp:197-214` |
| world band bridge (pre-EndWorld) | `src/system/world/Dir.cpp:448-461,474` |
| footwear rebind target | `src/system/bandobj/BandCharacter.cpp` (RebindInstStringsToRestBasis pattern) |
| arena/festival shot prefix (map.md was WRONG) | `coop_fs_<m>_<n/c><NN>` / `coop_bs_d_*` (Group C §1) |
| venue override (required for non-small_club) | `{meta_performer set_venue_override <v>}` (map.md §5); arena_01 CRASHES — use arena_02 |
| venue lighting path (cf A4) | `../milo-native-engine` per-environ SceneUniforms / `RB3_VENUE_LIGHT` |
| crowd/extras servo family | `scout-residual.md` (b)/(c) — no rebind for non-BandCharacters |
| retail ground-truth (club only) | `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` (NO arena/festival) |
