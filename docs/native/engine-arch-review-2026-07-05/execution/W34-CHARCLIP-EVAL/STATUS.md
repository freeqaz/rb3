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

## Self-grade vs charter acceptance

| # | Criterion | Status |
|---|---|---|
| 1 | STEP-0 attribution table committed; raw logs gzipped; per-probe-tag counts in STATUS | **MET** |
| 2 | Fix A/B: maxRatio 4.2 → ≤1.5, world-Y fling gone, ≥2 songs; set_play census non-regressed | see FIX section below |
| 3 | Screenshot A/B vs baseline | see FIX section below |
| 4 | Gates: batch_objdiff baseline-exact-or-improved; drawlog 792 PASS; rb3-tests 116/0 | see FIX section below |
| 5 | Checkpoint JSONs before returning | `evidence/step0.json` **MET**; `evidence/fix.json` see below |
| A3 | Frozen-remnant path verified cleared **or** attributed as a separate downstream item | see FIX section below |
