# C7/C8 "left-limb IK mispose" — investigation (2026-06-20): RESIDUAL NO LONGER REPRODUCES

**Verdict: the C7/C8 left-limb band-garment shard residual appears RESOLVED on current
master.** No fix landed (none safe to land — the target metric reads 0). Opus-only
investigation (Fable unavailable). Worktree reverted clean, no commit.

## Context
`491288ec` fixed the C8 rotation-basis root cause (rest-bake was in WORLD space →
verts on a `|placement|` lever → R·sinθ smear; now captured relative to the member
root). Its commit message named a remaining **"left-limb IK mispose class"**: band
V24 shard-guard dropped ~20.4 garments/frame with IK on vs ~4.9 with `RB3_NO_IK`.
This investigation targeted that residual.

## Findings (all measured on current master, native -O0 Debug, 3+3 gameplay runs)
1. **The metric reads 0.** Band-member garment guard-drops (`dir='player_*'`) = **0**
   with IK on AND off. The "20.4 vs 4.9" no longer reproduces: the C8 fix
   (`491288ec`) + the relaxed band-aware V24 guard (engine pin `1010f5f`: ratiocap
   4.0× / worldcap 110u / worldfloor 40u) already absorbed those band drops. The only
   V24 drops now are crowd/extras (`male_extras*`) + UI (`scrollbar`) — a separate,
   known issue, NOT C7/C8.
2. **CharIKFingers (the prime suspect) REFUTED by bisection.** Per-solver `Poll()`
   run-counters: CharIKFingers **never executes** in guitar gameplay (it's a
   keyboard/MIDI finger solver). The solvers that actually run on the left arm are
   `CharForeTwist` (`foreTwist_L.ik`) + `CharUpperTwist` (`upperTwist_L.ik`) (365
   calls each), plus `CharLookAt`, `CharNeckTwist`, `CharIKMidi` (`fret.ikmidi`),
   `CharIKHand` (`mic_stand.ikhand`).
3. **The candidate DC3-vs-RB3 divergence exists but is INERT on RB3.** DC3's
   `CharForeTwist`/`CharUpperTwist` carry an HX_NATIVE fix RB3 lacks: after
   `SetWorldXfm()` (sets world, leaves `mLocalXfm` stale) DC3 back-computes
   `mLocalXfm`. Ported it (rotation-only — a full port exploded the arm to 82,000u
   by clobbering the authored bone-length `.v.x`), but a consistency probe
   (`|boneWorld − parentWorld×boneLocal|` at each Poll entry) showed **xRowDot=1.0000,
   posDrift=0.00u with AND without the fix** — RB3's pollable ordering produces no
   cascade drift. The DC3 fix is a no-op on RB3. (Wii-neutrality of the attempted
   port was confirmed anyway — HX_NATIVE-stripped byte-identical; CharForeTwist 81.8%
   / CharUpperTwist 87.1% objdiff unchanged — then reverted.)
4. **The "176u smear" first measured was a camera-framing artifact** — guitar-string
   meshes (`chainsaw_strings`, `bolt_strings`, `guitar_brain_strings`) skinned to the
   fret hand, appearing only when that guitarist is framed; over-cap with IK **on and
   off** (`bolt_strings` 138u even under `RB3_NO_IK=1`). Per-run guard trips are
   dominated by which guitar the venue camera frames, not by IK.

## Open / adjacent (NOT the C7/C8 left-limb residual)
- **Guitar-string over-cap** (strings skinned to the fret hand exceed worldcap) —
  separate skinning issue, camera-dependent, IK-independent. Low priority.
- **Crowd/extras + UI V24 guard drops** (`male_extras*`, `scrollbar`) — separate known issue.
- **Tooling gap (the real blocker to further C7/C8 work):** the venue director's
  nondeterministic camera cuts make per-run guard metrics unreliable. A deterministic
  **"force band closeup"** test hook is the prerequisite to measure any genuine
  remaining band-pose residual to a confident win/loss. Recommended before any
  further C7/C8 effort.

## Bottom line
The hard C7/C8 work (rotation-basis) is done (`491288ec`) and the named left-limb
residual is no longer measurable. Don't re-chase it without first building the
deterministic band-closeup harness.
