# W34-CHARCLIP-EVAL — STATUS

**Lane:** per-frame VALUE trace L1→L4 on one detonating bone. **Agent:** work lane.
**Date:** 2026-08-01. **Base:** `d411aefa`.

---

## STEP-0 VERDICT (committed before any fix code)

**First layer where the value goes wrong: L3 (local→world composition).**
L1 decode and L2 blend kernels are **CLEAN**; L4 palette is **not reached** (the
invariant is already violated in the WorldXfm that L4 consumes).

**Root cause: `Multiply(const Transform&, const Transform&, Transform&)`
(`src/system/math/Rot.cpp:736`) — the native `#else` branch is NOT alias-safe,
while the `__MWERKS__` paired-single asm it replaces IS.**

```cpp
Multiply(a.m, b.m, res.m);                       // clobbers b.m when res === b
res.v.x = a.v.x*b.m.x.x + ... + b.v.x;           // then reads the clobbered b.m
```

The retail asm loads all of `_a` and `_b` into paired-single registers *before*
any store, so `res === b` is well-defined there. `res === a` is safe on both.

### Attribution table

| Layer | Site | Verdict | Value evidence |
|---|---|---|---|
| **L1** | `CharBonesSamples::EvaluateChannel` (90.76%), `ReadCounts` (61.93%) | **CLEAN** | `chLocalRow=(1.0000,1.0000,1.0000) chLocalDet=1.0000`; `chLocalV=(3.4516,0,0)` constant = authored length. A decode/stride/endian error would perturb these; they do not move. |
| **L2** | `CharBones::ScaleAdd/Blend/RotateBy/RotateTo` (91.6–97.3%) | **CLEAN** | Same evidence — the LocalXfm handed to `PoseMeshes` is an exact orthonormal rotation with the authored translation. |
| **L3** | world compose / cached `WorldXfm`; pin origin `BandIKEffector::DoFancyElbow` (`:538`) | **DEFECT** | `sep=6.9432` vs `pred=3.4516` (residual 5.19) with `parWorldDet=1.0000`, `parentOK=1`, `consC=consP=0`. `dirtyC=0` yet `WorldXfm_Force()` moves the bone `staleMove=5.19` → `sepAfterForce=3.4516` (**ratio exactly 1.000**). `propOK=1` in 1244/1244 → *not* a dirty-propagation miss. Backtrace names the pinner. |
| **L4** | engine `Rnd_Wgpu_RB3.cpp` palette compose | **NOT REACHED** | Error is upstream of the palette; no L4 tap needed, mitten/clamp untouched. |

### Discriminator chain (how the layer was named, not guessed)

1. **The oracle is exact.** `childWorld − parentWorld ≡ childLocal.v × parentWorld.m`
   in Milo's row-vector convention, so for a pure-rotation parent basis the ANAT
   ratio is **identically 1.000**. Ratio ≠ 1 ⇒ (a) scale in parent world, (b)
   stale/pinned cached world, or (c) wrong parentage.
2. `evt=ANATX` measured all three at once: **(a) refuted** (`parWorldRow=(1,1,1)`,
   `parWorldDet=1.0000`), **(c) refuted** (`parentOK=1`, `realParent` = assumed).
   → (b).
3. `staleMove`/`sepAfterForce` proved the cached world ≠ `local ∘ parentWorld`.
4. `propOK` (mark parent dirty, read child) **refuted** the DirtyCache-link
   hypothesis — so the cache was not stale-by-omission but **pinned on purpose**.
5. `RB3_WORLDPIN_PROBE` backtrace + `addr2line` named the pinner:
   `BandIKEffector::DoFancyElbow` (BandIKEffector.cpp:538).
6. `BAND_IK` showed the pin magnitude: `|pull|` up to **7.76** vs a 3.45 bone.
7. `BAND_IK_CONS` showed the arithmetic impossibility that closed it:
   `neutralOffsetLenSq=481.261` (neutral distance **21.94**) but
   `|destWorld − targetWorld| = 49.76` — a **2.27×** blow-up through a
   `Normalize()`d orthonormal rotation. Only a corrupted input transform explains it.
8. The corruption source is `NeutralWorldXfm` (`BandIKEffector.cpp:263`):
   `Multiply(tf38, tf, tf)` — **destination aliases the second argument**, and it
   is **recursive**, so the error compounds once per bone up the chain. Bogus
   neutral → bogus IK hand target → out of reach → `ComputeHandPullAndQuat` yanks
   the shoulder → `DoFancyElbow` world-pins the detached upperArm → spindly-branch arm.

