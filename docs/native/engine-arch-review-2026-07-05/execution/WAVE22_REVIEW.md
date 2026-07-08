# WAVE22_REVIEW — Fable pre-dispatch review (visual push: FOREARM / HUD / SWEEP)

**Reviewer:** Fable. **Date:** 2026-07-08. **Target:** `WAVE22_KICKOFF.md` (rb3 `26893122`).
Inputs verified against code at HEAD, not the kickoff's claims: `BandCharacter.cpp`,
`TrackPanel.cpp`, `BandScoreboard.cpp`, `BandStarDisplay.cpp`, `StarDisplay.cpp`,
`uidump_query.py`, `NATIVE_COMPAT_LEDGER.md`, `images/retail-screenshots/README.md`,
`git log --since=2026-07-02`, T2 M5 triage, R5 CLOSURE, WAVE21_CLOSEOUT Q7.

**VERDICT: DISPATCH-WITH-AMENDMENTS (A1–A9 binding).** The wave shape is right; one factual
error in the FOREARM charter (A1) flips binding-first from "cheap guard" to "leading
hypothesis," and one ledger find (A4) gives HUD a one-boot prime suspect.

## Q1 (R-A) — FOREARM binding-first: YES, and stronger than the kickoff argues

The kickoff (KICKOFF:41-42) says the arm is "compact-ish arm geometry that the torso rebind
already handles." **FALSE.** `RebindOutfitBonesToOwnSkeleton` is TORSO-WHITELIST-ONLY by
default: `strstr(mn, "trackjacket")||"vestdenim"||"plaidshirt"||"shred"`
(`src/system/bandobj/BandCharacter.cpp:1300-1305`); `gloves_resource`/`clearcoat_resource`
are NOT rebound — they plausibly still bind the shared static skeleton magnet (the exact
char-skinning-deform class, memory `acd9c19a`). A PERSISTENT top-center structure "unchanged
OFF→ON" is exactly what a wrong-instance static bind looks like.

No conflict with T2: `boneFallback=0` (M5:28-31) only excludes clamp/nonfinite — it does NOT
establish that the BOUND bone instance is player3's own. T2's "genuinely posed elevated" is a
statement about whichever instance the mesh binds; if that is the shared magnet, "elevated
static world" and "binding bug" are the same observation. Binding-first stands (WAVE19 errata
F4 already ordered this; hands lesson concurs).

