# Wave 26 — Kickoff (CROWD load-merge engine fix ∥ instrument-prop posing ∥ combo-glow)

**Author:** coordinator. **Status:** DRAFT — for Fable pre-dispatch review.
Parent: `WAVE25_CLOSEOUT_REVIEW.md` Q7 (Wave-26 ordering) + `W25-CROWD/STATUS.md` (the engine
hand-off charter) + `W25-FOREARM/STATUS.md` (the reframed prop-posing tail).
Engine pin `2088c68`. FOURTEEN defaults ON. **FIX wave, recon-first** — flag-gated fixes
default-OFF; coordinator-only default flips at close-out after E1. All lane agents = OPUS.

## COORDINATOR ACCEPTANCE (<pending review>)

_To be filled from `WAVE26_REVIEW.md`._

- **Hazard note:** engine tree carries uncommitted `M FxSendNative.cpp`; rb3 tree carries
  `native/src/rb3_session_trace.cpp` — never stage either. CLOSURES (do NOT reopen): hands-finger
  family CLOSED; FOREARM binding CLOSED; the gameplay WorldCrowd/RndMultiMesh oracle PROTECTED
  (`Crowd.cpp:884-1000`); the RndMesh loader PROVEN-CORRECT. The `RB3_IK_REACH_CLAMP` fix
  (14th default) STAYS — the prop-posing lane makes the clamp a dormant safety net, does NOT
  remove it.

## Shape

Two recon-first fix lanes from the W25 close-out (the CROWD async load-merge + the instrument-prop
target posing — same async/posing family, recon well together) + a light confirm-then-fix of the
S5 combo-glow. Each fix lane leads with a DISCRIMINATOR (checkpoint the verdict before fix code);
if the recon shows a tractable scoped fix, land it flag-first; if it's deeper than budget, narrow
+ hand off honestly (the W25 pattern). Lanes file-disjoint: CROWD = native async loader/merge +
crowd-proxy; PROP = instrument-resource/wardrobe prop bones; GLOW = now-bar/combo engine draw.

## Lanes

**Lane CROWD-MERGE — the sv3_a async load-merge that destroys the crowd clip (Opus):**
Lane dir `execution/W26-CROWD/`. CONFIRMED (W25): the streetslomo clip plays + skins correctly
for ~1.2s, then an async load-merge at beat 2.433 (pollFrame 72) DESTROYS the playing `crowdN.clp`
(`~Object`→`Replace(clip,NULL)`→`CharDriver::Replace`→`DeleteClip`→`mFirst=NULL`, 7× REPLACE-to-null
+ 7× DIE, second ENTER with `mFirstAtEntry=nil`) AND swaps `mClips` to a wrong player-only sub-bank.
Same native async-loader-interleaving class as the hands parse-time binding + the load-order fixes.
- **STEP 0 — recon WHY the merge fires at beat 2.433 (checkpoint before fix):** trace what triggers
  the second load-merge of the sv3_a / streetslomo dirs at pollFrame 72 — is it a DUPLICATE load
  (the native loader re-merging a dir already resident, cf the FilterSubdir/kInlineCached
  interleaving territory), a deferred sub-milo (`streetslomo_clips.milo`) arriving late, or a
  legitimate merge whose Replace-rewiring is wrong on native? Sites (from the W25 charter):
  FileMerger / DirLoader merge path + `Dir.cpp` / `Object.cpp:131` Replace rewiring. Name the
  trigger + whether it's native-specific (Wii atomic-load vs native poll-interleaved).
- **FIX (flag-first, default-OFF; PREFER the cheap forms per the close-out):** (a) SUPPRESS the
  duplicate load/merge if it's a native re-merge of a resident dir; OR (b) RE-FIRE `play_clip` on
  the crowd proxies after the merge settles (re-establish the loop) — both are lighter than
  clip-preservation surgery through the merge (which W25 proved use-after-frees). If in shared
  loader/char code, GATE + SCOPE (crowd_/streetslomo/sv3_a name-match) with byte-identical `#else`
  (the A7 leak guardrail carries — an un-scoped loader change hits every merge in the game). Do NOT
  touch the RndMesh loader (proven correct) or WorldCrowd/RndMultiMesh (`Crowd.cpp:884-1000`).
