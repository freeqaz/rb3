# Wave 25 — Kickoff (FIX wave: FOREARM CharIKHand target-space ∥ CROWD clip-trigger)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `W24-RECON/REPORT.md` (the confirmed root causes + fix charters + discriminators +
acceptance tests — BINDING as written) + `WAVE23_CLOSEOUT_REVIEW.md` Q6.
Engine pin `e6b3c64`. THIRTEEN defaults ON. **FIX wave** — flag-gated fixes default-OFF;
coordinator-only default flips at close-out after E1. All lane agents = OPUS.

## COORDINATOR ACCEPTANCE (from `WAVE25_REVIEW.md`, rb3 `232b8cc7` — DISPATCH-WITH-AMENDMENTS)

**A1–A9 adopted verbatim and BINDING; the review file overrides this kickoff wherever they
differ.** Both lanes faithfully execute W24-RECON; the amendments sharpen the two load-bearing
spots (FOREARM G3 decision, CROWD leak vector) + correct stale anchors. Headlines:

- **A1 (FOREARM — add sub-cause H-C):** the `:40` "300u" comment is prior CONTEXT (V26
  crowd/extras residual + the V23 `BandWardrobe::SyncTransProxies` fix pattern), NOT a
  contradiction (V26=crowd z≈300u, W24=band ~100u — two populations, same class; V32 proved Poll
  was never tested in-song, so this is known-but-never-addressed). **H-C = target/TransProxy
  RESOLUTION** (`bone_target_*` in `<inst>_resource.milo` reach the member via proxy/attach; a
  mis-resolved proxy places them wrong with no chain-parenting error). Root-compare catches H-C
  too but the FIX site differs from H-A. REQUIRED READING before fix: VENUE_RENDER.md V23 (:598),
  V26 (:1070-1100,:1163-1171), V32 (:1175-1341). Note: uniform offset on hands AND feet ikhands
  (ankle y≈173 vs pedal y≈108) ⇒ ONE root-frame error, mildly AGAINST H-B.
- **A2 (FOREARM G3 — 3-case table, NOT flat baseline equality):** baselines to RECORD in STATUS
  before any edit: unit `char/CharIKHand` **99.16526%**, `Poll__10CharIKHandFv` **96.127235%**,
  `MeasureLengths` 81.355%. (1) native-only/HX_NATIVE fix → batch_objdiff MUST equal baseline
  EXACTLY (any delta leaked into Wii → revert); (2) decomp-infidelity fix in shared code (the
  CharHair-99.6%-hid-a-CFG-bug precedent) → UN-gated, match% **≥** baseline (improvement
  expected, decrease FORBIDDEN), justified by `run_diff_inspect diagnose` showing CFG/semantic
  (not regalloc) mismatch; (3) H-B → CharIKHand untouched, trivially baseline.
- **A3 (FOREARM STEP 0):** include `run_diff_inspect mode=diagnose` on `Poll` AND audit
  `MeasureLengths` (81.4% — it computes the very lengths the solver uses; a semantic bug there
  stretches arms by construction) for CFG-class mismatch, gated by `bank_divergence.py`, BEFORE
  concluding "faithful port, gate the fix".
- **A4 (CROWD anchors — NOT the CharSync bring-up):** census shows `poll=3` (Characters ARE
  polled) → it's clip START/RESOLUTION, in-scope. Anchors: `CharDriver::Enter` autoplay
  (`src/system/char/CharDriver.cpp:157`, Play at :163), starved-replay :404-409
  (`mDefaultPlayStarved` ctor-0/milo-:607), **`mDefaultClip.Load(bs,false,mClips):604` = PRIME
  SUSPECT (silently nulls if `streetslomo_clips.milo` isn't merged into the driver's `mClips`)**,
  `CHARDRV_PROBE:342`/`RB3_NO_CLIP:341`. FIRST action = extend CHARDRV_PROBE to print
  `mDefaultClip`+`mClips` size. Decision tree: null-at-Enter → resolution/merge gap; set-but-
  Enter-never-fires → vignette-dir flow gap; no-default-authored → find the DTA `{...play}`
  trigger. Freeze secondary (`Character::Poll` mFrozen :248 / `BandCamShot::FreezeChar` :558) —
  poll=3 argues against, log once.
- **A5 (CROWD hand-off bound):** hand off ONLY if the WHOLE vignette Enter/flow machinery is
  absent natively; the CharSync/CharCache blocked item is a DIFFERENT subsystem — do NOT attempt
  a broad bring-up in this lane.
- **A6 (CROWD near-black material — DEFER):** discriminator = post-fix `RB3_ISOLATE_MESH=crowd_body`
  (engine `Rnd_Wgpu_RB3.cpp:2695`) max-pixel vs the recon 17/255 baseline. Figures form + value
  rises → close as consequence. Figures form but still dark (<~40/255) → separate ENGINE-side
  env/lighting fix (own flag, coordinator pin bump, DEFERRABLE to W26).
