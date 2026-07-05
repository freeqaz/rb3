# W0.3 — Per-draw state-log ring + draw-log golden test — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, under `flock /tmp/rb3-docs.lock`, with commit SHAs + blockers.

## W0.3.S1 — done

**Engine commit:** `9561a19` (milo-native-engine repo) — `W0.3: per-draw state-log ring + JSON dump + debug accessor (additive, inert when RB3_DRAWLOG off)`. `MILO_ENGINE_PIN` untouched (coordinator bumps per wave).

**What landed (all ADDITIVE):**
- `src/platform/RB3DrawLogDebug.h` (NEW) — defines `struct RB3DrawRecord` (the shared data contract: pipelineHash, blend/zMode/layout/flags/targetFormat, idx/tri/vert counts, meshNameHash, world[16] column-major, four opaque bind-group `const void*` tokens) + declares the three debug accessors `RB3DebugGetDrawLog()` / `RB3DebugSetDrawLogEnabled(bool)` / `RB3DebugDrawLogEnabled()`. Mirrors `RB3TexSharpenDebug.h`.
- `src/platform/Rnd_Wgpu_RB3.h` — `#include "platform/RB3DrawLogDebug.h"`; added `std::vector<RB3DrawRecord> mDrawLog;` + `bool mDrawLogForced` next to `mHaloDraws` (public section); decls `DrawLogOn()`, `RecordDrawLog(...)`, `DumpDrawLog()`.
- `src/platform/Rnd_Wgpu_RB3.cpp` — `DrawLogOn()` (cached-`static int` getenv RB3_DRAWLOG OR mDrawLogForced); `RecordDrawLog()` (reserve(512) lazy, POD push, FNV-1a name hash); `DumpDrawLog()` (per-stream dense-id `unordered_map<const void*,int>` + JSON writer, `%.6g` floats, hex hashes, guarded on `RB3_DRAWLOG_DUMP` path); emit call at the single main-mesh draw site (right after `mDrawnTris += nf;`); `mDrawLog.clear()` in `BeginFrame` next to `mHaloDraws.clear()`; `if (DrawLogOn()) DumpDrawLog();` at the `EndFrame` tail; the three debug free-function impls (operate on global `gBandRnd`; members are public so no friend needed).

**Re-grep note:** W1.1 had NOT landed at S1 time (engine HEAD was `7a490f2` W0.4). Draw site confirmed at `mPass.DrawIndexed(cachedIndexCount, ...)`; all record fields (`key`, `obj.world`, `mSceneBindGroup`/`matBG`/`objBG`/`boneBG`, `cachedIndexCount`, `nf`, `meshEntry.fpVerts`, `skinned`, `mesh`) verified in scope at that point.

**Build:** clang (NOT gcc — engine needs `-fms-compatibility-version`/`-fdelayed-template-parsing`). `cmake -B native/build-agent-W0.3 -S native -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` then `--target rb3-native -j8` -> `[100%] Built target rb3-native`, 0 errors.

**Verification:**
- **Populates-when-on (PASS):** `RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=/tmp/dl.json RB3_HTTP=1 rb3-native`, capture at splash frame -> valid JSON, `count==draws.length` (877-891 across runs), each draw has world[16], distinct per-draw `obj` dense ids, 14 distinct `pipe`, all contract fields present.
- **Inert-when-off:** exit-criterion #1 (two OFF runs -> byte-identical `/api/screenshot`) is UNVERIFIABLE on the available boot scene: the intro/splash cinematic is wall-clock/audio-driven and **non-deterministic across runs** — OFF-vs-OFF at a pinned frame (`MILO_SCREENSHOT_FRAMES=150`) already differs (2177961 vs 2173869 B), so any pixel diff is NOT attributable to this change. Inertness holds **by construction**: with `RB3_DRAWLOG` unset and no debug override, `DrawLogOn()==false` -> both `if(DrawLogOn())` branches skip entirely (no `.Get()`, no push, no dump); the only unconditional add is `mDrawLog.clear()` on an empty vector. The ON png size sits inside the OFF/OFF PNG-compression jitter band. A deterministic-scene byte-compare is deferred to S3 (which explicitly picks a run-reproducible scene).

**Deviations from PLAN:** (1) `RB3DrawRecord` is defined in `RB3DrawLogDebug.h` (not nested in `BandRnd`) so both the engine member and the test accessor reference it plainly; the header member is enabled via an `#include` from `Rnd_Wgpu_RB3.h`. (2) No `friend` decls needed — `mHaloDraws`/`mDrawLog` live in a public section (class is public from line 260). (3) Build required forcing clang (gcc is the default `c++` in this env and rejects the engine's compat flags). Pre-existing unrelated: a teardown SIGSEGV on the bounded 5-frame non-HTTP boot (occurs with RB3_DRAWLOG OFF, so not from this change).

**Remaining:** none for S1. Follows: S2 (comparator + golden gtest) and S3 (`/api/drawlog` endpoint + live capture script) build on the `RB3DrawLogDebug.h` accessors + the JSON dump format landed here.
