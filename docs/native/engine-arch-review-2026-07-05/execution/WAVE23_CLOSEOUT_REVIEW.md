# WAVE23_CLOSEOUT_REVIEW — Fable adversarial close-out (GRADE / CROWD / FOREARM)

**Reviewer:** Fable. **Date:** 2026-07-08. **Inputs:** the three lane STATUS/PLAN/evidence
trees (commits `3af48f67`, `0fad6137`, `94dfe32e`), `WAVE23_REVIEW.md` (A1–A9),
`W22-FOREARM/STATUS.md` (+ its ERR-2/ERR-4), and code at HEAD: `BandRetargetVignette.cpp`,
`BandCharacter.cpp:696-749`, engine `Rnd_Wgpu_RB3.cpp` (WASH_PROBE `:1119-1145` verified),
`src/system/rndobj/Mesh.cpp` (the "re-export" warning is at `Mesh.cpp:1116-1120`, not
"RndMesh.cpp:1118"), `native/src/rb3_http_handlers.cpp:1000-1075`, `rb3_render_mesh.cpp`.

**VERDICT: GRADE ACCEPT-WITH-ERRATA · CROWD ACCEPT-WITH-ERRATA · FOREARM REVISE.**
No forbidden files staged in any of the three commits (stat lists verified;
`rb3_session_trace.cpp` / `FxSendNative.cpp` untouched).

## Q1 — GRADE no-fix soundness: ACCEPT-WITH-ERRATA

