# W0.3 — Per-draw state-log ring + draw-log golden test

**Wave:** 1 · **Phase:** 0 (regression net, ADDITIVE) · **Planner:** Opus
**Lane refs:** `REFACTOR_PLAN.md` §Phase-1 W0.3; `06-arch-crosscut.md` §4.3 rec 2 (lines 281–287);
`01-renderer-core.md` §2b–2f (the state-leak analysis: `mSceneBindGroup` mutable member, per-draw
obj/mat/bone slots, the `a0f98ad` uniform-collapse class). **Disposition:** ADDITIVE infra — a new
per-draw record ring that is **inert (near-zero cost) when its env flag is off**, a debug accessor,
a dump-to-file path, an optional HTTP endpoint, and a new gtest. **Nothing here changes rendered
output**; every commit is purely additive (no MOVE mixed with a CHANGE — Hard Rule 1 satisfied by
construction because rendered pixels are byte-identical when the flag is off, and the new code paths
are the *only* thing each commit adds).

## Objective

The engine has **no per-draw state regression net** (`06` §4.3 finding at line 256: "No golden-image
/ SSIM / pixel-diff / per-draw-state test exists"). Every historical rendering bug in this codebase is
a per-draw or per-vertex geometry/state defect (`06` §4.3 line 267), and two of them are named
classes we MUST be able to catch mechanically:

1. **Co-location** — identical world transform across instances that should differ (the crowd/drum
   class: N band/crowd instances collapse onto one placement).
2. **Uniform / bind-group collapse** — the `a0f98ad` class (mesh-cache multi-draw uniform collapse):
   draws that should carry distinct per-object/material/bone uniforms end up sharing one bind group.

W0.3 adds a structured **per-draw record** emitted from the RB3 backend's `DrawMesh` into a ring
buffer (pipeline id, blend/zmode, scene/mat/obj/bone bind-group identity, world xfm, vert/index/tri
counts, mesh-name hash), a way to **dump it to a JSON file at frame end** and read it via an
**`/api/drawlog`** endpoint, and a **golden test** whose comparator (float-eps on xfms; exact on
counts/pipeline; equality-pattern on bind-group sharing) **catches both classes above** and is proven
to be able to **fail red**.

### Faithful-reference / measured citations (current HEAD — rebase before editing)

W1.1 (WGSL externalization) shares this engine file and runs *before* W0.3 (lane chaining
`W1.1 → W0.3`). W1.1 only moves raw-string shader literals; it does **not** touch `DrawMesh`'s draw
body, the class members, or `BeginFrame`/`EndFrame`. **All line numbers below are from HEAD
`21eae3d` and WILL drift** — anchor edits on the quoted *code*, not the numbers. Re-grep before
editing.

Engine file `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (6,696 lines at plan time):

- **The single main-mesh draw emit site** — `DrawMesh` (`void BandRnd::DrawMesh(RndMesh* mesh)` at
  `:3548`), final draw block `~:6087–6098`:
  ```cpp
      mPass.SetPipeline(pipe);
      mPass.SetBindGroup(0, mSceneBindGroup, 0, nullptr);
      mPass.SetBindGroup(1, matBG, 0, nullptr);
      mPass.SetBindGroup(2, objBG, 0, nullptr);
      mPass.SetBindGroup(3, boneBG, 0, nullptr);
      mPass.SetVertexBuffer(0, vbuf, 0, WGPU_WHOLE_SIZE);
      mPass.SetIndexBuffer(ibuf, wgpu::IndexFormat::Uint16, 0, WGPU_WHOLE_SIZE);
      mPass.DrawIndexed(cachedIndexCount, 1, 0, 0, 0);
      mDrawnMeshes++;
      mDrawnTris += nf;
  ```
  This is the ONE place a main mesh is actually rasterized (the `if (!pipe) return;` at `~:6065`
  already filters non-draws; all cull/skip early-returns above never reach here). **All record
  fields are in scope here:**
  - `key` (`PipelineKey`, built `:6013`) → hash for a stable pipeline id.
  - `mSceneBindGroup` (member), `matBG` (`:6002`), `objBG` (`:4297`), `boneBG` (`:5458`) → bind-group
    identity via `.Get()`.
  - `obj.world[16]` (`ObjectUniforms obj{}` at `:4189`, filled `MiloXfmToColMajor(mesh->WorldXfm(),
    obj.world)` at `:4270`, column-major) → the world xfm.
  - `cachedIndexCount` (`:4132` = `meshEntry.indexCount`), `nf` (`:3655` = tri count),
    `meshEntry.fpVerts` (vert count).
  - `mesh->Name()`, `skinned` (bool) → name hash + skinned flag.
- **`PipelineKey`** — `src/gfx/PipelineManager.h:34` (fields: shaderType/blend/zMode/cull/stencil/
  layout/targetFormat/sampleCount/hasDepth/alphaCut/alphaWrite/alphaToCoverage/depthBias) with an
  existing **`PipelineKeyHash` at `:59`** — reuse it for the stable, run-independent pipeline id
  (EXACT-compared).
- **`BeginFrame`** — `:1590`; per-frame reset block `~:1643–1660` (`mDrawnMeshes = 0`, ring
  `.Reset()`, `mHaloDraws.clear()`) — clear the draw-log ring here, next to `mHaloDraws.clear()`.
- **`EndFrame`** — `:1746`, closes at `~:1812` — dump-to-file hook goes at the tail (after the pass
  is finalized).
- **`mHaloDraws`** precedent — `Rnd_Wgpu_RB3.h:431–437`: a `struct HaloDraw { … }` + `std::vector<>`
  member, `reserve(16)` on first use, `clear()` in `BeginFrame`. **Mirror this exact pattern** for
  the draw-log ring member.
- **Debug-accessor precedent** — `src/platform/RB3TexSharpenDebug.h` (+ impls in `Rnd_Wgpu_RB3.cpp`
  `:845` `RB3DebugUploadTex` / `:853` `RB3DebugGetTexGpuInfo`): a small header exposing engine
  internals to `rb3-tests` without leaking the private cache. **Mirror this** for `RB3DrawLogDebug.h`.
- **Env-flag idiom** — cached `static int` + `getenv` (e.g. `sVenueLightEnabled()` `:1194`,
  `RB3_HIGHWAY_BLOOM_OFF`). W0.3's flag is **opt-IN** (default OFF): `RB3_DRAWLOG` enables recording;
  `RB3_DRAWLOG_DUMP=<path>` selects the dump file (default `drawlog.json` in cwd) and which frame.

Test-harness references:
- `rb3/native/tests/test_texsharpen.cpp:38–100` — `EnsureGpu()` = one-time headless
  `gBandRnd.InitGpu(64,64,true)` + `GTEST_SKIP()` when no device. Model for any GPU-gated case.
- `rb3/native/tests/test_helpers.h:146` `EngineTestFixture` / `.cpp:70` `EnsureEngineInit()` —
  RunBoot-style headless boot (no rendering); the comparator gtest needs neither GPU nor boot.
- `rb3/native/CMakeLists.txt:698–722` — `rb3-tests` source list + `gtest_discover_tests` (PRE_TEST).
  Add the new test `.cpp` to the list (line ~708, next to `test_wgsl_validation.cpp`).
- `rb3/native/src/rb3_http_server.cpp:270–340` — `svr->Get("/api/health"…)` /
  `svr->Get("/api/screenshot"…)` registration + the `QueueAndWait(kCmd…)` main-thread-marshal
  pattern. Model for `/api/drawlog`.
- `rb3/scripts/native/song-select-capture.py` and `song-end-test.py` — the `RB3_HTTP=1` headless
  harness pattern (poll `/api/health`, POST `/api/input`, GET a resource). Model for the golden
  capture script.

## Data contract (all three subtasks share this — define once in S1)

### Per-draw record (POD pushed on the hot path — cheap; identity is opaque)

| Field | Type | Source at emit site | Compare rule |
|---|---|---|---|
| `pipelineHash` | `uint64` | `PipelineKeyHash{}(key)` | **EXACT** |
| `blend` | `uint8` | `(int)key.blend` | EXACT (also in hash; kept for readable diffs) |
| `zMode` | `uint8` | `(int)key.zMode` | EXACT |
| `layout` | `uint8` | `(int)key.layout` (Static/Skinned) | EXACT |
| `targetFormat` | `uint32` | `(uint32)key.targetFormat` | EXACT |
| `flags` | `uint8` | bit0 hasDepth, bit1 alphaCut, bit2 alphaWrite, bit3 skinned | EXACT |
| `indexCount` | `uint32` | `cachedIndexCount` | EXACT |
| `triCount` | `uint32` | `nf` | EXACT |
| `vertCount` | `uint32` | `meshEntry.fpVerts` (≥0) | EXACT |
| `meshNameHash` | `uint64` | FNV-1a of `mesh->Name()` (empty → 0) | EXACT (**alignment key**) |
| `world[16]` | `float[16]` | `obj.world` (column-major) | **FLOAT EPS** (see below) |
| `sceneBG/matBG/objBG/boneBG` | `const void*` | `mSceneBindGroup.Get()`, `matBG.Get()`, `objBG.Get()`, `boneBG.Get()` | **SHARING PATTERN** (never value-compared; never dereferenced) |

The ring is `std::vector<RB3DrawRecord>` reserved once (e.g. `reserve(512)`), pushed only when
`RB3_DRAWLOG` is on. **Hot-path cost when off = one cached-`static int` branch then `return`** — no
allocation, no `.Get()` calls. Bind-group handles are stored as **opaque identity tokens only**
(never dereferenced), so their lifetime does not matter for comparison — but dense-ification (below)
happens at dump time, same frame, while handles are still alive.

### JSON dump format (the committed golden's on-disk shape — define in S1, consumed by S2/S3)

At dump time, for each of the four bind-group streams independently, assign **dense 0-based ids in
first-seen order across the frame** (`std::unordered_map<const void*,int>` → 0,1,2,…). This erases
raw pointers (run/host independent) while **preserving the sharing pattern**. Emit one JSON object:

```json
{ "frame": <int>, "count": <int>,
  "draws": [
    { "i":0, "name":"0x<hex>", "pipe":"0x<hex>", "blend":0, "zmode":1, "layout":0,
      "fmt":123, "hasDepth":true, "alphaCut":false, "alphaWrite":false, "skinned":false,
      "idx":36, "tris":12, "verts":24,
      "scene":0, "mat":0, "obj":0, "bone":0,
      "world":[1,0,0,0, 0,1,0,0, 0,0,1,0, 3.5,0,0,1] },
    ... ] }
```

Floats: print with `%.6g` (deterministic, lossless-enough for the eps compare). The dense ids
(`scene/mat/obj/bone`) are what encode collapse; `world` is what encodes co-location.

### Tolerance rules (define as a shared C++ comparator in S2; Python mirror in S3)

- **Alignment:** compare golden[i] against candidate[i] positionally. Assert `count` matches EXACT
  first; if counts differ → FAIL (a dropped/extra draw is itself a regression). (`meshNameHash` is
  emitted so a future version can align-by-name under draw-order jitter, but positional is the
  default and is what the deterministic scenes below require.)
- **EXACT fields:** `pipelineHash`, `blend`, `zMode`, `layout`, `targetFormat`, `flags`,
  `indexCount`, `triCount`, `vertCount`, `meshNameHash`. Any mismatch → FAIL with the field named.
- **World xfm (per element):** pass iff `|c - g| <= max(rotEps, relEps*|g|)` for the basis elements
  (indices 0–11) and `|c - g| <= max(transEps, relEps*|g|)` for the translation column (indices
  12–14); index 15 EXACT-ish (eps 1e-6). Defaults: `rotEps=1e-4`, `transEps=1e-2` (world units),
  `relEps=1e-4`. **Co-location is caught here:** the golden holds *distinct* per-instance xfms; a
  regression that co-locates instance B onto instance A yields B.world == A.world != golden[B].world
  → element eps FAIL.
- **Bind-group sharing pattern:** for each stream (scene/mat/obj/bone) and every pair (i,j),
  `(golden[i].id == golden[j].id)` MUST equal `(cand[i].id == cand[j].id)`. **Uniform/bind-group
  collapse is caught here:** golden has distinct `obj` ids per instance; a collapse makes two
  candidate draws share one id → the pair that was `false` in golden is `true` in candidate → FAIL,
  naming the two draw indices and the stream.
- The comparator returns a structured result: `{ passed: bool, failures: [ {drawIndex, field,
  golden, candidate} ] }`, so the gtest/script prints exactly what diverged.

## Subtasks

### W0.3.S1 — Engine: draw-log record + ring + emit hook + dump + debug accessor  *(model: opus)*

**Goal:** add the per-draw record ring to the RB3 backend, emit into it from `DrawMesh`'s draw site,
clear per frame, dump to JSON at frame end, and expose it to `rb3-tests` via a debug accessor. Inert
and near-zero-cost when `RB3_DRAWLOG` is unset. Rendered output byte-identical.

**Files to touch (engine repo `../milo-native-engine`):**
- `src/platform/Rnd_Wgpu_RB3.h` — add `struct RB3DrawRecord { … }` (the POD above), a
  `std::vector<RB3DrawRecord> mDrawLog;` member near `mHaloDraws`, and a private helper decl
  `void RecordDrawLog(const PipelineKey& key, const float world[16], const void* sceneBG,
  const void* matBG, const void* objBG, const void* boneBG, uint32_t idx, uint32_t tris,
  uint32_t verts, bool skinned, const char* name);` plus `void DumpDrawLog();`.
- `src/platform/Rnd_Wgpu_RB3.cpp` — implement the flag gate, `RecordDrawLog`, `DumpDrawLog`
  (dense-id + JSON writer per the format above), the `BeginFrame` clear (next to
  `mHaloDraws.clear()`), the `EndFrame` dump hook (at the tail), and the one-line emit call at the
  `DrawMesh` draw site (right after `mDrawnTris += nf;`). Include `#include "gfx/PipelineManager.h"`
  is already present (PipelineKey used in-file).
- `src/platform/RB3DrawLogDebug.h` — NEW: mirror `RB3TexSharpenDebug.h`. Declares
  `const std::vector<RB3DrawRecord>& RB3DebugGetDrawLog();` (or a copy-out `size_t
  RB3DebugCopyDrawLog(RB3DrawRecord* out, size_t max)` to avoid header-exposing the vector) and
  `bool RB3DebugDrawLogEnabled();` / `void RB3DebugSetDrawLogEnabled(bool)` so the gtest can force
  recording on without an env var. Implement these in `Rnd_Wgpu_RB3.cpp`.

**Approach:**
1. Re-grep on current HEAD: `grep -n "mPass.DrawIndexed(cachedIndexCount" src/platform/Rnd_Wgpu_RB3.cpp`
   → confirm the single main-mesh draw site; `grep -n "mHaloDraws.clear" ` → the BeginFrame clear
   spot; find `EndFrame` end. Do NOT trust the plan's line numbers.
2. Define `RB3DrawRecord` (POD) and add `mDrawLog` + a `static int sDrawLogEnabled = -1;` gate
   (cached `getenv("RB3_DRAWLOG")`), plus a `bool mDrawLogForced=false` overridable by the debug
   setter so tests don't need env.
3. `RecordDrawLog`: early-`return` unless enabled/forced; push a POD (copy `world[16]`, the four
   `const void*` tokens, counts, flags, `FnvHash(name)`). Never call anything expensive.
4. Emit: at the draw site add
   `if (DrawLogOn()) RecordDrawLog(key, obj.world, mSceneBindGroup.Get(), matBG.Get(), objBG.Get(), boneBG.Get(), cachedIndexCount, (uint32_t)nf, (uint32_t)meshEntry.fpVerts, skinned, mesh->Name());`
   — verify each identifier is in scope at that point (they are, per citations; if W1.1's rebase
   moved anything, adjust).
5. `BeginFrame`: `mDrawLog.clear();` next to `mHaloDraws.clear();` (reserve(512) once on first use).
6. `EndFrame` tail: `if (DrawLogOn()) DumpDrawLog();` guarded so it fires only when a dump path is
   set (env `RB3_DRAWLOG_DUMP`, or unconditionally to the default file when `RB3_DRAWLOG` is on — but
   only for the configured frame index if given, else every frame overwrites the same file which is
   fine for a single-frame golden).
7. `DumpDrawLog`: build four `unordered_map<const void*,int>` for dense ids; write the JSON exactly
   as specified (`%.6g` floats, hex hashes). Use `fopen`/`fprintf` (no new deps).
8. Debug accessor impls + header.
9. Commit (engine repo, under `flock /tmp/milo-engine-git.lock`), message
   `W0.3: per-draw state-log ring + JSON dump + debug accessor (additive, inert when RB3_DRAWLOG off)`.
   Do NOT bump `MILO_ENGINE_PIN`.

**Verification:**
- Build: `cmake -B /home/free/code/milohax/rb3/native/build-agent-W0.3 -S /home/free/code/milohax/rb3/native && cmake --build /home/free/code/milohax/rb3/native/build-agent-W0.3 --target rb3-native -j8`
- Inert-when-off proof: run `RB3_HTTP=1 rb3-native` **without** `RB3_DRAWLOG`, capture a
  `/api/screenshot` PNG; run again identically → PNGs byte-identical (`cmp`). (No rendered change.)
- Populates-when-on proof: run with `RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=/tmp/dl.json`, drive to any
  frame via the harness, confirm `/tmp/dl.json` exists, is valid JSON, and `count == draws.length`,
  with plausible `world`/`idx`/`obj` fields (a menu frame has many draws).
- Append `## W0.3.S1 — done` to `STATUS.md` (under `flock /tmp/rb3-docs.lock`) with the engine SHA.

---

### W0.3.S2 — Golden test: comparator + `test_draw_log_golden.cpp` + fail-red proof  *(model: opus)*

**Goal:** the regression net itself. A shared C++ comparator implementing the tolerance rules, and a
gtest that (a) proves it PASSES an eps-jittered-but-equivalent candidate, (b) proves it FAILS a
**co-located** candidate, (c) proves it FAILS a **bind-group-collapsed** candidate, and (d)
GPU-gated, drives a small deterministic synthetic frame through `gBandRnd.DrawMesh` and asserts the
ring captured the expected distinct records (a real golden against a committed tiny synthetic file).
(b) and (c) are the **fail-red demonstration** the brief requires.

**Files to touch (rb3 repo):**
- `native/tests/drawlog_compare.h` — NEW: header-only comparator over the JSON shape (parse the JSON
  into a `struct DrawLogFrame { std::vector<DrawRec> draws; }`, then `CompareResult
  CompareDrawLogs(golden, candidate, Tolerances)` implementing EXACT / world-eps / sharing-pattern
  rules; returns structured failures). Use a tiny hand-rolled JSON reader or the engine's existing
  JSON util if one is linkable — prefer self-contained parsing of THIS fixed shape to avoid a dep.
- `native/tests/test_draw_log_golden.cpp` — NEW: the gtest.
- `native/tests/goldens/drawlog/synthetic_scene.json` — NEW: the committed synthetic golden (a
  4-draw scene: two crowd-like instances with DISTINCT world xfms + distinct obj ids, plus two more).
  Hand-author it (or generate once via S1's dumper and commit).
- `native/CMakeLists.txt` — add `test_draw_log_golden.cpp` to the `rb3-tests` source list
  (line ~708). **See Risks: coordinate this one-line append with W0.1/W0.4.**

**Approach:**
1. Implement `drawlog_compare.h`: JSON parse of the S1 format → records; `CompareDrawLogs` per the
   tolerance rules (counts EXACT, listed fields EXACT, `world` per-element eps, four sharing-pattern
   passes). Failures carry `{drawIndex, field, goldenStr, candStr}`.
2. Write the gtest cases:
   - `MatchesGoldenWithinEps`: load `synthetic_scene.json` as both golden and candidate, perturb the
     candidate's `world` translations by `< transEps` and basis by `< rotEps` → `CompareDrawLogs`
     returns `passed=true`. (Proves tolerance is real, not zero.)
   - `CatchesCoLocation` **(fail-red)**: take the golden, overwrite draw[1].world with draw[0].world
     (co-locate the second crowd instance onto the first) → assert `passed=false` **and** a failure
     names `drawIndex=1, field=world`.
   - `CatchesBindGroupCollapse` **(fail-red)**: take the golden, set draw[1].obj = draw[0].obj (the
     `a0f98ad` collapse: two instances share one object uniform) → assert `passed=false` **and** a
     failure names the `obj` stream and the pair (0,1).
   - `CatchesDroppedDraw`: candidate with one fewer draw → `passed=false`.
   - `PopulatesFromRealDrawMesh` **(GPU-gated, `EnsureGpu()` + `GTEST_SKIP`)**: mirror
     `test_texsharpen.cpp`'s `EnsureGpu`; force `RB3DebugSetDrawLogEnabled(true)`; construct 2+
     minimal `RndMesh` instances with distinct `WorldXfm`s (reuse the mesh-build helper style from
     `test_bandpatchmesh.cpp`), run `gBandRnd.BeginFrame(cam) / DrawMesh(a) / DrawMesh(b) /
     EndFrame()` against a headless target, read `RB3DebugGetDrawLog()`, assert ≥2 records with
     **distinct** `world` translations and **distinct** `objBG` tokens. If wiring a full headless
     `DrawMesh` proves infeasible in-process (no `RndCam::sCurrent`, material setup), degrade this
     case to `GTEST_SKIP` with a clear reason and rely on S3's live capture to prove real-draw
     population — but attempt the real path first. The four comparator cases above are the
     non-negotiable fail-red core and require neither GPU nor boot.
3. Wire CMake; build `rb3-tests`; run the `DrawLogGolden*` cases.
4. Commit (rb3 repo, `flock /tmp/rb3-git.lock`), staging ONLY your new files +
   the one CMake line: `W0.3: draw-log comparator + golden gtest (fail-red on co-location + bind-group collapse)`.

**Verification:**
- Build: `cmake --build /home/free/code/milohax/rb3/native/build-agent-W0.3 --target rb3-tests -j8`
- `GTEST_FILTER='DrawLogGolden*' /home/free/code/milohax/rb3/native/build-agent-W0.3/rb3-tests`
  → all comparator cases pass; the GPU case passes or SKIPs (never fails on a headless host).
- **Fail-red audit (do + record in STATUS):** temporarily invert the `CatchesCoLocation`
  expectation to confirm the co-located candidate genuinely produces `passed=false` (the assertion
  fires), then revert. This proves the net can go red, not just green.
- Append `## W0.3.S2 — done` to `STATUS.md` with the rb3 SHA + the exact `ctest`/binary invocation.

---

### W0.3.S3 — `/api/drawlog` endpoint + live golden-capture script (integration net)  *(model: sonnet)*

**Goal:** the "boot headless to a deterministic scene, capture, diff vs committed golden" integration
net (brief item (b)), plus proof the ring populates from **real** draws. Mechanical: follow two
existing patterns (`/api/screenshot` registration + `song-select-capture.py`).

**Files to touch (rb3 repo):**
- `native/src/rb3_http_server.cpp` — register `svr->Get("/api/drawlog", …)` mirroring
  `/api/screenshot`: marshal to the main/render thread via the existing `QueueAndWait(kCmd…)`
  mechanism, have the handler call the S1 dump path (or a new `RB3DebugCopyDrawLog` → JSON) and
  return `application/json`. If a new `kCmd` enum + handler in `rb3_http_handlers.cpp` is needed,
  add it there following the `kCmdScreenshot` precedent. Keep it cheap; if marshalling proves
  involved, fall back to serving the last-dumped file written by S1's `RB3_DRAWLOG_DUMP` (still
  satisfies the "cheap" bar — a file read).
- `scripts/native/drawlog-golden.py` — NEW: boot `RB3_HTTP=1 RB3_DRAWLOG=1 rb3-native` (subprocess,
  like `song-select-capture.py`), navigate to a **deterministic scene** (prefer the boot screen or a
  freshly-entered `song_select` at a fixed frame — the most reproducible; avoid animated gameplay),
  GET `/api/drawlog`, and either (`--update`) write the committed golden or (default) diff against it
  using a Python port of the S2 tolerance rules, exiting non-zero on mismatch.
- `native/tests/goldens/drawlog/song_select.json` — NEW: the committed live golden (produced by
  `drawlog-golden.py --update` once, reviewed, committed).

**Approach:**
1. Add the endpoint (or file-serve fallback); rebuild `rb3-native`.
2. Write `drawlog-golden.py`: reuse the subprocess-boot + `/api/health` poll + `/api/input`
   navigation helpers from `song-select-capture.py`. Choose the scene that is byte-reproducible run
   to run (verify by capturing twice and diffing under the S2 tolerances — pick a scene where two
   captures already agree; if `song_select` jitters, fall back to the boot screen).
3. `--update` to write the golden; commit it after eyeballing the draw count is sane (a real scene
   has tens–hundreds of draws).
4. Mirror the S2 tolerance rules in Python (counts/pipeline EXACT, world eps, sharing pattern) so the
   default `drawlog-golden.py` invocation is a self-contained pass/fail integration check runnable in
   the full-rebuild gate.
5. Commit (rb3 repo, `flock /tmp/rb3-git.lock`), staging only your files:
   `W0.3: /api/drawlog endpoint + live draw-log golden-capture script`.

**Verification:**
- Build `rb3-native`; start `RB3_HTTP=1 RB3_DRAWLOG=1 build-agent-W0.3/rb3-native`; `curl
  localhost:<port>/api/drawlog` → valid JSON with `count>0`.
- `python3 scripts/native/drawlog-golden.py` (against the committed golden) → exit 0.
- **Fail-red audit:** hand-edit the committed golden's draw[0].world translation by `> transEps`
  and re-run → script exits non-zero naming draw 0 / world; revert the edit.
- Determinism proof: run `--update` twice into temp files; diff under tolerances → agree (records the
  chosen scene is reproducible). Note the chosen scene in STATUS.
- Append `## W0.3.S3 — done` to `STATUS.md` with the rb3 SHA, the chosen scene, and the two curl/py
  invocations.

## Exit criteria

1. **Inert when off:** with `RB3_DRAWLOG` unset, two identical `rb3-native` runs produce
   byte-identical `/api/screenshot` PNGs (rendered output unchanged; the ring adds one branch on the
   hot path). *(S1)*
2. **Populates when on:** `RB3_DRAWLOG=1` dumps a valid JSON draw log whose `count` equals its
   `draws` length and whose fields (world/idx/obj dense ids) are plausible for the scene. *(S1/S3)*
3. **Comparator catches co-location (fail-red):** the `CatchesCoLocation` gtest proves a candidate
   that duplicates one instance's world xfm onto another yields `passed=false` naming the offending
   draw + `world`; the fail-red audit confirms the assertion genuinely fires. *(S2)*
4. **Comparator catches bind-group collapse (fail-red):** the `CatchesBindGroupCollapse` gtest proves
   a candidate that shares one `obj` bind-group id across two instances yields `passed=false` naming
   the `obj` stream + the pair. *(S2)*
5. **Tolerance is real:** an eps-jittered candidate (translations `< transEps`, basis `< rotEps`)
   PASSES — the net does not false-positive on float noise. *(S2)*
6. **Integration golden green:** `scripts/native/drawlog-golden.py` boots headless to a committed
   deterministic scene, captures via `/api/drawlog`, and diffs green against the committed golden;
   its fail-red audit (perturb the golden `> transEps`) exits non-zero. *(S3)*
7. All commits are ADDITIVE (no MOVE mixed with a CHANGE); `MILO_ENGINE_PIN` untouched;
   `<KEY>:`-prefixed messages; only own files staged.

## Risks / conflicts

- **`Rnd_Wgpu_RB3.cpp` / `.h` shared with W1.1 (WGSL externalization).** W0.3 runs AFTER W1.1 in the
  same lane. W1.1 only relocates raw-string shader literals; W0.3 edits `DrawMesh`'s draw body, the
  class member list, `BeginFrame`, `EndFrame`, and adds a new header — **disjoint regions**, low
  collision. *Mitigation:* S1 MUST re-grep the draw site / clear site / `EndFrame` tail on the
  current HEAD (line numbers here are stale by design) and rebase the mental model onto the file as
  W1.1 left it. If W1.1 is still landing when S1 runs, check `git log --grep=W1.1` + STATUS and pull
  latest before editing.
- **`native/CMakeLists.txt` `rb3-tests` source-list append — shared with W0.1 and W0.4** (all three
  add one test `.cpp` line at ~708). *Mitigation:* the edit is a single self-contained line;
  `flock /tmp/rb3-git.lock`, `git pull`/rebase before commit, and if the append textually conflicts,
  re-apply the one line — never revert a sibling's line. Stage ONLY `CMakeLists.txt` + your own new
  files.
- **`native/tests/goldens/` new dir — potentially shared with W0.1/W0.4 goldens.** *Mitigation:* W0.3
  writes only under `native/tests/goldens/drawlog/`; no path overlap with a skin/bone golden.
- **`rb3_http_server.cpp` / `rb3_http_handlers.cpp` — could be touched by other harness work.**
  *Mitigation:* S3 adds a self-contained `Get("/api/drawlog", …)` registration + (if needed) one new
  `kCmd` enum value; append at the end of the registration block; flock + rebase; keep the diff to
  the endpoint only.
- **W0.6 flag registry.** W0.3 introduces `RB3_DRAWLOG` / `RB3_DRAWLOG_DUMP`. *Mitigation
  (non-blocking):* after S1 lands, note the flag in the W0.6 `NativeCompatFlags` sidecar if the
  registry is live; not required for W0.3 to be complete (it is a diagnostic opt-in, not a
  default-ON workaround).
- **Headless in-process `DrawMesh` may be infeasible** (needs `RndCam::sCurrent`, material state).
  *Mitigation:* S2's GPU-populate case degrades to `GTEST_SKIP`; the four comparator fail-red cases
  (no GPU, no boot) are the non-negotiable core, and S3's live capture proves real-draw population
  independently.
