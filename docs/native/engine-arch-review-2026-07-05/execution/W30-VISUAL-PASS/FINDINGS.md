# W30-VISUAL-PASS — visual sweep findings (2026-07-10)

Full boot→menus→song visual pass with the canonical harness. **Findings only — no
fix code, no source changes.** 3 boot runs total (budget: ≤4):

| Run | Harness | Result |
|---|---|---|
| 1 | `boot-to-song.py --hold 45 --interval 3` (song `123` = 20th Century Boy, guitar/expert, autohit) | PASS 15/15 shots (`/tmp/w30vp/run1`) |
| 2 | `band-closeup-capture.py --member all --frames 2` | PASS, pinned 34/34, 0 drops (`/tmp/w30vp/closeup`) |
| 3 | throwaway hub capture (`/tmp/w30vp/hub-capture.py`, reuses keyboard-to-gameplay helpers): splash → main_hub → PLAY NOW submenu → song_select | all 5 milestones OK |

Retail ground truth: `images/retail-screenshots/` (menu hub, song select ×4 Wii,
gameplay highway ×3 Wii). Gameplay retail refs are **highway-cam only** — wide
venue/band shots have **no retail pair**; those findings rest on internal
inconsistency (mid-air geometry, point-radial spike fans).

Known items NOT re-reported: unposed hands/instruments + idle-only band anims
(W30 primary lane), drummer 31u residual, green/olive faces (deferred), exit-trap
SIGSEGV, and every closed item in `execution/README.md` waves ≤29.

---

## F1 — Corrupted prop geometry: spike-fans + crumpled-cone meshes on members and around the drum kit  ⭐ top candidate

**Symptom.** Bundles of 6–10 thin beige/white "sticks" radiating from a single
point ("spike-fans") attached to band members' hands, plus crumpled-cone /
collapsed-frame meshes floating unsupported in mid-air around the drum kit and
stage. Persistent in EVERY wide gameplay frame across the 45s hold (both
lighting states), in venue closeups, and — notably — **also on main-hub street
walkers** (the right-side walker in the PLAY NOW submenu shot carries the same
beige stick-fan at his waist).

