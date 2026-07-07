# Wave 16 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent (2026-07-07). **Input:** `WAVE16_KICKOFF.md` (draft), README Waves
13–15 results + Wave-16 menu, `HANDS-ADJUDICATION/VERDICT.md` + `PLAN.md` + committed
`evidence/` (arm_summaries.txt, armC/W/S probe greps, offset_basis_derivation.py, E1 PNGs),
`W4.4-ROWFIX/STATUS.md`, `W4.5-GAMERTAG/STATUS.md`, `WAVE15_KICKOFF.md` (acceptance) /
`WAVE15_REVIEW.md`; plus direct source re-derivation: rb3
`src/system/bandobj/BandCharacter.cpp` (Poll ordering `:520-575`, seed capture `:974-1037`,
`RebindHeadHandsAtRest` `:1254-1847` — pass A `:1512-1749`, pass B `:1750-1818`, every
appendage flag branch), `src/system/rndobj/Mesh.{h,cpp}` (`SetBone` `:328/:226`),
`src/system/rndobj/Text.cpp` (`SetupCharVerts` `:1178-1237`, glyph-mesh material `:1165,1402`,
alt style `:394-411,554-566`), `src/system/ui/UILabel.cpp` (`DrawShowing` `:253-292`),
`src/system/ui/UIListSlot.cpp` (`:80-127`), `src/system/ui/UIListLabel.cpp` (`:65-90`),
`src/system/ui/UIListHighlight.cpp` (`:51-77`), `native/src/rb3_render_hook.cpp` (`:109-147`),
`native/src/rb3_platform_native.cpp` (`:98-122`); engine @ pin `84ccb9e` (verified HEAD; one
concurrent uncommitted `M src/platform/FxSendNative.cpp`, leave untouched):
`src/platform/Rnd_Wgpu_RB3.cpp` (SKEL_REBAKE `:3442-3556`, HANDS_ATTACH probe `:4736-4875`),
`src/platform/RB3MaterialBinder.cpp` (`:105,132-336`), `src/gfx/standard_wgsl.inc`
(`:722-778,850-855`), `NativeCompatFlags.classification.json` (`:1449,1456`).

## VERDICT: **dispatch-with-amendments**

The two-lane shape is right and Lane F is unusually well-founded — I re-derived the adjudicated
cell from `BandCharacter.cpp` and it is genuinely distinct from all seven dead artifacts and
genuinely never measured (A3). This wave's false premise is in **Lane T's framing**: the record's
claim (W4.4-ROWFIX STATUS §3B → README Wave-15 table → this kickoff) that "**the native
RndText/glyph shader ignores the font-material color**" is contradicted at source. The WGSL
multiplies `material.color.rgb` into EVERY draw including the text branch
(`standard_wgsl.inc:764,768-770`), the binder loads `mu.color` from `mat->GetColor()` for every
material (`RB3MaterialBinder.cpp:132-134`), text is force-prelit so nothing zeroes it
(`:280`), and the W4.2 relaxed-floor work PROVED dark focus-state `fontMat->SetColor` values
reach pixels through this exact path on the hub (that is what the default-ON
`RB3_UI_TEXT_FLOOR_RELAXED` flip was measured on, `RB3MaterialBinder.cpp:166-217`). So the gap
is *path-specific to the list-label route*, not a shader-global "make it honor material color"
engine item — and there is a precise, source-visible prime suspect the diagnosis must test
first: `UILabel::DrawShowing` propagates `mColorOverride` ONLY to the MAIN font's material
(`UILabel.cpp:266-270`); the ALT font's material is always set from `mAltTextColor` /
`GetStateColor` (`:279-292`), and multi-font/alt-styled RndText draws its glyph submeshes with
per-font materials (`Text.cpp:1402`) the override never touched — exactly reproducing the
ROWFIX observation "line 269 fires with dark, pixels stay luma 167". The fix may be entirely
rb3-side, and R-C's "global tint" worry largely dissolves (A2). Lane F dispatches with
implementation-semantics amendments: the "keep authored offsets" premise has two mutation
windows that must be verified rather than assumed (A4), and the §5 female gate as worded can
pass a broken female — 28.9° is the *failure* signature, not an acceptable bound (A6).

