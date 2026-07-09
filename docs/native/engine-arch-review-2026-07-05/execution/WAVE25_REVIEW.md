# Wave 25 — Fable pre-dispatch review (WAVE25_KICKOFF.md @ 7c791630)

**Reviewer:** Fable. **Verdict: DISPATCH-WITH-AMENDMENTS** (A1–A9 binding).
The kickoff faithfully executes the W24-RECON charters; both lanes are correctly
discriminator-first and closure-aware. Amendments below sharpen the two load-bearing
spots (Q1 prior history, Q2 G3 decision procedure), correct two stale anchors, and add
real code anchors so the lanes don't rediscover. Re-derived from code, report.json, and
the VENUE_RENDER V23/V26/V32/V38 history — not from the kickoff's own claims.

## Q1 — FOREARM H-A/H-B discriminator + the :40 V26/V32 "300u" history

**Discriminator: SOUND but incomplete — there is a known third sub-cause family.**
The root-compare (target parent-chain-to-root vs skeleton root, ~100u split) cleanly
separates H-A (resource-dir frame) from H-B (skeleton basis) and is cheap. But the :40
comment (`CharIKHand.cpp:36-43`) points at real prior context the lane MUST read first:

- **V26** (`docs/sessions/native/VENUE_RENDER.md:1070-1100, 1163-1171`): after the
  MakeRotQuat half-angle fix, the residual was `CharIKHand` *correctly orienting* the
  hand but aiming at an IK TARGET "authored/resolved at the wrong spot" — world
  translation z≈300–340u, **crowd/extras population**. Recommended fix pattern:
  the **V23 analog — `BandWardrobe::SyncTransProxies` slot-name rewire**
  (`src/system/bandobj/BandWardrobe.cpp:326`; V23 section at VENUE_RENDER.md:598,
  matched 86/88 `playerN_*.tp` slots for band members).
- **V32** (VENUE_RENDER.md:1175-1341): proved `CharIKHand::Poll` is never called
  pre-game; the 300u story was never tested in-song. W24-RECON is the FIRST in-song
  measurement — the :40 issue is **known-but-never-addressed**, not partially-addressed.

**300u vs 100u is NOT a contradiction to flag as an error**: V26's 300u was the
crowd/extras hands (z-axis, different population); W24's ~100u is band members vs
instrument-tip targets (`evidence/forearm-iktgt-targets.log`: hand y≈209, ALL targets
y≈85–126 — including the FEET ikhands, ankle y≈173 vs pedal toes y≈108). They are two
populations of the same class. **Third sub-cause (add as H-C):** target/TransProxy
*resolution* — the `bone_target_*` bones live in `<inst>_resource.milo` and reach the
member via proxy/attach wiring; a mis-resolved proxy (V23 class) places them in the
wrong frame without any "parenting" being wrong in the chain dump. The root-compare
discriminator as written actually catches H-C too (roots differ) but the lane would
misattribute the FIX site — H-A's "fix the resource-dir parenting" and H-C's "fix the
proxy slot-name resolution in the wardrobe/venue sync" are different edits. See A1.

Note also: the uniform offset on hands AND feet ikhands suggests ONE root-frame error,
not per-target authoring — consistent with H-A/H-C, mildly against H-B.

## Q2 — FOREARM match-neutral vs HX_NATIVE (G3) — the decision procedure

**Current match state (report.json, re-verified):** unit `main/system/char/CharIKHand`
= **99.165%**; `Poll__10CharIKHandFv` = **96.127%**; `IKElbow` = 98.659%; `PullShoulder`
= 99.107%; `MeasureLengths` = **81.355%**; inline helpers `Multiply(Transform,Vector3)`
= 71.484% (real C body at `src/system/math/Rot.h:10` — native-safe, Wii-side noise) and
`ScaleAddEq(Quat)` = 78.021%. The existing `RB3_NO_IK`/`IK_TGT_DBG` blocks are
`#ifdef HX_NATIVE` → the Wii object is unaffected by them.

**The kickoff's single gate "batch_objdiff==baseline = G3" is ambiguous and wrong for
one of the three outcomes.** Binding decision table (A2):

1. **Native-only fix** (data/flow/parenting in native-src, or an `#ifdef HX_NATIVE`
   block with byte-identical `#else` in shared code): `batch_objdiff` on
   `char/CharIKHand` + every touched src/system unit must equal baseline **exactly**
   (Poll 96.127235, unit 99.16526 — record these numbers in STATUS). Any delta = the
   gate leaked into the Wii build → fix or revert.
2. **Decomp-infidelity fix** (our C++ semantically diverges from Bank-8 asm inside the
   4% mismatch — the CharHair::SimulateInternal precedent, where a *99.6%* function hid
   a mis-scoped brace/CFG bug): fix UN-gated in shared code. Gate = match% **>=**
   baseline (improvement expected, decrease forbidden); `batch_objdiff==baseline` is
   NOT the criterion here and must not be treated as a failure when the % rises.
   Justify with `run_diff_inspect diagnose` showing the mismatch region is CFG/semantic
   (branch/`beq`/compare class), not regalloc noise.