- **(a) engaged=1 refutation: SOUND.** `RB3_WASH_PROBE` is real and pre-existing (engine
  `Rnd_Wgpu_RB3.cpp:1119/1128/1140`); the digest shows `engaged=1` on every hub environ, and
  the B_VENUE_OFF arm is *brighter* (frameMean 0.408 vs 0.344) — the venue path is engaged and
  net-darkening. Non-engagement (WAVE23_REVIEW's prime suspect (b)) is genuinely refuted.
- **(b) camera-phase: variance PROVEN, "SWEEP caught the worst phase" OVERSTATED.** The sweep
  (contrast_metric.txt) shows 2.17–6.88:1 across settle depths — but settle_150 (2.33) and
  settle_500 (2.17) measure *lower* contrast than baseline 5.26. "Worst" holds only on the
  frameMean/bright axis (settle_150 = 0.411 brightest). The gap to retail exists at every phase.
- **(c) NOT apples-to-apples as written.** The STATUS compares 5.26 against "wave-5 pre-fix
  2.6:1". The wave-5 **post-fix verified** number was **6.8:1** (impl 8.4) —
  `docs/native/render-polish-2026-06-11/PLAN.md:256`. Against the correct baseline: today's
  peak phase 6.88 ≈ 6.8 (fix holds at matched phase), but the arm-window 5.26 is *below* the
  wave-5 verified value. The conclusion survives via phase variance; the yardstick in the
  STATUS is the wrong one.
- **(d) Did it give up early? Mostly no, one mislabel.** Knob probes (grey key 0.08, dir
  exposure) genuinely can't darken the unlit-path backdrop, and PP_OFF lowers contrast. But the
  E1 relative finding — lit sign plates at **2.33×** backdrop vs retail **0.98×** *on identical
  360 assets* — is a real, unexplained native rendering difference (ue=1 sign plates too hot),
  not "authored-faithful". Deprioritizing it (outside the grade grant) is defensible;
  labeling it faithful is not. Log it as an open candidate (S2-residual/sign-compositing).
- S4 non-re-scope: correct given no fix. Gates trivially satisfied (no code) — accepted.

**ERRATA-G1** (append to W23-GRADE/STATUS.md): "The '2.6:1 pre-fix' comparison uses the wrong
baseline; wave-5's verified post-fix contrast was 6.8:1 (render-polish PLAN.md:256). Current
peak phase 6.88 matches it; the 5.26 arm-window sits below it (phase). Verdict unchanged."
**ERRATA-G2:** "'SWEEP caught the worst phase' is unsupported — settle_150/500 measure lower
contrast than baseline; phase *variance* is what is proven. The neon-plate 2.33-vs-0.98
relative gap on identical assets is an OPEN native rendering difference (deprioritized), not
'authored-faithful'."

## Q2 — CROWD handoff: ACCEPT-WITH-ERRATA (root-cause statement is instrument-limited)

- **Solid:** the drawlog proof (0 crowd body draws among 341/140-skinned, deterministic across
  boots), the census scoping, the a/b/c/d branch resolution, the `RB3_NO_CROWD_REBIND` dedupe,
  and the `gAltRev < 3 && NumBones() > 1` re-export warning (real, `Mesh.cpp:1116-1120`).
- **NOT solid: "every body mesh loads with 0 VERTICES" as decode-gap proof.** The census reads
  `mit->Verts().size()` (= `mVerts`, `rb3_http_handlers.cpp:1042`). But on HX_NATIVE the
  compressed-vert load path **leaves `mVerts` empty BY DESIGN** and fills `mCompressedVerts`
  ("mGeomOwner->mVerts stays empty; the backend uses mCompressedVerts",
  `Mesh.cpp::PostLoadVertices` native branch); the native draw gate is
  `mVerts.size() > 0 || mNumCompressedVerts > 0` (`rb3_render_mesh.cpp:126,455`). All 48
  `[CROWDMESH]` lines read verts=0 with **no positive control** — a band outfit mesh (which
  draws fine) would likely also read vert=0 on this metric. So "geomOwner=self + verts=0 ⇒
  genuine empty mesh" does not follow; the census never read `mNumCompressedVerts`.
- **A6 out-of-scope call: RIGHT.** Whatever the exact gap, it lives in the shared
  `PostLoadVertices`/mesh-rev read path; the gameplay WorldCrowd's multimesh source geometry
  rides the same loader (different milo assets, same code) — the "must A/B gameplay crowd"
  gate is correct and the risk claim is true in the way that matters.
- **Charter: actionable only after re-instrumentation.** Step 0 of Wave-24 must extend the
  census to report `mNumCompressedVerts` + run a band-mesh positive control. If crowd bodies
  show compressed=0 too → real decode gap in the old-rev (`gAltRev<3`) skinned path → fix
  scoped to that branch. If compressed>0 → the gap is downstream (draw/skin path), and the
  charter's target is wrong.

**ERRATA-C1** (append to W23-CROWD/STATUS.md): "The vert=0 census metric reads mVerts only;
on HX_NATIVE compressed meshes keep mVerts empty by design and draw from mNumCompressedVerts
(rb3_render_mesh.cpp:126,455). No positive control was run. 'Loads with 0 vertices' is
therefore *unconfirmed as the root cause*; confirmed facts are: 0 crowd body draws (drawlog)
and gAltRev<3 on these meshes. Wave-24 step 0 = re-census with mNumCompressedVerts + band-mesh
control before touching the loader."

## Q3 — FOREARM driver-naming: REVISE (plausible, NOT proven; W22 'correction' retracted)

Verified in source: `BandRetargetVignette::sIkfs` does include `bone_R/L-foreArm.ikf` +
`bone_R/L-hand.ikf` (`BandRetargetVignette.cpp:9-19`), `EnterDir` wires `ik->mMore`
(`:59-...`), `Poll` retargets `playerN` onto band chars. The probe/measurement chain is real.
But the headline claims fail adversarial scrutiny on the lane's **own committed evidence**:

1. **The y>50 trigger cannot distinguish fling from legitimate world placement.** In-song HI
   events fire with `clipType='guitar_body'` and pre-Poll y *already* 96.9
   (`fling-persistence.log` frame 13213, moved=237 on a `stand`→`closeup` window) — exactly
   what camera-group stage-mark placement/teleports produce. The onset jumps
   (pre y=-1.5 → post y=110-183 at frames 252-255) are equally consistent with the *first
   posed frame placing the whole member at its walk-on mark*: pre-Poll positions are the
   identical unposed rest pose for all members.
2. **The lane's own control contradicts "localized to the forearm chain".** The upperArm run
   logged **29,559 HI events — more than the forearm's 28,758** — and at the same onset frame
   252 the upperArm jumps to y=107 (`fling-persistence.log`). Whole-arm (plausibly
   whole-member), not forearm-chain.
3. **Its own heartbeats contradict "PERSISTENT every clip frame".** player3's forearm settles
   at y≈-50 *while vignette clip `player3_f` is playing* (frames 1440-2160,
   `r-forearm-clip-driver.log`), parent at -48 — anatomically sane, mid-vignette.
4. **W22's anatomical signature was never reproduced.** W22's load-bearing evidence was
   *same-frame* divergence (foreArm y≈182 vs upperArm y≈-0.8). W23 has no same-frame
   multi-bone capture; forearm and upperArm numbers come from separate boots. The
   "PERSISTENT, not transition-only — W22 corrected" re-rating is most plausibly threshold
   pollution; **W22's transition-only read may still be correct.**
5. **No probe line is tied to a spike-fan frame.** Both PNGs are in-song (score 50 / 575,
   count-in-adjacent + closeup) — the windows W22 already flagged — and neither is correlated
   to a vignette-clip HI event.

What IS sound: driver present + named on flung frames (freeze-class `67e87ae1` refutation
holds), POSE not skinning-compose (bone WorldXfm itself high *when* high), bilateral symmetry,
and the sIkfs source anchor. One un-followed lead worth keeping: at onset the members play
**crossed** clips (player0↔player3_m, player3↔player1_f) and go high; the one *matched*
pairing observed (player3↔player3_f) settles sane.

**ERRATA-F1** (append to W23-FOREARM/STATUS.md): "Close-out review: DRIVER NAMED is retracted
to DRIVER CANDIDATE. The y>50 instrument also fires on legitimate stage-mark placement
(in-song HI events are guitar_body-driven, pre-y already >50); upperArm logged MORE HI events
(29,559) than foreArm and jumps identically at onset; player3's forearm settles LOW mid-vignette.
'PERSISTENT / W22-corrected' is unproven — W22's transition-only characterization stands until a
same-frame multi-bone anatomical probe (child-parent distance > bone length) reproduces the
W22 divergence and correlates it with a spike-fan capture."

## Q4 — FOREARM severity/visibility: keep MED

If 28,758 genuine flings were real, every gameplay capture would look like
`forearm-playing.png`; the lanes' own coherent captures (HUD lane, guitarist intact) refute
constant explosion. The reconciliation: most HI events are placement pollution; the *visible*
artifact concentrates on count-in/cut/closeup frames (exactly W22 ERR-2's basis) and needs the
affected member on-camera. Severity stays **MED** (recurring, visible, screen-third fans on cut
frames) — do not upgrade to HIGH on the "persistent" claim, do not downgrade to LOW (ERR-2's
in-song recurrence evidence stands).

## Q5 — Probe edit safety: SAFE, keep in-tree

Verified at `BandCharacter.cpp:660-749`: both blocks are inside `#ifdef HX_NATIVE` (Wii object
byte-identical — batch_objdiff trivially baseline, though the lane's gate table omitted the
row); with `BAND_ANIM_PROBE` unset, `banim=false` and the only residue is two getenv calls —
no Find, no emission. drawlog-792 PASS confirms. Keep in-tree as Wave-24's acceptance
instrument, but Wave-24 must **replace the y>50 trigger with an anatomical trigger**
(same-frame |foreArm − upperArm| > bone-length, multi-bone emit) before trusting counts.

## Q6 — WAVE-24 RECOMMENDATION (ranked)

1. **FOREARM-RECON (do first, ~1 day, probe-only):** same-frame pelvis+upperArm+foreArm+hand
   probe with the anatomical trigger; correlate one spike-fan screenshot to probe lines; split
   walk-on-vignette window vs steady vs cut frames; check the crossed-vs-matched clip-pairing
   lead. Only then choose between IK-not-constraining / clip-decode / W22's transition class.
   **Do NOT dispatch a BandRetargetVignette/BandIKEffector fix lane on W23 evidence alone** —
   a mis-named driver sends the fix to the wrong TU.
2. **CROWD-RECON → engine fix (highest fix EV):** hours to re-census with
   `mNumCompressedVerts` + band-mesh control; if compressed=0 confirmed → RndMesh old-rev
   (`gAltRev<3`) skinned decode fix, scoped to that branch, with the mandatory gameplay
   WorldCrowd A/B. Tractable, medium risk (shared loader) — the branch-scoping keeps the
   protected oracle's hot path untouched.
3. **S5 combo-glow confirm** (small, deferred from SWEEP). 4. S3/S4 stay deferred/authored.

## Q7 — Census/flags for coordinator classification

- **NEW this wave:** `BAND_ANIM_YTHRESH` (probe threshold, inert unless BAND_ANIM_PROBE set),
  `CROWD_CENSUS_MESHES` (census verbosity env), `{rb3_crowd_census}` DTA func (read-only,
  native-only). Classify all three as diagnostic/probe, default-inert.
- **Pre-existing, not new:** `RB3_WASH_PROBE`, `RB3_VENUE_PROBE`, `STRIDE_PROBE`,
  `RB3_VENUE_FALLBACK_FIX`. No shipped-default changes anywhere in the wave. No pin bumps.
- Forbidden-file staging: **none** in `3af48f67` / `0fad6137` / `94dfe32e`.

## FINAL

GRADE **ACCEPT-WITH-ERRATA** (verdict (c) stands; fix the baseline + phase wording; log the
sign-compositing residual as open). CROWD **ACCEPT-WITH-ERRATA** (handoff right; root-cause
downgraded to candidate pending mNumCompressedVerts re-census). FOREARM **REVISE** (evidence
and probe kept; headline retracted to driver-CANDIDATE; W22 transition-only framing restored
pending the anatomical probe; Wave-24 must start with RECON, not a retarget/IK fix).
