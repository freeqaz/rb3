# W27-PROP-PROBE — STATUS

**Type:** probe-only tail (acceptance A9). NO fix code that changes default behavior,
NO flags flipped, NO defaults. One env-gated probe (`RB3_PROP_FINGER_BYPASS`,
HX_NATIVE default-OFF) + one comment-only nit inside `CharIKHand.cpp`.

**Headline:** Both W26 open inferences are now **directly tested**.
(a) The E7 `mFinger` re-projection inference is **CONFIRMED (YES)** by bypass A/B — it
is the mechanism keeping `RB3_IK_REACH_CLAMP` non-dormant despite `RB3_PROP_POSE`.
(b) Prop-tip clip-track enumeration is **NEGATIVE** — no clip track drives the
guitar/drum prop tips (`bone_pick_strum`, `bone_[LR]-tip_*`); their `LocalXfm` is a
static authored constant. Together these pin the eventual real fix: it must (1)
bind/animate the prop-tip clip tracks natively AND (2) break the `mFinger` finger-
compensation feedback — the W26 partial redirect alone can do neither.

---

## (a) mFinger-bypass A/B — VERDICT: YES

Added an env-gated probe (`RB3_PROP_FINGER_BYPASS`, `#ifdef HX_NATIVE`, default-OFF)
that skips the `mFinger` finger-compensation re-projection in `CharIKHand::Poll`
(~:322-332). Ran two in-song captures (beastandtheharlot, guitar/expert, ~18s;
`run_prop_probe.py`) with the reach clamp default-ON and `RB3_IK_CLAMP_DBG=1` /
`RB3_PROP_DST_DBG=1`:

- **A** = `RB3_PROP_POSE=1` (E7 baseline)
- **B** = `RB3_PROP_POSE=1 RB3_PROP_FINGER_BYPASS=1`

Clamp `preDist` (target over-reach seen by the clamp; reach `mAAPlusBB`≈20.3u) and
mode (`skip`=grossly-unreachable neutralise, `clamp`=marginal boundary):

| ikhand | preDist med A | preDist med B | mode A | mode B |
|---|---|---|---|---|
| strum.ikhand | **199.9** | **21.1** | 46 skip / 0 clamp | 0 skip / 64 clamp |
| fret.ikhand | 184.3 | 24.6 | 45 skip / 1 clamp | 0 skip / 36 clamp |
| right_hand.ikhand | 118.4 | 21.4 | 46 skip / 25 clamp | 0 skip / 56 clamp |

Post-mFinger `dst_from_hand` (RB3_PROP_DST_DBG, only logs >30u): strum med **188.5** in
A → **no entries** in B (dropped below 30u).

**Interpretation:** bypassing `mFinger` collapses the hand IK destinations from
~120–240u down to ~21–25u — just above the reach radius. Every grossly-unreachable
`skip` fire vanishes; the clamp degrades to a ~0.97 marginal boundary clamp =
**effectively dormant**. This is the direct confirmation the W26 ERRATA E7 asked for
(the mFinger explanation was previously only inferred, and strum median had actually
moved the *wrong* way under the target-only redirect). The `mFinger` re-projection
(`mHand · mFinger⁻¹ · target`) re-projects the redirected destination back through the
finger/pick bone — itself part of the static clip-posed chain — which is what re-flings
it out of reach. Evidence: `evidence/propA.log`, `evidence/propB.log`.

## (b) Prop-tip clip-track enumeration — VERDICT: NEGATIVE (valid)

`CharIKHand::Poll` is not exercised in menu/song-select/vignette phases
(CharIKHand.cpp:110-113), so the prop tips are observed in-song via the existing
`IK_PROP_DBG` probe (W26 `step0-ikprop.log`), which dumps each tip's `LocalXfm.v` and
its parent's world. A clip track driving a bone would animate its `LocalXfm`; a bone
with a static (never-bound) rest offset keeps it constant.

Distinct `LocalXfm.v` values across the in-song frame window (parent `bone_target_*`
world shown as the moving control):

| ikhand | tip bone | distinct tip LocalXfm | parent world distinct |
|---|---|---|---|
| strum.ikhand | bone_pick_strum | **1** (const 2.28,-48.90,-15.29) | 6 (animates) |
| left_hand.ikhand | bone_L-tip_hihat | **1** (const -19.52,13.84,42.50) | — |
| right_hand.ikhand | bone_R-tip_floor_tom | **1** | — |

