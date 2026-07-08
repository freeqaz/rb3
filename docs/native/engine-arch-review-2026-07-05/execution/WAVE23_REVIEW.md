# WAVE23_REVIEW — Fable pre-dispatch review (GRADE ∥ CROWD ∥ FOREARM-discovery)

**Reviewer:** Fable. **Date:** 2026-07-08. **Target:** `WAVE23_KICKOFF.md` (97f50c4a).
Inputs verified against code/assets, not trusted from the kickoff: engine `4a72845` (== pin,
`native/CMakeLists.txt:74`), `RB3PostProc.cpp`, `Rnd_Wgpu_RB3.cpp`, `Crowd.cpp`,
`BandCharacter.cpp`, the extracted 360 assets, `images/retail-screenshots/README.md`,
`uidump_query.py`/`song-select-capture.py`/`drawlog-golden.py` (all exist).

**VERDICT: DISPATCH-WITH-AMENDMENTS.** All three lanes are sound in scope and ordering, but
the GRADE discriminator misses the most likely mechanism (venue-light non-engagement →
flat-fallback flood), and CROWD's "crowd-rebind family first" is asset-refuted — the hub crowd
is NOT a WorldCrowd. Real anchors below so neither lane burns a day rediscovering them.

## Q1 — GRADE (R-A/R-D): distinct-but-adjacent path; the kickoff's A/B menu is incomplete

- **Flags exist and apply to main_hub.** `RB3_PP_OFF` (engine `RB3PostProc.cpp:239`,
  presence-truthy) disables the whole postproc intermediate; menus render through it and grade
  once at `BandRnd::EndFrame`, so it applies. `RB3_UI_POST_GRADE_OFF` (`RB3PostProc.cpp:273`)
  opts out of the default-ON menu UI grade-exemption (Wave-14 flip) — **but its direction is
  wrong for S2**: setting it re-WASHES the UI text; it cannot darken the backdrop. Expect a
  null result on the backdrop; keep it as a 1-boot dedupe only.
- **The hub backdrop IS on the shipped venue-light path.** The render-polish wave-5
  "menu-contrast Fix 3" comment (`Rnd_Wgpu_RB3.cpp:1196-1218`) is EXPLICITLY about the menu
  hub: ambient floors pulled down (`RB3_VENUE_AMBIENT_FLOOR` 0.008 / `RB3_VENUE_AMBIENT_CLAMP`
  0.09 / `RB3_VENUE_GREY_KEY` 0.22, `:1217-1219`) so authored point lights (lamppole/theater
  spots) carry the scene — measured then at hub 3x3 contrast ~2.6:1 vs retail ~10:1. It applies
  only when the world.cam venue path ENGAGES (`sVenueLightEnabled`, `:1113-1117`). If the hub
  env stopped engaging (miss reasons: venue_off / no_env / `mAmbientFogOwner==null`), frames
  fall to the flat default — **"1.0 white dir + 0.45 grey ambient" flood
  (`Rnd_Wgpu_RB3.cpp:1327-1335` comment)** — which is exactly SWEEP's "bright grey-green
  daytime feel". The kickoff's two-flag A/B cannot see this; `RB3_WASH_PROBE=1`
  (`:1119-1145`) exists precisely to tag the engagement decision + grey-key fallback per frame.
- **So the S2 mechanism table is (a) PP grade wash** (the grade also DESATURATES: wgsl `:222`
  mid_sat 0.026-0.079 graded vs 0.389-0.746 PP_OFF — matches "washed"), **(b) venue-light
  non-engagement/regression on the hub env, (c) authored/no-fix**. UI post-grade is (d),
  near-certainly null.
- **GT trust:** `yt_mhKNp9uAT48_menu_hub.png` is **360/PS3** (README:26); README:10-14 caps it
  to layout-valid, Wii-faithful only for color. BUT native consumes the SAME 360 assets
  (`ui/main/gen/main_hub.milo_xbox` + the sv3_a vignette below), and wave-5 already used this
  GT family as the hub contrast target — so it is usable for RELATIVE/structural judgments
  (contrast ratios, neon-vs-backdrop), not absolute brightness. The no-fix branch should be
  reframed: not "Wii might be brighter" (no Wii hub GT exists to check) but "current build
  still meets the wave-5 tuned contrast envelope → S2 is residual, close as known-limitation".
- **Cross-regression guardrail (R-D):** the shipped defaults live in exactly the TUs GRADE will
  touch. Required: no edits inside game.cam/`kGamePlaying`-gated branches; matched gameplay
  ON/OFF capture; UIGRADE's flush-count parity check (gameplay flush counts equal); song_select
  must not fall further below its parity band (known caveat 1.110→1.049, classification.json
  `RB3_UI_POST_GRADE` row); drawlog-792 flag-OFF; batch_objdiff==baseline. New knob default-OFF.

## Q2 — CROWD (R-B): real anchors found; NOT a WorldCrowd

