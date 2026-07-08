# R1-DOLPHIN D4 — frame-matched Wii-vs-native inter-bone JOIN (the R5-decisive table)

Lane D4 (Wave-C, the frame-matched join). Executes D3's exact priced unblock
(`D3_findings.md` §"Exact unblock") and delivers the FINAL artifact the R5
hands-endgame decision consumes. **Verdict: FRAME-MATCHED TABLE DELIVERED.**
Supersedes D3's unsynced-shell table (`D3_delta_table.*`) for the R5 gate.

## What D3 left open, and what D4 closes
D3 delivered the machinery + a delta table but its native and Wii shells were captured
at **un-synced frames**, so the middle/ring finger divergence could not be separated
from a pose-frame difference (conjugation ambiguity). D4 supplies the missing
`(clip, frame)` alignment and the **convention-invariant** metric that makes the
divergence claim clean.

## Two probe extensions (this lane's tools)
1. **Native** — `native/src/rb3_bonedump_native.cpp` now emits, per member, every
   `CharDriver`'s active clip name + `TopClipFrame()` + top-`CharClipDriver` beat
   (the D4 join key), and additionally captures `pelvis`/`spine` (the convention-pin
   reference bones). Build-green, default-OFF (`RB3_BONE_PROBE_OUT`).
2. **Native sweep harness** — `scripts/native/d4-bonedump-sweep.py`. Under
   `RB3_FIXED_CLOCK=1` the shell vignette advances a real 1/60 s per frame, so a burst
   of `/api/call` dumps samples the idle **`playerN` vignette across its whole loop
   [~0.25 .. 6.59]** (32 samples). Each snapshot carries its clip+frame.
3. **Wii** — `milo-trace tools/wii_bone_dirboot.py drivers`. Scans the Bank-8
   `CharDriver`(0x80c01468)/`CharClip`(0x80bfff44) vtables on the D2 patched-disc boot.

## The measured reason the join is pose-anchored, not beat-labelled
The Wii `drivers` probe finding (reproducible; `D4_wii_drivers.json`): **at the D2
frozen instant there is NO active `CharClipDriver` playing a `playerN` vignette clip.**
18 `CharDriver` instances and all 8 `playerN_{m,f}` clips are RESIDENT (370 named
`CharClip`s), but the player-clip heap referencers are config / name-relocation string
tables (MEM1), not live beat-bearing `CharClipDriver`s — `active_player_clip_drivers = 0`.
So the Wii artifact carries no readable vignette beat; **the Wii POSE itself is the
frame label**, matched against the dense native sweep. This is exactly the task-blessed
fallback ("capture densely… join on nearest-frame with the frame residual bounded"),
and the frame-match quality is bounded per member by the reference-bone residual below.

## The metric: convention-invariant |Δmag| (not the raw angle-delta)
The raw `angle(D_wii · inv(D_native))` is **conjugation-contaminated**: a per-bone
local-frame convention difference `D' = inv(L)·D·L` preserves the rotation ANGLE but
rotates its axis, so the raw delta reads large even when the pose is identical (the
anchor's raw 76–104° is this artifact, not a pose gap — D3's warning). The geodesic
**magnitude** `|relRot|` is invariant under that conjugation, so the primary metric is
`|Δmag| = | |relRot_native| − |relRot_wii| |`, and the decisive quantity is its
**FLOOR** = the smallest `|Δmag|` achievable by ANY frame of the shared vignette clip.

## Validity gates (all GREEN, RE-RUN on this new capture — OPTIONS §4 lint 3)
- **Convention self-calibration:** `layout=col, transpose=False, order=inv(P)·C`
  reproduces D2's 20 stored pairs to worst **0.133°** (same as D3; native measured in
  D2's own convention).
- **Red-team (known-bad pair):** Wii[l-forearm→l-hand] vs native[l-hand→l-thumb01]
  reads **157.5°** → machinery separates a known-bad pairing.
- **Convention pin (pelvis-identity, D3's explicit ask):** cross-side `pelvis→spine1`
  |Δrot| = **2.5–7.8°** (Wii |relRot| 0.77° ≈ identity) → local-frame convention is
  aligned to a few-degree floor on a non-hand shared bone.
- **Bilateral symmetry:** every member's L and R FLOORs agree (e.g. slot2 mr FLOOR
  L 26.6 / R 26.5) → real posed matrices, not noise.
- **Gender/member split (lint 2):** 4 members reported separately; the Wii D2 chain is
  identified as a **male** rig by its near-zero thumb FLOOR against the native males.
- **Anti-magnet (pointer-verified):** native `trans_addr` distinct per member (D3).
- **Evidence committed (lint 7):** tables + raw sweep + Wii driver census in `evidence/`.

## HEADLINE — the middle/ring finger divergence SURVIVES frame-matching (male rig)
Identify the apples-to-apples comparison by the thumb FLOOR (the thumb is nearly static
in the vignette AND structurally shared, so its FLOOR ≈ the member-identity match
quality). The Wii D2 chain is **male** — it matches the native males' thumbs to ~0:

| native member (vs Wii D2 male chain) | thumb FLOOR | **middle/ring FLOOR** (mean / max) | reads as |
|---|---:|---:|---|
| **slot2 male** (cleanest match) | **0.03°** | **26.5° / 41.2°** | strong REAL divergence |
| slot0 male | 0.4° | 7.5° / 12.5° | REAL divergence |
| slot3 female (cross-gender) | 7.5° | 17.6° / 33.9° | confounded by gender |
| slot1 female (cross-gender) | 5.6° | 2.4° / 5.4° | no clear divergence |

