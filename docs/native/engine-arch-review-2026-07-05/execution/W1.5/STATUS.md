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
