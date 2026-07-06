# W1.5 — Dedupe BandUniformRing vs UniformRingBuffer — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under
`flock /tmp/rb3-docs.lock`. Record commit SHAs, gate results (zero-Grow count, lineup
metrics, screenshot sha256, drawlog diff), and any blockers. Re-runs read this +
`git log --grep=W1.5` and skip done work.

Plan: `./PLAN.md`. Engine tip at planning: `8d6d895` (W1.4). Lane A next after W1.5: W1.7, W1.6.


## W1.5.S1 — done

**Engine commit:** `0cd227f` "W1.5: extract UniformRingBuffer into shared gfx TU (MOVE, byte-identical for DC3)".
Staged only: `src/gfx/UniformRingBuffer.{h,cpp}` (NEW), `src/platform/Rnd_Wgpu.{h,cpp}`,
`src/gfx/ShadowPass.cpp`, `CMakeLists.txt`. Pre-existing concurrent `src/platform/FxSendNative.cpp`
edit left untouched (hard rule 8). `MILO_ENGINE_PIN` NOT bumped (hard rule 3).

**What changed (pure relocation, DC3 behavior byte-identical):**
- NEW `src/gfx/UniformRingBuffer.h` — class body copied character-for-character from
  `Rnd_Wgpu.h@8d6d895` (rndobj-free Tier-1 header; only `#include <webgpu/webgpu_cpp.h>`).
- NEW `src/gfx/UniformRingBuffer.cpp` — `Init`/`Grow`/`Write` bodies verbatim from
  `Rnd_Wgpu.cpp@8d6d895` (incl. the `#ifdef DEBUG_LOGS` fprintf); added `#include <cstdio>`.
- `Rnd_Wgpu.h` — dropped inline `class UniformRingBuffer{}`, added
  `#include "gfx/UniformRingBuffer.h"` next to `gfx/UniformStructs.h`.
- `Rnd_Wgpu.cpp` — dropped the out-of-line `Init/Grow/Write` defs (banner replaced with a pointer comment).
- `ShadowPass.cpp` — added explicit `#include "gfx/UniformRingBuffer.h"` (was transitive);
  `ShadowPass.h` forward-decl `class UniformRingBuffer;` left as-is.
- `CMakeLists.txt` — added `src/gfx/UniformRingBuffer.cpp` to `MILO_ENGINE_GFX_SOURCES` (gfx core,
  compiled for BOTH flavors).

**MOVE proof (zero-line diff vs `8d6d895`):**
- Class body: sed-range diff of `class UniformRingBuffer {...};` (Rnd_Wgpu.h@8d6d895 vs new
  UniformRingBuffer.h) → empty (exit 0).
- Three method bodies (contiguous Init->Grow->Write): awk-extracted block diff vs
  `Rnd_Wgpu.cpp@8d6d895` → empty (exit 0).

**Single-definition check:** `grep -rn "class UniformRingBuffer {" src/` → exactly 1
(`src/gfx/UniformRingBuffer.h:13`).

**Builds (both flavors, clang):**
- rb3 flavor: `native/build-agent-W1.5` (clang — default `/usr/bin/c++` rejects
  `-fdelayed-template-parsing`, reconfigured with clang/clang++) → `cmake --build … --target
  rb3-native` exit 0, linked; `src/gfx/UniformRingBuffer.cpp.o` compiled into RB3's `milo-engine`.
  Log `/tmp/w15-rb3-build.log`.
- dc3 flavor: engine `build-agent-W1.5` configured with initial-cache derived from
  `build-agent-W0.1` (context ON / flavor dc3) → `cmake --build … --target milo-engine-tests`
  exit 0, linked; both `UniformRingBuffer.cpp.o` and `ShadowPass.cpp.o` compiled.
  Log `/tmp/w15-eng-build.log`.

**Remains:** W1.5.S2 (RB3 adopts shared class, drop `BandUniformRing`, wrap->grow CHANGE + gates),
W1.5.S3 (verification sweep). **Blockers:** none.

## W1.5.S2 — done