- **GATES:** `{rb3_crowd_census}` `animating > 0` on all 8 proxies; `RB3_ISOLATE_MESH=crowd_body`
  shows 8 LIT standing figures (camera-independent); THEN the deferred near-black material
  discriminator (isolate max-pixel vs the recon 17/255 baseline — rises to normal → close as
  consequence; still dark → separate engine env/lighting fix, own flag, deferrable to W27); hub
  center-street walkers appear (E1 vs `yt_mhKNp9uAT48_*` GT, STRUCTURAL/relative only); **MANDATORY
  WorldCrowd A/B flag-ON — gameplay crowd draw-counts + SSIM UNCHANGED**; flag-OFF drawlog-golden
  792 byte-identical; batch_objdiff==baseline on any touched src/system unit; rb3-tests 116/0.
  If the fix lands, RB3_CROWD_CLIP_KEEP's removal criterion fires (E-C2) — flag whether it's now
  redundant.

**Lane PROP — instrument-prop target-bone posing (reframed FOREARM tail; Opus):**
Lane dir `execution/W26-PROP/`. CONFIRMED (W25 Q1/Q7): the arm reach (~20u) is CORRECT — the IK
targets are mis-posed instrument-PROP bones (`bone_pick_strum` z=98 vs hand z=48.8;
`bone_mic_stand_bottom` y≈−30 below floor; fret targets 98-216u; `mic_stand.ikhand` reach=0). The
`RB3_IK_REACH_CLAMP` guard currently clip-poses in-song arms; fixing prop posing restores genuine
IK (clamp becomes dormant) + likely retires the ≤4.2 vignette residual + drumstick splay.
- **STEP 0 — recon WHY the prop bones are mis-posed natively (checkpoint before fix):** the
  `bone_target_*` / prop bones live under `<inst>_resource.milo` and reach the member via
  proxy/attach wiring (the V23 `BandWardrobe::SyncTransProxies` class,
  `src/system/bandobj/BandWardrobe.cpp:326`). Is the mis-pose (a) a wrong prop-attachment transform
  (the prop dir parented to the wrong frame — cf the A4 venue-env transpose class), (b) a
  proxy-slot resolution gap (SyncTransProxies not wiring the `*.tp` slots natively), or (c) an
  authored-vs-native prop rest-pose difference? Compare the prop-bone world vs the expected
  at-hand position; check whether Wii/retail poses these props correctly (`bin/analyze-function` on
  the attach/sync path + `bank_divergence.py`). Name the mechanism.
- **FIX (flag-first, default-OFF, HX_NATIVE unless a match-neutral native-divergence is proven):**
  correct the prop-bone posing so the IK targets sit at the hand's playing position. Do NOT reopen
  the hands/binding closures. Do NOT remove RB3_IK_REACH_CLAMP (it stays as the safety net).
- **GATES:** in-song IK re-engages (upperArm ratio stays <2.0 WITH the clamp now DORMANT — i.e.
  targets within reach, clamp fire-count drops toward 0); E1 — guitarist/drummer/vocalist closeup
  shows the playing hand AT the instrument (pick at strings, fret hand on neck, mic at mouth), no
  fan; the ≤4.2 vignette residual + drumstick splay measurably reduced; flag-OFF drawlog-792
  byte-identical; batch_objdiff per the G3 case (record baselines); rb3-tests 116/0.

**Lane GLOW — now-bar / combo-multiplier glow (SWEEP S5; Opus; light):**
Lane dir `execution/W26-GLOW/`. SWEEP S5 (LOW-MED): the now-bar/combo-multiplier ring reads plain,
no lit "Nx" glow vs retail. **CONFIRM-THEN-FIX:** first capture a DRIVEN-COMBO gameplay frame (Nx
multiplier active — the SWEEP note said this needs driven-combo capture) vs the retail GT and
verify the glow is actually missing on the current build. If confirmed: root-cause (an emissive/
additive material or a bloom-halo gate on the combo ring — cf the P1 gem bloom-halo work
`RB3_HIGHWAY_BLOOM_OFF`) and fix flag-first default-OFF. If it's already present/faithful or needs
driven-multiplier state we can't reach headless, report + defer. Gates: E1 vs GT (structural),
drawlog-792 flag-OFF, batch_objdiff==baseline, no highway/HUD regression.

