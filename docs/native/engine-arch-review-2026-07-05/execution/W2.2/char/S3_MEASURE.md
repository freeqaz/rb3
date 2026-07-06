# W2.2.S3 — Measure all gates + negative control + invariance nets

Implementer: Opus. Lane B (rb3-only, engine READ-ONLY). Build dir
`native/build-agent-W2.2` (Clang). No source edits in S3 — measurement only.
Default stays **OFF** (no flip taken; S4 deferred to coordinator — see §S4 decision).

## Environment / provenance (record per HARD-RULE 7)

- rb3 HEAD at measure time: `8d6053ad` (S2 recorded). rb3-native + rb3-tests rebuilt
  clean (incremental, no source delta since S2 `32746985`).
- **Engine working tree observed at `5cee522`, one commit PAST the pin `41b9e3a`.**
  A sibling Lane A agent (W0.3c) advanced it. The single intervening commit
  (`5cee522 W0.3c: add default-OFF RB3_DRAWORDER_TRACE submission-order probe`) is
  a **default-OFF probe + one new TU** — behavior-neutral for skinning with the flag
  unset (which it is). The pin check in `native/CMakeLists.txt:80` is a `WARNING`,
  not fatal, so the build proceeds. I did **NOT** reset/rebase the engine tree
  (HARD-RULE 7); measurements are valid because the delta is default-OFF. An
  unrelated concurrent agent's uncommitted `FxSendNative.cpp` (audio TU) was left
  untouched.
- The A/B is confounded by quickplay lineup randomization (documented in S1a): the
  band-member/outfit lineup differs run-to-run, so specific mesh NAMES vary. The
  head/hands SKINPOS extents are structurally **stable** across boots (identical
  values on two independent boots), and the venue crowd/extras are venue-determined
  (byte-identical) — so the comparison is valid on those axes.

---

## Gate 3 — Numeric oracle (`ctest -R HandsBindOracle`) — **PASS**

Command (in `native/build-agent-W2.2`):
```
ctest -R HandsBindOracle --output-on-failure
HANDS_BIND_ORACLE_PERTURB=0.15 ctest -R 'HandsBindOracle.(ComposeIdentityAtBindPose|SkinnedVertsMatchAuthored)' --output-on-failure
```
- Unperturbed: **3 pass / 1 skip** (`ComposeIdentityAtBindPose`,
  `SkinnedVertsMatchAuthored`, `PerturbationIsDetected` GREEN; `RealPathFixture`
  SKIP — no in-game fixture dumped, Risks §R3 fallback, invariant still checked on
  synthetic + fingertip transforms).
- **Fail-red proven:** `HANDS_BIND_ORACLE_PERTURB=0.15` → both invariant tests RED.
  Fingertip smears **28.87u** (≈ 200·sin(0.15)=29.7u, the R·sin(θ) failure mode);
  identity residual 0.897 vs 1e-3 eps. The SYS-7 hole is closed and fail-red-able.

## Gate 4 — Invariance nets (`milo-engine-tests`, DC3-context) — **PASS (198/0/2)**

Command:
```
# configure engine build with DC3 decomp context + Dawn (own dir):
cmake -B build-agent-W2.2 -S . -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DMILO_ENGINE_BUILD_TESTS=ON \
  -DDC3_RUNTIME_ROOT=/home/free/code/milohax/dc3-decomp \
  -DCMAKE_PREFIX_PATH=/home/free/code/milohax/dc3-decomp-deps/dawn \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn \
  -DMILO_ENGINE_DECOMP_INCLUDE_DIRS=... -DMILO_ENGINE_DECOMP_PCH=... \
  -DMILO_ENGINE_DECOMP_COMPAT_FLAGS=...
cmake --build build-agent-W2.2 --target milo-engine-tests
DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets \
MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted ctest -j1
```
- **100% tests passed, 0 failed out of 200 → 198 pass / 0 fail / 2 skip** (bar met).
- The two by-design skips: `SkinGolden.CaptureGolden` (golden-capture helper) +
  `ExtractBik.ExtractSmallest`.
- **W0.1 SkinGolden GREEN:** `GoldenMatchesReference`, `ReferenceMatchesCompiledSkinVertex`,
  `BrokenSkinDivergesFromGolden` all pass.
