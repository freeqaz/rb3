# N2 — Void / wall camera cuts during a song — implementation plan

**Authored:** 2026-05-29 (Opus planning agent, READ-ONLY: no source edits, no
build, no commit). This doc is the only write.

## HEADLINE — N2 is ALREADY FIXED in-tree (the dispatch is stale)

The dispatch was written against `STATUS_AND_NEXT_GOALS.md` (the V34 status
review), which was authored BEFORE the fix landed. While that doc was being
written, an implementation agent shipped the N2 fix:

- **Commit `83780743` "native: N2 void/wall camera-cut fallback — hold last
  band-framing cam instead of cutting to void"** (2026-05-28 22:16Z).
- Documented in `docs/sessions/native/VENUE_RENDER.md` as the **V36** section
  (lines 1388–1554).
- HEAD is `1df5db08`, well past `83780743`; `git diff 83780743 HEAD --
  src/system/bandobj/BandDirector.cpp` is **empty** → the fix is intact and
  unchanged on the current tree (the permuter has NOT wiped it).
- The fix block is present right now: `grep -c 'V36 (N2)\|sVoidcutLastGoodCam\|
  RB3_CAM_FALLBACK_OFF' src/system/bandobj/BandDirector.cpp` → **9 matches**.

The env opt-out the dispatch asked me to *propose* (`RB3_VOID_CUT_FIX_OFF`)
already exists under the name **`RB3_CAM_FALLBACK_OFF`**.

**Recommendation: no new code change for N2.** Verify the existing fix is
present + re-run the A/B to confirm it still works on the current build, then
close N2. The only thing to update is the stale `STATUS_AND_NEXT_GOALS.md`
scorecard (it still lists N2 as open). The rest of this doc records the
root-cause analysis and the shape of the landed fix so a future agent (or a
permuter-wipe recovery) has the full picture.

---

## 1. Root-cause analysis (what the code + evidence actually show)

### The defect (confirmed from screenshots)

- `screenshots/v34-status-review/09_f0700.png` — two slivers of pink/wooden
  stage wall at the top, the rest pure black. Classic intro-pan-on-wall.
- `screenshots/v34-status-review/deep/03_f3400.png` — mid-song: full gem
  highway + HUD render correctly, but the venue BACKGROUND is near-total black
  with two pink wall slivers up top. The during-play recurrence.

### The mechanism (the two-candidate hypothesis is BOTH, refuted in detail)

The venue draws through `BandDirector::DrawShowing()`
(`src/system/bandobj/BandDirector.cpp:316`). Since V19 the venue WorldDir is
loaded as `mCurWorld` but is NOT merged into `GetWorld()`, so the V22 HX_NATIVE
block points the venue's `CamOverride` at the director's active shot cam each
frame (`gw->mCameraManager.MiloCamera() ?: CurrentShot()` → `shot->GetCam()` →
`mCurWorld->SetCam(shotCam)`). The highway draws separately through its own
V12-locked `game.cam`; the engine re-emits scene uniforms per-mesh on
`RndCam::sCurrent` change, so both composite in one frame.

The void cut is the V22 follow faithfully pointing the venue draw-cam at a shot
whose frustum contains **no venue/band geometry**. Two distinct sub-cases, both
real (validated by VOIDCUT_DBG over a 12000-frame sweep, per V36):

- **(a) intro-pan on wall** — `coop_smallclub_intro_venue` is a targetless,
  path/offset-driven pan; its authored sweep dwells on the stage-edge wood
  panels with the band tiny/peripheral. This is the `f0700` class.
- **(b) during-play void cuts** — targetless `coop_all_f00`, `coop_bg_n01/n02`,
  `coop_front_n02`, `coop_gv_n02`: `hasT=0` AND the band-area sphere (centroid
  of the 4 V23-wired player roots, r=120u) is fully OUTSIDE the shot cam frustum
  (`coop_all_f00` sits c2band=373u). This is the `f3400` recurrence.

The two signals the original plan suggested were both proven DEAD natively
(V36 §TL;DR): (1) `mCurWorld->mSphere` radius is **zero** in the native
instanced state (`vsR=0` on all 11544 frames) so a venue-bounds-vs-frustum test
can never fire; (2) targets never collapse to origin (V23 wired the closeup
proxies), and void shots have *no* targets at all, so an origin test is
ambiguous. The working detector had to be derived empirically, not from those.

