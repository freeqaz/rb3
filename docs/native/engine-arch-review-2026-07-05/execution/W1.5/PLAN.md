# W1.5 — Dedupe `BandUniformRing` vs `UniformRingBuffer`

**Item:** REFACTOR_PLAN W1.5; lane doc `01-renderer-core.md` §5. Lane A, sequenced
`W1.2 → W1.3 → W1.4 → **W1.5** → W1.7 → W1.6` on `Rnd_Wgpu_RB3.cpp`. Assume W1.2–W1.4
commits are IN (engine tip verified `8d6d895`, "W1.4: move 2D quad pipeline …").

**Repos:** engine = `/home/free/code/milohax/milo-native-engine` (all code commits here, own
git repo, prefix `W1.5:`). rb3 = `/home/free/code/milohax/rb3` (STATUS.md + captures only).

---

## Objective

The engine has **two byte-for-byte-near-identical bump-allocated uniform ring buffers**:

- **`UniformRingBuffer`** — the DC3 backend's ring. Declared inline in
  `src/platform/Rnd_Wgpu.h:25-46`, defined in `src/platform/Rnd_Wgpu.cpp:165-202`.
  Also consumed by `src/gfx/ShadowPass.{h,cpp}` (`ShadowPass::Render(…, UniformRingBuffer&
  objectRing, UniformRingBuffer& boneRing, …)`, `Rnd_Wgpu.cpp:1020`).
- **`BandUniformRing`** — the RB3 backend's ring. Declared inline in
  `src/platform/Rnd_Wgpu_RB3.h:51-65`, defined in `src/platform/Rnd_Wgpu_RB3.cpp:126-145`.
  Four members: `mSceneRing / mMaterialRing / mObjectRing / mBoneRing` (`Rnd_Wgpu_RB3.h:265-268`).

Collapse them into **ONE class in a shared TU** (`src/gfx/UniformRingBuffer.{h,cpp}`, added to
the rndobj-free gfx core so **both** backend flavors compile it once). Keep the **superset**
overflow behavior and **prove RB3 output is unchanged**.

### The one real semantic difference (drives the MOVE/CHANGE split)

| aspect | `UniformRingBuffer` (DC3) | `BandUniformRing` (RB3) |
|---|---|---|
| `Init`/`Write` params | `wgpu::Device& / wgpu::Queue&` (by-ref) | by-value (ref-counted handles; ABI-compatible) |
| **overflow** | **`Grow()`** — allocate `2×`, reset offset, old buffer kept alive by Dawn refcount | **wrap** — `mOffset = 0`, **overwrites earlier in-frame writes** (comment: "defensive") |
| alignment | `kAlignment = 256` | `kAlign = 256` (same) |
| `label` null-guard | `label ? label : "UniformRing"` | assigns raw (RB3 always passes a literal → no diff) |
| extra accessor | `Capacity()` | — (additive; harmless) |
| default `mLabel` | `"UniformRing"` | `"BandRing"` (Grow-log only; no functional path) |

**Grow is the strict superset** (never loses in-frame data; wrap silently corrupts a frame on
overflow — a latent bug). So the unified class = `UniformRingBuffer` **with Grow**, and RB3
adopts it. Everything except overflow is identical, so:

- **S1 = MOVE commit:** relocate `UniformRingBuffer` verbatim into the shared TU. DC3 behavior
  byte-identical (pure relocation).
- **S2 = CHANGE commit:** RB3 drops `BandUniformRing`, uses the shared `UniformRingBuffer`
  (`wrap → grow`). This is the *only* behavior change; gate it with screenshot + drawlog +
  lineup + a **zero-Grow proof** at a deterministic scene.

### Why the CHANGE is expected to be a no-op for RB3 (must be PROVEN, not assumed)

RB3's rings are sized generously and reset every frame:
`mSceneRing 64 KiB` (`Rnd_Wgpu_RB3.cpp:925`) = 256 aligned writes/frame max; `mMaterialRing
256 KiB` (`:926`) = 1024/frame. **`mObjectRing`/`mBoneRing` are Init'd/Reset/Released but NEVER
`Write`-n** in RB3 (object/bone uniforms moved to per-slot persistent buffers `slot.objUB` /
`slot.boneBG`, `Rnd_Wgpu_RB3.cpp:2804-2818`, `:3976`) — grep confirms zero `mObjectRing.Write`/
`mBoneRing.Write`. RB3 has shipped on `wrap` without overflow, so at any normal scene **neither
`wrap` nor `Grow` ever fires** → the two behaviors are indistinguishable. The S2 gate makes this
concrete by instrumenting `Grow` to count and asserting **0 grows** at the test scene.