- **A7 (CROWD leak vector — MANDATORY guardrail):** an un-scoped autoplay in shared
  `CharDriver`/`Character` would fire on EVERY driver in the game (band, gameplay chars, WorldCrowd
  drivers). The fix MUST be (a) in native-src/band3 flow, OR (b) if in shared char code, flag-gated
  AND driver/dir-scoped (crowd_/streetslomo match) with a byte-identical `#else`. The mandatory
  WorldCrowd A/B runs **flag-ON vs baseline** (OFF-only proves nothing).
- **A8 (ownership split):** FOREARM owns `char/CharIKHand.cpp` + `bandobj/`; CROWD owns
  `char/CharDriver.cpp` (probe-only unless A7-scoped) + `band3/` flow + `native-src`.
- **A9 (stale anchors):** `BandCharacter.cpp:3829` is a one-shot `MeasureLengths()` resync (gated
  `unk224 & 2`), NOT the per-frame poll loop (real path: `Character::Poll` → `RndDir::Poll` →
  `mPolls` sorted at `Character::SyncObjects` :710-723); :3460/:3475 correct. rb3-tests 116/0
  applies to the CROWD lane too.

_(original kickoff placeholder retained below for provenance.)_

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either. CLOSURES (do NOT reopen): hands-finger
  family CLOSED; FOREARM binding CLOSED (own==bound at draw — this is a POSE/IK bug); the gameplay
  WorldCrowd/RndMultiMesh oracle is PROTECTED (`Crowd.cpp:884-1000`); the RndMesh loader is
  PROVEN-CORRECT (do NOT touch — CROWD is draw/anim-time, not load).

## Shape

Both bugs are the W24-RECON "correct skin, wrong-placed driving bone" class — small, low-blast
fixes that touch neither the mesh loader nor binding. Each lane leads with the recon's
pre-registered DISCRIMINATOR (so we fix the right sub-cause), then the scoped fix, then the
recon's ACCEPTANCE test. Lanes are file-disjoint (FOREARM = char/instrument-resource; CROWD =
crowd-proxy driver/hub-vignette + crowd-camera material).

## Lanes

**Lane FOREARM — CharIKHand upper-arm target-space fix (Opus):**
Lane dir `execution/W25-FOREARM/`. CONFIRMED (W24-RECON): the exploded arm/hand fan is
`CharIKHand::Poll()` (`src/system/char/CharIKHand.cpp:25`) mis-placing `bone_R/L-upperArm.mesh`
in-song — a ~100u IK target-space mismatch (hand y≈209 vs instrument-tip targets y≈105);
`RB3_NO_IK=1` collapses the upper-arm stretch p50 21.2/max 49.6 → 1.0/1.9 (discriminator only,
do NOT ship it — it also kills correct fret/drum hand posing).
- **STEP 0 — the H-A/H-B discriminator (CHECKPOINT before any fix):** dump
  `bone_target_snare.mesh`'s TransParent chain to root vs the member skeleton's root, compare
  world roots. Roots differ ~100u → **H-A** (instrument-resource target bones parented in the
  wrong world/root frame — fix the resource-dir parenting). Roots equal but the arm rest basis
  is already ~100u high pre-IK → **H-B** (skeleton arm rest/world basis). ALSO run
  `bin/analyze-function` on `CharIKHand::Poll` / `PullShoulder` + `scripts/analysis/bank_divergence.py`
  to check whether NATIVE diverged in the world-compose vs a faithful-but-broken port (the :40
  comment implies this path was under-tested on native) — this decides match-neutral vs
  HX_NATIVE-gated.
- **FIX (flag-first, default-OFF, HX_NATIVE unless the discriminator proves a match-neutral
  native-divergence):** correct the target-space so the IK solver reaches the instrument tips
  without over-rotating the upper arm. Do NOT reopen forearm binding (exonerated) or the hands
  family (CLOSED). Do NOT ship `RB3_NO_IK`.
- **GATES (recon acceptance):** in-song (game_screen) upper-arm max stretch ratio < 2.0 **WITH
  IK ON** (`scripts/native/_w24_forearm_capture.py`, parse `[BAND_ANIM] evt=ANAT`); E1 visual —
  guitarist/drummer closeup (pin `coop_g_cg`/`coop_d_*`) shows intact arms, NO spike-fan;
  flag-OFF drawlog-golden 792 byte-identical; `batch_objdiff==baseline` on `char/CharIKHand`
  (+ any touched src/system unit) = G3 Wii-match; rb3-tests 116/0.

**Lane CROWD — hub crowd-proxy clip-trigger fix (Opus):**
Lane dir `execution/W25-CROWD/`. CONFIRMED (W24-RECON): the sv3_a hub crowd bodies have real
geometry (compressed 1145-1929 verts, reach SubmitDraw) but the `streetslomo` walk clip NEVER
PLAYS on the crowd proxies (`animating=0`, no playing clip) → undriven skin palette scrambles →
`RB3_ISOLATE_MESH=crowd_body` shows a dark scrambled mass, not 8 figures; compounded by a
near-black crowd-camera material. The 0-verts + `gAltRev<3` decode leads are REFUTED.
- **STEP 0 — confirm the trigger gap (CHECKPOINT before fix):** trace where the sv3_a
  shell-vignette is SUPPOSED to start the crowd proxy driver's `streetslomo` clip (`src/band3/`
  shell-vignette flow or a native-src `CharDriver::Poll` shim) — census via `{rb3_crowd_census}`
  (`native/src/rb3_http_handlers.cpp` `RB3DtaCrowdCensus`, reports `animating/clip`). Name the
  exact flow/poll gap.
