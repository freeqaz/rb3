# Wave 23 — Kickoff (VISUAL PUSH cont.: hub grade ∥ hub crowd ∥ FOREARM pose-driver)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `WAVE22_CLOSEOUT_REVIEW.md` Q6 (Wave-23 ordering) + `W22-SWEEP/STATUS.md` (the ranked
menu). Owner directive: *"keep pushing through the visual bugs we can find."*
Engine pin `4a72845`. THIRTEEN defaults ON. **FIX + DISCOVERY wave** — flag-gated fixes
default-OFF; coordinator-only default decisions at close-out.

## COORDINATOR ACCEPTANCE (from `WAVE23_REVIEW.md`, rb3 `1cdff0ec` — DISPATCH-WITH-AMENDMENTS)

**A1–A9 adopted verbatim and BINDING; the review file overrides this kickoff wherever they
differ.** Two lanes materially re-anchored (A1 GRADE mechanism, A4/A5 CROWD target). Headlines:

- **A1 (GRADE mechanism — the kickoff's 2-flag A/B was incomplete):** the hub backdrop rides
  the shipped world.cam VENUE-LIGHT path (wave-5 "menu-contrast Fix 3",
  `Rnd_Wgpu_RB3.cpp:1196-1218`); if it stops engaging the frame falls to the flat
  "1.0 white dir + 0.45 grey ambient" flood (`:1327-1335`) = exactly SWEEP's wash.
  Discriminator = FOUR arms + probe: `RB3_PP_OFF=1`, `RB3_UI_POST_GRADE_OFF=1` (dedupe,
  expect NULL on backdrop — its direction re-washes, can't darken), **`RB3_VENUE_LIGHT_OFF=1`
  (PRIME: if the hub look doesn't change, venue-light isn't engaging on the hub)**,
  `RB3_WASH_PROBE=1` (engagement/grey-key digest, `:1119-1145`); optional
  `RB3_VENUE_FALLBACK_FIX=1` (`:1327-1340`).
- **A2 (GRADE numeric):** re-measure the wave-5 hub 3x3 contrast metric ON/OFF vs the history
  (2.6:1 → target ~10:1) so regression-vs-residual is decided NUMERICALLY. No-fix branch
  reframed: not "Wii might be brighter" (no Wii hub GT) but "still meets the wave-5 tuned
  contrast envelope → residual, close as known-limitation".
- **A3 (GRADE guardrail):** NO edits in game.cam/`kGamePlaying` branches; gameplay matched A/B
  + UIGRADE flush-count parity + song_select parity band (RB3_UI_POST_GRADE 1.110→1.049 caveat
  must not worsen) + drawlog-792 + batch_objdiff; new knob default-OFF; thirteen defaults intact.
- **A4 (CROWD re-anchored — NOT main_hub.milo):** the street scene is
  `world/vignette/shell/gen/sv3_a.milo_xbox` (`config/vignettes.dta:4-28`, fresh-profile pick);
  the walkers ARE authored there: `crowd_chars.grp`/`characters.grp` +
  `sv3/a/streetslomo/streetslomo{,_clips,_ao}.milo` + shared `world/shared/gen/vignette_chars.milo_xbox`,
  env `street_slomo_char.env`. Census these BY NAME in the live tree first
  (`rb3_http_handlers.cpp:690` pos-dump + uidump ROI).
- **A5 (CROWD tree — crowd-rebind is MISDIRECTED):** `strings sv3_a` has ZERO WorldCrowd →
  `RebindCrowdCharBonesToOwnSkeleton` never touches these. Discriminator = (a1) sub-/shared-milo
  load failure | (b) loaded-but-not-drawn (showing/draw-gate) | (c) mis-posed/off-screen | (d)
  loaded-but-never-animated (CharClipSet/driver not polled — cf hack-audit "chars.milo + CharSync
  Poll"). (a2 "not authored") is PRE-REFUTED. `RB3_NO_CROWD_REBIND` = 1-boot dedupe ONLY.
- **A6 (CROWD guardrail):** NO `BandCharacter.cpp` edits; NO `Crowd.cpp:884-1000` edits absent
  WorldCrowd proof; any fix = a NEW seam, vignette-dir-scoped (sv3_a/streetslomo name-gated),
  default-OFF.
- **A7 (FOREARM — no probe code needed for name-match):** `BAND_ANIM_PROBE='*'` wildcard
  already works (`BandCharacter.cpp:672`, prints real member Name() + playing clip) — run `'*'`
  FIRST (Wave-22's "player3" guess was wrong), set `BAND_ANIM_BONE=bone_R-foreArm.mesh`. The
  real gap = the `%30` throttle (`:700`) misses camera-cut frames → amend to EVENT-triggered
  emit (bone world-y > threshold), probe-only edit, allowed under lint 10.
- **A8 (FOREARM):** do NOT gate discovery on exact-frame reproducibility (camera cuts ±6 noise);
  long fixed-clock burst + event-triggered probe is the instrument. Deliverable = named driver +
  Wave-24 charter. If the driver is the walk-on/count-in freeze class (`67e87ae1`), note memory's
  caveat that count-in thin-geo shards were a SEPARATE pose-independent residual — don't conflate
  without probe evidence.
- **A9 (wave-wide GT):** `yt_mhKNp9uAT48_*` is 360/PS3 — content/layout authoritative (native
  loads the same 360 assets), absolute color CAPPED → E1 uses structural/relative metrics ONLY.

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either. Hands are CLOSED — no lane may
  re-charter the hands-finger family / re-attempt the 8 dead cells / the reskin. FOREARM
  binding is CLOSED (exonerated Wave 22) — Lane FOREARM is DISCOVERY-only, HARD STOP before
  any fix code.

## Shape

Three lanes from the SWEEP menu, in the close-out's ordering: the cheapest discriminator
first (S2 hub grade, which may re-scope S4), the ROI-confirmed S1 hub crowd, and the
upgraded FOREARM pose-driver as discovery-only. All independent; expected file-disjoint
(GRADE is engine post/grade + rb3 flags, CROWD is bandobj/world crowd + rb3, FOREARM is
discovery scripts + docs). S5 combo-glow and S3/S4 deferred per the close-out.

## Lanes

**Lane GRADE — hub over-bright/washed menu grade (S2; Opus):**
Lane dir `execution/W23-GRADE/`. SWEEP S2: main_hub reads opaque-bright/washed vs retail's
dark-night neon glow (`yt_mhKNp9uAT48_*` GT); neon signs + mats are correct/present, it's the
GRADE. **Discriminator FIRST (cheapest, per close-out):** A/B boot main_hub with
`RB3_PP_OFF` and `RB3_UI_POST_GRADE_OFF` (and check the shipped menu-lighting default — grep
the twelve/thirteen defaults for the menu-lighting flag) to localize whether the wash is (a)
post-process/tonemap, (b) the UI post-grade, or (c) menu scene lighting/exposure. CAVEAT
(SWEEP): this is exposure-family-ADJACENT but a DISTINCT menu scene — do NOT assume the venue
exposure fix applies; confirm the mechanism on main_hub specifically. If it localizes to a
native-only grade bug: fix flag-first default-OFF, E1 vs the retail GT (darker night glow,
neon reads as emissive-on-dark not flat-bright). If it's authored/faithful (retail Wii really
is this bright, or the GT is a different console/era): report NO-FIX with the A/B evidence and
close S2. Gates: matched-frame E1 vs GT, drawlog-792 flag-OFF byte-identical, batch_objdiff==
baseline on any touched src/system unit, no regression to gameplay grade (the venue-lighting +
UI-post-grade defaults must stay intact). NOTE: this re-scopes S4 (player1 avatar crop) —
report whether the grade finding changes the S4 assessment.

**Lane CROWD — main_hub mid-street crowd figures absent (S1; Opus):**
Lane dir `execution/W23-CROWD/`. SWEEP S1 (ROI-confirmed): retail main_hub shows ~3 walking
crowd silhouettes in the center-street band; native draws ZERO skinned draws there (only
static architecture). **Discriminator FIRST:** dump ALL owners/meshes in the hub `.milo`
scene (reuse the SWEEP ROI machinery + `uidump_query.py`) — are the crowd figures (a) absent
from the loaded scene entirely (load/asset gap), (b) present but not drawn (a
draw-gate/culling/showing issue), or (c) present-but-mis-posed/off-screen (the crowd-rebind
family — cf `WorldCrowd`, the char-skinning crowd path)? Check the crowd-rebind family FIRST
per the close-out. If it's a tractable draw-gate or rebind: fix flag-first default-OFF, E1 (3
walking figures appear in the center-street band, non-regressing to the rest of the hub). If
it's a deep asset/load gap: narrow + report + hand off (don't force a fix). Gates: matched E1
vs GT, drawlog-792 flag-OFF byte-identical, batch_objdiff==baseline, crowd oracle untouched
(the gameplay crowd must not regress — this is the MENU crowd).

**Lane FOREARM — the pose-driver, DISCOVERY-ONLY (upgraded MED; Opus):**
Lane dir `execution/W23-FOREARM/`. Wave-22 EXONERATED binding (own==bound at draw) and re-rated
this MED (recurs on in-song band-closeup camera cuts + count-in; the exploded arm/hand
spike-fans in `W22-HUD/evidence/` + the flip capture). **HARD STOP before any fix code — this
lane produces a NAMED DRIVER + a fix charter for Wave 24, nothing more.** Find what poses
`bone_R-foreArm` (and its bilateral L twin — the float is bilateral) to world y≈+182 on
camera-cut frames while body/upperArm stay at y≈0: clip driver? IK? the walk-on/count-in
pose-freeze class (`67e87ae1`, memory `walkon_countin_pose`)? a camera-cut-triggered pose
reset? Use BAND_ANIM_PROBE (the Wave-22 handoff noted the member-name match was broken — FIX
that first: the outfit-dir name didn't match "player3"/"player"), the T2 ROI machinery, and a
matched camera-cut-frame capture. Deliverable: the driver NAMED + a Wave-24 fix charter
(what to change, match-neutrally if possible; is it the same freeze class as walk-on).
Binding stays CLOSED (do not re-open); NO code edits to the skinning/rebind/mitten paths;
success = a named driver, not a fix.

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out).
Checkpoints `/tmp/wave23-checkpoints/<lane>.json` — check-first, write-before-return, update
every milestone; fix lanes checkpoint the discriminator verdict BEFORE fix code. PLAN/STATUS
under `execution/<KEY>/`. Evidence committed or it doesn't exist. New flags default-OFF; NO
default flips, NO pin bumps by lanes. Refuted/closed flags UNSET. THIRTEEN defaults stay ON.
Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-settling, pgid-only cleanup. Build
under `/tmp/rb3-native-build.lock` or own worktree. Stage only your own files by path; NEVER
`rb3_session_trace.cpp` / engine `FxSendNative.cpp`.

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified** — CROWD binding claims are pointer/dir; FOREARM
  compares bone WORLDS (bilateral); GRADE is pixel/grade not geometry.
- [x] **2. Split by population** — GRADE per-scene (menu vs gameplay); CROWD per-owner; FOREARM
  per-member + R/L.
- [x] **3. No unvalidated oracles** — E1 matched-frame vs retail GT is decisive; GRADE/CROWD
  discriminator-first before any fix; FOREARM is discovery (no fix, no oracle needed).
- [x] **4. Shipped-flag contradiction grep** — GRADE must not regress the venue-lighting/
  UI-post-grade defaults; CROWD must not touch the gameplay crowd oracle.
- [x] **5. Grants** — GRADE engine post/grade + rb3 flags; CROWD bandobj/world + rb3; FOREARM
  discovery scripts + docs only.
- [x] **6. Option table before 2nd fix attempt** — GRADE/CROWD do discriminator BEFORE fix;
  FOREARM is discovery-only (no fix); hands/binding closures restated.
- [x] **7. Evidence committed** — A/B captures, retail-vs-native crops, ROI queries.
- [x] **8. Flag hit-counts on negatives** — CROWD reports owner/draw counts; GRADE A/B deltas.
- [x] **9. Flavor-membership grep** — each fix lane step 0: verify edited TU compiles in.
- [x] **10. Instruments before fixes** — all three lead with a discriminator/instrument.

## Risks / open questions for the reviewer

- **R-A (GRADE):** is main_hub's grade path the SAME as the venue/UI-post-grade defaults or a
  distinct menu path? Which flags actually gate it (verify `RB3_PP_OFF`/`RB3_UI_POST_GRADE_OFF`
  exist + apply to the menu scene)? Is the `yt_mhKNp9uAT48_*` GT trustworthy for grade (it's
  360/PS3 — could the Wii menu be genuinely brighter)?
- **R-B (CROWD):** is the hub crowd a skinned-char crowd (WorldCrowd/char-skinning family) or a
  simpler sprite/prop? Give the lane the real hub `.milo` + crowd draw-site anchors if findable.
- **R-C (FOREARM):** is BAND_ANIM_PROBE's member-name match fixable cheaply, and is a matched
  camera-cut frame reproducible under fixed-clock (Wave-22 said the float is on camera cuts —
  are those deterministic enough to capture before/after)?
- **R-D:** any overlap risk between GRADE (post/grade) and the shipped venue-lighting/
  UI-post-grade defaults that could cause a cross-regression? Bless the guardrail.
- **R-E:** does CROWD's "crowd-rebind family first" risk touching the char-skinning path that
  the hands/FOREARM closures depend on? Scope guardrail.