### Faithful-reference citations (read before touching code — line numbers are CURRENT as of tip `8d6d895`, re-grep to confirm)

- DC3 ring decl: `src/platform/Rnd_Wgpu.h:25-46`. DC3 ring def: `src/platform/Rnd_Wgpu.cpp:161-202`.
- RB3 ring decl: `src/platform/Rnd_Wgpu_RB3.h:50-65`. RB3 ring def: `src/platform/Rnd_Wgpu_RB3.cpp:123-145`.
- RB3 ring members: `Rnd_Wgpu_RB3.h:265-268`. RB3 Init/Reset/Release/Write call sites:
  `Rnd_Wgpu_RB3.cpp:925-928` (Init), `:981-984` (Release), `:1453,:1803` (Write),
  `:1456,:1803` (`.Buffer()`), `:1535-1538` (Reset).
- `GpuDevice::Device()`/`Queue()` return `wgpu::Device& / wgpu::Queue&` (lvalue refs) —
  `src/gfx/GpuDevice.h:57-58` → by-ref params bind at every RB3 call site with NO change.
- ShadowPass consumer (DC3-only; not in RB3 flavor): `src/gfx/ShadowPass.h:6,14-15`,
  `src/gfx/ShadowPass.cpp:254-255`, called `Rnd_Wgpu.cpp:1020`.
- CMake: gfx-core source list `CMakeLists.txt:261-272` (`MILO_ENGINE_GFX_SOURCES`); DC3 flavor
  TUs `:303-312`; RB3 flavor TUs `:320-328`. The gfx core is compiled for BOTH flavors, so a new
  `src/gfx/UniformRingBuffer.cpp` there is linked into both.

---

## Subtasks

### W1.5.S1 — Extract shared `gfx/UniformRingBuffer.{h,cpp}` from DC3 (MOVE)  · model: opus

**Goal:** create the single shared class by relocating DC3's `UniformRingBuffer` **verbatim**
into a new gfx-core TU; make DC3 (`Rnd_Wgpu.*`, `ShadowPass.*`) consume it. **Zero behavior
change** for DC3 — a pure relocation. Does NOT touch any RB3 file.

**Exact files:**
- NEW `src/platform/../gfx/UniformRingBuffer.h`, NEW `src/gfx/UniformRingBuffer.cpp`
- `src/platform/Rnd_Wgpu.h` (drop inline class, add include)
- `src/platform/Rnd_Wgpu.cpp` (drop out-of-line defs)
- `src/gfx/ShadowPass.cpp` (add include so its full def is available), `src/gfx/ShadowPass.h`
  (keep the existing forward-decl `class UniformRingBuffer;` — no change needed; optionally
  replace with the include)
- `CMakeLists.txt` (add `src/gfx/UniformRingBuffer.cpp` to `MILO_ENGINE_GFX_SOURCES`, ~line 269)

**Approach (step-by-step):**
1. Re-grep to confirm current line ranges (they shift): `grep -n "UniformRingBuffer" src/platform/Rnd_Wgpu.{h,cpp} src/gfx/ShadowPass.*`.
2. Create `src/gfx/UniformRingBuffer.h`: include-guard/`#pragma once`, `#include <webgpu/webgpu_cpp.h>`,
   then the class body **copied character-for-character** from `Rnd_Wgpu.h:25-46` (public
   `Init/Reset/Release/Write/Buffer/Capacity`, private `Grow`, `kAlignment=256`, members).
   No rndobj/ includes — this stays a Tier-1 (rndobj-free) header.
3. Create `src/gfx/UniformRingBuffer.cpp`: `#include "gfx/UniformRingBuffer.h"` + the three
   method bodies (`Init`, `Grow`, `Write`) **copied verbatim** from `Rnd_Wgpu.cpp:165-202`,
   including the `#ifdef DEBUG_LOGS` fprintf in `Grow`. No logic edits.
4. In `Rnd_Wgpu.h`: delete the inline `class UniformRingBuffer { … };` block; add
   `#include "gfx/UniformRingBuffer.h"` near the other gfx includes (it sits next to
   `gfx/UniformStructs.h`).