---

## Amendments

### A1 (CRITICAL, Lane T) — re-frame: the shader already honors material color; diagnose the list-label route, alt-font override gap first; extend the grant to rb3 `src/system/ui`

The kickoff dispatches Lane T as "make the native glyph shader honor font-material color …
(engine text/glyph path + WGSL)". Source says the shader already does:

- `standard_wgsl.inc:764` — `baseColor = vec4f(material.color.rgb * vertexTint, …)` for every
  draw; the text branch (`:768-770`, `useAlphaAsRGB==1`) multiplies that same baseColor by the
  atlas alpha. `material.color` is never bypassed for text.
- `RB3MaterialBinder.cpp:132-134` — `mu.color` = `mat->GetColor()` per draw; `:280` force-prelits
  text (vertexTint = the glyph vertex color RndText writes at `Text.cpp:1237`); the W4.2
  relaxed floor (default-ON, `:208-232`) passes authored colors ≥0.06/channel through unchanged.
- Proof it works end-to-end: the Wave-7/W4.2 hub result — focused item renders its authored DARK
  `fontMat` color on the gold bar while unfocused siblings render dimmed grey, simultaneously,
  through `UILabel::DrawShowing`'s same mutate-set-draw pattern. If the glyph pipeline ignored
  font-material color, that shipped, measured fix could not exist.

So the ROWFIX Part-B observation (verified real: `UIListSlot.cpp:101-125` darkens the shared
`uicolor`, `UIListLabelElement::Draw` `:83` routes it as `SetColorOverride`, `UILabel.cpp:269`
sets the MAIN fontMat dark — yet text stays luma 167) must have a path-specific cause. Ranked
suspects, all cheaper than shader work:

1. **Alt-font material never receives the override** (`UILabel.cpp:279-292`: alt mat gets
   `mAltTextColor` or `GetStateColor`, NOT `mColorOverride`). If the music-library row text is
   alt-styled or multi-font (`Text.cpp:554-566` `<alt>` markup; `:1402` per-font-key submesh
   materials), the darkened material is not the one the visible glyphs draw with.
2. The visible glyphs belong to a **different label/text object** than the one darkened
   (3756 fires is a lot of draws; the probe never confirmed the darkened label OWNS the
   focused-row pixels).
3. Binder-side classification/floor anomalies for these specific meshes — directly observable.

**Step 1 is observation, not engine code:** run with `RB3_UI_FLOOR_DBG=1` (existing probe,
`RB3MaterialBinder.cpp:203-207`, dumps mesh/mat/pre-floor RGB for every UI-text draw) plus a
one-shot Text-side dump of which font/material each submesh of the focused-row label uses
(main vs alt, `Text.cpp:1165/1402`). Only after the drop point is NAMED does the lane decide
fix side. Consequence for the grant: **pre-authorize rb3 `src/system/ui/` (UILabel.cpp /
UIListLabel.cpp / UIListSlot.cpp — files W4.4 already touched) alongside the engine text TUs**,
or the likely fix (propagate the override to the alt-font mat, or route the rowfix color via
the text's own vertex color `RndText::SetColor`) is out-of-scope and the lane stalls mid-wave
exactly like C1/W4.4 did. Engine/WGSL edits may turn out to be ZERO.

### A2 (HIGH, Lane T / answers R-C) — the "global tint" risk is largely moot: no NEW multiply is introduced; do not scope by "!= white"

R-C asks whether honoring font-material color could tint every label because "fonts may rely on
white multiply today". The multiply already exists globally (A1), and font materials do NOT sit
at a passive authored white: **every `UILabel::DrawShowing` actively sets its font material's
color every draw** (`UILabel.cpp:266-277` — state color or override, unconditionally), and the
per-draw uniform snapshot demonstrably keeps simultaneous different colors correct (hub focused
vs unfocused). Non-UILabel RndText consumers inherit last-writer color on a shared font mat —
which is the Wii immediate-mode semantic too, i.e. the status quo, not a new risk. Therefore:

- The cheaper structural argument the kickoff asks for EXISTS: a fix that only routes the
  ALREADY-SET color to the correct material (or correct label) cannot change any draw whose
  material color was already reaching pixels. The blast radius is exactly the set of draws whose
  sampled material changes — enumerable from the diagnosis, not "all text".
- **Do NOT implement "multiply only when != white"** — that special-case would break nothing
  today but is dead logic on a multiply that already ships; and scoping to "UILabel-driven
  materials only" is equally unnecessary once the real drop point is named.
- Keep the kickoff's screen-sweep no-regression net (hub ≥2.0, W4.2 floor arms, Wave-7 labels,
  drawlog 792, DC3 zero-blast + Dawn WGSL gtest) as belt-and-braces — it is sufficient; with the
  structural argument it is no longer the only line of defense.