- **FIX (flag-first, default-OFF, BRANCH-SCOPED to the crowd-proxy driver / hub-vignette path
  ONLY):** start the `streetslomo` clip on the crowd proxies. If a near-black material persists
  after posing, an env-scoped lighting fix for the crowd draw camera (`Rnd_Wgpu_RB3.cpp` ~:5595,
  world.cam) is the secondary — but resolve posing FIRST (the material may be a consequence).
- **DO NOT** touch the RndMesh loader (`Mesh.cpp` compressed path — proven correct) or the
  WorldCrowd/RndMultiMesh gameplay path (`Crowd.cpp:884-1000` — protected oracle).
- **GATES (recon acceptance):** `{rb3_crowd_census}` `animating > 0`; `RB3_ISOLATE_MESH=crowd_body`
  capture shows 8 LIT standing figures (camera-independent — verify here, not just default hub
  framing which sits on a band-face closeup); hub center-street walkers appear (E1 vs the
  `yt_mhKNp9uAT48_*` GT, STRUCTURAL/relative per the GT-color caveat); **MANDATORY WorldCrowd A/B
  — gameplay crowd draw-counts + screenshot SSIM UNCHANGED** (the protected oracle must not move);
  flag-OFF drawlog-golden 792 byte-identical; batch_objdiff==baseline on any touched src/system unit.

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out).
Checkpoints `/tmp/wave25-checkpoints/<lane>.json` — check-first, write-before-return; fix lanes
CHECKPOINT the discriminator verdict BEFORE fix code. PLAN/STATUS under `execution/<KEY>/`.
Evidence committed or it doesn't exist. New flags default-OFF; NO default flips, NO pin bumps by
lanes (coordinator, ONCE, close-out). THIRTEEN defaults stay ON. Engine fixes commit in
`../milo-native-engine` first, coordinator bumps the pin. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`,
free ports, frame-settling, pgid-only cleanup. Build under `/tmp/rb3-native-build.lock` or own
worktree. Stage only your own files by path; NEVER `rb3_session_trace.cpp` / engine `FxSendNative.cpp`.

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified** — FOREARM compares bone WORLDS + root frames
  (matrices), not scalars; CROWD is pointer/flow identity (which driver, which clip).
- [x] **2. Split by population** — FOREARM per-member × R/L × instrument; CROWD per-proxy.
- [x] **3. No unvalidated oracles** — recon acceptance tests are the gates (stretch<2.0 WITH IK ON;
  animating>0 + lit isolate + WorldCrowd A/B); E1 visual is decisive.
- [x] **4. Shipped-flag contradiction grep** — FOREARM must not regress correct hand posing (the
  RB3_NO_IK trap); CROWD must not touch the protected gameplay crowd.
- [x] **5. Grants** — FOREARM char/CharIKHand + instrument-resource + evidence; CROWD crowd-proxy
  driver/hub-vignette + crowd-camera material + evidence.
- [x] **6. Option table before 2nd fix attempt** — FOREARM H-A/H-B discriminator BEFORE fix;
  CROWD trigger-gap trace BEFORE fix; closures restated.
- [x] **7. Evidence committed** — discriminator dumps, before/after captures, gate logs.
- [x] **8. Flag hit-counts on negatives** — FOREARM stretch-ratio distribution; CROWD animating/clip counts.
- [x] **9. Flavor-membership grep** — step 0: verify edited TU compiles into rb3-native.
- [x] **10. Instruments before fixes** — both lanes lead with the recon discriminator.

## Risks / open questions for the reviewer

- **R-A (FOREARM):** is the H-A/H-B discriminator sufficient, or could it be a THIRD cause (the
  :40 "hand-IK target ~300u away" VENUE_RENDER V26/V32 comment suggests prior work — is this a
  known-partially-addressed issue)? Should the lane check the V26/V32 history first?
- **R-B (FOREARM):** if native diverged from Wii in CharIKHand world-compose (H-B faithful-broken),
  is the fix match-neutral (restores the Wii match) or HX_NATIVE-gated? Bless the G3 handling.
- **R-C (CROWD):** is the streetslomo-clip trigger a `src/band3` flow gap or a native-src
  CharDriver/CharSync-Poll shim gap (the hack-audit "chars.milo + CharSync Poll" blocked item)?
  If it's the deep CharSync-Poll bring-up, is that in-scope or a hand-off?
- **R-D (CROWD):** is the near-black material a separate defect that needs its own fix even after
  posing, or should the lane defer it (recon MEDIUM confidence it may be camera-not-reached)?
- **R-E:** anything risking the protected WorldCrowd oracle, the hands/binding closures, or the
  proven-correct RndMesh loader.
