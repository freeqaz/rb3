# WAVE 31 KICKOFF — SET-PLAY-DISPATCH (primary) + HUD-GLYPHS + EXIT-TRAP + HUBWALKER-SHARDS (user-verified)

**Author:** Fable coordinator. **Status:** REVIEWED (WAVE31_REVIEW.md `f9c6559d`, A1–A12 adopted below) → DISPATCHED.
**Parent:** `README.md` §Wave 31 menu + `WAVE30_CLOSEOUT_REVIEW.md` Q(f).
**Base SHA (rb3):** the COORDINATOR ACCEPTANCE commit (child of review `f9c6559d`; A9 — the earlier `3a314d6c` was stale, an unrelated web commit `86646f94` intervened). **Engine pin:** `b36bcfc` (engine HEAD == pin; clean except the untouchable `M FxSendNative.cpp`). **15 defaults ON.**
**Lanes:** Opus via ultracode Workflow unless tagged otherwise. Coordinator = Fable.
**User directive (2026-07-12):** continue the polish wave loop; three new user bug reports folded in after coordinator repro (below).

**Concurrent, NOT part of this workflow:** web/guitar-input track (`c2fc51ac` lineage), permuter sweeps, decomp-synth. The engine tree's long-standing uncommitted `M src/platform/FxSendNative.cpp` (concurrent audio work): **never stage it**.

## User-report repro (coordinator, 2026-07-12, evidence `execution/W31-REPRO/evidence/` — on-disk per the 2026-07-10 gitignore ruling `387a70bf`: evidence lives under `execution/**/evidence/` UNTRACKED; hand-written docs stay tracked. Lint 7 "committed" is amended to "on disk under evidence/ + findings quoted in tracked STATUS")

1. **"Floating yellow square" (main menu): NOT-REPRODUCED on native** — highlight pill hugs + tracks focus at hub top-level, PLAY NOW submenu, mid-transition, song select (`a_hubtop_*.png`, `a_hub_*.png`, `a_hubmid_*.png`). Web deploy is FRESH (rebuilt 2026-07-12 05:17, includes all landed work) → stale-build hypothesis dead. Remaining hypotheses: web-specific divergence, non-16:9 window, or a screen/flow the sweep missed. → small web-side capture task (Lane C rider), not a blind charter.
2. **Face "skin tag": CONFIRMED on hub street walkers** — flesh-colored thin cone from the front hairline over the forehead, BOTH walkers (`crop_hub_center_face.png`, `crop_hub_right_face.png`). Morphology = the point-radial R·sin(θ) shard family (same walkers carry the W30-F1 waist stick-fans). Gameplay band faces CLEAN in pinned closeups (`closeup-g/g_coop_g_n01_0.png`). NOT the F5 jacket-patch family.
3. **"Floating legs": CONFIRMED as frozen airborne poses + collapsed support props** — vocalist frozen mid-jump feet unplanted (`crop_gp3_vocalist_body.png`); guitarist raised foot on a missing/collapsed monitor + magenta stick-fan guitar (`crop_gp3_guitarist_legs.png`); hub walkers crumpled-blob boots w/ hip-high dangling foot (`crop_hub_legs.png`). Standing members' feet DO reach the stage. Mechanism = dead in-song `set_play` intensity stream (Lane A's charter) + undriven prop bones (F1). NOT the closed W2.6 foot/shoe family.
4. (Rider observation) **F5 still reproduces** (`closeup-g/g_coop_g_cg_0.png`, `crop_cg_jacket_shards.png`) while the closeup gate reports PASS 10/10 — gate remains shard-blind, as recorded. F5 stays recorded-NOT-chartered (needs fresh hypothesis; two prior bisect-reverts).

## COORDINATOR ACCEPTANCE (adopted verbatim from WAVE31_REVIEW.md `f9c6559d` — BINDING, overrides drafts below where they differ)