- Nit: the ROWFIX darken constant `(0.06,0.05,0.02)` (`UIListSlot.cpp:115`) sits exactly AT the
  relaxed-floor boundary `kTrueInvisibleUiText=0.06` (`RB3MaterialBinder.cpp:219-227`) — one
  epsilon darker on all channels and the binder lifts it to 0.25. When Part B lights up, pick a
  dark in the authored hub band (~0.118) so the floor can never interact with the gate.

### A3 (HIGH, Lane F / answers R-A) — the adjudicated cell re-derived: genuinely distinct, genuinely never measured; CONFIRMED

Re-derivation from `BandCharacter.cpp` (all seven artifact branches read):

- **Today's DEFAULT for an appendage mesh:** pass A resolves `own = Find(bound->Name())`
  (`:1516`) and a rest basis — the SetDeformation-time seed (`NativeCaptureRestPoseAfterDeform`
  `:974-1037` → `mNativeRestPose`) or the first-distinct clip-free capture of `own`
  (`:1706-1714`, which OVERWRITES the seed). Pass B then **repoints** (`SetBone(b, owns[b],
  false)` `:1756-1757`) AND **always rebakes** — `Multiply(mesh->WorldXfm(), invRest,
  mesh->BoneOffsetAt(b))` `:1775` overwrites the authored offset unconditionally. Both rest
  sources are transients (the 42.6/87.3 bimodality's two capture classes) — the VERDICT §2
  mechanism verifies at source.
- **The new cell** = same pass-A resolution, pass-B repoint, and SKIP the `:1775` write for
  `apdMesh`. What survives: authored offsets (= `inv(B)` per arm-W xcheck 0.1°, matrix-level).
  What changes vs DEFAULT: only the offset anchor (transient R → authored B); binding identical.
  What changes vs `RB3_HANDS_SHELL_FIX` (`:1531-1556`): SHELL_FIX still BAKES —
  `rests[b] = NativeCharSpaceRestXfm(bound)` (one shared-B-derived anchor, incl. a translation
  re-derivation) forced onto every appendage mesh; the new cell keeps each mesh's own authored
  matrix — differing by 28.9° (female), 60-69° (gloves), ~170° (nails), and by translation
  even for male hands. What changes vs `RB3_NO_HEAD_REBIND` (arm W): binding (own vs static
  bound); offsets identical. So in the 2×2 {offset anchor}×{bone} table the cell
  (authored, own) is the one never run; the other six artifacts are baked variants
  (`:1573-1603` asset-rebake keeps `bound`+bakes; `:1613-1620/:1765-1772` world-rest bake;
  `:1406-1428` CONJ leaves offsets pristine but adds per-frame conjugation; POSEAWARE overrides
  after the rebind `:577-583`; RESKIN is vert-side). The torso rebind
  (`RebindOutfitBonesToOwnSkeleton` `:1102+`, `calcOffset=false`, authored offsets) ships the
  same composition for torso meshes. **Confirmed on all counts.**
- Expectation-setting: for MALE hands_naked the new cell is near-equivalent to SHELL_FIX
  *rotationally* (authored ≈ inv(B) at 0.1°) — the male Tier-1 should land ≈ arm S's 3.1° and
  the male E1 morphology should resemble `armS_burst_*`. The NEW information this fix adds over
  arm S is female/gloves/nails correctness + the translation component. Do not let a male-only
  reading get sold as the win; the gender/asset split IS the point.

### A4 (HIGH, Lane F) — "keep the authored offsets" is an assumption with two named mutation windows; verify provenance at repoint time, and pin the pass-A semantics for appendages

The VERDICT asserts the repoint runs before anything rebakes and that re-stuffed meshes re-enter
pristine. Two writers can violate that:

1. **Engine SKEL_REBAKE pre-pass** (`Rnd_Wgpu_RB3.cpp:3474-3546`, default-ON, opt-out
   `RB3_NO_SKEL_REBAKE`): fires per-draw on any UNREBOUND mesh (`!mNativeBonesRebound`) with
   ≥8 bones whose worst mesh-local skin >12u and whose worst bone's dir passes
   `IsBandMemberSkeletonFile` — and its dynamic-bone exclusion
   (`rb3_render_hook.cpp:137-147`) excludes finger/thumb/index/middle/pinky/ring names but
   **NOT `bone_R-hand`/`bone_L-hand`, wrist, or forearm slots**. `SetBone(b, bt, true)` there
   recomputes the offset in place — destroying the authored matrix for exactly the wrist-anchor
   slots the new cell depends on. Empirically arm W ran a whole protocol with hands never
   rebound and Tier-1 xcheck stayed 0.1° (so it did NOT fire there — likely the worst-bone dir
   gate), but the fix lane must **verify, not inherit**: log/assert offset pristineness at
   repoint time (pre-repoint Tier-1 xcheck vs `bound` ≈ 0°, or compare against the R1
   `[RESKIN_OFF]` capture-1 values), and if any mutation is observed, suppress SKEL_REBAKE for
   `apdMesh` under the flag (a one-line hook/policy guard).
2. **Pass-A miss semantics under the flag** — the kickoff says "resolve own as today" but under
   repoint-only two pass-A behaviors become choices, not inheritances: (i) apd bones resolving
   `own == bound` currently miss (`sNoBoundRebake=1` default, `:1636-1643`) → mesh stays pending
   forever → clamp/guard owns it. KEEP that (repointing to a static bound would freeze — arm W's
   P-frozen). (ii) the `clipPlaying` miss (`:1667-1704`) exists to protect a REST CAPTURE that
   the new cell no longer performs — dropping it for apdMesh would close the count-in pending
   window that motivated `RB3_HANDS_BIND_FIX` (S1a's guard-DROP grazes). Recommend first cut =
   keep pass A byte-unchanged (accept the pending window; it is today's behavior), and note the
   guard-drop census may therefore show the same count-in transients as the default — do not
   fail the gate on them; a follow-up may relax the clip guard for apd only if E1 shows count-in
   artifacts. Either way the lane's PLAN must state the choice EXPLICITLY before measuring.

Also fix in passing (same commit, comment-only): the W2.8e comment block
(`BandCharacter.cpp:1558-1572`) labels `bound` = per-member (129°) and `own` = shared magnet
(106°) — **inverted** relative to the adjudicated, measured model (arm-W freeze proof: `bound`
static + shared ptr; `own` per-member animating). That is the S.S1 label inversion still latent
in-tree; the implementing agent WILL read it while editing this exact region.

### A5 (MEDIUM, Lane F) — §5 gates verified in-tree; two instrument caveats to carry into the gate table

- The gates exist as claimed: Tier-1 `count(>5°)` + xcheck (`Rnd_Wgpu_RB3.cpp:4813-4825`),
  Tier-2 joint-attach + exactJoint (`:4826-4848`), Instrument-B (`RB3_HANDS_INSTR_B`,
  `:4872+`), guard-DROP (`:4306` region), drawlog 792, W2.1 crowd oracle (standing). The probe's
  default scope covers `hands_naked`/`finger`/`glove` — and fingernail meshes via the `finger`
  substring (`:4766-4768`); gloves/nails ARE measurable as the kickoff requires.
- **"Per-gender" is an analysis-side split, not an instrument property**: the probe emits
  per-mesh lines with `nb` (38=male / 40=female) and owner; the lane must split by `nb` (and
  member) exactly as `arm_summaries.txt` did. No engine probe change needed — good, since Lane F
  has zero engine grant.
- **Tier-1 has no PERTURB fail-red**: `RB3_HANDS_ATTACH_PERTURB` perturbs a local palette copy
  consumed ONLY by Tier-2's `haMat` (`:4779-4789,4841-4843`); Tier-1 reads `BoneOffsetAt`/
  `haRest` directly. Tier-1's fail-red control is the **flag-OFF arm itself** (arm C reproduces
  the 87.3/42.6 RED modes). State this in the gate table so a green Tier-1 is never credited to
  a perturb check it never had.
- **Tier-1 recapture caveat**: `haRest` is snapshotted once per bone-POINTER (`:4816`) at
  whatever pose `own` holds at the first post-repoint draw — a mid-clip recapture can inflate
  Tier-1 with no defect present. Arm S shows captures land ≈B in practice, but the lane should
  treat an isolated Tier-1 excursion with Tier-2-exact ≈0 AND clean E1 as a recapture artifact
  to re-measure, per the VERDICT's own scalar-vs-matrix lesson (§6.3) — not as instant failure.

### A6 (HIGH, Lane F / answers R-B) — the female prediction IS derivable from arm W + arm S, but the §5 gate wording can pass a broken female; pin it

What the committed evidence says: arm-S female Tier-1 = **28.9°, count 40/40** where
`off = inv(shared B)` and `restW` = female `own`'s play-time pose ⇒ **female `own` sits 28.9°
from B during play**. Arm-W female = her AUTHORED offsets are 28.9° from B (34/40 bones). The
two 28.9° readings are angle-equal; if they are the SAME rotation (axis match), then under the
new cell `authored_f · own_f_rest ≈ I` and the female reads ≈ the male's 3.1°. **Prediction:
female PASSES at count(>5°)==0, same gate as male.** But angle equality does not prove axis
equality — the exact scalar-vs-matrix slip this saga documented — and
`offset_basis_derivation.py` derives axes for MALE bones only (its inputs are the player0 male
`[RESKIN_OFF]` rows). Therefore:

- **Pre-register female PASS = Tier-1 count(>5°)==0 on nb=40** (not "≤ authored-gap"). The §5
  wording "≤ authored-gap on female … predicted small" is exploitable: a female reading of
  ~28.9° is numerically "≤ the authored gap" yet is precisely the FAILURE signature (own
  animating around B, not around her authored basis Bf).
- If female reads a stable ~29-58° mode while male is green: BEFORE invoking the Dolphin
  fallback, extend `offset_basis_derivation.py` with the female `[RESKIN_OFF]` capture-1 rows +
  her APD/probe rest matrices and compare AXES offline — free, zero-runtime, and it
  discriminates "axis mismatch (fix insufficient for female, need per-gender handling)" from
  "own mis-animation (Dolphin capture per §5 residual risk)".
- Gloves/nails: same structure — their authored offsets are their own asset binds (60-69°/~170°
  off B), and nothing measured yet shows their `own` bones rest at those binds; treat them as
  measure-first populations with the same count(>5°)==0 target but E1-weighted judgment
  (they are also the smallest pixel populations).

### A7 (LOW, cross-lane / answers R-D) — matrix effectively empty; one shared-tree discipline note; "ten defaults ON" verified

- Lane F: rb3 `src/system/bandobj/BandCharacter.cpp` only; instruments all pre-exist in engine
  (zero engine edits). Lane T: engine `RB3MaterialBinder.cpp` / `standard_wgsl.inc` /
  (per A1) rb3 `src/system/ui/*` — disjoint from `bandobj` on writes. **No write-write overlap.**
- Residual coupling: both lanes build the SAME engine working tree, which already carries a
  concurrent agent's uncommitted `M src/platform/FxSendNative.cpp` (both Wave-15 STATUS docs
  flagged it — leave untouched). Lane T's engine edits must be flag-gated default-OFF (the
  kickoff already requires this), which makes Lane F's engine rebuilds behaviorally identical
  flag-OFF; Lane F should record `git -C ../milo-native-engine rev-parse HEAD` + `status` in its
  evidence with each measurement run so any mid-wave drift is attributable.