## Process rules (carried) — VERBATIM per KICKOFF_TEMPLATE.md

Locks: rb3 `/tmp/rb3-git.lock`; engine `/tmp/milo-engine-git.lock`; classjson
`/tmp/milo-engine-classjson.lock` (append-only, single coordinator regen at close-out).
Checkpoints `/tmp/wave26-checkpoints/<lane>.json` — check-first, write-before-return; fix lanes
CHECKPOINT the discriminator verdict BEFORE fix code. PLAN/STATUS under `execution/<KEY>/`.
Evidence committed or it doesn't exist. New flags default-OFF; NO default flips, NO pin bumps by
lanes. FOURTEEN defaults stay ON. Engine fixes commit in `../milo-native-engine` first, coordinator
bumps the pin. Headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-settling, pgid-only
cleanup. Build under `/tmp/rb3-native-build.lock` or own worktree. Stage only your own files by
path; NEVER `rb3_session_trace.cpp` / engine `FxSendNative.cpp`.

## Pre-dispatch checklist — the ten §4 lints

- [x] **1. Matrix-relative + pointer-verified** — CROWD is merge/pointer identity; PROP compares
  prop-bone WORLDS + attach frames (matrices); GLOW is pixel/material.
- [x] **2. Split by population** — CROWD per-proxy; PROP per-instrument × R/L; GLOW per-multiplier-state.
- [x] **3. No unvalidated oracles** — recon acceptance tests are the gates; E1 decisive; GLOW
  confirm-then-fix re-validates the bug exists first.
- [x] **4. Shipped-flag contradiction grep** — CROWD must not touch the protected gameplay crowd;
  PROP must not remove RB3_IK_REACH_CLAMP or regress the highway; GLOW must not regress the bloom defaults.
- [x] **5. Grants** — CROWD native loader/merge + crowd-proxy; PROP instrument-resource/wardrobe;
  GLOW combo-ring engine draw.
- [x] **6. Option table before 2nd fix attempt** — each fix lane discriminates BEFORE the fix; the
  close-out's PREFER-cheap-forms (suppress-dup / re-fire) restated for CROWD; closures restated.
- [x] **7. Evidence committed** — merge traces, prop-bone dumps, driven-combo captures, gate logs.
- [x] **8. Flag hit-counts on negatives** — CROWD animating/clip counts; PROP clamp-fire-count drop;
  GLOW multiplier-state.
- [x] **9. Flavor-membership grep** — step 0: verify edited TU compiles into rb3-native.
- [x] **10. Instruments before fixes** — all three lead with a discriminator/confirm.

## Risks / open questions for the reviewer

- **R-A (CROWD):** is the beat-2.433 merge a native duplicate-load (suppressible) or a legitimate
  deferred sub-milo arrival (must re-fire, not suppress)? Which cheap form is right, and where
  exactly is the native-specific divergence (Wii atomic vs native poll-interleaved)?
- **R-B (CROWD):** does re-firing `play_clip` post-merge risk the A7 leak (firing on non-crowd
  drivers) or a re-entrancy with the merge? Bless the scope/timing.
- **R-C (PROP):** is the prop mis-pose the SyncTransProxies class (V23) or a prop-attach transpose
  (A4 class) — and is the fix match-neutral (restores Wii) or a native workaround? Could fixing it
  regress the CLOSEST-currently-correct hand poses (the clamp's no-op-in-reach cases)?
- **R-D (PROP):** the mic_stand reach=0 (no 2-deep chain) — is that a prop-posing fix or a separate
  MeasureLengths/chain issue? In scope?
- **R-E (GLOW):** is a driven-combo (Nx active) gameplay state reachable headless under fixed clock,
  or does the lane need a combo-forcing harness? If unreachable, is GLOW a defer?
- **R-F:** anything risking the protected WorldCrowd oracle, the proven-correct RndMesh loader, the
  hands/binding closures, or the shipped RB3_IK_REACH_CLAMP.