Bank-8 cross-check (`bin/analyze-function`, m2c): `ComputeHandPullAndQuat`'s
semantics **match** our source (same `distSq > maxReach² && GetType()==3` branch,
same factor, same zeroing) — its 73.6% residual is scheduling, not semantics. The
IK itself is faithful; it is being fed a corrupted neutral pose.
(Ghidra MCP was down this session; m2c was Bank-8-accurate and sufficient.)

**Probe honesty:** all probes are `HX_NATIVE` + `getenv`-gated and default OFF, so
the shipped path is unaffected. Two of them are *not* strictly read-only when
enabled: `ANATX` calls `cB->WorldXfm_Force()` and `pB->SetDirty()` — that is
deliberate (they *are* the stale/propagation discriminators) and it perturbs only
the probed frame, but a run with `BAND_ANIM_ANATX` set is not a clean behavioural
baseline. All A/B numbers above were taken with the *same* probe set on both
sides, and the gates (drawlog, rb3-tests) were run with every probe unset. The L4
fence needed no tap at all: `Rnd_Wgpu_RB3.cpp` was never opened.

### Repro correction (extends A3)

A bare boot does **not** detonate — at splash no clip plays
(`FirstPlaying=(nil)`, `maxRatio=1.000`, run1). The shell-vignette clips
(`player1_f`/`player2_f`/`player3_m`) only start once **`main_hub_screen`** is
reached. A3's "boot to venue" is therefore *boot → Start → main_hub → hold ~12 s*.
Harness: `scripts/native/_w34_shellvignette_trace.py --nav-hub`.

### Banned classes — NOT re-derived

padded-LoadData/stride (V38-refuted), the 8 dead bind-side classes, reskin,
authored-offset repoint, clip-SELECTION attack. None were touched or re-tested.

---

## Evidence (gzipped raw logs in `evidence/`)

| Run | File | Purpose |
|---|---|---|
| 2 | `run2.engine.log.gz` | first detonation repro with `evt=ANATX` |
| 3 | `run3.engine.log.gz` | stale test (`dirty*`, `staleMove`, `sepAfterForce`) |
| 4 | `run4.engine.log.gz` | propagation test (`propOK`) |
| 5 | `run5.engine.log.gz` | `ANATXPRE` root→leaf cache map |
| 6 | `run6.engine.log.gz` | `WORLDPIN` backtraces |
| 7 | `run7.engine.log.gz` | `BAND_IK` weights + pull at the pin site |
| 8 | `run8.engine.log.gz` | `BAND_IK_CONS` constraint targets |

### Per-probe-tag `grep -c` counts (evidence-honesty rule)

| Run | `evt=ANAT ` | `ANATBEAT` | `ANATX ` | `ANATXCHAIN` | `ANATXPRE` | `evt=HI` | `WORLDPIN` | `BAND_IK]` | `BAND_IK_CONS` |
|---|---|---|---|---|---|---|---|---|---|
| run2 | 5811 | 109 | 5811 | 40677 | – | 6101 | – | – | – |
| run3 | 4644 | 82 | 1505 | 10535 | – | 6087 | – | – | – |
| run4 | 3685 | 71 | 1244 | 8708 | – | 4842 | – | – | – |
| run5 | 3648 | 77 | 1190 | 8330 | 1071 | 5912 | – | – | – |
| run6 | 2434 | 64 | – | – | – | 5401 | 4000 | – | – |
| run7 | 3339 | 71 | 1016 | 7112 | – | 5939 | – | 3000 | – |
| run8 | 3650 | 71 | – | – | – | 5851 | – | 3000 | 2000 |

`run6/7/8` probe budgets (4000/3000/2000) are self-imposed emission caps in the
probe code, not natural event counts. run1 (splash-only, no nav) is intentionally
absent from the table: 19 `ANAT`-family lines, **0 detonations** — that is the
negative control for the repro correction above.

`report.json` regenerated at the start of this lane (2026-08-01 20:11) before any
match% was quoted.

---

## Boot-run ledger (bound: ≤10 for STEP-0 + fix verification)