**Engine commit:** `648dc40` "W1.5: RB3 adopts shared UniformRingBuffer, drop BandUniformRing (CHANGE: overflow wrap->grow)".
Staged only: `src/platform/Rnd_Wgpu_RB3.h`, `src/platform/Rnd_Wgpu_RB3.cpp`. Pre-existing concurrent
`src/platform/FxSendNative.cpp` edit left untouched (hard rule 8). `MILO_ENGINE_PIN` in
`rb3/native/CMakeLists.txt` NOT bumped (hard rule 3).

**What changed (the one CHANGE = overflow wrap->grow):**
- `Rnd_Wgpu_RB3.h` — added `#include "gfx/UniformRingBuffer.h"`; deleted the `class BandUniformRing`
  block (+ its comment); retyped the 4 members `mSceneRing/mMaterialRing/mObjectRing/mBoneRing`
  from `BandUniformRing` -> `UniformRingBuffer`.
- `Rnd_Wgpu_RB3.cpp` — deleted the `BandUniformRing::Init`/`::Write` out-of-line defs + banner
  (replaced with a one-line pointer comment). No call sites touched: `Init(mGpu.Device(),…)` /
  `Write(mGpu.Queue(),…)` bind the shared class's by-ref params to `GpuDevice::Device()/Queue()`
  lvalue refs (GpuDevice.h:57-58); `Reset()/Release()/Buffer()` identical in the shared class.

**Path-identity argument (why the CHANGE is inert):** the shared `UniformRingBuffer::Write`
non-overflow path is byte-identical logic to `BandUniformRing::Write` (same `kAlignment==kAlign==256`,
same `off=mOffset; WriteBuffer(off); mOffset+=aligned; return off`). The ONLY divergence is the
overflow branch (old `wrap`: `mOffset=0` vs new `Grow()`). `Init` differs only by a null-guard
(`label ? label : "UniformRing"`) never taken since RB3 always passes a string literal. So if
overflow never fires, the two are observably identical.

**Gate evidence (deterministic scenes; AFTER = commit `648dc40`, BEFORE = S1 tip `0cd227f`
built from a temporary engine worktree with `-DMILO_ENGINE_PATH`):**

1. **Zero-Grow proof (DEFINITIVE).** Temporarily made `UniformRingBuffer::Grow` print
   `[W15_GROW_PROBE] …` unconditionally (NOT committed — reverted before commit; `git diff
   src/gfx/UniformRingBuffer.cpp` empty at commit time). Built rb3-native (`build-agent-W1.5`) with
   the probe, ran the deterministic lineup scene headless. Engine log (21808 lines):
   `grep -c W15_GROW_PROBE` = **0**. Also **0** across the fixed-clock drawlog boots. Zero grows =>
   neither the old `wrap` nor the new `Grow` overflow path executes => provably inert at these scenes.

2. **Non-blind lineup gate (`scripts/native/lineup-gate.py` vs committed golden `w0.5-lineup`).**
   BOTH builds **PASS** all numeric layers:
   - AFTER (`648dc40`, /tmp/w15-after): `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`.
   - BEFORE (`0cd227f`, /tmp/w15-before): `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`.
   countC (per-slot mesh/skinned/vert draw counts) is exact vs golden on both (e.g. slot0
   meshes=140 verts=15395 ratio=1.0). segA/ratioB within golden tolerance on both.

3. **Screenshot sha256 / exact segA equality — NOT achievable at this harness (documented
   nondeterminism, NOT caused by this change).** Two runs of the SAME AFTER binary produce
   DIFFERENT lineup PNG sha256 and different segA (run1 sol=0.413 fill=0.203; run2 sol=0.591
   fill=0.702) — the lineup capture is boot/pose/wall-clock nondeterministic (W0.3 caveat).
   Exact screenshot-equality is therefore impossible before-vs-after regardless of my edit; the
   binding evidence is the zero-Grow proof + lineup PASS + path-identity argument (PLAN.md §S2
   fallback clause).

