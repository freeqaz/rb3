# Wave 15 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent (2026-07-07). **Input:** `WAVE15_KICKOFF.md` (draft), README Waves
12–14 results + Wave-15 menu, `WAVE14_KICKOFF.md` (acceptance) / `WAVE14_REVIEW.md`, hands-saga
STATUS docs (`RESKIN/`, `SKEL/`, `W2.8g/`, `W2.8f/`, `W2.8e/` + their committed `evidence/`),
Lane-B priors (`W4.3-C1/STATUS.md`, `UIGRADE/STATUS.md`), plus direct verification:
rb3 `src/system/bandobj/BandCharacter.cpp` (rebake `:1750-1758`, reskin block `:3130-3189`,
standing finding `:1273-1276`), `src/system/os/User.cpp`, `src/system/os/PlatformMgr_Wii.cpp`,
`src/band3/meta_band/AppLabel.cpp`, `src/band3/meta_band/OvershellSlot.cpp`,
`src/band3/meta_band/SongSelectPanel.cpp`; engine @ pin `fdf0ad9`:
`src/platform/Rnd_Wgpu_RB3.cpp:3299-3305`, `src/platform/RB3PostProc.{h,cpp}`,
`NativeCompatFlags.classification.json`; **pixel-level re-measurement** of the committed
captures (`/tmp/uigrade/default_songselect.png`, `/tmp/uigrade-uclean2/on_songselect.png`,
ROI crop + `_uigrade_gate.py` re-run) against `images/retail-screenshots/
yt_qRagnZCIMzk_song_select_list.png` + `yt_qSRJ8HHPXzM_song_select_wii.png`; raw-evidence
existence checks (`W2.8f/evidence/readings.txt`, `/tmp/wave13-skel-s1/gameplay.log`).

## VERDICT: **dispatch-with-amendments**