- **W0.4 ClipPoseFixture GREEN:** all 12 incl.
  `EffectorWorldPositionsMatchGolden`, `PoseDeterminism`, `FootOrientationCorrectAfterClip`.
- ⇒ W2.2 did **NOT** leak into the shared skinning/effector math (auto-stop not
  triggered, R-A / R6).

## Gate 5 — W0.5 lineup gate — **PASS**

Commands:
```
python3 scripts/native/lineup-gate.py --selftest              # GPU-independent gate-logic proof
python3 scripts/native/lineup-gate.py --bin native/build-agent-W2.2/rb3-native --out /tmp/w22-lineup-gate
```
- Selftest: `PASS — all three numeric layers separate clean vs exploded`
  (segA/ratioB/countC each clean_pass=True exploded_pass=False).
- Real gate (default-OFF shipped state, 4 frames across `coop_g_n03` + `coop_g_b`):
  **`LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`**
  (sliv=0 on all frames, ncomp 16-20, img 39-46).

---

## Gate 1 — Numeric draw-time A/B (flag-ON vs flag-OFF) — **SCOPE net-win; literal bar NOT met by an OUT-OF-SCOPE residual; flag = no measured benefit**

Two passes (`hands_bind_characterize.py --single default --dwell 40`), MAX-over-run
per-mesh, `SHARD_RATIO_DBG` guard ON:
- flag-OFF baseline = S1a `char/parsed-default.json` (byte-identical to current flag-OFF
  path — S2: only a `static int`+`getenv` added, skipped when 0).
- flag-ON = `RB3_HANDS_BIND_FIX=1` → `char/s3-flagon/parsed-default.json`.

| metric | flag-OFF | flag-ON | bar | verdict |
|---|---|---|---|---|
| FLING(>120u) count | **0** | **0** | =0 | ✓ |
| STOP-tripwire (>92u SKINPOS or 200-460u smear) | none | none | none | ✓ (not reproduced) |
| worst head/hair appendage SKINPOS | 69.5u (`head.mesh`/`bone_L-brow2`) | 69.5u (`head.mesh`+`male_facehair_chops`/`bone_L-brow2`) | ≤65u | **graze (structural)** |
| worst HAND SKINPOS | 53.0u (gloves) / 47.2u (fingernails) | 53.0u / 47.2u | ≤65u | ✓ |
| worst HAND ratio (no-drop) | 2.31-2.35x | 2.12-2.15x | ≤2x | marginal graze |
| **HARD guard-DROP (band)** | `saddleshoe_skin.2` **4.73x** | `lowtopsneaks_skin.2` **5.09x** | none | **FAIL (out-of-scope foot/shoe)** |