| # | Purpose | Outcome |
|---|---|---|
| 1 | splash-only repro attempt | negative control — no clip plays |
| 2 | `--nav-hub` + ANATX | detonation reproduced |
| 3 | stale test | `dirtyC=0`, `staleMove=5.19` |
| 4 | propagation test | `propOK=1` ×1244 |
| 5 | ANATXPRE chain map | whole chain pinned |
| 6 | WORLDPIN backtrace | `DoFancyElbow` named |
| 7 | BAND_IK values | `|pull|` 7.76 |
| 8 | BAND_IK_CONS | hip targets, 2.27× blow-up |

Runs 9–10 reserved for fix verification.

---

## FIX (landed) — `evidence/fix.json`

**One-line change class:** make the native `#else` branch of
`Multiply(const Transform&, const Transform&, Transform&)` (`src/system/math/Rot.cpp`)
**alias-safe**, by snapshotting `a.v` / `b.m` / `b.v` before `Multiply(a.m, b.m, res.m)`
overwrites `res.m`. The `__MWERKS__` asm it replaces already is alias-safe.

**UNCONDITIONAL, no `RB3_NO_*` gate** — this is a decomp-source divergence from
target semantics, not a native render heuristic. There is nothing to opt out of:
the pre-fix result was arithmetically wrong for aliased calls.

### Bone-anatomy A/B (the un-gameable oracle)

| Metric | Before (run8) | After (run9) |
|---|---|---|
| ANAT detonations (ratio > 1.5), shell-vignette window | **3650** | **0** |
| `maxRatio` (ANATBEAT) | 1.945 | **1.000** |
| IK shoulder `\|pull\|` max | 7.7599 | **0.0685** |

`1.000` is the *exact* theoretical value for a rigid skeleton under a
pure-rotation parent basis — the invariant is satisfied identically, not merely
pushed under a threshold.

### Gameplay A/B, ≥2 songs

| | song 1 | song 2 |
|---|---|---|
| shots | 13/13 PASS | 11/11 PASS |
| ANAT > 1.5 | 51 | 65 |
| …all of clipType | `guitar_body` | `guitar_body` |
| **vignette-typed detonations** | **0** | **0** |
| max ratio | 1.520 | 1.515 |

The residual is the pre-existing `guitar_body` 1.50-1.52 at-threshold flutter that
A3 explicitly says not to chase, reported separately as instructed.
**set_play census non-regressed:** `rhythm_norm` 6815, `rhythm_ext` 5669,
`rhythm_mel` 2361, `solo_norm` 675 — rhythm *and* solo dispatch alive.

### Gates

| Gate | Result |
|---|---|
| Wii match, touched units | **PASS — baseline-EXACT.** `math/Rot` 97.748400→97.748400, `bandobj/BandCharacter` 99.670180→99.670180, `bandobj/BandIKEffector` 93.575386→93.575386, `rndobj/Trans` 99.029840→99.029840. **0 units changed repo-wide**; overall 63.245117 / 77.56096 unchanged. (Full before/after `report.json`, per-function compare — not assumed from the `#else`.) |
| drawlog canonical-order | **PASS — 792 draws**, `--fixed-clock --canonical-order`, 303 known-residual within the committed `load_residual` bound, **0 unexpected** |
| rb3-tests | **PASS — 123 ran / 116 pass / 7 skip / 0 fail** (= baseline) |
| boot runs | 9 of the 10 bounded runs; 4 further screenshot-A/B runs under the charter's extra-runs carve-out |

### A3 frozen-remnant acceptance — **attributed, not claimed fixed**

The frozen-remnant path **persists**: 290 `evt=HI` samples at gameplay with
`FirstPlaying=(nil) clip='(none)'`, 32 of them parked above world y=100
(player1 258, player0 26, player3 6). **But those bones are no longer at a
*detonated* pose** — the run has zero vignette-typed detachments and a global max
ratio of 1.520. The frozen pose is now *stale-but-anatomically-valid*: the
character is parked at its shell-vignette stage spot instead of being re-driven
to its venue walk-on pose. That is the `BandCharacter.cpp:603-632` walk-on-snap
gap A3 pre-registered as a **separate in-fence defect** — a clip-DRIVE gap, not a
clip-EVAL gap. Recommended as a follow-up lane item; deliberately not folded in.

**Honesty note — "world-Y fling gone" is NOT claimed.** Investigation showed the
`evt=HI` y>50 threshold sits *below* the members' legitimate stage Y in the shell
vignette (the four stand at y ≈ 105/115/135/185), so `evt=HI` fires on the
character being *placed* into the scene
(`pre=(7.05,-0.69,53.99) → post=(14.01,105.45,52.56) moved=106` under
`clip='player3_m'`), not on a bone detaching. `evt=HI` counts are therefore
~unchanged (5851 → 5642) and that is **expected, not a miss**. The metric the
charter designated as un-gameable — the bone-length invariant — went to exactly
1.000.