- **Ten defaults verified**: Wave-15's nine (A8 there) + `RB3_PLAYER_NAME_FALLBACK` flipped to
  opt-out (`native/src/rb3_platform_native.cpp:119-122`, opt-out `RB3_PLAYER_NAME_FALLBACK_OFF`;
  classjson `:1449/:1456` "default-ON as of the Wave-15 coordinator E1 flip") = ten. The
  RB3PostProc.h stale comment is fixed (`:81-82` now reads default-ON).
- Line-anchor nits (doc-only): kickoff/VERDICT cite `UILabel.cpp:269` as "UILabel::Draw" — it is
  `DrawShowing` (`:253`); VERDICT §1 cites the `sBoneMergeDir` remap at
  `BandCharacter.cpp:4159-4181` — the live region is `:4106-4194` (drift, same code). Neither is
  load-bearing.

---

## Lane assessments

**Lane F (HANDS-FIX) — DISPATCH, with A4's provenance verification and A6's pinned female gate
as binding.** The crux holds under re-derivation: the cell is distinct (only unbaked-offsets +
own-binding combination), never measured, and the arm-W/arm-S evidence supports both the male
prediction (≈3.1°, arm S demonstrates achievability) and — with an axis caveat — the female one.
The implementation is genuinely small (skip one `Multiply` write for `apdMesh` in pass B behind
the flag), but pass-A semantics and the two offset-mutation windows are where a naive
implementation silently diverges from the adjudicated cell; the lane's PLAN must state those
choices before measuring. Fallback (Dolphin + milo-trace single-bone) stays pre-registered but
A6's offline axis check comes first for a female-only miss.