**slot2 (male) L-hand — the decisive row set** (Wii |relRot| vs native, FLOOR = best over the whole clip):

| pair | Wii \|relRot\| | native range | **FLOOR \|Δmag\|** | survives? |
|---|---:|---:|---:|---|
| forearm→hand (anchor) | 91.31 | 81.96–98.04 | **0.02** | collapses ✓ (shared) |
| hand→thumb01 | 123.64 | 119.0–124.9 | **0.06** | collapses ✓ (shared) |
| thumb01→02 | 23.56 | 23.58–27.36 | **0.02** | collapses ✓ (shared) |
| thumb02→03 | 11.95 | 5.39–11.98 | **0.03** | collapses ✓ (shared) |
| hand→middlefinger01 | 5.40 | 19.88–33.21 | **14.47** | **SURVIVES** |
| middlefinger01→02 | 24.37 | 45.04–76.09 | **20.68** | **SURVIVES** |
| middlefinger02→03 | 17.73 | 43.26–70.89 | **25.53** | **SURVIVES** |
| hand→ringfinger01 | 7.43 | 25.19–42.75 | **17.76** | **SURVIVES** |
| ringfinger01→02 | 26.13 | 66.06–78.08 | **39.93** | **SURVIVES** |
| ringfinger02→03 | 5.74 | 46.77–50.50 | **41.03** | **SURVIVES** |

Note the native |relRot| **range**: for every middle/ring pair the native finger's
LEAST-curled frame of the entire vignette is still far more curled than the Wii pose
(e.g. middle01→02 native ∈ [45, 76] while Wii = 24; ring02→03 native ∈ [47, 50] while
Wii = 5.7). The wrist anchor and the full thumb cascade match to <0.06°.

## Interpretation for R5 (bounded — R5 issues the verdict, D4 reports the deltas)
- **Surviving deltas (real skeleton/pose candidates):** the middle- and ring-finger
  inner-segment magnitudes on the **male** hand rig. No frame of the shared `playerN`
  vignette closes a 7.5° (slot0) → 26.6° (slot2) FLOOR; native curls those fingers
  systematically more than the Wii. This is the finger-specific divergence R5's
  pre-registered contract flags as **confirming the mechanism** (finger pairs diverge
  while wrist anchors agree).
- **Collapsing deltas (pose-term / exonerated):** the wrist **anchor** and the entire
  **thumb** cascade — FLOOR <0.06°, frame-matchable, structurally identical across
  Wii and native. The skeleton is exonerated there.
- **Bounds / caveats the verdict must carry:**
  - The Wii D2 pose is a **settled/rest pose** (no active vignette driver), matched
    against the native active-vignette envelope. The FLOOR being unreachable is strong
    (native's whole finger animation range excludes the Wii pose) but is a native-clip
    vs Wii-settled-pose comparison, not two frames of the same running clip.
  - Female members are **cross-gender** vs the male Wii chain (D2's NN chain picked a
    male); their numbers are confounded and must not carry the verdict. A Wii capture
    with a per-member gender-split chain would sharpen the female cells.
  - The convention floor is ~2.5–7.8° (pelvis pin); it is far below the 20–41°
    surviving male middle/ring FLOORs, so those survive the convention floor too.

## Reproduction
```bash
cd /home/free/code/milohax/rb3
flock /tmp/rb3-native-build.lock cmake --build native/build-native --target rb3-native -j"$(nproc)"
python3 scripts/native/d4-bonedump-sweep.py --out /tmp/d4sweep --n 32 --dt 0.32
# Wii ground truth already in evidence/D2_wii_bones.json; driver census (needs D2 boot):
#   (milo-trace) tools/wii_bone_dirboot.py boot-check && tools/wii_bone_dirboot.py drivers --out D4_wii_drivers.json
python3 scripts/analysis/interbone_framematch.py \
  --wii docs/native/engine-arch-review-2026-07-05/execution/R1-DOLPHIN/evidence/D2_wii_bones.json \
  --sweep-dir /tmp/d4sweep \
  --out-md D4_delta_table.md --out-json D4_delta_table.json
```
Raw native sweep: `D4_native_sweep_raw.tar.gz` (32 dumps + manifest, each carrying its
clip+frame). Full per-pair per-member numbers: `D4_delta_table.json`.

---

## CORRECTION (Wave-17 close-out, per R5 VERDICT §8 — SUPERSEDES the interpretation above)

The R5 adjudication (`execution/R5-HANDS-ENDGAME/VERDICT.md`, rb3 `fd2f83ac`) adversarially
re-derived this table from the committed raw sweep and **dissolved the headline**:

- The two male members play **different clips** (`player3_m` vs `player0_m` — the 3.5×
  FLOOR disparity is a clip variable); every finger pair is driven by **one scalar curl
  parameter** (cross-pair correlation 1.0000, all members); both male clips sample one
  shared monotone 1-D pose family; the Wii settled pose sits **below** both sampled curl
  intervals — while slot1, whose clip covers the relaxed region, floors the mid-segment
  pairs at 0.12–0.65°.
- The surviving 14–41° middle/ring FLOOR is therefore **clip curl-interval coverage vs a
  driverless settled pose**, NOT the vert-encoded inter-bone basis mechanism. The phrase
  "confirming the mechanism" above is a **banned citation** (VERDICT §8.1) — do not quote it.
- Scope correction (VERDICT/close-out F2): with `D4_wii_drivers.json` showing **0 live player
  clip drivers**, this is a settled-pose-vs-envelope FLOOR table, not a matched
  articulated-frame comparison. The articulated discriminator is **Lane D5** (VERDICT §4).