3. **H-B outcome** (skeleton basis wrong pre-IK): the fix is NOT in CharIKHand at all
   (skeleton placement / clip-apply / wardrobe) — case 1 or 2 applies to whichever unit
   is actually touched; CharIKHand stays untouched and trivially at baseline.

The kickoff's STEP-0 `bank_divergence.py` check is right; **add** (A3): run
`run_diff_inspect` mode=diagnose on `Poll__10CharIKHandFv` and audit `MeasureLengths`
(81.4% — it computes `mInv2ab/mAABB/mAAPlusBB`, the very lengths the solver uses; a
semantic bug there stretches arms by construction) for CFG-class mismatch before
concluding "faithful port, gate the fix". Precedents: V21 `Mtx.h` ASM_BLOCK no-op, V26
`MakeRotQuat`, CharHair 99.6%. Bank-5 caution: check `bank_divergence.py` verdict
before trusting Bank-5 DWARF bodies for this fn.

## Q3 — CROWD trigger-gap scope

**It is NOT the deep CharSync-Poll bring-up.** The blocked hack-audit item is
`meta_band/CharCache.cpp` + `meta_band/CharSync.cpp` (character-customize chars.milo
preview) — a different subsystem from the sv3_a world-vignette crowd. Decisively: the
recon census (`evidence/crowd-census-full-fields.txt`) shows **`poll=3` (kCharPolled)**
on all 8 crowd proxies — `Character::Poll` (Character.cpp:246) IS running, so the poll
bring-up is fine. The gap is clip **start/resolution**, narrow and in-scope. Real
anchors for the lane (A4):

- `CharDriver::Enter` — `src/system/char/CharDriver.cpp:157`: the ONE-SHOT autoplay
  `if (mDefaultClip) Play(...)` at :163. If Enter fired with `mDefaultClip==NULL`,
  nothing ever plays.
- `CharDriver::Poll` starved-replay at :408 requires `mDefaultClip && mDefaultPlayStarved`
  (`mDefaultPlayStarved` ctor-defaults 0, loaded from the milo at :607) — it will NOT
  rescue a null default clip.
- `mDefaultClip.Load(bs, false, mClips)` at :604 — resolution against the driver's
  `mClips` ClipCollection: if `streetslomo_clips.milo` isn't merged/wired into the
  collection natively, this **silently nulls**. Prime suspect.
- Existing probes to reuse, no new code needed first: `CHARDRV_PROBE` (CharDriver.cpp:342,
  prints `mFirst`/clipType per driver) — extend it one line to print `mDefaultClip` and
  `mClips` size; `RB3_NO_CLIP` kill-switch at :341.
- Freeze path (secondary): `Character::Poll` early-returns on `mFrozen` (Character.cpp:248);
  `BandCamShot::FreezeChar` (`src/system/bandobj/BandCamShot.cpp:558`) is the only
  external freezer — poll=3 argues against this branch, but log it once.
- Decision tree: `mDefaultClip` null at Enter → resolution/merge gap (fix clip-bank
  wiring or a branch-scoped native trigger); `mDefaultClip` set but Enter never fires →
  vignette-dir Enter flow gap (world_panel / shell flow); explicit-DTA-trigger design
  (no default clip authored) → find the vignette's `{... play}` script/EventTrigger and
  why it doesn't fire natively.

ONLY IF the trace shows the whole vignette-dir Enter/flow machinery is absent natively
(not just this clip) does it become a hand-off: checkpoint the trace, report
`at_limit`-style to the coordinator, do not attempt a broad bring-up in this lane (A5).

## Q4 — CROWD near-black material

**Defer — consequence-first is correct.** The kickoff's ordering (posing FIRST,
material only if it persists) matches the recon's MEDIUM-confidence caveat. Sharpen the
discriminator (A6): after `animating>0`, re-capture `RB3_ISOLATE_MESH=crowd_body`
(engine `Rnd_Wgpu_RB3.cpp:2695`) and compare max pixel value against the recon's 17/255
baseline. Figures form AND max value rises to normal-lit range → close R-D as
consequence. Figures form but still dark (<~40/255) → real second defect; it is an
ENGINE-side env/lighting fix (Rnd_Wgpu_RB3.cpp ~:5595, world.cam — cf. the P4
per-environ venue-lighting work and its grey-fallback-when-unlit path) → engine commit
+ coordinator pin bump per process rules; the lane should scope it as a separate
flag and may defer it to W26 if the walkers are visibly acceptable in E1.

## Q5 — WorldCrowd A/B safety