The three-lane shape is right, and for once the headline lane (H) is a synthesis, not a fix —
which is the correct response to seven measured dead artifacts and three premise inversions.
This wave's false premise is in **Lane B**: the kickoff imports the HUB's mechanism ("bright
bar compositing through AA glyph alpha") onto song_select, but pixel-level re-measurement of
the committed captures shows the native highlighted row has **no bright bar at all** — the row
fill is dark navy (ROI p60 luma = 15/255) with white text inside a yellow-trim box, while
retail is black text on a WHITE fill. The defect is a missing/occluded highlight FILL plus an
un-switched focus text color — a different family than bar-bleed, and one for which the record
already holds the strongest lead (U-CLEAN's z-occluded SETLISTS "selection quad", whose
occlusion the now-default `RB3_UI_POST_GRADE` depth `LoadOp::Load` deliberately PRESERVES).
The proposed contrast-ratio gate is polarity-blind and mis-anchored: the pre-registered
songselect "parity band [1.06,1.17]" is a dark-field/dark-field measurement (I reproduced
1.14 from the committed PNG: p5=13.2, p60=15.0 — both on the navy fill; the white TEXT is at
p95=190), so a REAL fix would violate the standing band while a polarity-inverted non-fix
could pass a pure ratio gate. Lane H's "one uncontested equation" verifies against source
with two footnotes and one genuinely contested background assumption (the rebake itself is a
native workaround, not ground truth — the derivation must start from the Wii composition,
which the flag set already exposes). Lane N is decidable from source in one step: the Wii
build itself falls back to localized "Player N" when no profile is signed in
(`PlatformMgr_Wii.cpp:489-496`) — so "PLAYER 1" is not a "sensible native default", it is the
FAITHFUL behavior, and hiding the label is not acceptable.

---

## Amendments

### A1 (HIGH, Lane H) — the equation verifies, but re-anchor the derivation at the WII composition; the rebake is a candidate defect-mask, not background truth

Verified against source, the kickoff's equation is real: the rebake writes
`mOffset = meshWorld · inverse(rest_own)` and binds to the live bone
(`BandCharacter.cpp:1750-1758`), the engine palette composes
`skin[b] = BoneOffsetAt(b) · own->WorldXfm()` in row-vector convention
(`Rnd_Wgpu_RB3.cpp:3299-3305`), so `skinPos(t) = v' · meshWorld · inv(own_rest_b) ·
own_live_b(t)` — and the reskin cannot change the `inv(own_rest)·own_live(t)` factor. Two
footnotes the adjudicator must carry (both have produced premise inversions before):

- **It is a per-vertex 4-bone weighted blend**, not a single-bone product:
  `skinPos(t) = v' · Σ_i w_i (meshWorld·inv(own_rest_bi)·own_live_bi(t))` — the single-bone
  form is what made Seam B look viable before the mixed-sign wall (`SKEL/STATUS.md`).
- **Spaces are mixed by design:** `rest_own` is CHAR-space (placement divided out,
  `NativeCharSpaceRestXfm`, `BandCharacter.cpp:933`), `own->WorldXfm()` is WORLD-space.
  W2.8c died on frame-mixing and A.S1 on a placement confound; any derivation must track this.

The genuinely contested part is what the kickoff treats as background: **the rebake itself is
an HX_NATIVE workaround** (`RebindHeadHandsAtRest` is native-only; Wii never rebakes). The
Wii composition is `skinPos_wii(t) = v · A_b · live_b(t)` with `A_b` the AUTHORED inverse
bind — and that composition is **already exposed natively behind `RB3_NO_HEAD_REBIND`**
(`BandCharacter.cpp:1256`). R2 measured the reskin-ON variant of that arm at mean 136u (much
worse), and the rebake exists precisely because the un-rebaked native hands smear. That is
the decisive syllogism the adjudication should START from:

> On Wii, authored verts + authored offsets + live bones render coherent hands. Natively, the
> SAME authored verts + authored offsets + native `own_live(t)` shard (`RB3_NO_HEAD_REBIND`
> arm). The authored data is byte-identical (same milos). Therefore either (a) native
> `own_live(t)` bone worlds differ from Wii's live bone worlds for these bones, or (b) the
> pairing `A_b ↔ own_b` is wrong natively (offsets applied to differently-based bones than
> the ones they were authored against — the bound/own split). Every bake in the record is an
> attempt to patch (a)/(b) downstream; all seven failed because the divergence is upstream.

Amend the Lane H brief: deliverable (a)'s derivation starts from `skin_wii = v·A_b·live_b`,
requires one cheap pre-registered measurement — the **clean** Wii-composition baseline
(`RB3_NO_HEAD_REBIND=1`, reskin OFF, wext + Instrument-B invariants; the R2 table only has
the reskin-ON contamination of this arm) — and must answer (a)-vs-(b) explicitly before
proposing any change. Also carry the dependency that the rebake applies per-mesh
all-or-nothing (`miss==0` pass-B gate, `:1750`) with a settle/clip-free capture guard
(`:1667-1705`) — per-bone application state is a VARIABLE (the Tier-1 42.6°↔87.3°
bimodality flips across freshness recaptures, `W2.8f/STATUS.md`), not a constant.

### A2 (HIGH, Lane H) — YES, require reproduction of the two load-bearing numbers; and preserve the APD_DIAG log out of /tmp BEFORE dispatch

The kickoff asks whether the adjudicator must reproduce the 87.3° Tier-1 angle and the
mixed-sign ~35° per-bone gaps from raw logs. **Yes — and it is cheap, so there is no excuse:**

- **87.3° is reproducible from COMMITTED evidence**: `W2.8f/evidence/readings.txt` line 1
  (`hands_naked.mesh 38 … min=42.60 med=87.30 max=87.30 n=2214`). I checked; it's there. But
  the adjudicator must also import W2.8f's OWN caveat: Tier-1 is **bimodal (42.6/87.3)**,
  flips across freshness-recapture events, and was explicitly demoted to "directional signal,
  NOT a trustworthy gate". Any derivation that uses 87.3° as a constant is already wrong.
- **The mixed-sign gaps live ONLY in volatile /tmp**: `/tmp/wave13-skel-s1/gameplay.log`
  (205 `[APD_DIAG]` lines) exists TODAY — I verified content (e.g. `bone_R-middlefinger03`
  bakedRest 106.0 / boundNow 129.9) — but `SKEL/evidence/` holds only the probe script
  (`s1_mechanism_probe.py`), not the log. **Coordinator action before dispatch: `grep
  APD_DIAG /tmp/wave13-skel-s1/gameplay.log > SKEL/evidence/apd_diag.log` and commit it**
  (or the lane's first act is a re-run). The RESKIN wext A/B raw logs are likewise
  uncommitted (regeneration command documented in `RESKIN/STATUS.md`; E1 PNGs committed).
- Rationale beyond hygiene: this saga has had **three label/provenance inversions** (W2.8e
  stale-bone confound; SKEL S.S1 "own/bound labelled backwards in the pre-rebind sample";
  R1's rest-shape thesis refuted by R2's own gate). A synthesis built on the STATUS docs'
  prose without touching the raw numbers would inherit whichever inversion is still latent.
  The two numbers above are the only ones the verdict will lean on that were produced by
  since-corrected instruments — reproduce those two, take the rest from STATUS.

### A3 (CRITICAL, Lane B) — the mechanism prior is wrong: there is NO bright bar on the native row; the highlight FILL is missing/dark, and the record already names the prime suspect

Retail truth: **confirmed, both row types.** `yt_qRagnZCIMzk_song_select_list.png` (focused
heading "N": black text, white fill, yellow left tab) and `yt_qSRJ8HHPXzM_song_select_wii.png`
(focused song row "Before I Forget": black text, white fill, yellow trim). Unfocused rows are
white-text-on-dark in both retail and native — the defect is confined to the focused row.

Native truth: **the kickoff's "white text" is right but its "white/yellow bar" mental model is
not.** Cropping the gate's own ROI (12,322)–(892,353) from the committed captures (both the
pre-flip `default_songselect.png` and the current-default `on_songselect.png`): the focused
row is white text on a **DARK NAVY fill** inside a yellow-trim box. Row-luma scan: the fill
field is ~13–15/255; only the yellow tab/border and the glyphs are bright. So:

- The hub mechanism (C1: bright bar alpha-3.56 compositing through AA text + grade lift)
  **does not transfer** — there is no bright bar here to bleed. Do not dispatch the lane to
  re-run C1's bar-dimming isolation ladder.
- The two coupled questions are: **(1) where is the white fill?** A fill quad exists in this
  scene family: U-CLEAN found a **z-occluded SETLISTS-row "selection quad"** revealed by the
  old depth-clear as the red band (`UIGRADE/STATUS.md` U-CLEAN §0), and the shipped fix
  (`RB3PostProc.cpp:87` region, menuBoundary depth `LoadOp::Load`) **deliberately keeps such
  quads occluded**. First probe = draw-log the focused-row rect: is a fill mesh submitted at
  all, with what color/alpha/blend/z, and does it survive the depth test? **(2) why is the
  text white?** — trace the focus-state color route on THESE labels (C1's probe method, not
  its hub conclusion): the song list rows are not the hub's `UILabelDir` five-button family,
  so `GetStateColor` plumbing may genuinely differ (the C1 finding "focus color IS applied"
  was proven ONLY for the hub, see A5).
- Sequencing: (1) before (2). If the white fill drew, black-vs-white text might already be
  governed by a fill-aware authored state; a text-color fix tuned against the dark fill would
  be re-tuned anyway.

### A4 (HIGH, Lane B) — the proposed gate is polarity-blind and collides with the standing UIGRADE band; pre-register a polarity-aware gate + a band re-registration

I re-ran `_uigrade_gate.py` on the committed captures: songselect default = **1.14** with
p5=13.2 / p60=15.0 / p95=190.8. Read what that means: **both percentiles the "contrast ratio"
uses land on the dark fill; the white text is the p95 tail.** Consequences, all three of which
the kickoff's "A11-style percentile contrast on the highlighted row (retail-calibrated)" gate
gets wrong as written:

1. **A real fix explodes the metric upward** (white fill p60→~220, dark text p5→~20 ⇒ ratio
   ~10, retail-style) — which VIOLATES the standing UIGRADE songselect parity band
   [1.06,1.17] that Wave-13/14 pre-registered and U-CLEAN passed at 1.125. If Lane B lands,
   the UIGRADE gate must be **re-registered for the new polarity in the same review cycle**,
   or every subsequent drawlog/UIGRADE sweep reads the fix as a regression.
2. **A pure ratio can pass with the WRONG polarity**: the current broken row already has
   span p95/p5 = 14.4 ("legible white-on-dark"). The gate must be directional:
   `p60(fill field) ≥ bright-threshold AND p5(text stroke) ≤ dark-threshold`, thresholds
   calibrated on the retail crops (both retail refs above), ROI split into fill-field vs
   glyph-stroke sub-regions rather than one rect.
3. **Fail-red**: perturb one arm (e.g. run with the fix flag OFF) and show the polarity gate
   reads RED on today's default — the current ROI+formula cannot do that (it reads an
   in-band "PASS" 1.14 on the broken screen).