**Evidence.** `evidence/gameplay_countin_props.png` (crumpled beige cone right
of drums; stick-fan floating above kit; white spike-fans at guitarist's hands),
`evidence/gameplay_wide_spikefans.png` (all four members with white stick-fans
at hands, 47s in), `evidence/closeup_vocals_broken_props.png` (coop_v_n01: no
vocalist in frame; broken wooden frame floating mid-air + large brown spike-fan
bottom-right), `evidence/playnow_submenu_native.png` (hub walker with stick-fan).

**Retail pair.** None (no retail wide venue-cam shot). Called on internal
inconsistency: meshes radiate from a point / float with no support, recur every
frame.

**Suspected layer.** Char/prop bone driving — very plausibly the SAME root cause
as the W30-BAND-PERF-CLIP primary lane (zero instrument-performance clips → prop
bones never driven → multi-bone prop meshes (drumsticks, stands, strings)
stretched between an at-rest bone and a live bone collapse into point-radial
fans). The hub-walker occurrence says it is not gameplay-only, which slightly
weakens the perf-clip-only theory — worth an explicit check in that lane.

**Severity.** visual-blocker (dominates every band camera cut).

**Proposed lane charter.** Fold into W30-BAND-PERF-CLIP as an acceptance
criterion: identify which mesh(es) the fans belong to (drawlog/mesh-name dump at
a pinned closeup), confirm they are prop-bone-skinned, and require fans gone (or
explicitly re-scoped) when perf clips land. Only open a separate lane if a
pinned-shot mesh dump shows non-prop-bone ownership.

## F2 — Gameplay score pill face is translucent white; digits unreadable at low scores

**Symptom.** The top-right score pill renders with a white, semi-transparent
face — the venue (hex fence, beams, characters) bleeds through the pill body —
and the digits are white-on-white (at count-in the "0" is nearly invisible;
"122" in the closeup is washed). Retail's pill is an opaque silver-rimmed
DARK face with high-contrast white digits.

**Evidence.** `evidence/crop_scorepill_translucent.png` (hex fence visible
through the pill), `evidence/crop_scorepill_lowcontrast.png`,
`evidence/gameplay_countin_props.png` (count-in: pill looks empty).
**Retail pair:** `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`
(opaque pill, "78,250").

**Suspected layer.** HUD material/blend — pill background mesh drawn with wrong
blend/tint (or its dark fill texture unbound → white), same family as F3.

**Severity.** polish (always on screen; readability).

**Proposed lane charter.** One-shot HUD-material fix lane: dump the score pill's
mesh/mat in a pinned frame, compare authored blend/tint vs native bind, fix the
bind. Verify vs retail pair.

## F3 — UI button/icon glyphs render as flat white shapes (discs/pills/rects) across menus

**Symptom.** Every controller-button / action glyph in the shell renders as an
untextured, untinted white shape: a solid white DISC over the "SE" of SETLISTS
in song_select; the footer verb icons ("HOLD TO MAKE SETLIST" / "HOLD FOR
SHORTCUTS") are plain white pills; the hub "NEXT MESSAGE" Y-button is a white
rectangle; the Player-1 overshell tab's MENU button is a white dot. Retail draws
colored button glyphs (green/orange/yellow) in all these slots.

**Evidence.** `evidence/crop_setlists_white_disc.png`,
`evidence/song_select_footer_pills.png` (footer),
`evidence/main_hub_native.png` (NEXT MESSAGE rect + MENU dot),
`evidence/song_select_run1.png`. **Retail pairs:**
`yt_qRagnZCIMzk_song_select_list.png` (PLAY ALL/NEXT HEADING colored icons,
"−" header glyph), `yt_mhKNp9uAT48_menu_hub.png` (yellow Y glyph). Exact-pixel
source: `ui_buttons_wii_spriters.png`.

**Suspected layer.** UI icon texture bind or per-glyph tint (one mechanism, many
consumers — likely the button-glyph atlas or the icon material's color source).

**Severity.** polish (broad cosmetic reach, every screen).

**Proposed lane charter.** Trace one glyph (song_select footer) from milo asset
to draw: is the atlas texture unbound (white fallback) or the tint dropped?
One fix should clear the whole class; verify on hub + song_select + overshell.

## F4 — Star-rating HUD: only earned pips drawn; retail shows a persistent 5-slot star row

**Symptom.** Under the score pill, native shows a single dark star badge at 0★,
growing to 2–3 badges as stars are earned. Retail always renders the full
5-slot star row (unearned slots as dim outlines) from count-in onward.

**Evidence.** `evidence/gameplay_countin_props.png` (one badge at 0★),
`evidence/gameplay_starpips_guitar.png` (two badges at ~1★),
`evidence/gameplay_wide_spikefans.png` (three at ~2★). **Retail pair:**
`yt_qRagnZCIMzk_gameplay_guitar.png` (5 stars visible).

**Note.** Distinct from the W22-closed "star meter never fills" false alarm —
filling works; this is the missing unearned-slot outlines (layout/visibility of
the empty slots).

**Severity.** polish.

**Proposed lane charter.** Small HUD lane: check the star row group in the HUD
milo for 5 authored slot meshes and why unearned slots are hidden natively
(showing state vs texture frame).

## F5 — Guitarist jacket "patch shard" explosion in venue closeups

**Symptom.** In guitarist closeups the black/white checkered jacket erupts in
jagged, extruded black plates (shards) scattered off the torso — classic
BandPatchMesh shard corruption, clearly on-camera in a standard venue closeup
(not just the lineup/closet context previously studied).

**Evidence.** `evidence/closeup_guitar_jacket_shards.png` (coop_g_cg, both
frames identical). **Retail pair:** none (no retail closeup ref — same gap as
the deferred green-faces item).

**Suspected layer.** BandPatchMesh / SetMeshVerts (memory: "deformed chars =
BandPatchMesh rewrites ×2, both reverted; closeup gate blind to patch shards";
`project_bandpatch_setmeshverts_oob` ASan harness exists). This finding gives
the previously-blind closeup gate a deterministic on-camera reproducer:
`band-closeup-capture.py --member guitar`, shot `coop_g_cg`, default song.

**Severity.** polish (visual-blocker within closeup cuts).

**Proposed lane charter.** Re-open patch-shard with the deterministic closeup
repro + the existing ASan harness; gate = shard-free coop_g_cg at pinned songMs.

## F6 — Main-hub scene missing night grade/bloom: bright flat daylight look vs retail dark neon street

**Symptom.** Native main_hub renders bright, flat, orange-lit buildings with
readable daylight detail. Retail hub is a dark night street: heavy vignette,
crushed shadows, glowing neon with bloom, characters as mid-distance
silhouettes. Sub-observation: the PALACE neon sign renders as unlit tan
letterforms with a misregistered colored outline layer (retail: glowing neon).

**Evidence.** `evidence/main_hub_native.png`,
`evidence/playnow_submenu_native.png`. **Retail pairs:**
`yt_mhKNp9uAT48_menu_hub.png`, `yt_mhKNp9uAT48_menu_playnow_submenu.png`.

**Suspected layer.** Hub-cam postproc/lightpreset (possible overlap with the
held `RB3_UI_POST_GRADE` decision and the venue-grade work — coordinator should
adjudicate whether this is that lane's residual before chartering new work).
Menu text/UI itself is fine — this is the 3D scene behind the UI.

**Severity.** polish (first screen every player sees; large fidelity gap).

**Proposed lane charter.** Probe the hub world.cam postproc chain (is a
ColorXfm/bloom authored in `main.milo` being skipped on the hub camera?) before
any tuning; explicitly reconcile with the UIGRADE-held rationale.

## F7 — Song-select/part-difficulty backdrop character clipped at extreme close range

**Symptom.** A band member stands so close to the song_select backdrop camera
that her thighs/hands fill the right edge under the album-art panel (and the
bottom-right of part_difficulty). Retail song select's right side is a dim,
distant backstage scene — no giant foreground character.

**Evidence.** `evidence/song_select_run1.png`,
`evidence/part_difficulty_native.png`. **Retail pair:**
`yt_qRagnZCIMzk_song_select_list.png`.

**Suspected layer.** Backdrop vignette char placement/camera (char standing at
a wrong node, or the vignette cam picking a wrong shot) — possibly the same
idle/placement class as the W30 lane, but in the shell context.

**Severity.** cosmetic.

**Proposed lane charter.** Low-priority: dump the shell vignette cam + char
node positions; compare against the authored backstage vignette.

## F8 — (low confidence) part_difficulty overshell card text overlap

**Symptom.** The overshell card shows "RIGHTY MODE" and "SONG DIFFICULTY"
overlapping with a red glyph-star row, and "CHOOSE INSTRUMENT" clipped by the
instrument icon. **Ambiguity declared:** W4.1(c) previously refuted a
part_difficulty "missing widgets" report as a mid-transition capture; this
single frame (1.0s after screen arrival) may equally be mid fan-out animation.
No retail pair exists for this screen (README "still missing").

**Evidence.** `evidence/part_difficulty_native.png`. **Retail pair:** none.

**Severity.** cosmetic (unconfirmed).

**Proposed lane charter.** None yet — needs a settle-frame recapture series
(the W4.1 method) before it is a finding.

---

## Checked and CLEAN (matched retail / no anomaly)

- **Splash/title**: RB3 logo over night city + PRESS START — consistent with the
  title ref; crisp text, no artifacts (`/tmp/w30vp/hub/00_splash.png`).
- **Hub menu text/layout**: PLAY NOW/CAREER/TRAINING/CUSTOMIZE/GET MORE SONGS
  typography, highlight bar, submenu fan-out (QUICKPLAY / START A ROAD
  CHALLENGE) all match retail; W4.1 grey quad confirmed still gone; ticker text
  present and clean.
- **Song select layout**: header, VIEWING count line, section headings with
  per-section song counts + 0/N★, song rows w/ artist italic, album-art "?"
  placeholder, red scrollbar sliver (adjudicated scrollbar thumb) — all match
  the Wii refs. "Player 1" gamertag fix holding (no "(null)").
- **Note highway**: 5-lane guitar track, gems/HOPOs, sustain trails, beat lines,
  now-bar, smasher pads, multiplier spinner (2x/4x), lane glow on hits — match
  the retail highway refs. White/silver gems+trails interpreted as Star Power
  phrases (faithful), not a defect.
- **Score HUD position**: top-right (W22 fix holding); score increments
  correctly with autohit.
- **Crowd meter** (left vertical bar): present, green, fills — matches retail
  placement.
- **Venue lighting engagement**: count-in cold/magenta preset → warm engaged
  lighting at ~16s → red preset later — lightpreset changes fire; no venue
  wash/grey regression seen in 15 frames (W8/W9 fixes holding).
- **Crowd**: crowd members render in crowd-adjacent cams (gameplay_012), no
  "crowd at origin" clumps, no missing-crowd regression.
- **Camera direction**: auto-director cuts between wide/closeup/behind shots on
  musical boundaries; closeup pin harness 34/34 deterministic.
- **Stability**: all 3 runs exited by harness contract (15/15 shots, 34/34
  pinned, 0 mesh drops, 0 band-ratio regression); no mid-run crash.

## Raw capture locations

- `/tmp/w30vp/run1/` (gameplay run, 18 png + health.jsonl + engine.log)
- `/tmp/w30vp/closeup/` (17 shots × 2 frames + manifest/verdict)
- `/tmp/w30vp/hub/` (5 milestones + engine.log)