4. **Draw-log (fixed-clock, `scripts/native/drawlog-golden.py --fixed-clock --frames 180`).**
   Deterministic per-boot (spread=0) but the splash draw **count wobbles in {883,888} on BOTH
   builds** (AFTER: [883,883,883]; BEFORE: [888,888,883]) — the residual W0.3 boot nondeterminism
   (README notes historical 885-888), NOT introduced by this change (BEFORE, which still uses the
   old wrap ring untouched by me, shows the same wobble). So an exact drawlog diff is not a clean
   gate here; it is advisory and consistent with no behavior change.

**Builds (both flavors green, clang):**
- rb3 flavor: `native/build-agent-W1.5` -> `rb3-native` + `rb3-tests` link clean at `648dc40`.
  Full `rb3-tests` suite = **70/70 PASSED** (incl. DrawLogGolden, StubCensus, NativeCompatRegistry,
  BandPatchMeshTest, CharLoad5b Wave-1 nets) — no new failures.
- dc3 flavor: unchanged by S2 (only RB3 files touched); validated at S1 (`0cd227f`,
  milo-engine-tests link).

**Exit-criteria greps (engine repo):**
- `grep -rn "BandUniformRing" src/` -> only the descriptive comment in `Rnd_Wgpu_RB3.cpp:123`
  (0 type/class/member references).
- `grep -rn "class UniformRingBuffer {" src/` -> **exactly 1** (`gfx/UniformRingBuffer.h:13`).
  `ShadowPass.h:6` `class UniformRingBuffer;` is the intended S1 forward-decl (not a definition).
- `gfx/UniformRingBuffer.cpp` listed once in `MILO_ENGINE_GFX_SOURCES`.

**Deviations from PLAN.md:** none in scope. The two "identical screenshot sha256" / "identical
segA/ratioB/countC before-vs-after" requirements were relaxed to "both PASS the golden gate" +
"exact equality documented-unachievable due to harness nondeterminism" — this is the PLAN.md §S2
item-3 explicitly-provided fallback (prefer zero-Grow + lineup + screenshot; when a layer is
boot-unstable, document and let the others bind). The nondeterminism was proven to be harness-side
(same-binary A/B differs), not from the wrap->grow change (BEFORE build, my-code-untouched, shows
identical wobble). Temporary Grow-probe edit to `UniformRingBuffer.cpp` was reverted and never committed.

**Remains:** W1.5.S3 (independent verification sweep + STATUS finalize). **Blockers:** none.

## W1.5.S3 — done

**Independent verification sweep, no source edits (as expected for this subtask).** Verified
against engine tip `648dc40` (W1.5.S2, unchanged — no new commits since S2).

**1. Fresh clean build, both flavors:**
- rb3 flavor: deleted `CMakeCache.txt`/`CMakeFiles` in `native/build-agent-W1.5` and
  reconfigured from scratch (`-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
  -DCMAKE_BUILD_TYPE=Debug`) — clean configure, reproduces the expected
  `MILO_ENGINE_PIN` mismatch warning (HEAD `648dc40` vs pinned `9561a19...`, correct per hard
  rule 3 — coordinator bumps once at wave end). `cmake --build native/build-agent-W1.5 --target
  rb3-native rb3-tests -j8` → **both targets built clean**, 0 errors, no new warnings (full log
  `/tmp/w15-s3-build.log`, 1694 lines). Fresh binaries confirmed by mtime
  (`rb3-native` 118705848 bytes, `rb3-tests` 120945416 bytes, both timestamped this run).
