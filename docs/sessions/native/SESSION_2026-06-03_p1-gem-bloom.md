# Session 2026-06-03 — P1 highway gem bloom LANDED (default-on)

The long-deferred P1 (gem bloom-halo) shipped via an **additive-halo-only**
redesign. Engine `59b7307`, rb3 pin bumped to match. Run as ultracode with a
**validation gate between every phase** (design→validate, implement→validate,
final-gate→land), with the main loop owning image adjudication + landing.

## Why it was deferred, and the redesign

The prior attempt (branch `trackA-bloom`) redirected the WHOLE highway into a
sampleable buffer and re-composited it (premultiplied-OVER) over the graded
venue. The highway is **semi-transparent** (surface.mat SrcAlpha + additive
gem_smasher_glow), so OVER-compositing it against the bright venue bled the venue
through and **washed the dark track** (proven: `BLEND=0` over-only still blew out).

Redesign = **additive-halo-only, capture-and-replay (Design B)**: during the live
game.cam pass, `DrawMesh` CAPTURES (no GPU work) a per-draw record for each
bloom-source mesh — pipeline + the **live pose-baked `mSceneBindGroup` HANDLE** +
mat/obj/bone bind groups + vbuf/ibuf. At `EndFrame` (after `mPass.End()`, before
`Finish()`), `CompositeHaloBloom` replays those draws into a transparent-cleared
sampleable buffer, runs a 2nd `BloomPass`, and **ADDITIVE-blits ONLY the blurred
halo** onto `mFrameView` (LoadOp::Load). The base highway is never redirected or
re-composited — so the dark track is untouched and `BLEND=0` is a true no-op.

## What the design-validation gate caught (before any code)

- **Killed Design A** (re-call `DrawMesh` at EndFrame): the V13 mid-frame camera
  re-pose (gems under scrolled `tf80`, now-bar/back under stationary `tf50`) means
  re-issuing against the single final pose would mis-project every source. Capture
  must store the per-draw **pose-baked scene bind group**, not re-derive it.
- **Frame-discarding MUST-FIX**: store the `wgpu::BindGroup` scene HANDLE, not a
  `uint32_t` offset (a dynamic-offset rebind would be a Dawn dynamicOffsetCount
  mismatch → whole frame discarded).
- 2nd `BloomPass` needs its own `Init()` (builds `mDefaultSampler`); keep the 64KB
  scene-ring; don't touch FlushPostProcMidFrame/MainColorTarget/ClearDepthForOverlay.

## What the build-validation gate caught (after implementation)

The implemented selector was **too broad** (wash returned): the property-based
`IsHaloSourceMat` caught `surface.mat` (the full-highway-quad watermark, emissive)
— blooming a full quad washes the track + lifts the black point — and the
additive-blend clause caught the HUD overdrive/streak **meter-glass lenses**
(bloom spilled into the HUD). **Fix** (main-loop, surgical): restrict to emissive
map present AND mult>0 AND name != "surface"; drop the additive-blend clause (the
now-bar `gem_smasher_glow` is still selected via its emissive map). Verified: a
matched-frame A/B shows a confined gem/now-bar halo with the track black-point
**preserved** (ON 6.0 vs OFF 7.3 — not lifted) and a clean HUD. Flipped
**default-on** (opt out `RB3_HIGHWAY_BLOOM_OFF=1`).

## Methodology lesson — the closing gate's visual FAIL was a FALSE POSITIVE

The closing gate's visual reviewer returned a confident, quantitative HARD FAIL:
"`BLEND=0` washes ~97% of the frame to flat gray," "off-highway delta > on-highway,"
"no demonstrable halo," "HUD wash." The code review said SAFE. **The pixels won —
but only after the main loop adjudicated them properly.** The reviewer compared
**misaligned cross-boot frames**: the venue's per-environ lighting + animated
director camera mean independent boots land the same `songMs` target on different
shots. The flagged `BLEND=0` frame caught a gray-void camera angle; the baseline
caught the pub. Refuted decisively two ways: (1) opt-out (bloom fully inert) and
`BLEND=0` (composite short-circuit) are pixel-functionally identical renders, yet
opt-out@2600 = pub while blend0@2600 = gray → pure camera variance; (2) across 5
times, `BLEND=0` luma stats are statistically indistinguishable from opt-out
(2600: 36.0/31.3 vs 39.4/33.2; 9000: 49.5/33.3 vs 48.5/31.2) — a real full-screen
additive wash would pin every frame at uniform high mean + low std; it doesn't.
**Lesson: quantitative reviewer analysis is invalid across camera-desynced
captures; matched-frame adjudication (or a non-cut/frozen camera) is mandatory for
A/B on this venue.** This is the documented "probe-first / main-loop adjudicates
images" principle applied in reverse — here the static read was right and the
"rigorous" pixel review was wrong, because its frames were misaligned.

## Result

Gems + now-bar now carry a soft, retail-accurate additive bloom; the dark track,
lanes, fret buttons, HUD, and moody venue are untouched. Default-on, opt-out
`RB3_HIGHWAY_BLOOM_OFF=1`; tunables `RB3_HIGHWAY_BLOOM_THRESH` (0.55) /
`RB3_HIGHWAY_BLOOM_BLEND` (0.7). Diff is purely additive (377 insertions, 0
deletions) — every site inert when opted out. Engine `59b7307`.

## Open / next

- Per-environ venue **exposure tuning** (some environs read bright/flat) — the
  remaining lighting-polish task; next gated pass.
- A non-blocking observation surfaced during verification: some director camera
  shots frame a flat-gray void (camera-angle dependent, bloom-independent, present
  with bloom off). Possibly a venue-env edge worth a look later; not a P1 issue.

## Method (ultracode, validation gate per phase)

Workflow 1 (design): 3 readers → 2 independent designers → 2 adversarial
validators → synth (returned a validated spec + go/no-go; killed Design A). →
main-loop gate. Workflow 2 (implement): 1 implementation agent in an isolated
engine worktree (proven build+capture recipe) → 2 validators (code + visual) →
synth (returned fix-first: wash). → main-loop applied the surgical confinement fix
+ default-on flip + adjudicated images. Workflow 3 (closing gate): code + visual
review → synth (fix-first, but the visual FAIL was a camera-desync false positive;
main loop refuted it with frame-stat + matched-frame analysis). → land. The main
loop owns the build/capture/image-adjudication; agents do design, implementation,
and validation. Isolated engine worktree + build dir under `~/tmp` (never `/tmp`).
