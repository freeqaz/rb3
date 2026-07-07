# R1-DOLPHIN D3 — native-side bone probe + Wii-vs-native inter-bone JOIN

Lane D3 (Wave-B, the native-side join). Builds the native half of the per-pair
inter-bone delta table and joins it against the D2 Wii ground truth. Verdict:
**TABLE DELIVERED + validated machinery**, with a **priced matched-clock caveat**
that bounds what the numbers may conclude for R5.

## What was produced
- `native/src/rb3_bonedump_native.cpp` — env-gated (`RB3_BONE_PROBE_OUT`) native
  bone probe, one `extern "C" int rb3bp_dump_bones()` reached over `/api/call`
  (RB3_REPLAY_API=1), modeled on `rb3_replay_capture.cpp`. Walks each band member's
  **OWN** `BandCharacter` dir (`TheBandDirector->GetCharacter` in gameplay,
  `TheCharCache->GetCharacter` in the shell), dumps every hand-chain
  `RndTransformable` bone's world+local matrix, parent, class, and **pointer
  identity**. Read-only, main-thread, HX_NATIVE only.
- `scripts/native/d3-bonedump-capture.py` — boot+nav harness (reuses
  keyboard-to-gameplay). Captures BOTH the shell (main_hub) and gameplay contexts,
  `RB3_FIXED_CLOCK=1`.
- `scripts/analysis/interbone_diff.py` — the join. Auto-calibrates the matrix
  convention against D2's stored values, red-teams the metric, emits the table.
- Evidence: `D3_delta_table.{md,json}` (primary, shell/D2-matched),
  `D3_delta_table_gameplay.{md,json}` (secondary, director band),
  `D3_native_bones_{shell,gameplay}.json` (raw dumps).

## Validity gates (all GREEN)
1. **Convention self-calibration (anti-oracle).** D2's JSON stores both raw
   per-bone worlds AND the precomputed per-pair D. The join brute-forces the
   homogeneous layout / transpose / product-order combo that reproduces D2's stored
   D from D2's raw worlds, and applies the *identical* convention to native. Result:
   `layout=col, transpose=False, order=inv(P)·C` reproduces **all 20** D2 pairs to a
   **worst residual of 0.133°**. The native side is measured in D2's own convention.
2. **Red-team (lint 3).** The delta metric fired on a deliberately MISMATCHED pair
   (Wii `L-forearm→L-hand` vs native `L-hand→L-thumb01`) reads **168.8°** →
   machinery separates known-bad. It is not silently reporting ~0.
3. **Anti-magnet, pointer-verified (lint 1 + campaign own/bound trap).** Every
   hand bone has a **distinct** `trans_addr` across all 4 members (0/54 shared).
   We dumped each member's OWN animating skeleton, never the shared static magnet.
   (Verified in `D3_native_bones_shell.json`.)
4. **Gender/member split (lint 2).** 4 members reported per-row: slot0/2 male,
   slot1/3 female. Aggregates shown but non-gating.
5. **Evidence committed (lint 7).** Raw dumps + tables in `evidence/`.

## Headline numbers (shell context, D2-matched — `D3_delta_table.json`)
- anchor (`forearm→hand`) delta mean **90.6°**; finger delta mean **29.7°**
  (max 89.6°).
- **The magnitude-agreement pattern (slot0 male, L-hand):** the ANCHOR and the
  THUMB chain agree in relRot *magnitude* to ~1° between Wii and native, while the
  MIDDLE/RING chains diverge 20–45°:

  | pair | Wii relRot | native relRot | Δmag | delta° |
  |---|---:|---:|---:|---:|
  | forearm→hand (anchor) | 91.31 | 92.73 | 1.4 | 114.8 |
  | hand→thumb01 | 123.64 | 124.86 | 1.2 | 63.2 |
  | thumb01→thumb02 | 23.56 | 23.58 | **0.02** | 15.7 |
  | thumb02→thumb03 | 11.95 | 11.98 | **0.03** | 8.0 |
  | hand→middlefinger01 | 5.40 | 19.83 | 14.4 | 21.0 |
  | middlefinger01→02 | 24.37 | 45.01 | 20.6 | 36.9 |
  | hand→ringfinger01 | 7.43 | 25.15 | 17.7 | 27.0 |
  | ringfinger01→02 | 26.13 | 65.96 | 39.8 | 56.2 |