- **The hub street scene is a shell vignette, not main_hub.milo.** `main_hub.milo_xbox` has
  ZERO city/street/neon strings (UI panel only). The scene =
  **`world/vignette/shell/gen/sv3_a.milo_xbox`** — selected for a fresh profile by
  `config/vignettes.dta:4-28` (sv3 backdrops, `TRUE → ("sv3_a")`); it carries the 56
  `neonsigns_*` hits + `sidewalk_*` meshes matching SWEEP's ROI provenance.
- **The crowd actors exist in the asset chain native loads:** sv3_a contains
  **`crowd_chars.grp`**, **`characters.grp`**, `street_slomo_char.env`, CharClip/CharClipSet/
  CharInterest/CharLipSync objects, sub-milos **`sv3/a/streetslomo/streetslomo{,_clips,_ao}.milo`**
  and the shared **`world/shared/gen/vignette_chars.milo_xbox`** (exists). "streetslomo" = the
  slow-mo walking pedestrians in the GT. So "(a) absent from the loaded scene entirely" splits:
  (a1) sub-milo/shared-milo load failure (plausible) vs (a2) actors absent from the asset
  (REFUTED — they're in the file). Add **(d) present-but-unanimated/hidden** — the vignette
  chars' CharClipSet/driver may not Poll natively (cf the hack-audit blocked item
  "chars.milo + CharSync Poll").
- **NOT WorldCrowd:** `strings sv3_a.milo_xbox | grep -c WorldCrowd` = **0**. The
  `RebindCrowdCharBonesToOwnSkeleton` path (`src/system/world/Crowd.cpp:929-932`,
  `RB3_NO_CROWD_REBIND`) hangs off WorldCrowd::Draw3DChars — it never touches these actors.
  "Check the crowd-rebind family FIRST" is therefore a misdirected primary; demote to a 1-boot
  dedupe A/B. Primary discriminator = live-tree census of the sv3_a dir: `{rb3_pos_dump}`
  (`native/src/rb3_http_handlers.cpp:690`) + `uidump_query.py --roi` + `RB3_DRAWLOG_PROV=1`,
  asking "do Character/mesh objects from crowd_chars.grp exist, are they showing, are their
  clips playing".
- **GT-artifact risk:** low. The GT frame is ~12s into a boot (same state native captures), the
  360 asset native loads authors the walkers, and the vignette is the persistent hub ambience.
  Residual risk: the walkers only animate after a dwell/attract trigger — cover with a
  multi-second dwell capture, not a single frame.

## Q3 — CROWD hands-closure safety (R-E): safe with an explicit no-touch list

The hub crowd path shares no code with the hands/FOREARM closures **if** the lane keeps to:
NO edits to `BandCharacter.cpp` (RebindHeadHandsAtRest / mitten / clamp / member rebind); NO
edits to `Crowd.cpp`'s gameplay rebind block (`:884-1000`) unless the discriminator proves
WorldCrowd involvement (asset evidence says it won't); any new rebind/show fix = a NEW seam
scoped to the shell-vignette dir (name-gated on the sv3_a/streetslomo dirs), default-OFF.
`RB3_NO_CROWD_REBIND` may be SET only for the 1-boot dedupe, never shipped set.

## Q4 — FOREARM discovery (R-C): scope right; probe fix is trivial; fix the throttle too

- **Member-name match: already solvable with zero code.** `BAND_ANIM_PROBE='*'` wildcard is
  supported (`BandCharacter.cpp:672`) and the emit prints the real member name (`:717`,
  `member='%s'`) plus the **playing clip name** — run `'*'` first, learn the real Name()s
  (Wave-22 guessed "player3"; Name() is evidently something else), then filter. Set
  `BAND_ANIM_BONE=bone_R-foreArm.mesh` (default is `bone_R-upperArm.mesh`, `:677`).
- **The real instrument gap is the throttle:** `emit = (frameCt++ % 30) == 0`
  (`BandCharacter.cpp:700`) will MISS most camera-cut frames. Amend: event-triggered emit
  (emit when `moved` is large or bone world-y exceeds a threshold, e.g. >50) — a probe-only
  edit, in-scope for a discovery lane under lint #10.
- **Determinism:** camera cuts are not exactly reproducible (ERR-4: ±6 burst noise), so do NOT
  gate discovery on a matched identical-frame before/after. The deliverable (driver NAMED)
  needs the clip/driver identity AT the event, which the event-triggered probe prints; a long
  fixed-clock burst suffices. If the driver turns out to be the walk-on/count-in freeze class
  (`67e87ae1`), note memory's caveat that count-in thin-geo shards were called a SEPARATE
  pose-independent residual — don't conflate without probe evidence.
- **HARD STOP framing is correct** and correctly restated (binding CLOSED, no skinning/rebind/
  mitten edits, output = named driver + Wave-24 charter). A match-neutral fix this wave is NOT
  plausible: no candidate mechanism is even named yet — keep discovery-only.

## Q5 — Sizing/ordering: RIGHT

S2-GRADE first (pure env-flag boots, no code, re-scopes S4) → S1-CROWD → FOREARM-discovery is
the close-out's ordering and survives review. Lanes are file-disjoint (engine postproc/renderer
vs rb3 world/vignette vs probe+docs). S5 stays deferred (the close-out's "piggyback driven-combo
confirm" was dropped — acceptable; don't bolt it onto an unrelated lane). S3 (no GT) and S4
(pending GRADE) correctly deferred. No merges needed.

## Q6 — Gates: right, with three additions

Kickoff's gates (matched-frame E1 vs GT, drawlog-792 flag-OFF, batch_objdiff==baseline, crowd
oracle untouched, venue/UI-post-grade defaults intact) are correct. Add: (1) GRADE — UIGRADE
flush-count parity + song_select parity-band non-worsening (Q1); (2) GRADE — re-measure the
wave-5 hub 3x3 contrast metric ON/OFF so the verdict is numeric, not eyeballed; (3) CROWD —
if any src/system/world TU is touched, one gameplay-venue crowd capture A/B (the gameplay
crowd renders via the same engine palette path the W2.3 rebind protects). E1 for GRADE must be
relative/structural (Q1 GT caveat), not absolute pixel luminance.

