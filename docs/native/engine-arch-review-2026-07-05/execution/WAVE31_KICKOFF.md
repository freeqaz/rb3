# WAVE 31 KICKOFF — SET-PLAY-DISPATCH (primary) + HUD-GLYPHS + EXIT-TRAP + HUBWALKER-SHARDS (user-verified)

**Author:** Fable coordinator. **Status:** DRAFT (pre-review).
**Parent:** `README.md` §Wave 31 menu + `WAVE30_CLOSEOUT_REVIEW.md` Q(f).
**Base SHA (rb3):** `3a314d6c`. **Engine pin:** `b36bcfc` (engine HEAD == pin; clean except the untouchable `M FxSendNative.cpp`). **15 defaults ON.**
**Lanes:** Opus via ultracode Workflow unless tagged otherwise. Coordinator = Fable.
**User directive (2026-07-12):** continue the polish wave loop; three new user bug reports folded in after coordinator repro (below).

**Concurrent, NOT part of this workflow:** web/guitar-input track (`c2fc51ac` lineage), permuter sweeps, decomp-synth. The engine tree's long-standing uncommitted `M src/platform/FxSendNative.cpp` (concurrent audio work): **never stage it**.

## User-report repro (coordinator, 2026-07-12, evidence `execution/W31-REPRO/evidence/` — on-disk per the 2026-07-10 gitignore ruling `387a70bf`: evidence lives under `execution/**/evidence/` UNTRACKED; hand-written docs stay tracked. Lint 7 "committed" is amended to "on disk under evidence/ + findings quoted in tracked STATUS")

1. **"Floating yellow square" (main menu): NOT-REPRODUCED on native** — highlight pill hugs + tracks focus at hub top-level, PLAY NOW submenu, mid-transition, song select (`a_hubtop_*.png`, `a_hub_*.png`, `a_hubmid_*.png`). Web deploy is FRESH (rebuilt 2026-07-12 05:17, includes all landed work) → stale-build hypothesis dead. Remaining hypotheses: web-specific divergence, non-16:9 window, or a screen/flow the sweep missed. → small web-side capture task (Lane C rider), not a blind charter.
2. **Face "skin tag": CONFIRMED on hub street walkers** — flesh-colored thin cone from the front hairline over the forehead, BOTH walkers (`crop_hub_center_face.png`, `crop_hub_right_face.png`). Morphology = the point-radial R·sin(θ) shard family (same walkers carry the W30-F1 waist stick-fans). Gameplay band faces CLEAN in pinned closeups (`closeup-g/g_coop_g_n01_0.png`). NOT the F5 jacket-patch family.
3. **"Floating legs": CONFIRMED as frozen airborne poses + collapsed support props** — vocalist frozen mid-jump feet unplanted (`crop_gp3_vocalist_body.png`); guitarist raised foot on a missing/collapsed monitor + magenta stick-fan guitar (`crop_gp3_guitarist_legs.png`); hub walkers crumpled-blob boots w/ hip-high dangling foot (`crop_hub_legs.png`). Standing members' feet DO reach the stage. Mechanism = dead in-song `set_play` intensity stream (Lane A's charter) + undriven prop bones (F1). NOT the closed W2.6 foot/shoe family.
4. (Rider observation) **F5 still reproduces** (`closeup-g/g_coop_g_cg_0.png`, `crop_cg_jacket_shards.png`) while the closeup gate reports PASS 10/10 — gate remains shard-blind, as recorded. F5 stays recorded-NOT-chartered (needs fresh hypothesis; two prior bisect-reverts).

## COORDINATOR ACCEPTANCE (to be filled from WAVE31_REVIEW.md — BINDING, overrides drafts below where they differ)

- (pending review)
- **Hazard note:** engine tree's uncommitted `M FxSendNative.cpp` — never stage it. Evidence dirs are gitignored (see above) — do NOT `git add -f` captures without coordinator sign-off.

## Shape

