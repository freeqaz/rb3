# WAVE22_CLOSEOUT_REVIEW — Fable close-out (visual push: HUD fix / FOREARM narrowing / SWEEP menu)

**Reviewer:** Fable. **Date:** 2026-07-08. **Inputs re-verified from committed evidence**, not
trusted from the STATUS files: commits `d15484f1`/`669c244b`/`d794a18c`, `TrackPanelDir.cpp` +
`TrackPanel.cpp` at HEAD, an independent `batch_objdiff` run, the retail GT
(`yt_qRagnZCIMzk_gameplay_guitar.png`), HUD captures 01/03 + crops, FOREARM logs
(`discriminator-loadbind-player3.log`, `pollsite-own-equals-bound.log`, `burst-roi-off-vs-on.txt`,
`burst-off-shot10.png`), and the SWEEP compare crops + ROI provenance.

**VERDICT: ACCEPT-WITH-ERRATA.** The HUD fix is sound and the flip is endorsed (with conditions);
FOREARM's exoneration logic is airtight but its LOW-severity rating is contradicted by the wave's
own captures (ERR-2, the load-bearing erratum); the SWEEP menu is honest and actionable.

## Q1 — HUD fix soundness + flip safety: ACCEPT; ENDORSE the flip

- **(a) K9 was genuinely mis-premised.** The retail GT (`images/retail-screenshots/
  yt_qRagnZCIMzk_gameplay_guitar.png`, reviewer-inspected) shows the score pill top-RIGHT
  (center ≈88% width) with the star row beneath it. K9's original rationale
  (`TrackPanelDir.cpp:270-272`, "in retail single-player it is supposed to be neutralized … so the
  scoreboard ends up at world x=0 (top-center)") was derived from milo-config reasoning, never from
  a GT screenshot — a premise error, correctly overturned. Ledger row
  (`NATIVE_COMPAT_LEDGER.md:101`) never cited GT either.
- **(b) Re-scope preserves K9's other duties.** The flag only gates the scoreboard call
  (`TrackPanelDir.cpp:346-347`); applause/mtv anchors still neutralized (`:348-349`).
  `TrackPanel::ShowApplauseMeter()` returns false unconditionally (`TrackPanel.h:52`, no override
  anywhere in `src/` — reviewer-grepped), and `ConfigureTracks` hides `applause_meter.grp` via
  `b15` (`TrackPanelDir.cpp:369-373`) → inert in standard play. **Caveat:** `SetupApplauseMeter`
  can `SetShowing(true)` on the BotB path (`:515`); the flip doesn't touch that path, but the
  "visually inert" claim is standard-play-scoped, not universal (ERR-1).
- **(c) objdiff 100% is real.** The entire K9 block sits inside `#ifdef HX_NATIVE`
  (`TrackPanelDir.cpp:252-368`); the star diagnostic likewise (`TrackPanel.cpp`, d15484f1 hunk).
  Independently re-run: `ConfigureTracks__13TrackPanelDirFb` = 100.0% raw+fuzzy;
  `Poll__10TrackPanelFv` = 97.72% — pre-existing by construction (the edit cannot reach the Wii .o).
- **(d) Flip default-ON: YES, endorsed.** Blast radius is exactly the SP scoreboard position
  (K9 block already gated `nPlayers==1 && unk24c==1`, `:298` — multiplayer/band layouts never enter
  it, so no multiplayer regression is possible). GT is unambiguous. Conditions: invert to
  `RB3_HUD_SCOREBOARD_TOPRIGHT_OFF` opt-out (HUB_TICKER_YFIX lifecycle), one post-flip capture +
  drawlog gate, and update the ledger K9 row (ERR-3). Watch items (no GT, don't block): vocal-solo
  SP (`f19=0` path, `:247-249`) and BotB-SP scoreboard placement.

## Q2 — Star-meter false alarm: ACCEPT (residual noted)

- The dismissal is evidence-based, not hand-waved: `RB3_HUD_STAR_DBG` (committed diagnostic,
  `TrackPanel.cpp` Poll) shows `SetNumStars` fed rising f (0.190@12s → 3.405@90s), and
  `crop_starfill_hud.png` shows 3 filled + 1 dark pip at 17,458 pts — layout matches the GT's
  under-pill horizontal row. The 07-02 sighting at <1.0 star seeing one outline is the correct
  initial state under progressive reveal (`ResetStars` hides 1-4).
- **Residual (untested, LOW):** (i) the 5th pip / gold-star ≥5★ state was never driven (oracle
  stopped at 3.4); (ii) whether retail shows all 5 outlines from song start vs progressive reveal
  is unverified — no early-song retail GT frame exists. A single early-song frame from the same
  yt video would close it. Neither justifies reopening the bug.

## Q3 — FOREARM narrowing honesty: ACCEPT the exoneration; REVISE the severity

- **(a) Exoneration is airtight.** The lane's own load log shows `distinct=1` for EVERY slot —
  pelvis, spine, thighs included (`discriminator-loadbind-player3.log`) — so load-hook distinct is
  the generic pre-rebind state, not an arm-specific binding signature; the DRAW-site signature is
  load-bearing, and there `own==bound` pointer-identical 75/75 with the bone ITSELF posed to
  y≈171-187 (`pollsite-own-equals-bound.log:9-18`). Pose, not binding. Correct.
- **(b) No missed repoint target — credible.** The member-subtree capture (ObjDirItr over the
  member's own dir) found the foreArm/hand instance IS the magnet (`capturedOwn==bound`); only
  upperArm has a distinct `skeleton_unshared` instance. Three variants no-op'd, `FOREARM_REBIND`
  fired 0. A cross-member repoint would be wrong by construction. Not a premature give-up —
  the honest kill of the binding-fix class.
- **(c) Severity: "transition-only, LOW" is NOT honest as worded — re-rate LOW→MED (ERR-2).**
  The float frames are recurring in-song band-closeup CAMERA CUTS, not walk-on/count-in
  transitions: 6/16 frames of the lane's own burst are the low-skinned closeup class with the
  float in 3+, and `burst-off-shot10.png` shows a screen-third shard fan. Independently, BOTH HUD
  A/B captures (01/03 — different venue, different character, score 260 = ordinary gameplay) show
  the same artifact family (flesh limb right of the singer + pink/white mass top-left). Two lanes'
  routine captures hitting it ≈2-for-2 is not LOW. Also the gate row "ZERO in steady gameplay" is
  contradicted by the lane's own table: ON run `dl_01` (skinned=307) = 6 draws (ERR-4).

## Q4 — SWEEP menu quality: ACCEPT

- Reviewer-inspected `main_hub_native_vs_retail.png`: S1 crowd figures ARE in the retail frame and
  absent natively (ROI provenance shows zero skinned owners center-street) — real candidate,
  MED-HIGH fair; the RB3_NO_CROWD_REBIND check-first is the right dedupe. S2 is visually striking
  (native hub reads full-daylight vs retail night — arguably HIGH severity) but correctly
  confidence-capped for grade-family adjacency; add "check the render-polish wave-1/2
  `menu-lighting` default for regression/incompleteness" to its discriminator. S3 honestly
  LOW-confidence (no GT). S4 correctly parked (authored + C8/hands exclusions). S5's early-combo
  caveat is honest; retail GT does show the "4x" ring, so the driven-combo confirm is cheap and
  worth doing. Refutation of 07-02 #6 (subway back_border) is ROI-backed. Dedupe against the
  shipped defaults, C8, venue, hands, FOREARM, HUD is explicit (STATUS "Exclusions applied").
  Menu is actionable.

## Q5 — Cross-lane + hazards: CLEAN

- A2 guardrail respected: no finger-slot/clamp/mitten behavior edits; probes were log-only inside
  the two rebind hooks and all reverted; `669c244b` is docs+evidence only; `git status` shows no
  W22 code residue (the dirty `native/src/rb3_session_trace.cpp` belongs to another agent and was
  not staged by any W22 commit — verified all three `--stat` file lists; FxSendNative.cpp untouched).
- Consistency: the HUD-lane floats (01/03) and the FOREARM shard fan are the same artifact FAMILY
  (band-member limb mispose on closeup-cut frames) in different venues — the lanes corroborate
  each other, which is precisely what forces ERR-2.

## ERRATA (append-only)

- **ERR-1** `W22-HUD/STATUS.md:27-28` ("their neutralization is visually inert"): scope to
  standard play — append "(BotB `SetupApplauseMeter` can show `applause_meter.grp`
  (`TrackPanelDir.cpp:515`); that path is unchanged by this fix and has no GT)".
- **ERR-2** `W22-FOREARM/STATUS.md:67-69` ("player-visible severity is LOW"): re-rate **MED** and
  reword "transition-only" → "recurring in-song band-closeup camera-cut frames (plus count-in);
  visible in the HUD lane's own gameplay A/B captures at score 260".
- **ERR-3** `NATIVE_COMPAT_LEDGER.md:101` (`RB3_APPLY_HANDLER_FIX_OFF` row): stale — append "W22
  re-scope: scoreboard neutralization was GT-refuted (retail SP = top-RIGHT); scoreboard leg now
  gated by RB3_HUD_SCOREBOARD_TOPRIGHT (`d15484f1`)"; add the new flag's own row at flip time.
- **ERR-4** `W22-FOREARM/STATUS.md:45` ("ZERO in all steady-gameplay frames"): correct to "zero in
  all OFF steady frames; one ON steady frame (dl_01, skinned=307) showed 6 draws — attributed to
  burst nondeterminism (fix fired 0×), so the metric has ~±6 noise".
- **ERR-5** `W22-HUD/STATUS.md:44` (flag-OFF gate "PASS"): label as PASS-BY-CONSTRUCTION (code
  inspection of the wrapped call), not a measured drawlog-golden run; the post-flip capture
  (Q1 condition) supplies the measured half.

## WAVE-23 RECOMMENDATION

1. **HUD flip** (coordinator, hours): flip `RB3_HUD_SCOREBOARD_TOPRIGHT` → default-ON as the 13th
   default, per Q1d conditions (invert flag, post-flip capture, ledger rows).
2. **S2 hub grade discriminator FIRST** (cheapest lever, may be a one-flag fix and also
   re-scopes S4): `RB3_PP_OFF` / `RB3_UI_POST_GRADE_OFF` A/B + check the shipped menu-lighting
   default. If grade-path, fix; if not, charter as new.
3. **S1 hub crowd** (real content gap, MED-HIGH): actor-census discriminator per SWEEP
   (`RB3_NO_CROWD_REBIND` first, then hub-dir owner dump).
4. **FOREARM pose-driver lane — DISCOVERY-scoped, not parked** (upgraded by ERR-2): MED severity +
   two-lane corroboration justifies one instrumentation lane (what drives `bone_R-foreArm` to
   y≈+182 on closeup-cut frames — clip driver vs camera-cut pose reset, cf `67e87ae1`), with a
   hard stop before any fix code. Do NOT re-run binding experiments (closed here).
5. **S5 driven-combo confirm** (cheap, piggyback on any gameplay capture): ≥4x combo + now-bar
   ROI; only charter if absent under confirmed multiplier.
6. **S3/S4: defer** pending GT acquisition (S3) and the S2 outcome (S4).

**Final verdict: ACCEPT-WITH-ERRATA.** HUD = genuine win, flip endorsed. FOREARM = honest kill of
the binding class, severity corrected upward. SWEEP = thin but honest menu; Wave 23 has a clear
cheapest-first ordering.