Keep: hub no-regression (≥2.0 with `RB3_UI_POST_GRADE` ON), drawlog 792 flag-OFF, partdiff
band. Add: the U-CLEAN SETLISTS red-band gate (redDom + %red on that ROI, both arms) — see A7.

### A5 (MEDIUM, Lane B) — import the C1/UIGRADE priors PRECISELY: the song_select row was never probed, and "same mechanism expected" is C1's conjecture, now doubted

From `W4.3-C1/STATUS.md` verbatim: the path-tracing, the NOBAR/dim isolation ladder, and the
"focus color applied + reaches shader" findings were all established **on the hub five-button
family only**; the final section says song-select highlighted row + partdiff GUITAR were
"**Not re-verified this pass**… same render/postproc wash mechanism is **expected** to apply;
deferred". `UIGRADE/STATUS.md` adds only that both screens are grade-INERT (songselect
PP_OFF 1.11 vs default 1.14; partdiff 1.42 vs 1.41) — a ratio measurement, not a color-route
probe. So the kickoff's R-B instinct is right but the import must be surgical: **inherit
C1's probe METHOD (`RB3_UILABEL_DBG`-style state/color logging + binder final-color logging),
not its hub CONCLUSIONS** — and note A3's pixel evidence actively contradicts the "same
mechanism" conjecture (no bright bar exists on the song_select row; C1's escalation Option B
does not describe this screen). partdiff GUITAR needs its own 60-second ROI/polarity
characterization before inheriting anything — its 1.41 sits on a *different* visual layout
(yellow EASY bar + dark text per the ROI comment) and may genuinely BE a bar-bleed sibling.

