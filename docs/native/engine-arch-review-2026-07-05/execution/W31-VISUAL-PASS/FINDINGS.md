# W31-VISUAL-PASS — second visual sweep, post-flip audit (2026-07-12)

Chartered as the visual countersign of the W30 `RB3_PROP_POSE_FULL` flip. **The
tree advanced two waves between charter and execution** — this pass ran on
post-W32-close-out HEAD (`8f14bc8a`, engine pin `2ea8e34`, 17 defaults ON), so
it also countersigns the W31 set_play fix, the W31 F3 glyph fix, and the W32
MIDIDRV-Enter (prop-fan) fix. **Findings only — no fix code, no source
changes.** 5 boot runs (budget: <=5):

| Run | Harness | Result |
|---|---|---|
| 1 | `band-closeup-capture.py --member all --frames 2` | PASS pinned 34/34, 0 drops (`/tmp/w31vp/closeup`) |
| 2 | `boot-to-song.py --song beastandtheharlot --hold 36 --keep`, then `msg:game:jump:600000` on the kept engine → endgame flow capture | 12/12 shots + endgame series; **engine SIGSEGV on CONFIRM at `coop_endgame_screen`** (V2) |
| 3 | throwaway hub sweep (`/tmp/w31vp/hub-capture.py`) | splash+hub captured; extra START opened the overshell player menu; CONFIRM on "PLAY ON XBOX LIVE" → **engine SIGSEGV** (V3); career/training/F8-settle legs NOT reached |
| 4 | run-2 rerun with `--env RB3_NO_MIDIDRV_ENTER_FIX=1` (A/B) | 12/12 shots — discriminator for V1 + before-image for the F1 fan family |
| 5 | `boot-to-song.py` default song (20th Century Boy, W30's song/venue) | 10/10 shots — V1 generality proof |

Ground truth: `images/retail-screenshots/` (no `xenia/` subdirectory existed at
execution time — used the existing yt_*/fandom refs). Retail-pair gaps carried
from W30: no wide venue-cam ref, no closeup ref, and **no results-screen ref**.

Known items NOT re-reported: F2 score pill + F7 sidebar backing (chartered
W33), F4 (closed NOT-A-BUG), F5 patch shards (recorded, needs fresh
hypothesis), F6 hub grade (blocked on UIGRADE reconciliation), F8 overshell
card overlap (my settle-series leg died with run 3; the run-5 part_difficulty
frame reproduces the overlap at ~1s but remains a single-frame capture —
still unadjudicated), green/olive faces (deferred, seen again on a hub walker),
hub-walker/crowd SKEL shards (Lane D STOP, binding), drummer right-hand
residual, exit-time teardown (FIXED W31 — not re-seen).

---

## Flip/fix regression audit (job 1) — per-member verdict

The planned instrument-hands closeup audit was **blocked by V4** (all 34 pinned
closeup frames are near-black; fretboard/strum detail unreadable). Verdict
assembled from the lit wide/auto-director frames of runs 2/4/5 instead:

- **Point-radial prop fans (W30-F1): GONE on the current tree, and the W32 fix
  is causally confirmed** — run 4's `RB3_NO_MIDIDRV_ENTER_FIX=1` A/B brings the
  magenta/white fans straight back (`evidence/ab_mididrv_off_fans_return.png`),
  ON-tree frames across 22 gameplay shots show none. The A/B image doubles as
  the archival before-image the charter asked for (job 3); kit cones did not
  reproduce in any lit frame.
- **Guitarist**: upright and playing in lit frames (run5 `gameplay_008` right),
  hands at the instrument; no fans. Closeup-grade fret/strum verification
  remains OWED (V4).
- **Bassist**: no fans, but repeatedly caught in grossly wrong full-body poses
  — collapsed face-down over the bench (run2 `gameplay_004`), bent-double
  pretzel with sticks (run2 `gameplay_005`) → V1, not a flip artifact.
- **Drummer**: at the kit, sticks render as sticks (thin lines, no radial
  bundles) in both silhouette closeups and lit frames.
- **Vocalist**: natural at the mic in some frames; folded backwards with a leg
  horizontal in others (run5 `gameplay_008`) → V1.
- **Keys**: the club venue has no keys spot; `coop_k_*` pins frame the drum
  hardware — unassessable, as in W30.

**Net: the two prop fixes (PROP_POSE_FULL + MIDIDRV_ENTER) countersign clean.
The band's overall visual state nonetheless REGRESSED since W30 via V1, which
is neither of them (A/B-proven for MIDIDRV; PROP_POSE was already ON in W30's
post-flip retest, which was fan-free AND upright).**

---

## V1 — Band members in grossly wrong full-body poses during gameplay (regression since W30)  ⭐ top candidate

**Symptom.** Band members collapse into non-anatomical full-body poses that no
authored performance clip plausibly contains: bassist face-down over the bench
seat; a member bent double with limbs radiating, holding drumsticks at the
bassist's stage spot; vocalist folded sideways/backwards at the mic with one
leg horizontal, feet off the floor; members crumpled in a heap beside the
stage. Persistent across the hold (most wide/mid frames show at least one
broken member), on BOTH songs tested, same club venue W30 captured.

**Evidence.** `evidence/bath_wide_poses_broken.png`,
`evidence/bath_bassist_collapsed_bench.png`,
`evidence/bath_red_mangled_member.png` (all beastandtheharlot),
`evidence/default_song_vocalist_folded.png` (default song — W30's exact
song/venue). **Before-state:** W30's
`../W30-VISUAL-PASS/evidence/gameplay_wide_spikefans.png` shows all four
members UPRIGHT (idle, hand fans only) in the same venue.

**Discriminators run.**
- NOT W32 MIDIDRV-Enter: run 4 (`RB3_NO_MIDIDRV_ENTER_FIX=1`) still shows the
  collapsed-pose class (`evidence/ab_mididrv_off_fans_return.png` — fans
  return, poses stay broken).
- NOT song-specific: run 5 reproduces it on the default song.
- Regression window: W30 visual pass (post-PROP-flip retest, band upright) →
  now. The dominant behavioral change in that window is **W31 SET-PLAY** — the
  band actually plays performance/body clips in-song for the first time.

**Suspected layer.** Performance-clip application, NOT clip selection: the W31
acceptance was census-based (CHARDRV_PLAY counts, intensity distribution) and
never visually gated a full-body pose. Two candidate mechanisms for the lane
to discriminate: (a) the **SKEL seed-R rotation-basis class** named by
W31-HUBWALKER-SHARDS — band chars playing body clips are a NEW surface for
that closed family (CharCache player0-3 carried 0 skinned meshes THEN because
they were idle; the Lane-D STOP was scoped to crowd/extras+outfit fringe, and
this is new-hypothesis evidence, not a re-open of the same cell); (b) sit/
stand group or root-orientation mis-application in the `play_group`/`set_play`
path (E3's sit-churn bound was numeric, not visual).

**Severity.** visual-blocker (dominates every band camera cut, both songs).

**Proposed lane charter.** W33 candidate: pin one broken member at a fixed
songMs (closeup harness + a LIT venue moment or a lighting override), dump the
playing clip name + per-bone pose vs the clip's authored keyframes, and
discriminate basis-bug vs wrong-clip. Acceptance = band poses anatomical at
matched songMs, CHARDRV_PLAY census unchanged (don't undo W31).

## V2 — CONFIRM on the results screen → SIGSEGV in `BandCharDesc::NameToDrumVenue` (flow-blocker, root-caused)

**Symptom.** After song end, pressing CONFIRM (CONTINUE) on
`coop_endgame_screen` kills the engine: SIGSEGV at address (nil). The player
can finish a song but never return to the shell. 1/1 reproduction.

**Backtrace (symbolized, addr2line on run-2 engine.log):**
`UIPanel::Unload` → DTA script → `BandWardrobe::Handle` →
`BandWardrobe::OnUnloadVenue` (BandWardrobe.cpp:954) →
`BandCharacter::SetTempoGenreVenue(Symbol(), Symbol(), "")`
(BandCharacter.cpp:3368) → `BandCharDesc::NameToDrumVenue("")`
(BandCharDesc.cpp:587) → crash.

**Root cause (read, not fixed — findings-only).**
`sDrumVenueMappings` (BandCharDesc.cpp:18) is a 10-entry pair table with **no
empty-string sentinel**. The scan condition
`*sDrumVenueMappings[offset / 4] != 0` walks indexes 0,2,4,6,8 — all non-empty
— then dereferences **index 10, past the end of the table**. During venue LOAD
the real venue name strstr-matches early and returns; the UNLOAD path passes
`""`, never matches, and always walks off the end. On Wii the adjacent .data
byte happened to terminate the loop; native reads a garbage/NULL pointer.
Classic decomp OOB latent since the table was transcribed; only the endgame →
unload path reaches it.

**Evidence.** run-2 engine.log backtrace (above); screens leading in:
`evidence/endgame_results_j0_GO.png`. **Severity.** flow-blocker.
**Proposed fix shape** (for the lane, one line): sentinel entry or bounded
scan — a native-safety bound (the Wii bytes are what they are).

## V3 — Overshell "PLAY ON XBOX LIVE" → MILO_FAIL, and the assert formatter itself SIGSEGVs

**Symptom.** In the player-menu popup (START on main hub → RETURN / PLAY ON
XBOX LIVE / CHARACTERS / OPTIONS / DROP OUT), confirming "PLAY ON XBOX LIVE"
kills the engine.

**Backtrace (symbolized, run-3 engine.log).** Two layers:
1. `OvershellSlot::Handle(attempt_register_online)` →
   `AttemptRegisterOnline` → `BeginOverrideFlow` → `UpdateState` →
   `GenerateCurrentState` → `OvershellSlotStateMgr::GetSlotState`
   (OvershellSlotState.cpp:182) → `MILO_FAIL("OvershellSlotState %d does not
   exist")` — the online-register flow's slot state is missing natively
   (online is out of port scope; entry should degrade to a no-op/dialog
   instead of an assert).
2. The assert path ITSELF crashes: `Debug::Fail` → `Debug::Modal`
   (Debug.cpp:377) → `MakeString<String, String, Symbol>` →
   `FormatString::operator<<(String const&)` (MakeString.cpp:312) → SIGSEGV.
   **Any MILO_FAIL routed through this Modal formatting path dies without
   printing its message** — hygiene bug that will mask every future assert of
   this shape.

**Evidence.** `evidence/hub_overshell_xboxlive_precrash.png` (menu open,
option highlighted, one CONFIRM before death); run-3 engine.log backtrace.
**Severity.** medium (user-reachable from every screen's player menu; two
independent bugs, the formatter one being campaign-wide hygiene).

## V4 — Closeup-harness venue lighting collapsed to blackout + single yellow key (delta vs W30, cause unadjudicated)

**Symptom.** All 34 pinned closeup frames (17 shots × 2, spanning HUD scores
0 → ~4.9k on the default song) render the venue near-BLACK with a single hard
yellow key light: members are dim yellow rim-silhouettes, the set is
invisible. W30's same-harness, same-song, same-shot captures were fully lit
warm interiors (e.g. `../W30-VISUAL-PASS/evidence/closeup_vocals_broken_props.png`
vs `evidence/closeup_vocals_blackout.png` — same `coop_v_n01` pin).

**Ambiguity declared.** This may be a lighting regression OR faithful venue
mood lighting newly live: W31 SET-PLAY revived the in-song mood stream, and if
venue lightpreset selection couples to it natively, the blackout-with-key look
could be the song's authored intense/verse lighting that W30's dead stream
never reached. Un-pinned gameplay runs 2/4/5 in the SAME venue show lit
presets cycling (red/warm/dark), so lighting is alive globally — the blackout
is specific to the pinned-closeup state/window. No retail closeup pair exists
to adjudicate.

**Evidence.** `evidence/closeup_vocals_blackout.png`,
`evidence/closeup_bass_keylight_only.png`,
`evidence/closeup_drums_sticks_silhouette.png`.
**Severity.** blocker for the closeup harness as a convergence gate (its
frames are no longer judgeable), regardless of faithfulness.
**Proposed probe.** Log the active lightpreset at the pinned frames (W30 vs
now, one boot each) before chartering anything.

## V5 — Results screen: artist line renders "j0", SOLO SCORE renders "GO" (non-numeric text corruption)

**Symptom.** On `coop_endgame_screen` after beastandtheharlot: the header line
under the song title — where the artist ("Avenged Sevenfold", correct in the
gameplay title card, run-2 `02_gameplay_first.png`) belongs — renders as
"j0"; the SOLO SCORE value renders as "GO" (not a number; HUD score during the
run was ~9.4k+ before the jump). "Previous Best: 0" and "0% EXPERT" render
fine, so digits per se work elsewhere on the panel.

**Evidence.** `evidence/endgame_results_j0_GO.png` (stable across a 5s settle
series). **Retail pair:** none (no results-screen ref — flagging for the
reference wishlist). Called on internal inconsistency: a score field showing
letters, an artist field showing "j0".

**Suspected layer.** Results-panel text binding (wrong token/locale string or
glyph remap on that panel's font), possibly score handoff into the results
flow after `{game jump}` (worth re-checking with a natural song end).
**Severity.** polish-high (the canonical post-song screen).

## V6 — Endgame vignette: giant deformed dark-green character + white shard planes behind the results UI

**Symptom.** The backstage vignette behind `coop_endgame_popups_screen` /
`coop_endgame_screen` renders a giant dark-green limb/torso crossing half the
frame (devil-horns hand identifiable at results-screen scale), white broken
shard-planes top-right, and a barely-lit deformed figure group — where retail
plays a normal-scale band celebration. Popups themselves (goals, "You are now
an Arena Rocker!", +fans meter) render correctly.

**Evidence.** `evidence/endgame_popup_giant_green_limb.png`,
`evidence/endgame_results_j0_GO.png` (backdrop). **Retail pair:** none.
**Suspected layer.** Most likely V1's class on the endgame stage (the vignette
chars play celebration body clips) + the deferred green-face lighting family —
recommend folding into the V1 lane as a second capture surface rather than
chartering separately. **Severity.** polish (behind UI, but every song end).

---

## Sub-observations (not chartered; single-frame or likely-authored)

- **part_difficulty backdrop char: detached white hand/finger fragments**
  floating beside the vignette girl's glove (`evidence/partdiff_backdrop_hand_shards.png`)
  — hands/SKEL family scent, shell context; single frame.
- Characters occasionally render fully UNLIT (black) against a lit wall
  (run2 `gameplay_010`, `evidence/bath_unlit_char_lit_wall.png`), and with
  strong magenta/purple skin under warm presets (run5 frames) — char-env vs
  venue-light split, same family as the deferred green-faces item; needs the
  retail closeup ref that item is already blocked on.
- Small white lattice quad floating top-left in `coop_k_*` closeup pins
  (visible in `evidence/closeup_drums_sticks_silhouette.png` — cosmetic,
  possibly an unlit light-grid mesh).
- run2 `gameplay_006`: a magenta guitar tangled inside the stacked-chairs set
  dressing — instrument prop at a wrong node, single occurrence.
- Hub camera at +2s frames signage, not the street (run 3) — the intro pan
  was still in flight; not a finding.

## Checked and CLEAN (matched retail / W30 baseline / no anomaly)

- **F3 button glyphs (W31 fix, 16th default): HOLDING** — song_select footer
  SELECT/NEXT HEADING glyphs colored (green/yellow), endgame CONTINUE/RESTART
  glyphs colored, no white lozenges anywhere in 5 runs.
- **Song select layout**: header, VIEWING count, section headings + 0/N-star,
  artist italics, album "?" placeholder — match Wii refs; "Player 1" gamertag
  holding. (Right-edge backdrop char = F7/W33, not re-reported.)
- **Note highway**: gems/HOPOs/sustains/SP phrases/multiplier/lane glow all
  match the retail highway refs on both songs; star pips fill progressively
  (F4's faithful behavior confirmed at 0-2 stars).
- **Score HUD**: top-right, increments; crowd meter present and filling.
- **Song-end chain**: jump → `coop_endgame_popups_screen` →
  `coop_endgame_screen` reaches the results flow (W31 song-end fixes holding
  up to the V2 crash on exit).
- **Hub street walkers**: upright, walking, no waist fans in the frames
  captured (Lane-D-closed family not re-opened by what I saw).
- **No mid-gameplay crash** in any of the 5 runs (crashes are V2/V3, both on
  user-driven transitions).

## Raw capture locations

- `/tmp/w31vp/closeup/` (34 frames + manifest/verdict, engine log
  `/tmp/rb3-bandcloseup-cap-40979.log`)
- `/tmp/w31vp/run2/` + `/tmp/w31vp/endgame/` (beastandtheharlot + endgame flow)
- `/tmp/w31vp/run4-ab/` (MIDIDRV opt-out A/B), `/tmp/w31vp/run5-default/`
- `/tmp/w31vp/hub/` (splash/hub + V3 engine.log)