- dc3 flavor: rebuilt `milo-engine-tests` in the existing `milo-native-engine/build-agent-W0.1`
  cache (context ON / DC3 flavor, the working recipe from W0.1/STATUS.md) at the same engine
  HEAD `648dc40` → linked clean; `src/gfx/UniformRingBuffer.cpp.o` present in the link (shared
  gfx-core TU compiles for both flavors, confirming S1's exit criterion 4 independently).

**2. Grep exit criteria (engine repo `src/`):**
- `grep -rn "BandUniformRing" src/` → **only 1 hit**, the descriptive comment left behind at
  `src/platform/Rnd_Wgpu_RB3.cpp:123` ("`// (BandUniformRing removed in W1.5 — the four rings now
  use the shared...`") — **zero type/class/member references**, matching the PLAN.md exit
  criterion in spirit (grep for the identifier as a *symbol*, not as English prose in a comment,
  is 0). Noting this explicitly since a bare `grep -c` would report 1, not 0.
- `grep -rn "class UniformRingBuffer" src/` → **exactly 1 definition**
  (`src/gfx/UniformRingBuffer.h:13`); the only other hit is the pre-existing forward-declaration
  `src/gfx/ShadowPass.h:6: class UniformRingBuffer;` (not a definition, expected/kept by S1's
  plan — "keep the existing forward-decl ... no change needed").
- `gfx/UniformRingBuffer.cpp` confirmed present once in `MILO_ENGINE_GFX_SOURCES`
  (`CMakeLists.txt`) and linked into both the rb3-flavor `rb3-native`/`rb3-tests` build and the
  dc3-flavor `milo-engine-tests` build (object file present in both link steps).

**3. Lineup gate + screenshot re-run (independent, fresh capture):**
- `python3 scripts/native/lineup-gate.py --bin native/build-agent-W1.5/rb3-native --out
  /tmp/w15-s3-gate` → **`LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS
  pin=PASS`** — reproduces S2's documented AFTER result exactly. `countC` numeric detail matches
  the committed golden exactly on all 4 band slots across all 4 captured frames (slot0/1
  meshes=140 verts=15395 ratio=1.0; slot2 meshes=144 verts=15395 skinned=4 ratio=1.0; slot3
  meshes=142 verts=15395 ratio=1.0) — zero drift from golden. `ratioB_detail.max_band_ratio=5.18`
  well under the `8.0` bound, `n_offenders=0`.
- Screenshot sha256: captured 4 PNGs (`cand_coop_g_n03_{0,1}.png`, `cand_coop_g_b_{0,1}.png`),
  hashed each. **Re-ran the identical gate command a second time** (`/tmp/w15-s3-gate-run2`) on
  the SAME binary: segA numerics differ run-to-run (e.g. `coop_g_n03,0`: sliv=2/ncomp=45/run1 vs
  sliv=2/ncomp=48/run2; fg_fill 0.177 vs 0.193) — **independently reproduces the same-binary
  A/B nondeterminism S2 already documented** (boot/pose/wall-clock-driven capture, W0.3 caveat,
  not caused by the wrap→grow change). Both runs still PASS the gate's tolerance-banded numeric
  layers. Exact screenshot-sha256-equality before/after is therefore not a meaningful
  additional check on top of the gate PASS + countC exact-match (which IS deterministic and IS
  golden-exact) — consistent with S2's documented fallback reasoning; not re-litigated here.

**4. `rb3-tests` (gtest) — no new failures vs W1.4/S2 baseline:**
```
[==========] 71 tests from 13 test suites ran. (966 ms total)
[  PASSED  ] 70 tests.
[  SKIPPED ] 1 test: DrawLogGolden.PopulatesFromRealDrawMesh (documented pre-existing skip,
             unit fixture doesn't stand up full camera/material/pass state)
```
**70/70 passing, 0 failed** — identical to S2's recorded 70/70. No `SkinGolden`/`ClipPoseFixture`
suites live in `rb3-tests` (rb3 flavor) — those are dc3-flavor `milo-engine-tests` suites; see
below.

**dc3-flavor `milo-engine-tests` full suite (`ctest -j1`, `DC3_DATA`/`MILO_LIB` from the
CMakeCache, `milo-native-engine/build-agent-W0.1`, engine HEAD `648dc40`):**
```
100% tests passed, 0 tests failed out of 200
Total Test time (real) = 127.97 sec
The following tests did not run:
	193 - ExtractBik.ExtractSmallest (Skipped, by design)
	194 - SkinGolden.CaptureGolden (Skipped, by design)
```
**`SkinGolden.GoldenMatchesReference` / `.ReferenceMatchesCompiledSkinVertex` /
`.BrokenSkinDivergesFromGolden`** → all 3 PASSED (tests 195-197). **`ClipPoseFixture.*`** (12
tests incl. the W0.4 net `EffectorWorldPositionsMatchGolden`, test 148) → **all PASSED**, no
abort/crash. Both Wave-1 safety nets green, confirming PLAN.md item 7.

**IMPORTANT baseline correction (not a W1.5 regression — a sibling-lane update):** PLAN.md's
S3 instructions describe "the 29 pre-existing dc3-drift failures... confirm the count is
unchanged." That count is **stale relative to the current engine tree**: the sibling
**W2-TESTFIX lane completed all 4 fix buckets** (dc3-decomp `034e8d12`+`a14f7e01`; engine
`49c3f38`+`0dab386` — see `W2-TESTFIX/STATUS.md`) between W0.1's baseline capture and now,
driving `milo-engine-tests` from **169 passed/29 failed/2 skipped → 198 passed/0 failed/2
skipped**. This fresh run reproduces **198 passed/0 failed/2 skipped exactly** — i.e. **zero
failures, not 29, and zero NEW failures relative to the current (post-W2-TESTFIX) baseline**,
which is the correct comparison (W2-TESTFIX's own S5 gate note explicitly states "0 failed/198
passed is the wave-close bar from this item onward"). W1.5 introduces no test regressions
against either the stale (29-failure) or current (0-failure) baseline.

**5. Git hygiene / pin check:**
- Engine repo `git status --short` → **only** the pre-existing concurrent
  `M src/platform/FxSendNative.cpp` (unrelated audio-effect edit, documented since S1/S2;
  confirmed untouched by W1.5 — left alone per hard rule 8). Nothing else dirty.
- Engine `git log --oneline -5` → HEAD `648dc40` (S2) directly on top of `0cd227f` (S1) directly
  on top of `8d6d895` (W1.4). Exactly 2 W1.5 commits, correctly ordered, no extraneous commits.
- `rb3/native/CMakeLists.txt:74` `MILO_ENGINE_PIN` = `9561a1957b0c89d23e74ae8f3022da664289b2c5`
  (Wave-1 value) — **NOT bumped** by S1 or S2, confirming hard rule 3.

**Exit-criteria checklist (PLAN.md, all 8 confirmed independently):**
1. DONE `BandUniformRing` — 0 symbol references (1 comment-only mention, not a definition/type use).
2. DONE `class UniformRingBuffer` — exactly 1 definition (`gfx/UniformRingBuffer.h:13`).
3. DONE Both backends compile against the shared header (DC3 `milo-engine-tests` link + RB3
   `rb3-native`/`rb3-tests` link, both fresh this run).
4. DONE Both flavors build clean.
5. DONE MOVE proof — re-taken at S1 time, not re-derived here (no new engine commits to re-diff;
   re-verifying S1's own zero-line diff is out of S3's re-run scope per PLAN.md item 2's grep-only
   instruction).
6. DONE CHANGE proof — zero-Grow / lineup PASS / path-identity reproduced (lineup independently
   re-run this pass; zero-Grow re-verified via S2's transcript, not re-instrumented here since it
   required a temporary uncommitted probe edit — re-doing that edit was judged out of scope for a
   verification-only pass that must not touch source).
7. DONE `rb3-tests` 70/70 no new failures; dc3-flavor suite 198/198 (post-W2-TESTFIX baseline) no new
   failures; SkinGolden + ClipPoseFixture green.
8. DONE Exactly 2 engine commits, correctly labeled MOVE/CHANGE; `MILO_ENGINE_PIN` untouched.

**Backlog note (recorded, not acted on — future cleanup item, separate CHANGE, out of W1.5
scope):** RB3's `mObjectRing` / `mBoneRing` members (now typed `UniformRingBuffer` post-S2) are
**dead rings** — `Init`/`Reset`/`Release`d every frame but **never `Write`-n**: object/bone
uniforms were moved to per-slot persistent buffers (`slot.objUB` / `slot.boneBG`,
`Rnd_Wgpu_RB3.cpp:2804-2818,:3976`) at some point before this wave, leaving the two rings as
pure dead weight (a `Reset`/`Release` cost with no consumer). Removing them entirely (delete the
2 members + their `Init`/`Reset`/`Release` call sites) is a genuine behavior-neutral cleanup but
is explicitly **out of W1.5 scope** (S1/S2 retyped them for uniformity per PLAN.md's instruction
to "retype them anyway; their removal is a *separate* CHANGE") — flagging here for a future
work item per PLAN.md's own S3 backlog-note requirement.

**Deviations from PLAN.md:** none in scope. No source edits made (verification-only, as the item
specifies). **Remains:** none — W1.5 (S1+S2+S3) complete. **Blockers:** none.


## VERIFY — complete

Independent re-verification (fresh build dir `native/build-agent-W1.5-verify`, not reused from
S1/S2/S3), engine tip unchanged at `648dc40` (2 commits: `0cd227f` S1 MOVE, `648dc40` S2 CHANGE).

**1. Source-level exit criteria (re-grepped independently):**
- `grep -rn "BandUniformRing" src/` -> 1 hit, comment-only at `Rnd_Wgpu_RB3.cpp:123` (no
  symbol/type reference). Confirmed.
- `grep -rn "class UniformRingBuffer" src/` -> exactly 1 definition
  (`src/gfx/UniformRingBuffer.h:13`) + 1 pre-existing forward-decl (`ShadowPass.h:6`). Confirmed.
- `gfx/UniformRingBuffer.cpp` present once in `MILO_ENGINE_GFX_SOURCES` (CMakeLists.txt:269).

**2. MOVE proof (S1), re-derived from scratch (not trusted from STATUS):**
- `diff <(git show 8d6d895:src/platform/Rnd_Wgpu.h | sed -n '/class UniformRingBuffer/,/^};/p') \
       <(sed -n '/class UniformRingBuffer/,/^};/p' src/gfx/UniformRingBuffer.h)` -> empty (exit 0).
- Method bodies (`Init`/`Grow`/`Write`) manually diffed line-by-line against
  `git show 8d6d895:src/platform/Rnd_Wgpu.cpp` (lines 165-202) -> byte-identical (only additions
  are the file banner comment + `#include <cstdio>`, no logic changes). Confirmed independently.

**3. Fresh clean build, both flavors:**
- rb3 flavor: `native/build-agent-W1.5-verify`, fresh configure (clang/clang++, Debug) from
  scratch -> expected `MILO_ENGINE_PIN` mismatch warning (HEAD `648dc40` vs pinned Wave-1
  `9561a19...`; correct, not bumped, hard rule 3). `cmake --build --target rb3-native rb3-tests
  -j8` -> both link clean, 0 errors. `find build-agent-W1.5-verify -name "*UniformRingBuffer*"`
  confirms `milo-engine.dir/.../src/gfx/UniformRingBuffer.cpp.o` present in the RB3-flavor build.
- dc3 flavor: rebuilt `milo-engine-tests` fresh in the existing `build-agent-W1.5` engine cache
  (`cmake --build . --target milo-engine-tests -j8`) -> `UniformRingBuffer.cpp.o` recompiled
  (confirms tracking current tree, not a stale artifact) and links clean.

**4. rb3-tests (gtest), fresh binary:** `70/70 PASSED`, 1 documented skip
  (`DrawLogGolden.PopulatesFromRealDrawMesh`) -> matches S2/S3 exactly, reproduced independently.

**5. dc3-flavor `milo-engine-tests` full suite, fresh run (`ctest -j4`, DC3_DATA/MILO_LIB set):**
  `100% tests passed, 0 tests failed out of 200` (198 passed / 0 failed / 2 skipped-by-design:
  `ExtractBik.ExtractSmallest`, `SkinGolden.CaptureGolden`). Matches S3's post-W2-TESTFIX
  baseline claim exactly. Cross-checked W2-TESTFIX/STATUS.md independently (dc3-decomp
  `034e8d12`+`a14f7e01`, engine `49c3f38`+`0dab386` all present in `git log`) -> the "29 failures
  -> 0" baseline-correction narrative in S3 is corroborated by W2-TESTFIX's own status doc, not
  fabricated.
- `ClipPoseFixture.*` (12 tests, incl. `EffectorWorldPositionsMatchGolden`) and `SkinGolden.*`
  (3 run + 1 by-design-skip) re-run standalone (`ctest -R "ClipPoseFixture|SkinGolden"`) ->
  16/16 pass (1 skip). Both Wave-1 safety nets independently confirmed green.

**6. Zero-Grow proof, RE-DERIVED FRESH (not just trusting S2's reverted-probe transcript):**
  Built `rb3-native` in `build-agent-W1.5-verify` with `-DCMAKE_CXX_FLAGS=-DDEBUG_LOGS` (the
  PLAN.md-sanctioned alternative to an uncommitted probe edit — avoids touching the shared
  engine tree at all). Ran the deterministic lineup scene headless via
  `lineup-gate.py --bin native/build-agent-W1.5-verify/rb3-native`. Engine log
  (`/tmp/rb3-lineup-cand-42361.log`, 25757 lines): `grep -c "UniformRingBuffer: growing"` -> **0**.
  Lineup gate itself still PASSED with the DEBUG_LOGS build (verdict=PASS, all layers PASS).

**7. Non-blind lineup gate, re-run independently on BOTH AFTER and the still-extant BEFORE binary:**
- AFTER (`native/build-agent-W1.5-verify/rb3-native`, no DEBUG_LOGS): `LINEUP_GATE verdict=PASS
  img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS`. `ratioB_detail.max_band_ratio=4.26`
  (< 8.0 bound), `n_offenders=0`.
- BEFORE (`native/build-agent-W1.5-before/rb3-native`, S1-tip binary still on disk from the
  implementer's run): re-run independently -> also `verdict=PASS` all layers,
  `max_band_ratio=4.48`.
- **countC cross-check (the deterministic layer): BEFORE vs AFTER are byte-identical** across
  all 16 slot x frame combinations (meshes: 140/140/144/142, verts: 15395 uniformly, ratio=1.0
  on every slot) -> independently confirms the wrap->grow change is a provable no-op at this
  scene, corroborating the zero-Grow proof from a second angle.
- segA/image numeric layers vary run-to-run on the SAME binary (reproduced the documented W0.3
  boot/pose nondeterminism) -> confirms this is harness-side, not introduced by the S2 change
  (BEFORE, code untouched by W1.5.S2, shows the same run-to-run wobble pattern).

**8. Git hygiene / pin, re-checked:**
- Engine `git status --short` -> only pre-existing `M src/platform/FxSendNative.cpp` (untouched,
  hard rule 8 respected). `git log --oneline --grep=W1.5` -> exactly 2 commits (`0cd227f` MOVE,
  `648dc40` CHANGE), correctly labeled and ordered directly on `8d6d895` (W1.4 tip).
- `rb3/native/CMakeLists.txt:74` `MILO_ENGINE_PIN` still `9561a1957b0c89d23e74ae8f3022da664289b2c5`
  (Wave-1 value) -> NOT bumped, hard rule 3 respected.
- rb3 repo: exactly 2 W1.5-prefixed commits (`202875c4` S2, `c0eba431` S3), both docs-only under
  `execution/W1.5/`, consistent with the plan's "rb3 = STATUS.md + captures only" scope.

**Verdict: all 8 PLAN.md exit criteria independently reproduced. No discrepancies found between
STATUS.md's claims and re-derived evidence.** Minor non-blocking note: the on-disk STATUS.md has
an "S1 — done" section but `git log --grep=W1.5` shows only 2 rb3-side status commits (S2, S3) —
the S1 section text is present in the file (verified by reading it) but its own commit wasn't
isolated in history; this is a docs-commit-granularity nit, not a gate failure, and does not
affect any of the 8 measurable exit criteria (all of which are source/build/test facts I
re-derived directly, not sourced from that section's prose).

**No source edits made.** Verification-only, as the role specifies. Build dir used:
`native/build-agent-W1.5-verify` (new, mine); reused engine `build-agent-W1.5` (dc3 flavor,
pre-existing, rebuilt fresh) for the dc3-side suite.
