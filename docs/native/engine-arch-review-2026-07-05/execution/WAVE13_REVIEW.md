# Wave 13 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent (2026-07-07). **Input:** `WAVE13_KICKOFF.md` (draft), README Waves 1–12
(esp. Wave-12 table + Wave-13 menu), `WAVE12_KICKOFF.md`/`WAVE12_REVIEW.md`, STATUS docs
(`W2.8g/`, `W4.3-C1/`, `W4.3-C2/`, `W4.3-C34/PLAN.md`, `W2.8e/`, `W0.3d-b/`),
`docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` (present, 104KB), `/tmp/wave12-checkpoints/`,
plus direct source verification: rb3 `src/system/bandobj/BandCharacter.cpp`,
`src/system/world/Crowd.cpp`, engine @ pin `44716f4` (`src/platform/Rnd_Wgpu_RB3.cpp` 5,899 lines,
`RB3PostProc.{h,cpp}`, `RB3MaterialBinder.cpp`, `gfx/Shaders/rb3_postproc.wgsl.inc`).

## VERDICT: **dispatch-with-amendments**

The lane structure is right (the two Wave-12 failures each named a real, in-reach fix; the re-lane
of hands to a loader lane and the C1 grant are both correct calls). But the kickoff again contains
the class of error this gate exists for — this time concentrated in the two headline lanes:
**Lane S's fix statement conflates two different rest bases and omits the gender-pose half of a fix
the source itself already documents** (with an explicit "would also touch the crowd" warning the
kickoff does not cite), and **Lane G's shape menu includes one shape that is not implementable as
stated, while the natural shape carries a venueGrade/chroma-preserve trap that would silently
regress the authored B+W menu look**. Lane C's grant range is stale against today's file and the
C34 verdict the kickoff plans to "fold in" does not exist yet.

---

## Amendments

### A1 (CRITICAL, Lane S) — "the member's own ANIMATING bone carrying `skeleton_unshared.milo`'s authored rest" conflates two rests; the documented fix has TWO halves, the kickoff names one