**Verdict: (c) both** — the intro authored offset AND the V22 follow pointing at
band-free shots. Neither is a transform bug; the camera plumbing is correct, the
shots simply don't frame the venue. This was determinable statically (the code +
the VOIDCUT_DBG runtime data captured in V36); no further instrumentation needed.

---

## 2. Files-to-edit list

**For N2 itself: ZERO files to edit — the fix is already present and correct.**

The edit that landed (and that any permuter-wipe recovery would re-apply) is a
single matched-fork block:

| File | Layer | Function / block | Status |
|---|---|---|---|
| `src/system/bandobj/BandDirector.cpp` (lines ~335–421, inside the V22 block of `DrawShowing()`) | **(a) matched-fork** | V36 (N2) void/wall fallback; also the `#include` at line 16 for `CamShot::GetCam()` | **PRESENT, committed `83780743`, unchanged at HEAD** |

It relies (read-only) on these existing APIs, all confirmed to exist:
- `RndCam::CompareSphereToWorld(const Sphere&)` — `src/system/rndobj/Cam.h:63`
  (`return s > mWorldFrustum`).
- `BandCamShot::mKeyframes` (`ObjVector<CamShotFrame>`) + `CamShotFrame::HasTargets()`
  — `src/system/world/CameraShot.h:33,239`; `BandCamShot` —
  `src/system/bandobj/BandCamShot.h:16`.
- `BandWardrobe::GetCharacter(int)` — `src/system/bandobj/BandWardrobe.h:46`
  (the 4 band-player roots; `TheBandWardrobe` global).

**Concurrency / dep-graph:** N2 owns `src/system/bandobj/BandDirector.cpp`
(matched-fork). Since no edit is required, N2 imposes **no serialization
constraint** at all right now — it can be closed and any agent touching
`BandDirector.cpp` for other reasons (e.g. N5 if it grows into the director) is
unblocked. (Were a re-apply needed, it would serialize against all
`BandDirector.cpp` owners, per the status-doc concurrency table.)

---

## 3. Fix approach (as landed — for reference / wipe-recovery)

Inside the V22 `DrawShowing()` follow, before the unconditional
`mCurWorld->SetCam(shotCam)`, compute `framesVenue`:

1. **Resolved-target guard (load-bearing):** if the shot's first keyframe
   `HasTargets()` (`dynamic_cast<BandCamShot*>(shot)`, `mKeyframes[0]`), it is a
   framed shot (closeup / dircut) → keep. This is what protects every V23
   closeup (0 of 3444 touched).