## Interpretation for R5 (honest, bounded)
The **absolute cross-side deltas are large but must NOT be promoted to a "native
skeleton defect" verdict as-is**, for three measured reasons:

1. **D2's artifact carries no clip/frame join key.** The plan's join is on
   `(member, clip, frame)` (§3.6). D2's static shell dump recorded no
   CharClipDriver clip/frame, so a frame-exact "matched clock" join is *impossible
   from the existing Wii artifact*. Both sides are in their respective, un-synced
   idle/rest shell poses — the delta necessarily includes pose-frame difference.

2. **The "equal magnitude, large delta" signature is a frame/convention
   conjugation, not a scale defect.** `inv(W_parent)·W_child` is invariant to a
   global world-frame change but a per-bone LOCAL-frame convention difference
   (`D' = inv(L)·D·L`) *preserves the rotation angle and rotates only its axis* —
   exactly what the anchor/thumb rows show (Δmag ≈ 0.02–1.4° but delta 8–115°). The
   thumb chain matching to 0.02–0.03° is strong evidence the underlying pose is
   structurally the same there and the residual is a measurement-convention /
   unmatched-frame artifact, not a magnitude bug.

3. **The middle/ring divergence is the one genuinely pose-level signal** (native
   fingers more curled: middle01→02 45° vs Wii 24°), consistent with a different
   idle-clip frame OR a real finger-pose difference — *these cannot be separated
   without a shared frame label.* Gameplay/director context is worse still and
   bilaterally ASYMMETRIC (L-hand 169° vs R-hand 84° — guitar fret≠strum), i.e. a
   genuinely different playing pose, so its larger deltas are expected, not defect.

**Bottom line for the R5 gate:** the table + machinery are delivered and validated
end-to-end (convention exact, red-team red, anti-magnet, gender-split). The
matrix-relative deltas exist for every M-2 pair on both hands and all 4 members.
But a clean "reskin-vs-closure" verdict needs a **shared clip+frame label on both
sides**, which the current D2 artifact lacks. This is the priced blocker.

## Exact unblock (to convert the table into an R5-decisive datum)
- **Cheapest:** add a clip/frame join key to BOTH sides. Native: extend
  `rb3bp_dump_bones()` to also emit the member's `CharClipDriver` clip name + frame
  (a few lines — the driver is reachable from the BandCharacter). Wii: extend
  `milo-trace tools/wii_bone_dirboot.py` to read the same CharClipDriver state at
  the frozen frame D2 already pauses on. Then drive BOTH to the same
  `(clip, frame)` and re-run this join — the frame-difference term drops out and the
  residual is the real skeleton/convention delta.
- **Convention pin:** additionally calibrate the per-bone local-frame convention on
  a KNOWN-shared bone (e.g. pelvis at identity, or forearm→hand where structure is
  proven equal) to null the `inv(L)·D·L` conjugation, isolating the pose term.

## Reproduction
```bash
cd /home/free/code/milohax/rb3
# 1. build (probe TU is default-OFF; RB3_BONE_PROBE_OUT gates it)
flock /tmp/rb3-native-build.lock cmake --build native/build-native --target rb3-native -j"$(nproc)"
# 2. capture native bones (shell = D2-matched; add --force-gameplay for director band)
python3 scripts/native/d3-bonedump-capture.py --out /tmp/d3
# 3. join vs the D2 Wii ground truth (auto-calibrates convention, red-teams, emits table)
python3 scripts/analysis/interbone_diff.py \
  --wii docs/native/engine-arch-review-2026-07-05/execution/R1-DOLPHIN/evidence/D2_wii_bones.json \
  --native /tmp/d3/native_bones_shell.json \
  --out-md D3_delta_table.md --out-json D3_delta_table.json
```
The `/api/call` symbol resolves via the `nm` static vaddr + PIE load-bias path
(`rb3bp_dump_bones` is not in `.dynsym`, same as `rb3rc_capture_sweep`); the harness
does this automatically.