The kickoff (Where-we-are #1, Lane S charter) states the fix as: make the mesh bind to "the
member's own ANIMATING bone carrying `skeleton_unshared.milo`'s authored rest." Against source,
that phrase is internally inconsistent:

- **`skeleton_unshared.milo` is MALE-BIND for every member.** `BandCharacter.cpp:3934-3935`
  (2026-06-06 hard-evidenced investigation, embedded in `OnSubDirAction`): *"skeleton_unshared.milo
  is itself male-bind; the gender pose comes from the outfit/clip."* The authored 129.9°/119.3°
  per-member rests (W2.8e A.S1 table) are the **outfit gender binds** — they live on the mesh's
  embedded static bind copy (`bound = mesh->BoneTransAt(b)`, `BandCharacter.cpp:1296-1298` "a
  STATIC embedded bind-pose skeleton copy") and, for bones the existing rebinds fix, in the
  **SetDeformation gender-bind rest pose** the per-member skeleton holds at first Poll
  (`BandCharacter.cpp:519-527`).
- The same source block names the full fix and its cost: `BandCharacter.cpp:3932-3935` — *"The
  faithful fix must un-share `char/main/skeleton.milo` for the band at the name-resolution / share
  layer (broad, high-risk; **would also touch the crowd**) AND pose each per-member skeleton to its
  outfit's gender bind."* Two halves. A per-member instance freshly loaded from the same male-bind
  file, never gender-posed, has the SAME ~106°-family rest as the magnet — instancing alone leaves
  `invOff` at 106° and the 87° gap intact. **The fix only closes the gap if the new per-member
  instance's rest (as captured by the first-distinct-resolve path,
  `BandCharacter.cpp:1595-1664`) is the gender bind** — which the head/torso precedent suggests
  happens via SetDeformation for bones already in the per-member skeleton, but is UNVERIFIED for a
  newly-un-shared skeleton.milo instance and for finger bones specifically.

**Amend S1's deliverable to answer three existence questions explicitly, before any S2 design:**
1. Do the finger/hand bones exist in the per-member skeleton at all? If `Find` falls through to the
   shared root because the per-member skeleton lacks those bones, there is nothing to "resolve to" —
   the fix becomes "extend the per-member skeleton + drive the new bones," a different (larger) seam.
2. Does the animation actually drive a per-member instance after un-sharing? If CharClips/CharBones
   bind their targets by captured pointers into the shared root (rather than per-dir name
   resolution), the un-shared instance is UNDRIVEN — a freeze, i.e. the 5th dead class re-created at
   the loader layer. S1 must name where the driver resolves its bone targets.
3. Does SetDeformation (or any gender-pose step) reach the new instance before the
   first-distinct-resolve rest capture? If not, S2 must add the gender-pose half explicitly.

Extend the STOP-TRIPWIRE accordingly: *"if the per-member resolved bone exists but does not ANIMATE
(freeze detector: distinct wext values ≤3), STOP and report the driver seam — do not fall back to
any offset bake."* The kickoff's current tripwire only forbids bakes; this failure mode is a
resolution change that lands as a freeze.

### A2 (HIGH, Lane S) — the crowd risk is documented in source, not hypothetical; the stated crowd gate names no instrument

`BandCharacter.cpp:3932-3933` says the un-share seam "would also touch the crowd" — because the
share is established at generic name-resolution (`each char RESOURCE milo lists char/main/
skeleton.milo as a share=true non-inlined subdir, so the FIRST loader creates it and every
subsequent reference (DirLoader::Find) shares it (ObjectDir::LoadSubDir, Dir.cpp)`,
`BandCharacter.cpp:3919-3923`; `LoadSubDir` at `src/system/obj/Dir.cpp:849`). Crowd characters load
char milos through the same layer, and their load-bearing rebind is
`RebindCrowdCharBonesToOwnSkeleton` (`src/system/world/Crowd.cpp:930`, called at `:433`) — proven
24× shard-drop when disabled (W2.3). The kickoff's gate "crowd clamp/spread byte-identical" names
no instrument and as written cannot fail red. **Require:**
- (a) **Scope the seam band-side.** The change must be scoped to the band-character install/merge
  path (the `OnSubDirAction`/install-filter territory, `BandCharacter.cpp:3938-3943`, where the
  kMerge→kReplace shim already lives), NOT a global change to `Dir.cpp`/`DirLoader` share
  resolution. A prior global attempt is already a documented dead end (`BandCharacter.cpp:3929-3931`
  — pruning the shared skeleton subdir "strips ALL outfit bones").
- (b) **Name the crowd instruments:** W2.1 crowd placement oracle GREEN in BOTH arms (flag-ON and
  OFF); the `RB3_NO_CROWD_REBIND` fail-red (24× shard-drop) still reproduces; crowd guard-DROP
  census (SHARD_GUARD by dir) unchanged. These exist and can fail red; "byte-identical
  clamp/spread" alone cannot.
- S1's interaction analysis must additionally show, by construction or by measurement, that
  `Crowd.cpp:930`'s resolution inputs are untouched.

### A3 (HIGH, Lane S) — S1 must name the actual writer of `hands_naked`'s final offsets; there is on-record evidence it is NOT (only) `RebindHeadHandsAtRest`

W2.8e A.S1's rule-7 disclosure (`W2.8e/STATUS.md:107-111`): its BandCharacter-side provenance probe
"never fired — `hands_naked` is rebound via a path other than `RebindHeadHandsAtRest`." Candidate
paths S1 must map (all reachable read-only): the GeomOwner propagation branch
(`BandCharacter.cpp:1438-1448` — the drawn mesh inherits `mNativeBonesRebound` from its owner, whose
offsets were baked elsewhere), and the ENGINE-side rebake/fling-clamp paths gated on
`mNativeBonesRebound` (`Rnd_Wgpu_RB3.cpp` ~`:3330` SKEL_REBAKE region; band shard guard
`:5035-5095`). If the final 106° offsets are baked by a path Lane S does not patch, a loader-side
fix silently no-ops (or worse, half-applies → mixed palette → V24 ratio drop,
`BandCharacter.cpp:1449-1453`). This is a mapping task, not a grant expansion — the renderer stays
read-only.

### A4 (MEDIUM, Lane S gates) — add the mechanism-level provenance gate; wext ≤60u is defensible but symptom-level

The ≤60u threshold is consistent with the existing instrument convention (`RB3_DUALSKIN_MINWEXT`
default 60u; greaserjacket clean-body control 12-54u wext, shellMax 1-4u; W2.8e freeze signature
pinned 56/80u would FAIL it via the max + the freeze detector). Keep it. But every gate listed is
symptom-level and skeleton-side — the class of metric this lane's own history shows can be confused
(W2.8e's unsatisfiable dual-skin metric). **Add:** post-fix, per-member `invOff` provenance —
`inv(off)` for hands bones must equal the member's own gender-bind rest and **differ across members**
(the 129.9°/119.3° family), no longer identical 106.0° across members. This is asset-derivable,
pose-independent, uses the already-landed probes (`RB3_APD_DIAG` / dual-skin), directly tests the
named mechanism, and cannot be gamed by any clamp. Also pre-register the greaserjacket control
unchanged in both arms.

### A5 (HIGH, Lane G) — shape (b) is not implementable as stated; shape (a) exists already as Tier-2 and carries a venueGrade/chroma-preserve trap

Verified composite order on engine `44716f4`:
- **Tier 1 (menus):** the WHOLE frame — venue backdrop AND UI — renders interleaved into the
  intermediate (`BeginFrame` target select `Rnd_Wgpu_RB3.cpp:1905-1913`), then ONE composite at
  EndFrame: `Rnd_Wgpu_RB3.cpp:1993-1996` → `RunPostProcComposite(mFrameView)` with `venueGrade`
  defaulting **false** (`Rnd_Wgpu_RB3.h:249`). UI is NOT a separate submission on menus.
- **Tier 2 (gameplay):** `FlushPostProcMidFrame` (`RB3PostProc.cpp:44-90`, fired via `DoPostProcess`
  `:92-101` at `Rnd::EndWorld`, or via `ClearDepthForOverlay` `Rnd_Wgpu_RB3.cpp:2307-2321`)
  composites the venue mid-frame and re-opens the pass on the FRAMEBUFFER — highway/HUD/UI then
  draw **ungraded** on top. This is the faithful Wii semantic (grade at EndWorld, 2D panels after).

Consequences:
- **Shape (a) is the real fix and mostly exists:** generalize the Tier-2 flush to menu screens
  (fire the flush at the venue→UI boundary). It is also the *faithful* shape — menus over-grading
  UI natively is the infidelity.
- **Shape (b) ("tag UI draws and have the grade shader pass them through") is NOT implementable as
  stated:** the grade is a fullscreen composite sampling a texture (`RunPostProcComposite`,
  `RB3PostProc.cpp:217`); per-draw identity does not exist at composite time. The only real variant
  is a pixel mask (stencil/alpha channel of the intermediate marking UI pixels) — implementable but
  riskier (the intermediate's alpha is sampled by at least the sky-dome RTT path,
  `Rnd_Wgpu_RB3.cpp:5221-5237` area). Rank it below (a).
- **THE TRAP:** `FlushPostProcMidFrame` hardcodes `RunPostProcComposite(mFrameView,
  /*venueGrade=*/true)` (`RB3PostProc.cpp:55`), while Tier-1 menu composites run `venueGrade=false`.
  The default-ON FIX-H2 chroma-preserve is gated on `venueGrade > 0.5`
  (`gfx/Shaders/rb3_postproc.wgsl.inc:228-230` — deliberately, "so the menu/song_select B+W
  'etched' look is untouched"). Reusing the flush on menus WITHOUT parameterizing `venueGrade`
  would activate chroma-preserve on the hub/song-select B+W film grade — an authored-look
  regression that **drawlog cannot catch** (same draws, changed uniform). Lane G must (i) pass
  `venueGrade=false` for a menu-boundary flush (or plumb the parameter), and (ii) pin a B+W menu
  backdrop capture (hub venue ROI, excluding UI) as an explicit ON≈OFF gate.

### A6 (MEDIUM, Lane G) — range grant: declare `:1973-2010`; the trigger site may fall outside the granted files; state WHY menus don't flush today as S1's first question

The end-of-frame composite region on today's file is `EndFrame` at `Rnd_Wgpu_RB3.cpp:1973` with the
Tier-1 composite at `:1993-1996` and `CompositeHaloBloom` at `:2005` — the kickoff's "~:1990-2010"
is roughly right; declare `:1973-2010`. But shape (a)'s edit is likely NOT there: the flush
machinery lives in the already-granted `RB3PostProc.cpp`, and the missing piece is the **trigger**
at the menu venue→UI boundary. Why menus never flush mid-frame (per the `:1986-1991` comment) is
unestablished — whether `EndWorld`/`DoPostProcess` fires late, fires after UI panels drew, or not
at all on menu screens is S1's first question, and the answer determines the trigger site. If the
trigger must come from the panel/draw ordering game-side (a `rb3_render_hook.cpp` or panel-seam
call, mirroring how `TrackPanel::Draw` → `ClearDepthForOverlay` triggers Tier 2 on gameplay), that
file is OUTSIDE the current grant — S1 must declare it in PLAN.md and get coordinator sign-off
before S2, rather than improvising. Possible secondary region: BeginFrame target selection
`:1905-1913` if a UI-pass split is chosen.

### A7 (MEDIUM, Lane G gates) — calibrate per screen against the PP_OFF control; two of three gated screens have NO baseline; do not build on (or co-flip) `RB3_HUB_TEXT_CONTRAST`

- PP_OFF reaches only **2.20 on the hub** (retail 4.17; default 1.95) — grade exemption alone
  passes the ≥2.0 gate with a 10% margin; the residual is the OTHER factor (bright bar compositing
  through semi-transparent AA text — the NOBAR experiment made text near-black,
  `W4.3-C1/STATUS.md:42-48`). song-select highlighted row and partdiff GUITAR were **never
  measured** (`W4.3-C1/STATUS.md:85-88` "Not re-verified this pass"). **Require S1 to capture
  default + PP_OFF baselines on all three gated screens first**; where PP_OFF itself reads <2.0,
  the absolute gate is unachievable by Lane G's mechanism and the pre-registered pass criterion for
  that screen must be PP_OFF-parity (ON within ε of the same-boot PP_OFF control), not absolute 2.0.
  If hub margin matters, shape (c) (opaque text) is the complement that attacks the bar-bleed
  factor — a legitimate combined-shape outcome, pre-register it as allowed.
- `RB3_HUB_TEXT_CONTRAST` (engine `RB3MaterialBinder.cpp:152-153`, default-OFF): it is a
  *faithfulness* fix (Wii clamps src alpha to [0,1]; the bar animates to 3.56) but it measurably
  **worsened** the contrast metric (1.95→1.81, `W4.3-C1/STATUS.md:44-45`). It is orthogonal to
  grade exemption: Lane G must run its gate with the clamp in its current default (OFF) and must
  NOT co-flip it; its flip is a separate coordinator decision on faithfulness grounds.

### A8 (MEDIUM, Lane C) — the C34 verdict does NOT exist at review time; and the C3 grant range is wrong on today's file

- `/tmp/wave12-checkpoints/` contains A-S1/A-S2/B-S1/B-S2/C1/C2.json but **no C34.json**, and
  `execution/W4.3-C34/` contains only PLAN.md (no STATUS.md) as of 2026-07-07. The kickoff's "its
  verdict is folded into the acceptance section at dispatch time" is currently unsatisfiable. If
  the side agent has not delivered by dispatch, Lane C3 must START from the committed
  `W4.3-C34/PLAN.md` (whose probe plan + verdict branches are sound and already reviewed-quality) —
  i.e. re-run the diagnosis; do not dispatch C3 with a fix charter against a verdict that doesn't
  exist.
- The conditional grant "~:5040-5090" for "the DrawMesh xfm region" is **stale**: on engine
  `44716f4` those lines are the band shard-guard/ratio-cap block (`RB3_BAND_SHARD_*`,
  `Rnd_Wgpu_RB3.cpp:5035-5095`). The obj.world/xfm composition in DrawMesh is at
  **`:3241-3273`**. Re-derive the grant by symbol at acceptance time, and note it would then be a
  SECOND writer region in Lane G's TU → see A9.

### A9 (LOW, cross-lane) — same-TU concurrency: sequence any C3 engine edit after Lane G

Lane S is renderer-read-only (its gates use already-landed instruments at `:4351-4966`) and its
only engine write is a classification.json append under the established lock — no collision. But if
C3's verdict branch (b) escalates into `Rnd_Wgpu_RB3.cpp`, that TU would have two writer lanes.
Disjoint line ranges is not the standard this campaign has used — single-writer-per-TU is
(WAVE10/W2.8e precedent). Coordinator-sequence: C3's engine edit (if any) lands after Lane G's
commits, or in a follow-up stage.

### A10 (LOW, Lane C2a) — pre-register the z/compositing guard

Per `W4.3-C2/STATUS.md:57-77` the backing lives in the sibling `song_select_details.milo` sub-panel
and the next step (never-submitted vs submitted-and-dropped) is correctly the lane's first task.
For the fix: "show the sub-panel" must be gated on draw-order evidence (details bg draws BEFORE the
grid `.idd`s — verifiable in the drawlog sequence) plus a grid-glyph-visibility ROI check on the
capture. Precedent risk: W4.1's `playnow.lsw` showed 360-ARK panels can carry natively-opaque quads
Wii hides — the details sub-panel may bring its own such quads along with the wanted backing;
census the sub-dir contents before showing it wholesale.

---

## Lane-by-lane assessment

**Lane S — right re-lane, under-specified fix statement, one unowned risk.** The B.S2 evidence
chain is genuinely strong (six composition cells measured dead; the 87° gap irreducible with any
single live bone; the rest-free Instrument-B discriminators are the trustworthy gate class). The
re-lane to loader/skeleton-merge is the only unrefuted direction and the kickoff is right to take
it. What the kickoff misses is that the source already documents this exact fix — with a two-part
requirement (un-share AND gender-pose, `BandCharacter.cpp:3932-3935`) and an explicit crowd
warning — and that three existence questions (finger bones per-member? driver follows? gender pose
reaches the instance?) determine whether "make Find resolve the member's own animating bone" is
even coherent as a single seam. With A1-A4 adopted, dispatch. Model choice (Opus) is right; this is
the hardest lane.

**Lane G — correct target, wrong shape menu, fixable at S1.** The C1 diagnosis is solid and the
grant is the right call. The corrected picture: menus take Tier-1 (UI interleaved into the graded
intermediate) while gameplay already draws UI post-grade via Tier-2 — so the fix is "make menus do
what gameplay already does," which is both lower-risk than the kickoff implies (machinery exists,
faithful semantic) and booby-trapped in one specific place (venueGrade/chroma-preserve, A5). Gate
calibration needs the per-screen PP_OFF baselines (A7) or the lane can fail/pass for reasons
unrelated to its change. With A5-A7 adopted, dispatch.

**Lane C — dispatchable with corrected premises.** C2a's plan matches its STATUS evidence. C2b+C4
"one family" is an assumption, not a finding — C34's PLAN explicitly proves C4 is NOT C3's rotation
bug (primary-only ActionElement skips the rotXfm at `InlineHelp.cpp:338-340`) and leaves C4 open
between an engine layout bug and a game-side missing offset (`W4.3-C34/PLAN.md:86-93`,
verdict branches at `:105-110`). Keep the PLAN's branches; don't pre-commit the fix side. C3 starts
from the PLAN unless the checkpoint lands (A8). Sonnet-impl/Opus-escalate tiering is fine.

**Process rules:** the pgid-only cleanup rule is a good add (C34 stall). Checkpoint path
`/tmp/wave13-checkpoints/` consistent. Nothing else to amend.

---

## Direct answers to the kickoff's risk questions

**R-A (Lane S seam + protection):** (i) The interaction analysis alone is NOT sufficient crowd
protection — the source itself warns the share-layer seam touches the crowd
(`BandCharacter.cpp:3933`); require the band-side scoping + the three named crowd instruments of
A2 (placement oracle both arms, RB3_NO_CROWD_REBIND fail-red reproduction, per-dir guard-DROP
census). (ii) "Rebind the outfit mesh to the unshared skeleton bone post-load" is NOT an
alternative seam — it is exactly the measured 5th dead class (`RB3_APPENDAGE_ASSET_REBAKE`: the
per-member bone is a static copy → freeze, `BandCharacter.cpp:1280-1309`). (iii) Why
`RebindHeadHandsAtRest` doesn't already fix this, from source: its default path REPOINTS
`bound`→`own=Find(name)` and rebakes `off = meshWorld·inv(rest_own)`
(`BandCharacter.cpp:1595-1664, 1700-1725`) — coherent AT REST w.r.t. the drawn bone, but `own` is
the shared magnet whose male-bind basis differs from the outfit's authored gender bind by the 87°
that IS the smear; no runtime bone carries the authored basis AND animates, so no rebind variant
can close it (that is what six dead cells proved). The seam is therefore genuinely
loader/share-layer + gender-pose — the kickoff's direction is right, its statement incomplete (A1).

**R-A addendum (the S1-MATCH pinning question):** the W2.8e invOff evidence pins the magnet at
**offset-BAKE time** — `inv(off)` is read from the mesh's baked `BoneOffsetAt` at draw, which
reflects the rest captured at the first clip-free distinct resolve; the DRAW-time magnet binding is
separately established (the `SetBone` repoint + `APD_DIAG`'s `own=%p` pointer log). One honest
caveat: "identical 106° across members" is by itself ALSO consistent with per-member instances of
the same male-bind FILE (never gender-posed) — the shared-single-instance claim rests on the
2026-06-06 pointer-level probes (`parent==nil` shared root, `BandCharacter.cpp:3917-3925`), which
is why A1's question 3 (does the gender pose reach the instance?) is load-bearing: it is the
difference between "instancing fixes it" and "instancing changes nothing."

**R-B (Lane G shape):** UI is NOT a separate submission on menus — it is interleaved into the
single intermediate pass (Tier 1), so pure reordering of an existing submission is not available.
But the mid-frame flush (Tier 2) already implements scene-pass → grade → ungraded-UI on gameplay,
and generalizing it to menus is shape (a) with existing machinery and the faithful Wii semantic.
Shape (b) is not implementable as stated (no per-draw identity at composite time); its only real
variant is a pixel mask, ranked below (a). Shape (c) is a complement targeting the bar-bleed
factor, not the grade. The one thing that must not be cargo-culted: the flush's hardcoded
`venueGrade=true` (A5's trap).

**R-C (Lane C):** C2a's guard = draw-order evidence + grid-visibility ROI + sub-panel content
census before showing (A10). The C2b/C4 family premise is ASSUMED — treat as a hypothesis with
C34-PLAN's verdict branches, not a fix premise (A9-adjacent, A8).

**R-D (cheapest global no-regression net for Lane G):** four cheap layers, strongest first:
(1) **gameplay invariance** — gameplay already runs Tier-2, so flag-ON must be pixel-identical on a
pinned gameplay frame (songMs-pinned, fixed-clock): a near-free, high-power check that the change
is menu-scoped; (2) the pinned **B+W menu backdrop ROI** (venue area of hub, UI excluded) ON≈OFF —
the venueGrade trap detector (A5); (3) `wash_score` on the pinned venue shot within the established
per-boot noise band (BOOTRNG floor is known: mid_sat 0.067-0.362 — use the W0.3d-b `--tol` 150ms
lever and N≥3/arm, don't single-shot it); (4) drawlog 792 + milo-engine-tests incl. Dawn WGSL gtest
(already listed). A full screen-sweep SSIM adds little over (1)+(2) given per-boot lighting noise;
if used, restrict to menu screens with UI-excluded ROIs.

---

## Source appendix (verified at review time)

- rb3 `src/system/bandobj/BandCharacter.cpp` (HEAD, master): `:519-527` head-rebind pre-Poll
  ordering + SetDeformation gender-bind rest; `:574` torso rebind post-Poll; `:1101-1236`
  `RebindOutfitBonesToOwnSkeleton` (torso-scoped, `Find`-repoint, authored offsets kept);
  `:1253-1797` `RebindHeadHandsAtRest` (first-distinct-resolve rest capture `:1595-1664`, two-pass
  apply + repoint + `off = meshWorld·inv(rest)` `:1700-1725`, GeomOwner propagation `:1438-1448`);
  dead-cell documentation in-source `:1260-1344` (`RB3_APPENDAGE_REST_ROT`,
  `RB3_APPENDAGE_ASSET_REBAKE` freeze root-cause `:1296-1306`, `RB3_HANDS_SHELL_FIX` B-S2 cell
  `:1316-1339`, shell-fix branch `:1476-1506`); the 2026-06-06 shared-skeleton investigation +
  two-part faithful fix + crowd warning `:3915-3937`; merge-filter shim `:3938-3943`.
- rb3 `src/system/world/Crowd.cpp`: `:45/:433/:930` `RebindCrowdCharBonesToOwnSkeleton`.
- rb3 `src/system/obj/Dir.cpp`: `:849` `ObjectDir::LoadSubDir` (the share layer named at
  `BandCharacter.cpp:3923`).
- engine `44716f4` `src/platform/Rnd_Wgpu_RB3.cpp` (5,899 lines): BeginFrame target select
  `:1905-1913`; `EndFrame` `:1973`; Tier-1 composite `:1993-1996` (venueGrade default false per
  `Rnd_Wgpu_RB3.h:249`); halo `:2005`; `ClearDepthForOverlay` Tier-2 trigger `:2307-2321`;
  obj.world composition `:3241-3273`; wext/dualskin/attach/Instrument-B probes `:4351-4966`; band
  shard guard (the mis-granted "5040-5090") `:5035-5095`.
- engine `src/platform/RB3PostProc.cpp`: `FlushPostProcMidFrame` `:44-90` (hardcoded
  `venueGrade=true` at `:55`); `DoPostProcess` `:92-101`; `MainColorTarget` `:127-140`;
  `RunPostProcComposite(dst, venueGrade)` `:217`.
- engine `gfx/Shaders/rb3_postproc.wgsl.inc`: chroma-preserve gated on
  `chromaPreserveActive>0.5 && venueGrade>0.5` `:228-230`.
- engine `src/platform/RB3MaterialBinder.cpp`: `RB3_HUB_TEXT_CONTRAST` alpha clamp `:152-153`;
  relaxed UI-text floor `:166-219`.
- Checkpoints: `/tmp/wave12-checkpoints/{A-S1,A-S2,B-S1,B-S2,C1,C2}.json` present; **C34.json
  absent**; `execution/W4.3-C34/` = PLAN.md only.