### Screenshots

`postfix_gameplay_{000,003,005}.png` vs baseline `gameplay_{000,003,005}.png`.
**Caveat, stated plainly:** this is **not** a songMs-matched pair. The committed
baseline `manifest.json` records songMs only for the `handcloseup_*` shots (and
its own shot pin failed, `shot_resolved: null`), and the post-fix run landed on a
differently-lit venue, so camera and venue do not correspond 1:1. Qualitatively
the post-fix members render as intact humanoid figures with no spike-fans or
branch-shard clusters, against a baseline (`handcloseup_play_01.png`,
`gameplay_005.png`) showing an unmistakable splayed tree-branch hand fan.
**Coordinator does E1**; the bone-length oracle is the load-bearing result.

---

## Self-grade vs charter acceptance

| # | Criterion | Status |
|---|---|---|
| 1 | STEP-0 attribution table committed; raw logs gzipped; per-probe-tag counts in STATUS | **MET** |
| 2 | maxRatio 4.2 → ≤1.5 across ≥2 songs; world-Y fling gone; set_play non-regressed | **MET on the oracle** (4.2 → **1.000 exact**, 2 songs, 0 vignette detonations, set_play intact). **PARTIAL on "world-Y fling gone"** — not claimed; shown above to be a threshold artifact of the HI metric, not a defect. |
| 3 | Screenshot A/B vs baseline, same songMs ±150 ms | **PARTIAL** — A/B produced and committed, but songMs-matching was not achievable from the committed baseline (no gameplay songMs recorded; baseline shot pin had failed). Caveat stated; coordinator does E1. |
| 4 | batch_objdiff baseline-exact-or-improved; drawlog 792 PASS; rb3-tests 116/0; ≤10 boots | **MET** (baseline-exact, 792 PASS, 116/0/7, 9 bounded boots) |
| 5 | Checkpoint JSONs before returning | **MET** — `evidence/step0.json` (committed pre-fix), `evidence/fix.json` |
| A3 | Frozen-remnant verified cleared **or** attributed as a separate downstream item | **MET via attribution** — persists, but no longer a detonated pose; assigned to `BandCharacter.cpp:603-632`. |

**Fences respected:** `Rnd_Wgpu_RB3.cpp` mitten/clamp untouched (no L4 tap was
needed — the error is upstream at L3); bind/rebake/reskin untouched; crowd chain
untouched; lighting/wash/postproc/UI untouched; **engine repo untouched — no
engine commit, `MILO_ENGINE_PIN` not bumped.**

## Coordinator E1 (Fable, 2026-08-01, post-fix HEAD)

Independent re-capture with the SAME two harnesses as the 2026-08-01 baseline
(boot-to-song.py defaults + hand-closeup-capture.py; evidence/e1_fixed_*.png
vs the morning baseline PNGs in this dir):

- **E1 = PASS.** Branch-hand family visually RESOLVED: guitarist shows
  individual separated fingers both hands (baseline: yellow twig-claws);
  right member's giant flesh branched claw GONE (recognizable gloved hands);
  skeletal mask-face gone (face renders with glasses).
- Residuals, all pre-attributed elsewhere: thin angular floating structure
  over the track (A3 walk-on-snap frozen-remnant item, BandCharacter.cpp:
  603-632 — follow-up lane); dark-face band + magenta/green casts (lighting/
  wash family, out of scope); guitar_body 1.50-1.52 flutter (A3: do not
  chase).
- Caveat honored: not songMs-matched (baseline recorded no gameplay songMs and
  its shot pin failed); judgment is qualitative on symptom classes, which is
  what E1 is for. The bone-length oracle (3650→0 detonations, maxRatio 1.000
  exact) is the load-bearing result.

**Wave disposition: FIX RATIFIED (unconditional, correctly ungated — decomp
divergence restored to target semantics; Wii match byte-identical).** The same
alias fix also covers RndTransformable::SetTransParent and CharIKHand.cpp:459
call sites — closed IK/forearm residuals (FOREARM-FLOAT, W28-PROP right_hand
~39u) should be re-measured opportunistically next wave before re-chartering
anything in that family.