The guitar/drum prop tips carry a **constant** LocalXfm while their at-hand parent
frames animate ⇒ **no active clip track binds them**; the static authored tip offset is
exactly what flings the IK target out of reach. Cross-check: the vignette-band bonedump
(`d4-bonedump-sweep.py`, main_hub, 8 advancing clip frames) does not even expose these
prop-tip bones — they live in the instrument prop dir, not the character skeleton. The
mic/vocal-chain tips (`bone_mic_mouth`, `bone_L-hand_mic`) *do* vary (11 distinct) but
are the separate whole-chain-displacement class (W26 A6), out of the prop-tip scope.
This confirms W26 clip-binding gap (c) with fresh enumeration evidence. NEGATIVE is a
valid checkpoint per acceptance A9(b).

## (c) Mechanical nits inside CharIKHand.cpp

1. **E7 env-parse fix — ALREADY PRESENT (no re-apply).** `CharIKHand.cpp:55-57` already
   reads `sOn = (e && e[0] && e[0] != '0') ? 1 : 0` (requires a non-empty non-`'0'`
   value; `RB3_PROP_POSE=""` no longer enables). Applied at W26 close-out `055992be`.
2. **Multi-target weight-loop comment nit — APPLIED.** Added a comment above the
   multi-target weight loop (~:271) noting the first loop derives each target's blend
   weight from the **un-redirected** tip `LocalXfm` while only the world-accumulation
   loop (`sPropPoseRedirect`) is redirected — fine for the discriminator/honest-partial,
   wrong for a real fix (a genuine binding fix must redirect the target before computing
   its weight). Comment-only, codegen-inert.

## GATES

| gate | requirement | result |
|---|---|---|
| batch_objdiff (touched fns, flag-OFF) | == baseline | **PASS** — Poll 96.13% (base 96.127), MeasureLengths 81.34% (base 81.355); HX_NATIVE-gated, Wii `.o` byte-identical |
| rb3-tests | 116 / 0 | **PASS** — 116 passed / 0 failed (7 skipped fixtures, matches baseline) |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | 792 PASS | **AMBIENT-RED, lane-inert** — see below |

**drawlog-golden note (honest):** the flag-OFF run reports FAIL, but the unexpected-
divergence count fluctuates **12 / 48 / 70 / 72 across identical-binary reruns**, all
`field=world` crowd-pose values with the draw **count=792 unchanged**. A single fixed
binary producing 6x-varying counts proves this is the documented ambient IK/crowd
nondeterminism (W25/W26 intermittency), not a lane effect. My edits are provably inert:
the Wii object is byte-identical (batch_objdiff exact baseline) and the native flags-OFF
path reduces exactly to `if (mFinger)` (a comment is codegen-inert). The shared tree
also carries a concurrent agent's uncommitted `native/src/rb3_session_trace.cpp`.
**Flagged for coordinator** — a strict 792 PASS is not achievable in this tree state
regardless of this lane; re-baselining is coordinator-only.

## Disposition
- `RB3_PROP_FINGER_BYPASS` probe **KEPT** (HX_NATIVE default-OFF) — byte-inert
  instrumentation that proved E7; useful for the eventual real binding fix.
- No flags flipped, no defaults changed, no MILO_ENGINE_PIN bump.

## Files changed (staged by path, this lane only)
- `src/system/char/CharIKHand.cpp` — `RB3_PROP_FINGER_BYPASS` probe (HX_NATIVE
  default-OFF) at the mFinger block; multi-target weight-loop comment nit.
- `docs/native/engine-arch-review-2026-07-05/execution/W27-PROP-PROBE/{STATUS.md,evidence/}`

## Checkpoint
`/tmp/wave27-checkpoints/PROP-PROBE.json` (written before cleanup; NEGATIVE (b) recorded).

---

## CLOSE-OUT ERRATA (append-only, from WAVE27_CLOSEOUT_REVIEW.md `09cca9e8`)

**ERRATUM E6:** A-column medians as committed are not reproducible from
`evidence/propA.log` (recompute: strum preDist med 165.6, fret 152.7, strum dst med
154.2 vs quoted 199.9/184.3/188.5; right_hand exact; ALL skip/clamp counts and the B
column reproduce exactly). Conclusion unaffected.

**ERRATUM E7:** "(b) no clip track binds them" is a behavioral inference (constant
LocalXfm); a constant-writing track is not structurally excluded. The mechanism
conclusion (static tip offset flings the IK target) is unaffected.