The gameplay crowd is the disjoint `RndMultiMesh` instancing path (`WorldCrowd::CharData
::mMMesh`, protected oracle `src/system/world/Crowd.cpp:884-1000`) — a clip-trigger fix
scoped to the sv3_a proxy drivers cannot reach it **unless the fix is placed in shared
`CharDriver`/`Character` code un-scoped**. That is the one leak vector: an un-scoped
autoplay in `CharDriver::Enter`/`Poll` would fire on EVERY driver in the game (band
members, gameplay chars, WorldCrowd anim drivers). Guardrail (A7): the fix must be
(a) in native-src / band3 flow, or (b) if in shared char code, gated by flag AND scoped
by driver/dir identity (crowd_/streetslomo match, as the census already does) with a
byte-identical `#else`. And the mandatory WorldCrowd A/B must run **flag-ON vs
baseline** (an OFF-only A/B proves nothing about the fix); flag-OFF is covered by
drawlog-792. With A7, the A/B set is sufficient.

## Q6 — Scope/overlap + closures

Lanes are file-disjoint as charted EXCEPT one plausible collision: if FOREARM's H-A/H-C
verdict lands the fix in `BandWardrobe`/`BandCharacter` (bandobj) while CROWD extends
`CHARDRV_PROBE` in `src/system/char/CharDriver.cpp` — different files, same module;
build-lock rules already cover it. Declare it: FOREARM owns `char/CharIKHand.cpp` +
`bandobj/`; CROWD owns `char/CharDriver.cpp` (probe-only unless A7-scoped) + band3
flow + native-src (A8). Neither reopens hands/binding (both fixes are pose/trigger
side) nor touches the RndMesh loader. Two lanes is right-sized; do not add the R-D
material fix as a third workstream unless Q4's discriminator demands it.

**Stale-anchor corrections (A9):** (1) `BandCharacter.cpp:3829` is NOT a "per-frame
poll loop" — it is a one-shot `MeasureLengths()` re-measure inside the deform-resync
block (gated by the `unk224 & 2` flag). The actual per-frame path is `Character::Poll`
→ `RndDir::Poll` → the dir's `mPolls`, sorted by `CharPollableSorter` at
`Character::SyncObjects` (Character.cpp:710-723). (2) :3460/:3475 (AddObject → `unk5d0`
collection) are correct.

## AMENDMENTS (binding)

- **A1 (FOREARM):** add sub-cause **H-C** = target/TransProxy resolution (V23
  `BandWardrobe::SyncTransProxies` class). Required reading before fix:
  VENUE_RENDER.md V23 (:598), V26 residual (:1070-1100, :1163-1171), V32 (:1175-1341).
  If roots differ, distinguish H-A (parent frame wrong) from H-C (proxy resolved to
  wrong/stale object) before editing.
- **A2 (FOREARM, G3):** replace the flat `batch_objdiff==baseline` gate with the Q2
  three-case table. Record baseline numbers (unit 99.16526, Poll 96.127235) in STATUS
  before any edit. Case-2 (un-gated faithfulness fix) requires a diagnose-mode
  CFG-mismatch justification and forbids any % decrease.
- **A3 (FOREARM):** STEP 0 must include `run_diff_inspect diagnose` on Poll AND an
  audit of `MeasureLengths` (81.4%) for semantic divergence.
- **A4 (CROWD):** use the Q3 anchor set (CharDriver.cpp :157/:163/:341/:342/:404-409/
  :604-607; Character.cpp :246-254; BandCamShot.cpp :558) and the Q3 decision tree;
  first action = extend `CHARDRV_PROBE` to print `mDefaultClip`/`mClips` size.
- **A5 (CROWD):** hand-off ONLY if the whole vignette Enter/flow machinery is absent;
  the CharSync/CharCache blocked item is out of scope and is NOT this bug (census
  poll=3 proves polling works).
- **A6 (CROWD):** R-D discriminator = post-fix isolate capture max-pixel vs the 17/255
  baseline; dark-after-posing → separate engine-side fix (own flag, pin-bump via
  coordinator, deferrable to W26).
- **A7 (CROWD):** no un-scoped behavior change in shared `CharDriver`/`Character`;
  WorldCrowd A/B must be run **flag-ON**.
- **A8:** file-ownership split declared in each lane PLAN (FOREARM: CharIKHand.cpp +
  bandobj; CROWD: CharDriver.cpp probe + band3/native-src).
- **A9:** fix the :3829 anchor description in lane briefs (MeasureLengths resync, not
  poll loop); add rb3-tests 116/0 to the CROWD gate list too (kickoff lists it only
  for FOREARM).

## Final verdict

**DISPATCH-WITH-AMENDMENTS.** Charters, discriminators, closures, and gates are
faithful to W24-RECON; A1–A9 above are binding on the lanes. The two subtle items are
resolved: Q1 — the :40 comment is prior *context* (V26 crowd/extras residual + the V23
SyncTransProxies fix pattern), not a contradiction, and adds H-C; Q2 — G3 is a
three-case decision, not a single baseline-equality check, with CharIKHand::Poll
currently at 96.127% and real precedent (CharHair, V21, V26) that sub-100 regions can
hide semantic bugs.