**Lane T (RndText color) — DISPATCH RE-FRAMED (A1/A2).** Right target (it blocks the ROWFIX
flip), wrong mechanism statement imported from the record. Rename in dispatch: "focused-list-row
text color drop" (not "make the glyph shader honor font-material color" — it already does).
Probe order: binder-side `RB3_UI_FLOOR_DBG` + Text-side font/mat attribution FIRST; the
alt-font override gap (`UILabel.cpp:279-292`) is the prime suspect; engine/WGSL work only if
the drop point is proven engine-side. Grant must include rb3 `src/system/ui/`. The
no-regression net stands; with A2's structural argument, a scoped fix does not need the
"!= white" special-casing R-C floats. THEN the ROWFIX Part-B re-run + READY_FOR_FLIP package as
the kickoff specifies — that part is well-formed (directional two-region gate + SETLISTS
red-band + hub ≥2.0 are the correct carried gates from W4.4/A4-A7 of Wave 15).

---

## Answers to the kickoff's open questions

**R-A (what does the cell concretely change):** answered in A3. Vs today's DEFAULT: pass B keeps
the repoint (`SetBone(b, own, false)`, `:1756-1757`) and skips ONLY the offset overwrite
(`:1775`) for apdMesh — so the composed palette becomes `authored_invBind · own_live(t)` instead
of `(meshWorld·inv(transient R)) · own_live(t)`. Vs `RB3_HANDS_SHELL_FIX`: SHELL_FIX still bakes
(shared-B-derived anchor + fresh translation onto every appendage mesh, `:1531-1556`); the cell
keeps each mesh's own authored matrix — identical binding, different offsets (28.9°/60-69°/~170°
rotation for female/gloves/nails, translation for male). Vs the 7 artifacts: every one either
bakes an offset, freezes the bone, or adds a per-frame transform; none is (authored, own).
Distinct and never measured — the wave's premise stands.