2. **Targetless → band-visibility test:** average the 4 `TheBandWardrobe->
   GetCharacter(i)->WorldXfm().v` into a band centroid; build a `Sphere` of
   radius **120u** (or **40u** if the shot name contains `intro`, so the wide
   intro pan's dwell-on-wall moments register); if
   `shotCam->CompareSphereToWorld(bandSphere)` returns true (sphere culled), the
   band is fully outside the frustum → `framesVenue = false`.
3. **Action:** if `framesVenue` (or fallback off): `SetCam(shotCam)` and record
   `sVoidcutLastGoodCam` (a function-local `static RndCam*` — no matched-fork
   header growth, permuter-safe). If void: hold `sVoidcutLastGoodCam` (do NOT cut
   to the void shot). If no last-good yet (first cut bad, pre-gameplay): take the
   shot cam anyway so the venue draws through *a* cam.

It is an additive `#ifdef HX_NATIVE … #endif` block (no `#else` needed — the
whole V22/V36 follow is native-only; the matched fork's `DrawShowing()` just
calls `mCurWorld->DrawShowing()`).

**Env opt-out:** `RB3_CAM_FALLBACK_OFF=1` skips the detection entirely → raw V22
follow → reproduces the baseline void cuts. This IS the A/B gate the dispatch
asked for. (Do NOT introduce a second `RB3_VOID_CUT_FIX_OFF` name — it would
duplicate this.) Default: fix ON.

---

## 4. Regression risks (how V22/V23 cinematography is protected)

- **A/B gate present:** `RB3_CAM_FALLBACK_OFF=1` is a true revert to raw V22.
  Verified: `off_run1/04_f3400` reproduces the exact baseline void; `final/
  04_f3400` shows the club. So the fix (not director variance) is provably
  responsible, and the "genuinely like RB3" V22/V23 cinematography is one env
  var away at all times.
- **Closeups untouched:** the `hasT=1` resolved-target guard means every V23
  instrument closeup (which has a real band-character target) is classified
  `framesVenue=1` and committed unchanged — 0 of 3444 closeup frames affected.
- **Wide establishers untouched:** `coop_all_f07/f10`, `coop_d_cd02`,
  `coop_dir_lt02`, `coop_dir_crowd` are `hasT=0` but the 120u band sphere is
  inside their frustum → 100% `framesVenue=1`. The intro tight-40u rule is gated
  on `strstr(name,"intro")` so it never touches these.
- **Highway / HUD untouched:** the fix only ever writes the venue's
  `CamOverride`; the V12 `game.cam` down-highway lock is not touched — gems/HUD
  draw through it as before.
- **Watch for:** (i) permuter wiping the block — re-verify with `grep "V36 (N2)"`;
  the body-local `static RndCam*` keeps it header-free so re-apply is a single
  body replace. (ii) a future song whose band centroid genuinely sits outside a
  legitimately-framed targetless wide shot — the 120u radius is the tuned guard;
  if a new song regresses, widen it or add a name allowlist rather than removing
  the test.

---

## 5. Verification recipe

Re-run the V36 reproducer on the CURRENT build (confirm the committed fix still
behaves). `MILO_SCREENSHOT_DIR` MUST be absolute and pre-exist; re-run until
mesh-count > 0 (menu→gameplay reach is flaky, per V24/V32):

```
MILO_SCREENSHOT_DIR=/abs/dir/on \
MILO_SCREENSHOT_FRAMES=700,1400,2400,3400,4200,5200,6400,7600,8600,10000,11500 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=12000 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
# A/B baseline: prepend RB3_CAM_FALLBACK_OFF=1 and a different /abs/dir/off
# Per-shot canary: prepend VOIDCUT_DBG=1
```

**Key frame: f3400** (the during-play recurrence — the unambiguous test):
- **Fixed (expected):** highway over a rendered club interior — purple-lit
  walls, ceiling beams, a stage prop. (Matches `v36-camera-cuts/final/04_f3400`.)
- **Void (the defect, == `RB3_CAM_FALLBACK_OFF=1`):** highway over near-total
  black, two pink wall slivers at top. (Matches `v34-status-review/deep/
  03_f3400` and `v36-camera-cuts/off_run1/04_f3400`.)

I confirmed this exact A/B from the already-captured screenshots while writing
this plan; both render as described.

Regression checks: `final/03_f2400` (teal guitar closeup still framed),
`on_run1/05_f4200`+`06_f5200`+`07_f6400`+`09_f8600` (wide establishers still
framed), `CAM_DBG=1` confirms `prism_gem_*`/`gem_smasher_*` still draw under
`game.cam`, clean exit 0.

---

## 6. Honest assessment

**Confidence: HIGH that N2 is already resolved.** This is not a speculative
"someone fixed it" — the commit, the in-tree block, the unchanged HEAD diff, the
documented V36 write-up, and the captured A/B screenshots (which I viewed and
they match the claims) all agree. The recommended action is verification +
closing the item, not new code.

**One honest residual (out of scope, NOT a regression):** the very FIRST
intro-pan frames (`f0600`–`f0700` pre-gameplay) can still frame the wall because
the fallback has no `sVoidcutLastGoodCam` to hold yet — there is no prior good
cut to revert to. `v36-camera-cuts/on_intro/02_f0700` shows the crowd+club once
the first good cut lands, but the earliest sweep moments remain wall. Fully
fixing those would require re-authoring the intro shot's `mWorldOffset`/path
(risks regressing authored cinematography) or synthesizing a band-framing cam —
deliberately deferred as out-of-scope for a low-false-positive fix. The
dispatch's PRIMARY target (the recurring during-play void cuts, `f3400`-class)
is robustly handled.

**If the goal is to also kill the earliest intro frames**, that is a *separate,
higher-risk* mini-item (re-author or synth-cam) and should be scoped on its own
with explicit acceptance that it touches authored cinematography — do NOT fold
it into N2's "done."