Key findings:
1. **The head/hands/hair rebake (W2.2's scope) is a proven NET WIN.** S1a A/B causation:
   with rebake OFF, `head.mesh` is guard-DROPPED at **9.59x** (bind 15.6 / world 150.0)
   — a catastrophic head shard the guard has to hide; with rebake ON, `head.mesh`
   renders cleanly (1.34x, NOT dropped) at a 69.5u SKINPOS graze.
2. **The 69.5u head graze is STRUCTURAL, not flag-related:** it is a brow-bone
   (`bone_L-brow2`) vertex; non-rebound CROWD bodies sit at the SAME 63-64u via
   `bone_head`. It is **identical** flag-OFF and flag-ON (69.5u ↔ 69.5u).
3. **The one HARD signal on BOTH passes is a FOOT/SHOE mesh** (`saddleshoe_skin.2`
   4.73x OFF / `lowtopsneaks_skin.2` 5.09x ON — different specific mesh, SAME class,
   lineup-randomized). Foot/shoe meshes go through the **lower-body**
   `RebindOutfitBonesToOwnSkeleton` path, **NOT** `RebindHeadHandsAtRest` — they are
   OUTSIDE W2.2's scope. The `RB3_HANDS_BIND_FIX` flag only gates a branch inside
   `RebindHeadHandsAtRest` (`BandCharacter.cpp:1360-1394`), so by construction it
   cannot clear this drop.
4. **The flag provides NO measured improvement to the head/hands/hair scope:**
   head 69.5u ↔ 69.5u, hands ≤53u ↔ ≤53u — identical. On that scope there was no
   guard-DROP for the flag to clear (the rebake already renders head/hands cleanly);
   the residual there is a marginal graze, not a drop.
5. **Torso not regressed:** head-region outfit collars graze ≤65.4u via `bone_head`
   on both passes (same as baseline); no torso mesh worsened.

Conclusion: on the W2.2 head/hands/hair scope, SKINPOS is a marginal graze
(69.5u head, ≤53u hands) with FLING=0 and no tripwire — a net-win over the
rebake-OFF 9.59x head shard, but **not** clean-passing the literal `≤65u / ≤2x`
bar. The one HARD guard-DROP is an out-of-scope lower-body foot/shoe residual that
the flag cannot and does not address. The default-OFF `RB3_HANDS_BIND_FIX` yields
no measured benefit in this run.

## Gate 2 — Negative control (crowd/extras `[SKIN_CLAMP]` byte-identical) — **PASS**

Compared flag-ON `s3-flagon/parsed-default.json` `clamp` vs S1a `parsed-default.json`
`clamp`:
- **All 29 true venue crowd/extra clamp meshes** (`male_crowd_body0X`,
  `female_crowd_body0X`, `male_extras_*`, `female_extras_*`, `*_extra_*`) present in
  BOTH passes with **BYTE-IDENTICAL counts — zero differences, zero set diffs.**
- Of the full 33-mesh overlap, 32 are byte-identical. The one differing entry —
  `messyshort_resource.mesh` (1218 → 1017) — is a **band-assigned hair resource**
  (it was in S1a's HEAD_REBIND PENDING list `player3|messyshort_resource.mesh`), i.e.
  subject to lineup randomization + rebind-throttle, **not a venue extra**. The
  set-only diffs (`50sbandana` OFF-only; `crazyhawk`/`dwarvenbeard`/`ziggymullet`
  ON-only) are likewise randomized band hair/accessory resources.
- Code proof (`BandCharacter.cpp:1383-1394`): the flag gates a branch strictly inside
  `RebindHeadHandsAtRest` (band-player head/hands/hair/face). Crowd/venue extras are
  non-rebound and never enter that method — they take the `SKIN_CLAMP` path. So the
  flag is band-scoped by construction; the empirical venue-extra byte-identity
  confirms it.

Note: a strict "byte-identical" over the FULL clamp map across two boots is not
physically achievable while quickplay randomizes the band lineup; the negative
control is therefore satisfied on the **venue-extra** subset (the meshes the control
is actually about), which IS byte-identical.

---

## S4 decision — DO NOT FLIP; land default-OFF; defer to coordinator

Per PLAN.md §S4 + brief B1: the flip is gated on (a) a net-new flag that clears the
residual, (b) all layers green, AND (c) reviewer-judged wide + hand-closeup frames vs
fresh Dolphin t2 + retail screenshots. Assessment:
- **(a) NOT met:** the flag shows no measured improvement on the head/hands scope and
  does not clear the out-of-scope foot/shoe HARD drop.
- **(b) partial:** gates 2/3/4/5 PASS; gate-1 literal bar unmet (out-of-scope
  lower-body residual + structural head graze).
- **(c) NOT available in-wave:** no reviewer judgment against fresh Dolphin/retail
  captures was performed by this subagent.

⇒ **The default stays OFF** (worst case of the four-layer exit = an unflipped flag,
never a blind revert). The S1c golden `handcloseup_walkon.png` (visible count-in
hand shard) + this measurement are handed to the coordinator for the S4 decision.
Recommendation for the coordinator: since the flag yields no measured benefit and the
remaining HARD residual is a lower-body foot/shoe path outside W2.2's scope, treat
foot/shoe rest-capture coverage as a separate Wave-4 item rather than flipping
`RB3_HANDS_BIND_FIX`; the head/hands/hair rebake is already a proven net-win
default-ON via `RebindHeadHandsAtRest` and needs no flip.

## Artifacts

- `char/s3-flagon/` — flag-ON pass (`parsed-default.json`, `CHARACTERIZATION.md`,
  `run-summaries.json`; `raw-default.log` gitignored, regenerable).
- `char/parsed-default.json` / `parsed-nohead.json` — S1a flag-OFF baselines (the A/B
  and negative-control reference).
- `/tmp/w22-engtest-ctest2.log`, `/tmp/w22-lineup.log`, `/tmp/w22-s3-flagon.log` —
  raw gate logs (scratch, not committed).