Fix wave, four lanes: one faithful-dispatch multi-edit (A, primary — the W30 perf-clip recharter), one texture/material family fix (B), one hygiene payoff (C, exit-trap), one user-verified diagnosis lane (D, hub-walker shards — diagnosis-first, STEP-0 checkpoint before ANY fix code, per the three-supersessions rule). Reviewer's job: gate validity on A's census acceptance (E3 sit-churn bound, E4 verbatim-quote rule), collision audit (A and D both read char/anim surfaces; B and the HUD milo assets; C touches teardown paths every lane's harness exercises), and whether D is correctly scoped as diagnosis-only given the SKEL family's five dead fix classes + R5-HANDS-ENDGAME closure.

## Lanes

**Lane A — W31-SET-PLAY-DISPATCH (primary; Opus planner + Opus executor):**
Make the song-authored venue-mood stream (`[play]`/`[intense]`/`[mellow]`/`[solo]`) dispatch `set_play` to BandCharacter natively. Discriminator-first, checkpointed BEFORE any lever:
(i) does the mood/venue event data exist parsed natively (BandDirector-side census)?
(ii) who should send it (DTA venue scripts vs song.anim events)?
Spans >1 system — expect a scoped multi-edit, not a one-line lever. Key sites from W30: receiver `HANDLE(set_play, OnSetPlay)` `src/band3/bandobj/BandCharacter.cpp:4340`, rewrite `:4508`, Symbol `Symbols.h:202`/`Symbols.cpp:192`; only in-song driver today = `play_group` from `BandCamShot::StartAnim` (leaves `mPlayFlags`=IR 0x1000); perf clips RESIDENT (`stand_rhythm_*`=P/PM, `stand_solo_*`=PS); beat-0-only census 190×mask=2 + 4×mask=3.
**Acceptance (mechanical, quoted verbatim from Q(f) — E4 rule):** "with the DEMO LEVER OFF, sustained rhythm/solo `CHARDRV_PLAY` census (Lane-1 A/B rerun, OFF=W29-idle baseline); **no sit-group churn** (E3 bound: ON-run `grp='sit'` BANDPERF_STATE same order as OFF, not thousands); F1 gameplay retest — drumstick/prop-tip bones driven, cones/fans gone or explicitly re-scoped; retire `RB3_BAND_PERF_FORCE_PLAY` at close-out."
**W31-REPRO addendum (binding):** re-shoot the floating-legs evidence crops at matched songMs — vocalist no longer frozen mid-jump across a sustained window (`crop_gp3_vocalist_body.png` pair), guitarist support-prop/leg pose re-graded (`crop_gp3_guitarist_legs.png` pair). New flag (if any) default-OFF + class.json append under lock; the DEFAULT FLIP (if earned) is coordinator-executed at close-out with Caveat-B-style residual framing.
Owned surfaces: `src/band3/bandobj/` + `src/band3/game/` band-perf plumbing (WRITE, HX_NATIVE-gated); BANDPERF_*/CHARDRV_PLAY probes (KEEP, standing instruments). READ-ONLY: engine char/CharDriver/CharClip (A8/W28 arbitration carried), `native/src/rb3_render_hook.cpp`.
Exit: acceptance census + crops, or a priced NO-GO naming the missing data layer.

**Lane B — W31-HUD-GLYPHS (Opus):**
F2 (translucent score pill) + F3 (white glyph class) + F4 (star-slot row) as ONE HUD material/texture-bind family lane. Quoted acceptance (Q(f)): "trace one glyph end-to-end, fix the bind, verify the class across hub/song_select/overshell + the pill/star row vs retail." Start glyph: song_select footer pill (F3). Retail pairs: `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` (pill + 5-star row), `yt_qRagnZCIMzk_song_select_list.png`, `yt_mhKNp9uAT48_menu_hub.png`; exact-pixel atlas `ui_buttons_wii_spriters.png`. Shipped-flag contradiction check (lint 4) against the flag registry BEFORE claiming any "engine drops tint" mechanism (W15 lesson). Fix flag default-OFF; per-screen A/B crops in evidence.
Owned surfaces: engine `RB3MaterialBinder.cpp`/texture-bind path IF the mechanism lands there (disjoint from Lane A; NO edits to `rb3_render_hook.cpp` without coordinator sign-off), else game-side UI. READ-ONLY otherwise.
Exit: one-mechanism fix verified across the class vs retail pairs, or split memo proving F2/F3/F4 are ≥2 mechanisms with each priced.

**Lane C — W31-EXIT-TRAP (Opus; hygiene):**
The exit-time teardown SIGSEGV (rc=-11/139, sometimes SIGABRT 134) every gate tolerates (`drawlog-golden.py:183-190,234-237`, `song-end-test.py:269`, W30 process rule 10). Known class: Dawn/GPU device teardown at static-dtor order (W0.3.S1 origin; W1.1/W2.2/W1.4 STATUS refs). **No committed symbolized backtrace exists — STEP 0 = capture one** (debug build + gdb/ASan on the bounded 5-frame non-HTTP boot; distinguish from the SEPARATE web-release "past-score-screen" trap targeted by `scripts/native/_exit-trap-test.py`, and from the W26 in-song panel-unload teardown — do NOT conflate, README:858).
THEN one fix: explicit device/queue teardown ordering (or a scoped `_exit`-class bypass ONLY if the ordering fix is priced prohibitive — that choice needs coordinator sign-off, it would freeze the trap not fix it).
Acceptance: bounded non-HTTP boot exits rc=0 ≥10/10; rb3-tests clean; drawlog-golden + lineup gates PASS with the rc-tolerance lines REMOVED from `drawlog-golden.py` (the payoff — gates stop tolerating); no new flags unless the fix is behavioral (then default-OFF).
Rider (Sonnet side-task, same lane): web-side hub-menu capture — boot the deployed web release build via the Playwright harness, capture hub top-level + submenu focused states, grade the yellow-highlight geometry vs `W31-REPRO/evidence/a_hubtop_00_focused.png`. CONFIRMED-on-web → file as web-specific finding with crops (no fix this wave); clean → close the user report as not-reproduced-anywhere with evidence.
Owned surfaces: engine gfx teardown path + `scripts/native/drawlog-golden.py` tolerance lines (sequenced LAST, after A/B/D harness runs complete — coordinator lands it). READ-ONLY: everything else.
Exit: rc=0 boots + tolerance lines removed, or a priced NO-GO w/ symbolized backtrace on record.

**Lane D — W31-HUBWALKER-SHARDS (Opus; diagnosis-ONLY, user-verified):**
The hub-walker residual family, now user-visible on faces: forehead flesh cone (`crop_hub_center_face.png`/`crop_hub_right_face.png`) + waist stick-fans (W30-F1) + crumpled boots/dangling foot (`crop_hub_legs.png`). Hypothesis to TEST, not assume: head-anchored scalp/hair mesh verts weighted to an undriven/wrong-basis bone (walk clips don't drive prop/appendage tracks — Q(f): "may need walk-clip prop-track scoping"). STEP-0 discriminators (checkpointed, NO fix code — three-supersessions rule):
(i) name the shard meshes + their bound bones (matrix-relative + pointer-verified, lint 1; split per-walker, lint 2);
(ii) are those bones driven by `playerN_{f,m}` walk clips (track enumeration, W27(b) method)?
(iii) is the basis error the SKEL 87°-family (then it's R5-HANDS-ENDGAME territory — STOP and memo) or a distinct undriven-track gap (then a scoped fix is legal)?
Acceptance: mechanism named w/ per-mesh per-bone table + verdict SKEL-family vs undriven-track; IF (and only if) undriven-track, a default-OFF scoped fix MAY land w/ before/after hub crops + flag hit-count (lint 8). Census caveat: hub walkers = player0-3 (W29 binding Q(b)); key by object pointer or state the name-key caveat (E7).
Owned surfaces: probes + (conditionally) a narrow walk-clip/prop-track scoping fix, default-OFF. READ-ONLY: BandPatchMesh (F5 is NOT this lane), CharDriver/CharClip write paths.
Exit: the discriminator table + verdict; fix only on the (iii)-undriven branch.

## Process rules (carried) — VERBATIM, do not edit per-wave

Locks (add ONLY your own files under flock; NEVER stage another lane's or the engine's uncommitted edits):
- rb3 repo commits — flock `/tmp/rb3-git.lock`
- milo-native-engine commits — flock `/tmp/milo-engine-git.lock`
- engine `class.json` append (new flags) — flock `/tmp/milo-engine-classjson.lock` (append-only; a SINGLE coordinator regen at close-out — lanes never regen)
- native / build-native builds — flock `/tmp/rb3-native-build.lock` (or own build dir)
- milo-trace — its own repo, its own norms

Checkpoints: `/tmp/wave31-checkpoints/<lane>.json` — **check-first** (a valid checkpoint is AUTHORITATIVE; do NOT redo checkpointed work after a harness restart), **write-before-return**, update at EVERY milestone.

PLAN/STATUS: `execution/<KEY>/PLAN.md` + `STATUS.md` per lane; human-readable detail lives in STATUS, the final agent message is the structured output.

Evidence: on disk under `execution/<KEY>/evidence/` — `/tmp` is scratch (§4 lint 7, as amended by the `387a70bf` gitignore ruling: evidence files are untracked; findings + counts quoted in tracked STATUS; raw probe logs gzipped in evidence/).

Flag discipline: new flags default-OFF + `class.json` append under lock; **NO default flips, NO pin bumps by lanes** (the coordinator bumps the pin ONCE at close-out). Refuted flags UNSET. 15 defaults stay ON. Commit per review cycle.

Harness: headless `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, frame-count settling, **pgid-only cleanup (NEVER `pkill` by name)**. The known exit-time teardown SIGSEGV (rc=-11 after work completes) is tolerated by A/B/D — Lane C is chartered to kill it; only the coordinator lands the tolerance-line removal, LAST.

Dispatch prompts quote acceptance blocks VERBATIM, never paraphrase (E4, coordinator-owned).

## Pre-dispatch checklist — the ten §4 lints (BINDING on every lane)

- [x] **1. Matrix-relative + pointer-verified, or it isn't a bone claim.** Lane D STEP-0(i) requires it explicitly; Lane A makes no bone claims (census-based). B/C: N/A — no bone claims.
- [x] **2. Split by gender/mesh/route by default; aggregates cannot refute.** Lane D per-walker/per-mesh rows; Lane A census split per-member (guitar/bass/drums/vocals) per-group; B per-screen crops. C: N/A — single-process teardown.
- [x] **3. No unvalidated oracles as gates.** Lane A gate = CHARDRV_PLAY census validated in W29/W30 (OFF=idle baseline exists, lever-ON=55 known-GOOD separation). Lane C gate = rc code (trivially valid). Lane D is diagnosis-only. Lane B gate = human-eye retail pairs (the E1 method).
- [x] **4. Shipped-flag contradiction check.** Lane B MUST grep `NATIVE_COMPAT_LEDGER.md` before any "engine drops tint" claim (W4.2 text-floor + W15 precedent). Lane D vs `RB3_PROP_POSE_FULL` (15th default): the flip's own retest says hub-walker fans SURVIVE it — no contradiction.
- [x] **5. Diagnosis lanes get wide read grants.** Lane D: WIDE read (engine char/rnd + band3 + assets). Lane A discriminators: wide read across band3/system/anim event data.
- [x] **6. Enumerate the option table before the second fix attempt in any family.** Lane D is bound to the SKEL family coverage table (R5-HANDS-ENDGAME closure) — verdict SKEL-family ⇒ STOP+memo, no 7th cell. Lane A is the family's adjudicated winner path (W30 mechanism naming).
- [x] **7. Evidence on disk under `execution/<KEY>/evidence/`** (amended per `387a70bf`), findings quoted in tracked STATUS. W31-REPRO evidence already in place.
- [x] **8. Flag hit-count on every negative result.** Lane A: dispatch-site hit counter mandatory (a "census unchanged" negative requires proof the dispatch fired). Lane D conditional fix: hit-count required. Lane B: bind-path hit count on any no-visual-change claim.
- [x] **9. Flavor-membership check before scoping a lane on a file.** Lane A: BandDirector/BandPerformer/BandCharacter TUs — verify in rb3-native CMake source lists BEFORE chartering edits (W3 DC3-only lesson). Lane C: teardown TU is engine gfx — verify compiled into rb3-native. B/D likewise on first-named TU.
- [x] **10. Tools ship as pre-dispatch diagnosis gates, not post-mortems.** Lane A's instrument (CHARDRV_PLAY census + BANDPERF probes) already exists and graded W30. Lane C's instrument = symbolized backtrace FIRST (STEP 0) before any fix. Lane D discriminators before fix. Lane B: end-to-end glyph trace before the bind fix.

## Close-out (coordinator, end of EVERY wave — owner directive 2026-07-07)

1. Post-wave Fable review → `WAVE31_CLOSEOUT_REVIEW.md` (verdict validity, gate coverage, shipped-flag contradictions, over-generous self-grades; errata one sequence E1..En).
2. Docs updated: README results table + Wave-32 menu; STATUS finalized; ROADMAP amended; **single classjson regen (`python3 scripts/analysis/native_compat_census.py gen`) AFTER flip + `RB3_BAND_PERF_FORCE_PLAY` retirement; ONE pin bump iff engine moved.**
3. Findings summary handed to owner (user-facing; includes the three user reports' dispositions).

## Risks / open questions for the reviewer

- **R-A:** Lane A's discriminator (i) may find the venue-mood stream simply ABSENT from natively-parsed song data (song.anim/venue DTA not loaded) — is the acceptance then reachable this wave, or should the kickoff pre-authorize a bounded data-plumbing sub-scope (and where's its edge)?
- **R-B:** Lane C removing the rc-tolerance from `drawlog-golden.py` while lanes A/B/D run concurrently — is sequencing LAST sufficient, or must the removal be a separate post-wave coordinator commit gated on 10/10 rc=0 reproduction on the FINAL merged tree?
- **R-C:** verify the plans/edit-ranges still match the CURRENT tree at Base SHA `3a314d6c` — W30 sites (`BandCharacter.cpp:4340/:4508`, probe families post-retirement `d32292d2`) may have shifted; re-derive BY SYMBOL (A8/W13 rule).
- **R-D:** Lane D's (iii) SKEL-vs-undriven discriminator — is it actually decidable from track enumeration alone, or does it need a live-bone A/B (and if so, is that in-scope for a diagnosis lane)?