## Q7 — GT trustworthiness across the wave

`yt_mhKNp9uAT48_*` (hub): 360/PS3, README:26 — content/layout authoritative (native loads the
same 360 milo_xbox files), absolute color capped. Verdict impact: none for CROWD (content
question), GRADE must use relative metrics + the reframed no-fix branch (Q1). Gameplay/song
select GTs (`yt_qRagnZCIMzk_*`) are true Wii captures and stay fully trusted for the
cross-regression checks. S3 remains unfixable-by-GT (none exists) — deferral stands.

## AMENDMENTS (binding)

- **A1 (GRADE):** extend the discriminator matrix to FOUR arms + one probe boot:
  `RB3_PP_OFF=1`, `RB3_UI_POST_GRADE_OFF=1` (dedupe, expected null on backdrop),
  `RB3_VENUE_LIGHT_OFF=1` (if the hub look does NOT change, venue-light is not engaging on the
  hub — prime suspect), and `RB3_WASH_PROBE=1` (read the engagement/grey-key digest,
  `Rnd_Wgpu_RB3.cpp:1119-1145`). Optional fifth: `RB3_VENUE_FALLBACK_FIX=1` (`:1327-1340`) —
  if it alone darkens the hub, the flood is the flat-default fallback.
- **A2 (GRADE):** before chartering any fix, re-measure the wave-5 menu-contrast 3x3 metric
  (`Rnd_Wgpu_RB3.cpp:1196-1218`, floors `:1217-1219`) on the current build; report vs the
  2.6:1→~10:1 history so regression-vs-residual is decided numerically.
- **A3 (GRADE guardrail):** no edits inside game.cam/`kGamePlaying` branches; gameplay matched
  A/B + flush-count parity + song_select parity band + drawlog-792 + batch_objdiff; new knob
  default-OFF; the thirteen defaults untouched.
- **A4 (CROWD anchors):** scene = `world/vignette/shell/gen/sv3_a.milo_xbox`
  (`config/vignettes.dta:4-28`); actors = `crowd_chars.grp`/`characters.grp` +
  `sv3/a/streetslomo/streetslomo{,_clips,_ao}.milo` + `world/shared/gen/vignette_chars.milo_xbox`;
  env `street_slomo_char.env`. Census these BY NAME in the live tree first
  (`rb3_http_handlers.cpp:690` pos-dump + uidump ROI).
- **A5 (CROWD tree):** discriminator = (a1) sub-/shared-milo load failure | (b) loaded but not
  drawn (showing/draw-gate) | (c) mis-posed/off-screen | (d) loaded but never animated
  (CharClipSet/driver not polled — cf hack-audit "chars.milo + CharSync Poll"). (a2) "not
  authored" is pre-refuted. `RB3_NO_CROWD_REBIND` A/B = 1-boot dedupe only, not "first".
- **A6 (CROWD guardrail):** no `BandCharacter.cpp` edits; no `Crowd.cpp:884-1000` edits absent
  proof of WorldCrowd involvement; any fix = new seam, vignette-dir-scoped, default-OFF.
- **A7 (FOREARM):** run `BAND_ANIM_PROBE='*'` + `BAND_ANIM_BONE=bone_R-foreArm.mesh` (no code
  needed for the name-match); amend the `%30` throttle (`BandCharacter.cpp:700`) to
  event-triggered emission (probe-only edit, allowed).
- **A8 (FOREARM):** do not require exact-frame reproducibility; long fixed-clock burst +
  event-triggered probe is the acceptance instrument. Deliverable unchanged (named driver +
  Wave-24 charter).
- **A9 (wave-wide):** E1 comparisons against `yt_mhKNp9uAT48_*` use structural/relative
  metrics only; absolute-color deltas vs a 360/PS3 capture are not actionable evidence.

**FINAL: DISPATCH-WITH-AMENDMENTS** — A1/A4/A5/A7 are the load-bearing ones; the rest are
guardrail formalizations of what the kickoff already intends.