### A6 (MEDIUM, Lane N) — the provider is found; the fix-or-hide question is answered by the Wii source itself; sharpen the brief to a one-file change + E1

Traced end-to-end (agent-verified, then spot-checked):

- Label consumers: overshell plate `user_name.lbl` (`OvershellSlot.cpp:96`, set via
  `update_user_name` message `:1343-1345`) and the DTA-driven header route
  (`AppLabel::OnSetUserName`, `AppLabel.cpp:356-369`, handler `set_user_name` `:759` — the
  song_select header's stats.grp gamertag element, cf. `SongSelectPanel.cpp:106`).
- All routes converge on `AppLabel::SetUserName` → `User::UserName()` / direct
  `ThePlatformMgr.GetName(i)` (`AppLabel.cpp:159,161`; `LocalUser::UserName`,
  `User.cpp:107`).
- **Native null source:** `PlatformMgr::GetName(int)` is a NULL-returning weak stub
  (`native/src/dta_link_stubs.s`, `_ZNK11PlatformMgr7GetNameEi`); the real impl is Wii-only.
  `SetDisplayText(NULL, true)` then formats the glibc `"(null)"`.
- **The decision evidence the kickoff asked for:** `PlatformMgr_Wii.cpp:489-496` — when not
  signed in (or no profile name), Wii returns `MakeString("%s %d", Localize(player, 0),
  pad + 1)` = localized **"Player N"**. Retail screenshots show the gamertag in BOTH the
  header and the overshell plate when a profile exists. Native has no profile subsystem ⇒ the
  not-signed-in branch is the faithful state. **Therefore: implement, don't hide.** A strong
  `PlatformMgr::GetName` in `native/src/rb3_platform_native.cpp` mirroring the Wii fallback
  (verify `Localize(player, 0)` resolves natively — menu localization already works — and
  keep `MakeString` semantics/lifetime) fixes header + overshell + every other consumer
  (`SessionUsersProviders.cpp:103`, `Track::SetUserNameLabel`) in one place. Per-pad
  numbering (`pad + 1`) falls out for free. Hiding the label would be UNfaithful and touches
  N label sites instead of one provider. E1 = song_select header + overshell plate captures.

### A7 (MEDIUM, cross-lane) — collision matrix is NOT empty: Lane H probes and Lane B probes/fix gravitate to the same engine TUs, and Lane B's fix likely interacts with the shipped `RB3_UI_POST_GRADE` depth semantics

- **Lane H:** every instrument it needs already exists in-tree, env-gated
  (`RB3_HANDS_ATTACH_PROBE`, `IK_SHARD_VERT`, `RB3_APD_DIAG`, `RB3_HANDS_INSTR_B`,
  `RB3_RESKIN_PROBE`, `RB3_NO_HEAD_REBIND`). Constrain the lane's "probes allowed" to
  **existing probes + rb3-side additions only — ZERO new engine TU edits** unless it first
  claims single-writer. That empties H's side of the matrix.
- **Lane B:** the diagnosis needs draw-log probes (engine `Rnd_Wgpu_RB3.cpp` /
  `RB3MaterialBinder.cpp`), and per A3 the FIX plausibly lives in the depth/draw path of the
  menu flush re-open (`RB3PostProc.cpp:87` region) — i.e. the kickoff's "fix flag-first
  game-side" is probably wrong-sided. Pre-authorize the declared-range grant now
  (`RB3PostProc.cpp` flush re-open + `RB3MaterialBinder.cpp` + read-only draw-log region of
  `Rnd_Wgpu_RB3.cpp`), single-writer to Lane B, or the lane stalls mid-wave in escalation
  exactly like C1 did in Wave 12.
- **The specific interaction (answers R-C):** U-CLEAN's `LoadOp::Load` exists to keep a
  z-occluded selection quad from surfacing as the red band. If Lane B's fix makes selection
  fills draw (un-occlude, re-order, or re-material them), the red band can come back on
  SETLISTS. **Mandatory Lane B gate: re-run the U-CLEAN SETLISTS red-band check (redDom /
  %red) both arms**, plus hub 2.204-class no-regression. Conversely the menu flush itself is
  measured near-inert on these rows (1.143 OFF vs 1.125 ON) — grade interaction is NOT the
  worry; depth interaction is.
- Lane N is disjoint (native/src + E1) — no collision. Lanes B and N both capture
  song_select E1s; have each note the other's flag state in captures.

### A8 (LOW) — "nine defaults ON" tally verified; one stale comment

Wave-14's A9 verified seven anchor-by-anchor; the two Wave-14 flips check out at source:
`RB3_UI_POST_GRADE` default-ON in the accessor (`RB3PostProc.cpp:254-261`, opt-out
`RB3_UI_POST_GRADE_OFF`) + classjson "live … default-ON as of the Wave-14 coordinator E1
flip"; `RB3_SS_ART_YFIX` default-ON via opt-out pattern (`SongSelectPanel.cpp:153-154`).
Nine is right. Nit: `RB3PostProc.h:81` still says "RB3_UI_POST_GRADE (default-OFF)" — stale;
fix in passing on the next granted engine edit (doc-only, no lane needed).

### A9 (LOW, Lane H) — disposition (c) must pin the mitigation's flag polarity and keep the gates alive

If the verdict is "option set closed", the retirement note must state the shipped mitigation
precisely — `RB3_NO_SKIN_CLAMP` is a double-negative (unset = clamp ON) — and must carry the
still-armed gates (wext ≤60u-without-freeze, Instrument-B rest-free invariants, Tier-2 ≤1u,
guard-DROP census) into the backlog item so a future asset-pipeline fix inherits them instead
of re-deriving. The A.S3 finding stands: any future "hands correct" default-flip is BLOCKED
while `hands_naked` draws at ~106u.

---

## Lane assessments

**Lane H (hands adjudication) — RIGHT LANE, amend the anchor + evidence discipline (A1, A2,
A9).** Synthesis-not-fix is correct after R2. The equation is verified at source but is the
NATIVE-workaround composition; the adjudicator's ground zero is the Wii composition
`v·A_b·live_b`, already exposed by `RB3_NO_HEAD_REBIND`, plus the (a)-bone-worlds vs
(b)-pairing dichotomy. Deliverable ordering (a)/(b)/(c) is sound; (b) is well-posed (see R-A).
Budget one clean pre-registered baseline run (Wii-composition arm) — it is the only
measurement missing from the record, and it discriminates (a) from (b) almost by itself.

**Lane B (bar-bleed polarity) — RIGHT TARGET, WRONG MECHANISM PRIOR + WRONG GATE (A3, A4,
A5, A7).** Retail-truth confirmed both row types; native white-on-navy confirmed at pixel
level from the committed captures. Rename the lane in dispatch (it is not "bar-bleed" on this
screen — "focused-row fill+polarity") so the agent doesn't anchor on C1's hub ladder. Probe
order: fill-quad draw-log first, label color route second. Gate: polarity-aware two-region
(A4) with fail-red on today's default; plus SETLISTS red-band and hub no-regression (A7).
Expect the fix to need the declared engine grant — authorize it at dispatch.

**Lane N (gamertag) — READY, and smaller than the kickoff thinks (A6).** Provider, consumers,
and the faithful behavior are all pinned in source now. This is a one-provider fix
(`rb3_platform_native.cpp` strong `GetName` with the Wii "Player N" fallback), flag-first,
E1 on header + overshell. Sonnet is the right size. The "decide with evidence of what other
screens do" instruction is already discharged: Wii source + retail screenshots say show
"Player N", never hide.

---

## Answers to the kickoff's open questions

**R-A (what CAN establish Wii ground truth for own_live/own_rest/verts):** Ranked:

1. **The decomp source (this repo) — structural truth, free.** The Wii build has no rebake
   and no bound/own split at composition time: the skin uses the authored `RndBone::mOffset`
   against the bone Trans the mesh resolved at load; animation reaches those bones via
   `CharBones`/`CharBonesMeshes` (`src/system/char/CharBones*.{h,cpp}`) writing the SAME
   Trans objects. Source answers: what matrix the Wii path composes, which instance
   `BoneTransAt` points to, and where the native loader/skeleton-merge diverges to produce
   TWO instances. It cannot give numeric bone worlds.
2. **rb3-viewer (`--pose-dump`, `--test-bone`) — asset-level numeric truth, cheap.** Dumps
   `skeleton_unshared.milo` authored rests + mesh `mOffset` directly: establishes what
   `own_rest` and `A_b` SHOULD be without any emulator. (The R1 probe log
   `RESKIN/evidence/reskin-probe-gameplay.log` already has the authored-offset dump.)
3. **Dolphin + milo-trace (`../milo-trace`) — runtime numeric truth, the decisive one, not
   free.** Its charter is exactly this (capture real call records / bone worlds via JIT
   hooks, replay vs decomp). A capture of hands bone `WorldXfm`s + the composed matrices on
   real Wii execution gives `own_live(t)` ground truth no native probe can. This is the
   pre-registered experiment if (b) is the verdict.
4. **Bank-5 DWARF / Ghidra bank8** — types/semantics corroboration only (CharBones layouts,
   `SetBone(calcOffset)` semantics); no runtime values. **dc3-decomp** — shared-engine
   corroboration for source reading (its `CharBonesMeshes` is the same Milo lineage).

So (b) IS well-posed: if the derivation is underdetermined, the missing measurement is
either the cheap native Wii-composition baseline (A1) or the Dolphin bone-world capture (3),
in that order.

**R-B (Lane B priors):** Imported precisely in A3/A5 — grade-inert confirmed (UIGRADE
measured PP_OFF≈default on both screens), C1's color-route findings are hub-only and the
song_select row was explicitly never probed, and the pixel evidence now argues the mechanism
differs from the hub's. The lane starts past C1's dead ends by inheriting its probes, not
its conclusions.

**R-C (interaction with default-ON `RB3_UI_POST_GRADE`):** The grade/flush component is
measured near-inert on these rows (songselect 1.143→1.125 across the flip; partdiff
1.409→1.414) — no interaction worth designing around. The DEPTH component is the real
coupling: the shipped menu-boundary `LoadOp::Load` intentionally preserves the occlusion of
a selection quad in exactly this scene, and Lane B's most likely fix territory is that quad's
visibility. Gate per A7 (SETLISTS red-band + hub ratio, both arms), and re-register the
UIGRADE songselect band after a polarity fix lands (A4.1).

---

## Source appendix (verified anchors)

- Rebake: `src/system/bandobj/BandCharacter.cpp:1750-1758` (`mOffset = meshWorld ·
  inverse(restWorld)`, `SetBone(b, owns[b], false)`); char-space rest `:933`
  (`NativeCharSpaceRestXfm`); settle/clip-free capture guard `:1667-1713`; standing finding
  `:1273-1276`; `RB3_NO_HEAD_REBIND` `:1256`; reskin block + refutation header + equation
  `:3130-3189`.
- Engine palette: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:3299-3305`
  (`skin[i] = BoneOffsetAt(i) · boneTrans->WorldXfm()`, row-vector, world-space out).
- UI post-grade default + flush re-open: `RB3PostProc.cpp:254-261` (default-ON accessor),
  `:282-286` (`RB3FlushMenuUIPostGrade`), depth `LoadOp::Load` region `:87` per
  `UIGRADE/STATUS.md` U-CLEAN §1; stale comment `RB3PostProc.h:81`.
- Lane B measurements (this review): `_uigrade_gate.py` ROI `songselect (12,322,892,353)`;
  re-run on `/tmp/uigrade/*.png` reproduces hub 1.95/2.20, songselect 1.14 (p5=13.2,
  p60=15.0, p95=190.8), partdiff 1.41; ROI crop of `default_songselect.png` and
  `on_songselect.png` = white glyphs on ~15-luma navy fill + yellow trim. Retail:
  `images/retail-screenshots/yt_qRagnZCIMzk_song_select_list.png` (focused heading, black on
  white), `yt_qSRJ8HHPXzM_song_select_wii.png` (focused song row, black on white).
- Lane H raw evidence: `W2.8f/evidence/readings.txt` (Tier-1 min=42.60 med=87.30 max=87.30
  n=2214 — committed); `/tmp/wave13-skel-s1/gameplay.log` (205 APD_DIAG lines — VOLATILE,
  commit a grep before dispatch per A2); `RESKIN/evidence/` (probe log + E1 PNGs; wext A/B
  logs regenerable-only per its STATUS).
- Lane N: `native/src/dta_link_stubs.s` (`_ZNK11PlatformMgr7GetNameEi` weak NULL stub);
  `src/system/os/PlatformMgr_Wii.cpp:489-496` (faithful "Player N" fallback);
  `src/system/os/User.cpp:107`; `src/band3/meta_band/AppLabel.cpp:159,161,356-369,759`;
  `src/band3/meta_band/OvershellSlot.cpp:96,1343-1345`; `src/band3/meta_band/
  SongSelectPanel.cpp:106` (header stats.grp gamertag element).
- Defaults: `SongSelectPanel.cpp:153-154` (`RB3_SS_ART_YFIX_OFF` opt-out); engine classjson
  `RB3_UI_POST_GRADE` / `RB3_UI_POST_GRADE_OFF` entries.