- **A1 (BLOCKING, adopted):** Lane A's drafted write grant `src/band3/bandobj/` DOES NOT EXIST. Real surface = **`src/system/bandobj/`** (BandCharacter.cpp, BandCamShot.cpp, BandDirector.cpp) + `src/band3/game/`. Sites re-derived: `HANDLE(set_play, OnSetPlay)` at `src/system/bandobj/BandCharacter.cpp:4344`, `OnSetPlay` rewrite at `:4512`. Lint 9 re-earned for Lane A on the corrected TUs.
- **A2 (MAJOR, adopted):** collision fence — `src/system/bandobj/BandCharacter.cpp` + `BandCamShot.cpp` are **Lane A exclusive-write**. Lane D probes go engine-side or a distinct TU (RELOAD_PROBE already sits at BandCharacter.cpp:50 — D must not touch it); any Lane D write toward CharClip/CharDriver requires coordinator arbitration mid-wave.
- **A3 (MAJOR, adopted):** "sustained" DEFINED: rhythm/solo `CHARDRV_PLAY` entries in **≥3 of 4 songMs quartiles** of the gameplay window; the dispatch log must show **≥2 distinct intensity values tracking authored events** (a constant `set_play(P)` = the lever re-badged = FAIL); do NOT grade against the lever's 55-count.
- **A4 (MAJOR, adopted):** E3 bound NUMERIC: ON-run `grp='sit'` BANDPERF_STATE count **≤ 10× OFF baseline** (OFF=16), total BANDPERF_STATE within **~2× OFF**; same greps run on the committed gz by lane AND coordinator E1.
- **A5 (MAJOR, adopted):** "cones/fans gone" leg rests on the errata-softened W27(b) enumeration (README:889). Lane A STEP-0 adds **static track enumeration of the resident perf clips for prop-tip tracks**; if perf clips carry NO prop-tip tracks, that acceptance leg is pre-agreed "explicitly re-scoped" (not a lane failure).
- **A6 (MAJOR, adopted; answers R-A — the branch is LIVE):** current tree has ZERO venue-mood (`[intense]`/`[mellow]`/`[solo]`) handling anywhere; BandDirector has no set_play/mood hits. Pre-authorized routing sub-scope IFF the events are already parsed natively somewhere (discriminator (i)): **≤2 TUs, no new file-format parsing, no engine writes**. If the events are NOT in natively-parsed data → priced NO-GO memo, not a heroic parser.
- **A7 (MAJOR, adopted; answers R-B):** rc-tolerance removal = a **separate post-merge coordinator commit** gated on 10/10 rc=0 on the FINAL merged tree. TWO tolerance sites decided explicitly: `drawlog-golden.py:183-190,234-237` (remove) and `song-end-test.py:269` (keep as crash-DETECTION band — it also catches SIGABRT 134; narrow it only if Lane C's fix covers SIGABRT too). No existing gate asserts nonzero rc.
- **A8 (MINOR, adopted):** Lane B engine-write grant tightened: STEP-0 = named-TU mechanism checkpoint + **coordinator ack before the first engine write**. Lint-4 registry sweep must cover: `RB3_HUB_TEXT_CONTRAST` (same RB3MaterialBinder.cpp), the W4.2 text floor, ROWFIX, SCOREBOARD_TOPRIGHT, and the FilterSubdir white-texture (black-head W2.7) precedent.
- **A9 (MINOR, adopted):** Base SHA restated (header). Lane C web rider verifies the graded deploy actually contains the current tree's shims before grading.
- **A10 (MINOR, adopted):** W26 teardown descriptor corrected: the panel-unload teardown is **splash→main_hub** (README:858), not "in-song". Lane C must not hunt a phantom in-song teardown.
- **A11 (MINOR, adopted):** web rider harness NAMED: `scripts/web/menuhub-probe.mjs` + `scripts/web/keyboard-to-gameplay.mjs` (Playwright/chromium, shared `scripts/web/lib/core.mjs`). Rider is feasible as chartered.
- **A12 (MINOR, adopted; answers R-D):** Lane D discriminator (iii) is NOT decidable by track enumeration alone (W27(b) precedent). Pre-authorized: a **read-only live-bone transform probe** (matrix-relative + pointer-keyed per lints 1/2 + E7) inside the diagnosis grant. Still NO fix code before the (iii) verdict.
- **Hazard note:** engine tree's uncommitted `M FxSendNative.cpp` — never stage it. Evidence dirs are gitignored (see above) — do NOT `git add -f` captures without coordinator sign-off.
- **Late-breaking user report (2026-07-12, post-review):** "icons for difficulty on song select are now missing and it's still clipping." Coordinator verification capture in flight (song-FOCUSED row needed; the first sweep only had a shortcut row focused). Disposition recorded below in Lane B before dispatch: if icons are MISSING/WHITE-FALLBACK on a focused song row, they join Lane B's verification class (plausibly the same texture-bind family as F3); "still clipping" = F7 (known, cosmetic backlog, NOT chartered). If a regression is indicated, coordinator decides bisect-vs-fix-forward mid-wave.

## Shape

Fix wave, four lanes: one faithful-dispatch multi-edit (A, primary — the W30 perf-clip recharter), one texture/material family fix (B), one hygiene payoff (C, exit-trap), one user-verified diagnosis lane (D, hub-walker shards — diagnosis-first, STEP-0 checkpoint before ANY fix code, per the three-supersessions rule). Reviewer's job: gate validity on A's census acceptance (E3 sit-churn bound, E4 verbatim-quote rule), collision audit (A and D both read char/anim surfaces; B and the HUD milo assets; C touches teardown paths every lane's harness exercises), and whether D is correctly scoped as diagnosis-only given the SKEL family's five dead fix classes + R5-HANDS-ENDGAME closure.

## Lanes

**Lane A — W31-SET-PLAY-DISPATCH (primary; Opus planner + Opus executor):**
Make the song-authored venue-mood stream (`[play]`/`[intense]`/`[mellow]`/`[solo]`) dispatch `set_play` to BandCharacter natively. Discriminator-first, checkpointed BEFORE any lever:
(i) does the mood/venue event data exist parsed natively (BandDirector-side census)?
(ii) who should send it (DTA venue scripts vs song.anim events)?
(iii) (A5) static track enumeration of the resident perf clips for prop-tip tracks — decides up front whether the "cones/fans gone" acceptance leg is achievable or pre-agreed re-scoped.
Spans >1 system — expect a scoped multi-edit, not a one-line lever. Key sites (re-derived at review, A1): receiver `HANDLE(set_play, OnSetPlay)` `src/system/bandobj/BandCharacter.cpp:4344`, rewrite `:4512`, Symbol `Symbols.h:202`/`Symbols.cpp:192`; only in-song driver today = `play_group` from `BandCamShot::StartAnim` (leaves `mPlayFlags`=IR 0x1000); perf clips RESIDENT (`stand_rhythm_*`=P/PM, `stand_solo_*`=PS); beat-0-only census 190×mask=2 + 4×mask=3. A6 bounds the routing sub-scope (≤2 TUs, no new parsing, no engine writes); if the mood events aren't in natively-parsed data → priced NO-GO.
**Acceptance (mechanical, quoted verbatim from Q(f) — E4 rule):** "with the DEMO LEVER OFF, sustained rhythm/solo `CHARDRV_PLAY` census (Lane-1 A/B rerun, OFF=W29-idle baseline); **no sit-group churn** (E3 bound: ON-run `grp='sit'` BANDPERF_STATE same order as OFF, not thousands); F1 gameplay retest — drumstick/prop-tip bones driven, cones/fans gone or explicitly re-scoped; retire `RB3_BAND_PERF_FORCE_PLAY` at close-out."
**W31-REPRO addendum (binding):** re-shoot the floating-legs evidence crops at matched songMs — vocalist no longer frozen mid-jump across a sustained window (`crop_gp3_vocalist_body.png` pair), guitarist support-prop/leg pose re-graded (`crop_gp3_guitarist_legs.png` pair). New flag (if any) default-OFF + class.json append under lock; the DEFAULT FLIP (if earned) is coordinator-executed at close-out with Caveat-B-style residual framing.
Owned surfaces (A1/A2): `src/system/bandobj/` (**BandCharacter.cpp + BandCamShot.cpp exclusive-write**, BandDirector.cpp) + `src/band3/game/` band-perf plumbing (WRITE, HX_NATIVE-gated); BANDPERF_*/CHARDRV_PLAY probes (KEEP, standing instruments). READ-ONLY: engine char/CharDriver/CharClip (W28 arbitration carried), `native/src/rb3_render_hook.cpp`. "Sustained" + census anti-gaming per A3; E3 numeric bound per A4.
Exit: acceptance census + crops, or a priced NO-GO naming the missing data layer.

**Lane B — W31-HUD-GLYPHS (Opus):**
F2 (translucent score pill) + F3 (white glyph class) + F4 (star-slot row) as ONE HUD material/texture-bind family lane. Quoted acceptance (Q(f)): "trace one glyph end-to-end, fix the bind, verify the class across hub/song_select/overshell + the pill/star row vs retail." Start glyph: song_select footer pill (F3). Retail pairs: `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` (pill + 5-star row), `yt_qRagnZCIMzk_song_select_list.png`, `yt_mhKNp9uAT48_menu_hub.png`; exact-pixel atlas `ui_buttons_wii_spriters.png`. Shipped-flag contradiction check (lint 4) against the flag registry BEFORE claiming any "engine drops tint" mechanism (W15 lesson). Fix flag default-OFF; per-screen A/B crops in evidence.
Owned surfaces (A8): STEP-0 = named-TU mechanism checkpoint + **coordinator ack before the first engine write**; then engine texture-bind path (disjoint from Lane A; NO edits to `rb3_render_hook.cpp` without coordinator sign-off) or game-side UI as the checkpoint names. Lint-4 sweep covers: `RB3_HUB_TEXT_CONTRAST`, W4.2 text floor, ROWFIX, SCOREBOARD_TOPRIGHT, FilterSubdir white-texture (W2.7) precedent. READ-ONLY otherwise.
**Late-add (user report 2026-07-12):** song-select per-instrument difficulty icons reported MISSING on a focused song row (+ F7 clipping "still" present — F7 stays unchartered backlog). Coordinator verification capture lands pre-dispatch or early-wave into `W31-REPRO/evidence/b_songfocus_*`; if MISSING/WHITE-FALLBACK confirmed, the sidebar icons join this lane's verification class (same suspected texture-bind family); if regression indicated, coordinator decides bisect-vs-fix-forward mid-wave.
Exit: one-mechanism fix verified across the class vs retail pairs, or split memo proving F2/F3/F4 are ≥2 mechanisms with each priced.

**Lane C — W31-EXIT-TRAP (Opus; hygiene):**
The exit-time teardown SIGSEGV (rc=-11/139, sometimes SIGABRT 134) every gate tolerates (`drawlog-golden.py:183-190,234-237`, `song-end-test.py:269`, W30 process rule 10). Known class: Dawn/GPU device teardown at static-dtor order (W0.3.S1 origin; W1.1/W2.2/W1.4 STATUS refs). **No committed symbolized backtrace exists — STEP 0 = capture one** (debug build + gdb/ASan on the bounded 5-frame non-HTTP boot; distinguish from the SEPARATE web-release "past-score-screen" trap targeted by `scripts/native/_exit-trap-test.py`, and from the W26 **splash→main_hub** panel-unload teardown (A10 correction — it is NOT in-song) — do NOT conflate, README:858).
THEN one fix: explicit device/queue teardown ordering (or a scoped `_exit`-class bypass ONLY if the ordering fix is priced prohibitive — that choice needs coordinator sign-off, it would freeze the trap not fix it).
Acceptance: bounded non-HTTP boot exits rc=0 ≥10/10; rb3-tests clean; drawlog-golden + lineup gates PASS. Per A7: the rc-tolerance removal from `drawlog-golden.py:183-190,234-237` is a **separate post-merge coordinator commit** gated on 10/10 rc=0 on the FINAL merged tree; `song-end-test.py:269` KEEPS its crash-detection band (narrow only if the fix also covers SIGABRT 134). No new flags unless the fix is behavioral (then default-OFF).
Rider (Sonnet side-task, same lane): web-side hub-menu capture — boot the deployed web release build via `scripts/web/menuhub-probe.mjs` / `scripts/web/keyboard-to-gameplay.mjs` (Playwright, shared `lib/core.mjs` — A11), FIRST verifying the deploy contains the current tree (A9), capture hub top-level + submenu focused states, grade the yellow-highlight geometry vs `W31-REPRO/evidence/a_hubtop_00_focused.png`. CONFIRMED-on-web → file as web-specific finding with crops (no fix this wave); clean → close the user report as not-reproduced-anywhere with evidence.
Owned surfaces: engine gfx teardown path. `scripts/native/drawlog-golden.py` tolerance lines = COORDINATOR-owned post-merge (A7). READ-ONLY: everything else.
Exit: rc=0 boots + tolerance lines removed, or a priced NO-GO w/ symbolized backtrace on record.

**Lane D — W31-HUBWALKER-SHARDS (Opus; diagnosis-ONLY, user-verified):**
The hub-walker residual family, now user-visible on faces: forehead flesh cone (`crop_hub_center_face.png`/`crop_hub_right_face.png`) + waist stick-fans (W30-F1) + crumpled boots/dangling foot (`crop_hub_legs.png`). Hypothesis to TEST, not assume: head-anchored scalp/hair mesh verts weighted to an undriven/wrong-basis bone (walk clips don't drive prop/appendage tracks — Q(f): "may need walk-clip prop-track scoping"). STEP-0 discriminators (checkpointed, NO fix code — three-supersessions rule):
(i) name the shard meshes + their bound bones (matrix-relative + pointer-verified, lint 1; split per-walker, lint 2);
(ii) are those bones driven by `playerN_{f,m}` walk clips (track enumeration, W27(b) method)?
(iii) is the basis error the SKEL 87°-family (then it's R5-HANDS-ENDGAME territory — STOP and memo) or a distinct undriven-track gap (then a scoped fix is legal)? Per A12: (iii) is NOT decidable by track enumeration alone — a **read-only live-bone transform probe** (matrix-relative, pointer-keyed; engine-side or distinct TU per A2, NOT BandCharacter.cpp/BandCamShot.cpp) is pre-authorized inside the diagnosis grant. Still NO fix code before the (iii) verdict.
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
- [x] **9. Flavor-membership check before scoping a lane on a file.** Lane A: FAILED at draft (grant named nonexistent `src/band3/bandobj/` — A1 BLOCKING), re-earned at review on the corrected `src/system/bandobj/` TUs (reviewer verified BandCharacter.cpp/BandCamShot.cpp/BandDirector.cpp in the rb3-native source lists). Lane C: teardown TU is engine gfx — verify compiled into rb3-native. B/D likewise on first-named TU.
- [x] **10. Tools ship as pre-dispatch diagnosis gates, not post-mortems.** Lane A's instrument (CHARDRV_PLAY census + BANDPERF probes) already exists and graded W30. Lane C's instrument = symbolized backtrace FIRST (STEP 0) before any fix. Lane D discriminators before fix. Lane B: end-to-end glyph trace before the bind fix.

## Close-out (coordinator, end of EVERY wave — owner directive 2026-07-07)

1. Post-wave Fable review → `WAVE31_CLOSEOUT_REVIEW.md` (verdict validity, gate coverage, shipped-flag contradictions, over-generous self-grades; errata one sequence E1..En).
2. Docs updated: README results table + Wave-32 menu; STATUS finalized; ROADMAP amended; **single classjson regen (`python3 scripts/analysis/native_compat_census.py gen`) AFTER flip + `RB3_BAND_PERF_FORCE_PLAY` retirement; ONE pin bump iff engine moved.**
3. Findings summary handed to owner (user-facing; includes the three user reports' dispositions).

## Risks / open questions for the reviewer — ALL ANSWERED at review

- **R-A → A6:** the data-absence branch is LIVE (zero venue-mood handling in tree); bounded routing sub-scope pre-authorized (≤2 TUs, no new parsing, no engine writes), else priced NO-GO.
- **R-B → A7:** tolerance removal = separate post-merge coordinator commit gated on 10/10 rc=0 on the FINAL merged tree; `song-end-test.py:269` keeps its detection band.
- **R-C → A1/A9:** sites re-derived (`src/system/bandobj/BandCharacter.cpp:4344/:4512`); Base SHA restated; probes/lever verified present post-retirement.
- **R-D → A12:** not decidable by enumeration alone; read-only live-bone probe pre-authorized; no fix before verdict.