**Exact discriminator (one boot, zero code):** `RB3_LOADBIND_PROBE=1` + `SKEL_REBIND_PROBE=1`
(`BandCharacter.cpp:94`, `:1279`) — for player3 × {`gloves_resource.mesh`,
`clearcoat_resource.mesh`} × slots {`bone_R-foreArm`,`bone_R-foreTwist1/2`,`bone_R-hand`}:
log `mesh->BoneTransAt(b)` (ptr + owning dir + world) vs `Find<RndTransformable>(name,false)`
on player3's dir — the same `own != bound` test the rebind loop already runs
(`BandCharacter.cpp:1316-1322`). Outcomes: bound≠own AND bound-world elevated AND own-world
at-arm ⇒ binding bug (arm-scoped rebind fix, see Q5); bound==own ⇒ binding exonerated, go
pose (kickoff's clip/IK/attachment/transpose list is fine, bilateral R-vs-L as specified).

## Q2 (R-B) — determinism: frame-exact matched pairs NOT guaranteed; gate on burst-ROI

Wave-19 T1 proved the frame-assignment residual exists without injected jitter (ambient
thread actors) — two boots will not produce byte-matched band frames even under
`RB3_FIXED_CLOCK=1`. But the float is PERSISTENT (M5: present across burst frames, unchanged
OFF→ON), so the gate doesn't need frame-exact pairs. **Protocol:** reuse
`t2_worldroi_burst.py` (RB3_FIXED_CLOCK=1 RB3_DRAWLOG_PROV=1; `uidump_query.py --roi` has
bones_in_roi, verified `scripts/native/uidump_query.py:151-190`). PRIMARY numeric gate,
robust to jitter: across an N-frame burst, flag-ON shows ZERO player3
gloves/clearcoat draws intersecting the above-heads strip (T2's ROI [620,278,140,34],
generalize to the y<~300 band-top band) — WHILE those meshes still draw with nonzero count
and bbox intersecting player3's body (anti-"fixed by disappearing" clause). SECONDARY:
R-foreArm vs L-foreArm world symmetry within the envelope other members show. E1 = human
review of burst PAIRS (not matched frames). This replaces KICKOFF:48-49's "matched-frame E1"
wording as the formal gate; per-member split (players 0-2 unchanged) falls out of the ROI
provenance owner field.

## Q3 (R-C) — HUD: both bugs likely STILL OPEN; confirm-then-fix is right; real anchors found

No commit since 2026-07-02 touches score-HUD position or star fill (`git log --since` sweep:
`512a1bde` = song_select/hub-ticker Y-anchor, `9a7c40eb` = album-art assembly, `8626729a` =
web score-screen exit — none gameplay-HUD). Keep confirm-then-fix + pivot clause.

**Anchors (give these to the lane verbatim):**
- Score feed: `src/band3/bandtrack/TrackPanel.cpp:113` (`mScoreboard =
  mTrackPanelDir->Find<BandScoreboard>("scoreboard")`), `:607-618` (alternating-poll
  `SetScore` / `SetNumStars`, star path gated on `TheGame->mProperties.mShowStars`,
  `GameMode.cpp:76` `show_stars`).
- Score display: `src/system/bandobj/BandScoreboard.cpp:15` (SetScore), `:38`
  (SetNumStars → mStarDisplay).
- Star fill: `src/system/bandobj/BandStarDisplay.cpp:16` (`SetNumStars`: fill =
  `mStarSweepAnims[i]("sweep.mnm")->SetFrame(1,1)` + `full.trig` EventTrigger +
  `stars_offset.tnm` Animate), `:78` (`SetupStars`: star0..star4 RndDirs). "One outlined
  star, never fills" hypotheses, ranked: (a) SetNumStars never called with rising f
  (`mShowStars` false natively, or `GetNumStarsFloat()`≈0); (b) `sweep.mnm`
  SetFrame / EventTrigger are render-inert natively → fills never show; (c) the visible
  single star is simply the initial state under (a)/(b) since `mStars[i]->SetShowing(true)`
  only reveals successors on fill.
- **Position prime suspect (A4):** ledger row `RB3_APPLY_HANDLER_FIX_OFF` — a DEFAULT-ON
  workaround described as "single-player scoreboard/applause **right/left.grp x-translation
  neutralization** (K9)" (`NATIVE_COMPAT_LEDGER.md:101`). Neutralizing the scoreboard's
  right-group x-translation is a direct candidate mechanism for "HUD pill mid-screen instead
  of top-right." One boot with `RB3_APPLY_HANDLER_FIX_OFF=1` A/Bs it before any new code.
  If confirmed, the fix is a re-scope of K9, not a new anchor constant. Fallback family:
  authored-anchor nudge pattern per `512a1bde` (RB3_HUB_TICKER_YFIX precedent).

## Q4 (R-D) — SWEEP ground truth: swap the kickoff's primary gameplay refs

README platform note: layout identical Wii/360/PS3; Wii most faithful. The kickoff
(KICKOFF:60-61) names `gameplay_highway_wikipedia.jpg` (400×225 LOW-RES 360/PS3, composed
multi-instrument HUD ≠ single-player native runs) + `fandom_gameplay_guitar.png`. **Primary
should be the Wii trio** `yt_qRagnZCIMzk_gameplay_{guitar,drums,drums_starpower}.png`
(1280×720; `drums_starpower` shows the FILLED 5-star HUD = the star-meter oracle image);
fandom pair secondary; wikipedia tertiary (layout only). main_hub: `yt_mhKNp9uAT48_*`
(360/PS3, valid layout). song_select: `yt_qRagnZCIMzk_*` (Wii). `yt_qSRJ8HHPXzM_*` are
camera-of-TV — geometry-distorted, NEVER pixel-diff, setlist-view reference only. All are
YouTube-compressed: diff layout/position/presence, not color calibration.
**Shipped-defaults exclusion grep target:** `docs/native/engine-arch-review-2026-07-05/
NATIVE_COMPAT_LEDGER.md` (default=on rows) + `execution/README.md` "defaults now" wave lines
+ the memory's don't-re-find list (C8 skin family, venue exposure, offline chrome) + the
2026-07-02 items already FIXED (`512a1bde` hub-ticker/song_select-overlap, `9a7c40eb`
album-art). `/tmp/visdiff-20260702` still exists but is PARTIAL (3 entries) — re-capture;
treat the memory doc, not the tmp dir, as the prior baseline.

## Q5 (R-E) — hands-closure safety: REAL hazard; bone-slot-scoped guardrail required

`gloves_resource.mesh` binds R+L FINGER bones (M5:14). A wholesale gloves rebind re-points
finger slots into the coherent-own-basis regime that Waves 20-21 measured TORN (the
torso-only scope exists precisely because "high-bone head/hands/face ... shards",
`BandCharacter.cpp:1290-1293`; `RB3_SKEL_REBIND_FULL` is the known-broken fail-red control).
That would be a de-facto hands re-charter. **Guardrail (binding):** (i) `clearcoat_resource`
(arm bones only per M5) may be whitelisted mesh-level; (ii) `gloves_resource` must be
BONE-SLOT-scoped — rebind only slots named `bone_R-foreArm`/`foreTwist1`/`foreTwist2`/
`bone_R-hand` (+ L twins if measured equally wrong), NEVER finger-named slots; (iii) no
edits in `RebindHeadHandsAtRest` or the mitten path; never set `RB3_SKEL_REBIND_FULL`;
(iv) E1 includes a hand-region crop asserting finger rendering is UNCHANGED vs flag-OFF
(mitten-ON default); (v) success claims limited to float removal — no hands-articulation
language; CLOSURE cited only as the backlog key. The mitten-OFF "arguably worse" absorption
check (KICKOFF:52-53) is fine and cheap — M5 already recorded OFF→ON invariance.

## Q6 — scope/overlap: disjoint by FILE, but the kickoff's HUD grant line is wrong

"HUD is band3 UI" (KICKOFF:23) is inaccurate: the score/star classes live in
`src/system/bandobj/` (BandScoreboard/BandStarDisplay/StarDisplay — Wii-matched decomp
units). FOREARM also writes `src/system/bandobj/` (BandCharacter.cpp). No collision — the
FILE sets are disjoint (BandCharacter.cpp vs BandScoreboard/BandStarDisplay/StarDisplay +
TrackPanel.cpp) — but state the file-scoped grants explicitly and give HUD the same
Wii-inertness obligations FOREARM already carries (A6). SWEEP is read-only scripts+docs;
no collision. Three lanes (2 fix + 1 discovery) matches proven wave shapes — right-sized.

## Q7 — gates/lints: 792 right; three additions

drawlog-792 flag-OFF byte-identical is the correct standing regression gate for both fix
lanes. Additions: (a) HUD gets `batch_objdiff==baseline` on any touched `src/system` unit +
`#ifdef HX_NATIVE` guards (kickoff gives this only to FOREARM); (b) FOREARM's formal gate is
the Q2 burst-ROI numeric (presence/absence + still-drawn + bilateral), E1 demoted to human
burst-pair review; (c) HUD retail-anchored E1 = drawlog-bbox check (scoreboard bbox center in
the right-third / top band vs the Wii guitar shot) + star-fill oracle driven past 1.0 stars
via the jump-to-end harness (`scripts/native/song-end-test.py` pattern) asserting fill draws
appear; (d) rb3-tests 116/0 on any BandCharacter.cpp touch. Lint 9 stands: both fix lanes
confirm their TU compiles into rb3-native at step 0. `keyboard-to-gameplay.py` +
`song-select-capture.py` exist (verified); `--roi` verified.

## AMENDMENTS (binding)

- **A1 (HIGH, FOREARM):** delete "the torso rebind already handles" (KICKOFF:41-42) — the
  whitelist `BandCharacter.cpp:1300-1305` excludes gloves/clearcoat. Binding = LEADING
  hypothesis. Run the Q1 slot-identity discriminator (one boot, probes only) before any code.
- **A2 (HIGH, FOREARM):** Q5 guardrail verbatim — clearcoat mesh-level OK; gloves BONE-SLOT
  scoped, never finger slots; no RebindHeadHandsAtRest/mitten edits; finger no-change crop
  in E1; float-removal claims only.
- **A3 (MED, FOREARM):** replace matched-frame E1 with the Q2 burst-ROI gate + bilateral
  secondary; reuse `t2_worldroi_burst.py`.
- **A4 (HIGH, HUD):** step 0 = one boot with `RB3_APPLY_HANDLER_FIX_OFF=1` (ledger:101, K9
  scoreboard right/left.grp x-translation neutralization) as the mid-screen prime suspect,
  BEFORE new anchor code.
- **A5 (MED, HUD):** adopt the Q3 anchor pack; primary retail GT = Wii
  `yt_qRagnZCIMzk_gameplay_*` incl. `drums_starpower` for the filled-star state; wikipedia
  shot demoted to layout-tertiary.
- **A6 (MED, HUD):** extend the HUD grant to the named `src/system/bandobj` files with
  Wii-inertness gates (HX_NATIVE + batch_objdiff==baseline + rb3-tests 116/0).
- **A7 (MED, SWEEP):** exclusion grep = NATIVE_COMPAT_LEDGER default-on rows +
  execution/README "defaults now" lines + fixed items `512a1bde`/`9a7c40eb` + C8/venue/
  offline-chrome; re-capture rather than trusting the partial `/tmp/visdiff-20260702`.
- **A8 (LOW, SWEEP):** trust table per Q4; camera-of-TV never pixel-diffed; diff
  layout/presence not color.
- **A9 (LOW, process):** both fix lanes' checkpoints record the discriminator/A-B verdict
  (`/tmp/wave22-checkpoints/<lane>.json`) before proceeding to fix code, so a re-run skips
  the boot.

**FINAL: DISPATCH-WITH-AMENDMENTS.** No lane blocked; A1/A2/A4 are load-bearing — dispatch
with the kickoff's COORDINATOR ACCEPTANCE block citing them.