5. In `Rnd_Wgpu.cpp`: delete the `UniformRingBuffer::Init/Grow/Write` out-of-line defs
   (the `// UniformRingBuffer` section). Leave the banner comment or remove — cosmetic only.
6. In `ShadowPass.cpp`: add `#include "gfx/UniformRingBuffer.h"` at the top (it calls
   `.Write()/.Buffer()` on the rings, so it needs the full def; today it gets it transitively —
   make it explicit so the relocation can't break its TU). Leave `ShadowPass.h`'s forward-decl.
7. CMake: add `src/gfx/UniformRingBuffer.cpp` to `MILO_ENGINE_GFX_SOURCES` (`CMakeLists.txt`
   ~line 269, alongside `GpuResourceRegistry.cpp`).
8. Commit (engine, under `flock /tmp/milo-engine-git.lock`, stage ONLY these files):
   `W1.5: extract UniformRingBuffer into shared gfx TU (MOVE, byte-identical for DC3)`.

**Verification (S1):**
- MOVE proof — re-derive a **zero-line diff of the moved body** (like W1.1's verifier did):
  `diff <(git show 8d6d895:src/platform/Rnd_Wgpu.h | sed -n '/class UniformRingBuffer/,/^};/p') \
       <(sed -n '/class UniformRingBuffer/,/^};/p' src/gfx/UniformRingBuffer.h)` → empty (modulo
  the include-guard lines). Same for the three method bodies vs `Rnd_Wgpu.cpp@8d6d895`.
- Build the **dc3 flavor** so the moved code + ShadowPass compile & link:
  configure/build `milo-engine-tests` in the engine's own `build-agent-W1.5` dir per the W0.1
  recipe (initial-cache from a working `build-tests` cache to get `context: ON` +
  `dc3_runtime_sources.cmake`; see `W0.1/STATUS.md:53-98`). `cmake --build … --target
  milo-engine-tests` must link. `grep -rn "class UniformRingBuffer" src/` → exactly ONE
  definition (in `gfx/UniformRingBuffer.h`).
- Build the **rb3 flavor** (`native/build-agent-W1.5`, target `rb3-native`) to confirm the new
  gfx-core TU compiles in RB3's build too (RB3 doesn't use it yet, but it's now linked) — must
  build clean.

---

### W1.5.S2 — RB3 adopts shared `UniformRingBuffer`; drop `BandUniformRing` (CHANGE: wrap→grow)  · model: opus

**Goal:** delete `BandUniformRing`; retype RB3's four rings to the shared `UniformRingBuffer`.
This unifies overflow to `Grow`. Prove RB3 output byte-unchanged at a deterministic scene.

**Exact files:**
- `src/platform/Rnd_Wgpu_RB3.h` (remove `BandUniformRing` class `:50-65`; add
  `#include "gfx/UniformRingBuffer.h"`; retype the 4 members `:265-268`)
- `src/platform/Rnd_Wgpu_RB3.cpp` (remove `BandUniformRing::Init` + `::Write` defs `:123-145`
  and their banner)
- STATUS.md capture evidence (rb3 repo, docs only)

**Approach:**
1. Re-grep current ranges: `grep -n "BandUniformRing\|mSceneRing\|mMaterialRing\|mObjectRing\|mBoneRing" src/platform/Rnd_Wgpu_RB3.{h,cpp}`.
2. `Rnd_Wgpu_RB3.h`: add `#include "gfx/UniformRingBuffer.h"` (near the other `gfx/*` includes,
   `:25-30`). Delete the `// Simple bump-allocated uniform ring …` comment + `class
   BandUniformRing { … };` block. Change the four member declarations from `BandUniformRing`
   to `UniformRingBuffer`.
3. `Rnd_Wgpu_RB3.cpp`: delete the `// BandUniformRing` banner + `BandUniformRing::Init` and
   `BandUniformRing::Write` bodies (`:123-145`). Leave all call sites untouched — they compile
   unchanged: `Init(mGpu.Device(), …)` and `Write(mGpu.Queue(), …)` bind lvalue refs (verified
   `GpuDevice.h:57-58`); `Reset()/Release()/Buffer()` are identical in the shared class.
4. Build `rb3-native` (`native/build-agent-W1.5`) → must compile & link clean.
   `grep -rn "BandUniformRing" src/` → **0**.
5. Commit (engine, `flock`, stage only the 2 files):
   `W1.5: RB3 adopts shared UniformRingBuffer, drop BandUniformRing (CHANGE: overflow wrap→grow)`.

**REQUIRED before/after evidence procedure (this is a CHANGE — gate is mandatory):**
Baseline = **post-S1** `rb3-native` (S1 does not touch RB3, so post-S1 == pre-S2 RB3 behavior).
Capture BEFORE (at S1 tip) and AFTER (at S2 tip), same deterministic scene, all three layers:

1. **Zero-Grow proof (the definitive no-op proof).** For the AFTER build only, build with
   `DEBUG_LOGS` defined (`-DCMAKE_CXX_FLAGS=-DDEBUG_LOGS`, or temporarily make the `Grow`
   fprintf unconditional — do NOT commit that edit) so `UniformRingBuffer::Grow` prints
   `"UniformRingBuffer: growing …"`. Run the deterministic scene headless; `grep -c growing`
   the stderr → **must be 0**. Zero grows ⇒ neither the old `wrap` nor the new `Grow` overflow
   path ever executes ⇒ the change is provably inert for this scene. Record the count in STATUS.md.
2. **Non-blind lineup gate (primary deterministic visual gate).** Run
   `python3 scripts/native/patch-lineup-capture.py` then
   `python3 scripts/native/lineup-gate.py` against the committed golden
   `scripts/native/goldens/w0.5-lineup/golden.json` on BOTH the BEFORE and AFTER builds. Both
   must **PASS** with identical `segA / ratioB / countC` metrics (this gate is proven fail-red
   in `W0.5/STATUS.md`). Diff the two runs' metric JSON → identical.
3. **Screenshot hash.** Drive `rb3-native` headless (`RB3_HTTP=1`) to the same lineup scene;
   `/api/screenshot` → PNG on BEFORE and AFTER; `sha256sum` both → **identical**. (If the raw
   splash boot is used instead, note W0.3's wall-clock nondeterminism — prefer the lineup
   harness, which is pose-deterministic.)
4. **Draw-log diff (where stable).** `RB3_DRAWLOG=1` + `/api/drawlog` (or
   `scripts/native/drawlog-golden.py`) at the lineup scene on BEFORE and AFTER; diff → empty.
   If drawlog is boot-count-unstable at the chosen scene (W0.3 blocker; W0.3b in parallel may
   land a frozen-clock seam — use `RB3_REPLAY_FIXED_CLOCK` if available), fall back to the
   zero-Grow proof + lineup + screenshot as the binding evidence and note it in STATUS.md.

Record all four results (with the exact commands + hashes/counts) in STATUS.md under
`## W1.5.S2 — done`.

---

### W1.5.S3 — Independent verification sweep + STATUS finalize  · model: sonnet

**Goal:** adversarially re-run the gates from a clean build and finalize the record. No source
edits expected (this is verification-only, like the W1.4 verifier pass).

**Exact files:** STATUS.md only (rb3 repo, under `flock /tmp/rb3-docs.lock`).

**Approach / verification commands:**
1. Fresh `cmake --build native/build-agent-W1.5 --target rb3-native rb3-tests -j8` at S2 tip
   (delete `CMakeCache.txt` first if reusing, to force a clean configure) → both build clean.
2. `grep -rn "BandUniformRing" /home/free/code/milohax/milo-native-engine/src` → **0**.
   `grep -rn "class UniformRingBuffer" …/src` → exactly **1** (`gfx/UniformRingBuffer.h`).
3. Re-run the S2 lineup gate + screenshot hash independently → PASS / identical.
4. `rb3-tests` (gtest) → no NEW failures vs the W1.4 baseline. (The 29 pre-existing dc3-drift
   `milo-engine-tests` failures are out of scope — W2-TESTFIX; confirm the count is unchanged,
   don't try to fix them.) SkinGolden / ClipPoseFixture (Wave-1 nets) still green.
5. Confirm `git status --short` in the engine repo shows ONLY W1.5's files staged/committed +
   the pre-existing untouched `FxSendNative.cpp` concurrent edit (leave it — hard rule 8).
   Confirm `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` was **NOT** bumped (hard rule 3).
6. Append the final `## W1.5.S3 — done` verdict + backlog note (below) to STATUS.md.

**Backlog note to record (do NOT act on it here):** RB3's `mObjectRing` / `mBoneRing` are now
dead (Init/Reset/Release but never Write — object/bone uniforms live in per-slot buffers). Their
removal is a *separate* CHANGE, out of W1.5 scope; flag for a future cleanup item.

---

## Exit criteria (measurable)

1. `grep -rn "BandUniformRing" milo-native-engine/src` → **0 matches**.
2. `grep -rn "class UniformRingBuffer" milo-native-engine/src` → **exactly 1** (in
   `src/gfx/UniformRingBuffer.h`); its `.cpp` is listed once in `MILO_ENGINE_GFX_SOURCES`.
3. Both backends consume the shared class: DC3 (`Rnd_Wgpu.*` + `ShadowPass.*`) and RB3
   (`Rnd_Wgpu_RB3.*`, 4 members retyped) compile against `gfx/UniformRingBuffer.h`.
4. Engine builds clean in **both** flavors: `rb3-native` + `rb3-tests` (rb3 flavor) and
   `milo-engine-tests` link (dc3 flavor).
5. **MOVE proof (S1):** the relocated class body + 3 method bodies diff **zero lines** against
   `Rnd_Wgpu.{h,cpp}@8d6d895` (modulo include-guard).
6. **CHANGE proof (S2):** at the deterministic lineup scene — (a) `Grow` fires **0 times**,
   (b) `lineup-gate.py` PASS with identical `segA/ratioB/countC` before vs after, (c) screenshot
   `sha256` identical before vs after, (d) drawlog diff empty (or documented-unstable with the
   other three binding).
7. `rb3-tests` has **no new failures** vs the W1.4 baseline; SkinGolden + ClipPoseFixture green.
8. Exactly **two** engine commits, correctly labeled: S1 `W1.5: … (MOVE, byte-identical for
   DC3)`, S2 `W1.5: … (CHANGE: overflow wrap→grow)`. `MILO_ENGINE_PIN` untouched.

---

## Risks / conflicts

- **Lane A serialization on `Rnd_Wgpu_RB3.cpp`.** W1.2→W1.3→W1.4 are IN (tip `8d6d895`). W1.7→W1.6
  run AFTER W1.5. W1.5's RB3 edits are tiny and localized (remove one class + retype 4 members +
  remove 2 method defs near `:123-145` / `:265-268`), well clear of the DrawMesh body the later
  items touch. No concurrent editor of this file during W1.5.
- **CHANGE risk = overflow-only.** The wrap→grow swap changes behavior *only* if a ring
  overflows in a frame. The zero-Grow proof + lineup gate make the no-op concrete. If a scene
  ever DID overflow, `wrap` (old) would corrupt and `Grow` (new) would fix it → the gate would
  show a difference and we'd stop and escalate. RB3 has shipped on wrap, so overflow is not
  expected in normal scenes.
- **Dead rings.** `mObjectRing`/`mBoneRing` are never written in RB3. Retype them anyway (they're
  members); their removal is explicitly OUT of scope (separate CHANGE) — see S3 backlog note.
- **DC3 not exercised at runtime here.** DC3's move correctness rests on the byte-identical
  relocation proof + `milo-engine-tests` link (S1) — same discipline W1.1's verifier used. We do
  not run DC3 gameplay in this wave.
- **Parallel lanes:** W0.3b (drawlog frozen-clock seam) may land mid-wave — if it does, use
  `RB3_REPLAY_FIXED_CLOCK` to make the drawlog layer binding; otherwise it's advisory and the
  other three layers gate. W2-TESTFIX owns the 29 pre-existing dc3-drift test failures — do NOT
  touch them; only confirm the count is unchanged.
- **Expected CMake warning:** after committing engine changes, `rb3/native/CMakeLists.txt`'s pin
  check (`:74-84`) emits a HEAD≠`MILO_ENGINE_PIN` warning. This is EXPECTED and correct — the
  coordinator bumps the pin once at wave end (hard rule 3). Do not bump it.
- **Git hygiene:** all add/commit under `flock /tmp/milo-engine-git.lock`; stage only your files
  (never `-A`/`-a`); never `git reset/rebase/checkout--/restore` on the shared tree (hard rule 7);
  never revert a sibling lane's line (hard rule 8). No new runtime env flag is introduced, so no
  `NativeCompatFlags` registration is needed.
