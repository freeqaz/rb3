# W28-PROP-FIX — PLAN

**Lane:** W28-PROP-FIX (optional tail, concurrent, disjoint files). Owner: Opus.
**Flag:** `RB3_PROP_POSE_FULL` (HX_NATIVE, default-OFF, byte-identical `#else`),
building on the existing `RB3_PROP_POSE` scaffold in `CharIKHand.cpp`.
**Owned files:** `src/system/char/CharIKHand.cpp` ONLY (+ this lane dir). Piece (3)
would require `CharDriver.cpp` / `CharClip*.cpp`, which the W28 CROWD lane owns this
wave — **DEFERRED** per acceptance A8 (see below).

## Mechanism (fully pinned by W26/W27 — nothing to re-discover)

The guitar/drum playing-hand IK targets are *tip* bones (`bone_pick_strum`,
`bone_[LR]-tip_<piece>`) whose parent is a correctly-posed authored "at-hand" frame
(`bone_target_*`). The tip carries a large STATIC `LocalXfm` offset that an animation
clip track is supposed to drive down to the strings/head each beat. Natively that
prop-bone clip track is never bound, so the tip stays at its rest offset and flings
the IK target far past the arm's reach — the `RB3_IK_REACH_CLAMP` safety net then
neutralises the hand (dormant-IK look / spike-fan).

W27 (`RB3_PROP_FINGER_BYPASS` A/B) proved the residual mechanism: even with the W26
`RB3_PROP_POSE` target redirect, the `mFinger` finger-compensation re-projection
(`mHand · mFinger⁻¹ · target`) re-projects the redirected destination back through the
static-posed finger/pick bone and re-flings it out of reach (over-reach 120-240u vs
reach 20.3u; bypass collapses it to 21-25u).

## The three pieces (A8)

| # | piece | file | this wave |
|---|---|---|---|
| 1 | break the `mFinger` re-projection feedback | `CharIKHand.cpp` | **DONE** |
| 2 | redirect the target BEFORE the multi-target weight loop | `CharIKHand.cpp` | **DONE** |
| 3 | bind/animate the prop-tip clip tracks | `CharDriver.cpp`/`CharClip*.cpp` | **DEFERRED** |

### Piece (1) — CharIKHand.cpp-local — DONE
`RB3_PROP_POSE_FULL` extends the `if (mFinger && !sFingerBypass)` gate with
`&& !sPropPoseFull()`, i.e. it skips the finger-compensation re-projection (same effect
as the W27 `RB3_PROP_FINGER_BYPASS` probe, now folded into the real fix). Once the
target is redirected to the at-hand parent frame this re-projection is wrong (it assumes
`vec` is where the *finger* lands, not the hand).

### Piece (2) — CharIKHand.cpp-local — DONE
Under `RB3_PROP_POSE_FULL` the multi-target weight loop now redirects `itTrans` via
`sPropPoseRedirect` BEFORE deriving the target's blend weight from `LocalXfm`, so weight
and world position (which the second loop already redirects) agree. `sPropPoseRedirect`
is a deterministic function of `itTrans`, so the positional `locfloats` index alignment
between the two loops is preserved. Under plain `RB3_PROP_POSE` the weight loop stays
un-redirected (the documented W27 honest-partial); only FULL makes them consistent.
`RB3_PROP_POSE_FULL` also forces `sPropPoseRedirect`'s `sOn` on regardless of
`RB3_PROP_POSE`.

### Piece (3) — CharDriver/CharClip*-local — DEFERRED (A8 arbitration)
**Exact site + needed edit (documented, not applied):** the prop-tip bones
`bone_pick_strum` / `bone_[LR]-tip_*` live in the instrument prop dir and carry a
constant authored `LocalXfm` (W27(b) enumeration: 1 distinct tip LocalXfm across the
in-song window while the at-hand parent frame animates → **no active clip track binds
them**). The real piece (3) is to bind & animate that clip track so the tip is driven
to the playing-contact position each beat, which removes the static fling at its source
and would make even the redirect unnecessary. That binding lives in the clip-play /
clip-track application path — `CharClipSet` clip-track→bone binding and/or
`CharDriver` clip resolution (`MyFindClip` kDataObject branch `CharDriver.cpp:345-347`,
`SetClips` :294, `mClips` load :890). Per acceptance **A8(i)** the PROP tail may NOT
write `CharDriver.cpp` or `CharClip*.cpp` this wave (CROWD lane owns them). DEFERRED;
CharIKHand-local pieces (1)+(2) proceed regardless and are sufficient to eliminate the
over-reach for the played instrument (strum/fret).

## Acceptance (A8, numeric)
Same harness/song/window as W27 (`W26-PROP/run_prop_probe.py`, beastandtheharlot
guitar/expert ~18s), reach clamp default-ON, flag-ON: strum/fret/right_hand **skip=0**
AND `dst_from_hand` **0 entries >30u**; plus visible hand-on-instrument in
`/api/screenshot`. Numbers computed by the committed `analyze_prop_ab.py` (E6 lesson).

## Gates
batch_objdiff exact-baseline flag-OFF on touched fns; drawlog-golden flag-OFF 792;
rb3-tests 116/0; boot A/B flag-ON reaches gameplay crash-free. Checkpoint
`/tmp/wave28-checkpoints/PROP-fix.json`. Raw logs gzipped into `evidence/raw/` +
per-log probe-count table in STATUS (A7).