**R-B (female):** answered in A6. Arm S's female 28.9° (against the shared-B bake) equals her
authored-vs-B gap from arm W — evidence her `own` rests AT her authored basis during play, which
PREDICTS female count(>5°)==0 under the new cell, subject to an axis check the committed
derivation never did for female bones. Gate: same count(>5°)==0 as male; a stable ~29° female
reading = failure signature → offline axis extension of `offset_basis_derivation.py` BEFORE the
Dolphin capture.

**R-C (global text-tint risk):** answered in A2. There is no new multiply — material color
already reaches every text draw (`standard_wgsl.inc:764`; hub dark-focus proof). Every UILabel
re-sets its font mat's color each DrawShowing, so no font "relies on white". The safe fix is
routing the already-set color to the correct material/label; blast radius enumerable from the
diagnosis. Keep the screen sweep; skip the "!= white" scoping.

**R-D (collision matrix):** answered in A7. Empty on writes (bandobj vs ui/engine-text); the one
real coupling is the shared engine working tree + the concurrent `FxSendNative.cpp` edit —
flag-gating (already mandated) + engine-state logging in Lane F's evidence closes it.

---

## Source appendix (verified anchors)

- Rebind pass A/B: `src/system/bandobj/BandCharacter.cpp:1512-1749` (pass A: `own` resolve
  `:1516`, own==bound miss `:1636-1643`, clipPlaying miss + BIND_FIX `:1667-1704`, distinct
  capture `:1706-1714`), pass B `:1750-1818` (`SetBone(b,owns[b],false)` `:1756-1757`, offset
  overwrite `:1775`, flag `:1802`), latch `:1819-1846`; seed `:974-1037`; Poll ordering
  `:520-530` (rebind pre-`Character::Poll`); SHELL_FIX `:1531-1556`; ASSET_REBAKE `:1573-1603`;
  REST_ROT `:1613-1620,1765-1772`; CONJ exclusion `:1406-1428`; stale inverted comment
  `:1558-1572`; `RB3_NO_HEAD_REBIND` `:976,1256`; torso rebind `:1102-1224`.
- Engine hands instruments: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4736-4875`
  (Tier-1 `:4813-4825`, PERTURB Tier-2-only `:4779-4789,4841-4843`, Tier-2 `:4826-4848`,
  Instrument-B `:4872+`, probe scope `:4766-4768`); SKEL_REBAKE `:3442-3556` (gate `:3474`,
  `SetBone(b,bt,true)` `:3545`); hook policy `rb3/native/src/rb3_render_hook.cpp:109-147`
  (dynamic-bone list excludes fingers, not hand/wrist `:137-147`).
- Text path: `src/system/rndobj/Text.cpp:1165,1402` (glyph submesh materials = per-font
  `GetMat()` pointer), `:1237` (vertex color = style.color), `:394-411,554-566` (alt style);
  `src/system/ui/UILabel.cpp:253-292` (`:266-270` override → MAIN fontMat only; `:279-292` alt
  mat never sees override); `src/system/ui/UIListLabel.cpp:65-90` (`SetColorOverride` `:83`);
  `src/system/ui/UIListSlot.cpp:101-125` (rowfix darken `(0.06,0.05,0.02)` `:115`);
  `src/system/ui/UIListHighlight.cpp:51-77` (Part A + `RB3RowfixSetFillDrawn`).
- Engine text color: `milo-native-engine/src/platform/RB3MaterialBinder.cpp:105`
  (isLikelyUiText), `:132-134` (mu.color = mat color), `:166-232` (W4.2 relaxed floor,
  default-ON `:208-217`, `kTrueInvisibleUiText=0.06` `:219`), `:280` (text force-prelit),
  `:336` (useAlphaAsRGB); `milo-native-engine/src/gfx/standard_wgsl.inc:722,759-771`
  (`material.color.rgb * vertexTint` `:764`; text branch `:768-770`), `:850-855` (prelit =
  register color).
- Evidence: `HANDS-ADJUDICATION/evidence/arm_summaries.txt` (arm C protocol-valid; arm W male
  0.1°/8-slot mixed anchors/female 28.9°, freeze proof; arm S male 3.1° 0/1038, female 28.9°
  40/40); `evidence/offset_basis_derivation.py` (male-only axes; 87.2° closure).
- Defaults/flip: `rb3/native/src/rb3_platform_native.cpp:119-122` (opt-out
  `RB3_PLAYER_NAME_FALLBACK_OFF`); engine classjson `:1449,1456`; `RB3PostProc.h:81-82`
  (comment now default-ON). Engine HEAD = pin `84ccb9e`; working tree carries one concurrent
  `M src/platform/FxSendNative.cpp` (untouched).
