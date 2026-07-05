# W0.3b — Frozen-sim-clock seam + stable draw-log integration golden — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask,
written under `flock /tmp/rb3-docs.lock`, with commit SHAs + blockers + the
required byte-identical / determinism evidence.

_(No subtask entries yet — planning complete, implementation pending.)_

## W0.3b.S1 — done

**rb3 commit:** `352d19ef` — `W0.3b: trace-free RB3FixedClockActive()/RB3FixedClockDt() accessors + ledger regen`.
**engine commit:** `834954b` (milo-native-engine repo) — `W0.3b: register RB3_FIXED_CLOCK + RB3_FIXED_CLOCK_DT_MS (feature) + orphaned RB3_DRAWLOG* (probe) in NativeCompat registry`. `MILO_ENGINE_PIN` UNTOUCHED (Hard Rule 3; coordinator bumps per wave — configure-time HEAD!=pin WARNING is expected/correct).

### What landed
- `native/src/rb3_replay.h` — declared `bool RB3FixedClockActive()` + `float RB3FixedClockDt()` under the existing `#ifdef HX_NATIVE` block, with a doc-comment header matching the file style (trace-free semantics, default 1/60s dt, `RB3_FIXED_CLOCK_DT_MS` ms override incl. 0.0 = true freeze).
- `native/src/rb3_replay.cpp` — implemented both after `RB3ReplayFixedClock()`. NEW independent file-static cache (`gFixedClockActive` / `gFixedClockDt`) — does NOT reuse `gReplay.fixedClock` (semantics differ: trace-free vs trace-gated). Presence rule EXACTLY matches `RB3ReplayFixedClock` (`v && *v && strcmp(v,"0")!=0`). Dt: default `1.0f/60.0f`; `RB3_FIXED_CLOCK_DT_MS` parsed via `strtod`, honoured when finite & `>=0`, `ms/1000.0f`. Web mirrors the `EM_ASM_INT` pattern (`window.__rb3FixedClock`) + `EM_ASM_DOUBLE` (`window.__rb3FixedClockDtMs`) so `rb3-web` links (browser-run deferred).
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — added `RB3_FIXED_CLOCK` + `RB3_FIXED_CLOCK_DT_MS` (`class: feature`, owner `session-telemetry/determinism`, default off; presence/value read modes). **DEVIATION (see below):** also registered `RB3_DRAWLOG` + `RB3_DRAWLOG_DUMP` (`class: probe`).
- `NativeCompatFlags.gen.inc` (engine) + `NATIVE_COMPAT_LEDGER.md` (rb3 docs) — REGENERATED via `native_compat_census.py gen` (not hand-edited).

### Verification (evidence)
- `python3 scripts/analysis/native_compat_census.py --selftest` -> `selftest: 14/14 PASS`.
- `python3 scripts/analysis/native_compat_census.py check` -> **EXIT 0**: `check: OK — 229 scanned flags all present in registry, regen clean.` (Was EXIT 1 before: 4 unregistered getenvs — the 2 new FIXED_CLOCK + the 2 pre-existing DRAWLOG.)
- `cmake --build native/build-agent-W0.3b --target rb3-native -j8` -> `Built target rb3-native` (EXIT 0; own build dir, CC/CXX=clang).
- Runtime smoke: `RB3_FIXED_CLOCK=1 MILO_MAX_FRAMES=3 MILO_HEADLESS=1 rb3-native` vs flag-off — both EXIT 0, identical 11-line output, zero fixed-clock output (accessors unused -> no consumer yet). Inert as designed.

### Byte-identical / inertness evidence (REQUIRED)
- `git diff --numstat` on the rb3 commit's code files: `58 0 native/src/rb3_replay.cpp` + `22 0 native/src/rb3_replay.h` — **pure insertion, 0 deletions**: two new functions + doc comment header only, NO edit to any existing function body (`RB3ReplayFixedClock` and everything else untouched).
- Engine commit staged ONLY `NativeCompatFlags.classification.json` + `NativeCompatFlags.gen.inc` (NOT the concurrent owner's uncommitted `FxSendNative.cpp` change — left unstaged per Hard Rule 8).
- No rendered path reaches the new symbols (unconsumed until W0.3b.S2 wires SEAM 1) => inert by construction; no screenshot compare needed for S1 (per PLAN).

### Deviation from PLAN (recorded per instructions)
- PLAN's exact-files listed only the two `RB3_FIXED_CLOCK*` sidecar entries, but exit-criterion #1 requires `census check` to exit 0, and W0.3's own `RB3_DRAWLOG` / `RB3_DRAWLOG_DUMP` (engine `Rnd_Wgpu_RB3.cpp` getenvs) were never registered by that lane — leaving them out keeps `check` at EXIT 1. Registered both as `class: probe` (additive registry rows only; no code touched). Necessary to satisfy exit-criterion #1; W0.3 is already committed (engine `9561a19`) so these getenvs are orphaned, not owned by an in-flight lane.

### Remains / blockers
- None for S1. S2 (wire SEAM 1 trace-free path + loader determinism + prove two boots byte-identical) is unblocked: the primitive is in place. No `MILO_ENGINE_PIN` bump (coordinator).

## W0.3b.S2 — partial

**rb3 commit:** `0026bee0` — `W0.3b: wire trace-free fixed-clock seam (SEAM 1 widen + frame advance + deterministic loader drain + fixed boot RNG seed)`. No engine-repo commit (all seams live in rb3 `src/`); `MILO_ENGINE_PIN` UNTOUCHED.

### What landed (all flag-gated on RB3FixedClockActive(), off-path byte-identical)
- `src/system/obj/Task.cpp` — `RB3TaskReplayFixedClock()` gate widened to `(RB3ReplayFixedClock() && RB3ReplayActive()) || RB3FixedClockActive()`. SEAM-1 dt source now branches `RB3ReplayActive() ? RB3ReplayDtForFrame(gRB3TraceFrame) : RB3FixedClockDt()` (a pure refactor on the pre-existing replay path — when the flag is off, `RB3ReplayActive()` is true in the branch so the value is identical to the old unconditional `RB3ReplayDtForFrame`). File-static accumulator + once-per-frame guard unchanged in shape.
- `src/App.cpp` `RunOneFrame` — added an `else if (RB3FixedClockActive()) RB3TraceSetFrame(frame);` branch off the `gRB3TraceActive` fast-path, so the seam once-per-frame accumulator (`gRB3TraceFrame != sReplayLastFrame`) advances every frame with no loaded trace. No telemetry side-effects.
- `src/system/utl/Loader.cpp` — `#include "rb3_replay.h"` (HX_NATIVE) + `drainToEmpty = (mPeriod >= 1e29f) || RB3FixedClockActive()` in BOTH the native and web `LoadMgr::Poll()` arms, so under the flag the loader fully drains each Poll and the wall-clock time-slice stops being a per-frame jitter source (the resident set at absolute frame N becomes a function of the frozen sim state alone).
- `src/system/os/System.cpp` — under `RB3FixedClockActive()` (and no trace), pin the boot RNG seed to `0x5EED` instead of the wall-clock time-of-day seed (`dt.mSec + dt.mMin*60 + dt.mHour*3600`), via the existing extern-forward-decl pattern next to `RB3ReplaySeed`/`RB3TraceSetSeed`.

### Frame-index correctness (PLAN Risk verified)
On native, `TaskMgr::Poll()` runs exactly once per `RunOneFrame` (App.cpp:613; the retail `inclusive_ui_poll` path at App.cpp:957 is never reached on the native loop, and `SystemPoll(false)` skips System.cpp:706). Even a double-Poll would be safe: the `gRB3TraceFrame != sReplayLastFrame` guard advances `sReplaySeconds` at most once per frame index.

### Determinism proof — COUNT EXACT (primary W0.3 blocker CLOSED)
Bounded harness `MILO_MAX_FRAMES=60 RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1 RB3_DRAWLOG_DUMP=<f> MILO_HEADLESS=1 RB3_GAME=1 RB3_DATA=orig-assets/extracted rb3-native`, three independent boots:
- draw **count = 888 / 888 / 888** (EXACT). Before this item the count jittered 885-890 and failed the comparator on the EXACT `count` gate before reaching any tolerance check — that specific blocker (W0.3 VERIFY #6) is resolved. Root cause of the count jitter was the **wall-clock boot RNG seed** (fixed by the System.cpp seam); the loader-drain + frozen-clock seams are also load-bearing (necessary but not sufficient alone — count still moved 884-887 with the clock frozen until the seed was pinned).

### Determinism proof — TOLERANCE-CLEAN NOT met (blocker; see recommendation)
`scripts/native/drawlog-golden.py::compare_drawlogs` on the three fixed-clock dumps: count matches but **~36 draws diverge on the `world` transform** (all three pairs, `passed=false`, ~600-760 field divergences). Root-caused (evidence: dt=0 true-freeze, fixed seed, RB3_NO_FACE, stabilizer stack RB3_GAMEWARM_OFF/TEX_PREWARM_OFF/ASYNC_OPEN_OFF, and ASLR-disabled `setarch -R` sweeps):
- The divergence is entirely **character animation**, NOT the sim clock (survives `RB3_FIXED_CLOCK_DT_MS=0` true freeze) and NOT the RNG (survives the fixed seed).
- **~10 skinned (body) draws** = ASLR pointer-order in the skin/pose iteration: with ASLR disabled (`setarch -R`) ALL skinned divergence vanishes (0 skinned draws diverge).
- **~26 non-skinned draws (7 meshes = character eyes/face)** = a residual uninit-memory / look-at nondeterminism in CharEyes / face-servo that survives frozen clock + fixed seed + ASLR-off + stabilizers (ASLR-off residual still 139-259 field divergences, max world delta ~0.03, itself run-varying).
- Conclusion: this is a **pre-existing deep engine char-animation nondeterminism** (pointer-order-dependent skin iteration + uninitialized/nondeterministic eye-look-at state) that the harness EXPOSES but does not cause. It is outside the clock/loader/seed determinism scope this subtask defined, and closing it requires a separate engine investigation (pointer-order-stable skin iteration + deterministic/zeroed CharEyes look-at state). No speculative engine seam added (PLAN: do not add engine seams speculatively).

### Off-path unchanged (by construction)
`git show 0026bee0` — every added line is inside a `RB3FixedClockActive()`-gated branch (App.cpp `else if`, System.cpp `else if`), the widened `RB3TaskReplayFixedClock()` gate (adds an OR term; the pre-existing `&&` path is untouched), or the `|| RB3FixedClockActive()` OR-term on Loader.cpp `drainToEmpty`. The Task.cpp dt refactor yields the identical value on the replay path when the flag is off. `numstat`: `+7/-0 App.cpp`, `+13/-2 Task.cpp` (the 2 deletions are the dt-line refactor), `+10/-0 System.cpp`, `+15/-2 Loader.cpp` (2 deletions = the two `drainToEmpty` comment/line edits). Pixel compare on the OFF path is not meaningful (the boot is non-deterministic when off — the very bug), per the W0.3 by-construction argument.

### rb3-tests
`rb3-tests --gtest_filter=DrawLogGolden*` → **9 PASSED / 1 SKIPPED** (`PopulatesFromRealDrawMesh`, GPU-fixture-gated by design). No regression.

### Remains / blockers (for S3 + coordinator)
- **Exit criterion 3 (tolerance-clean across 3 boot-pairs) is BLOCKED** on the engine char-animation nondeterminism above. Count-exact is achieved; the world-transform tolerance gate is not.
- **Recommended paths (coordinator decision needed):**
  1. **S3 capture-config**: capture the golden at a scene/config with no animated character on screen (if one is reachable at boot) — then the render golden is char-free and tolerance-clean.
  2. **New engine W-item**: (a) make the skin/pose iteration pointer-order-stable (kills the skinned-body divergence without needing ASLR-off), and (b) zero/freeze CharEyes+face-servo look-at state under `RB3_FIXED_CLOCK` (kills the eye/face residual). Register any new flag like S1; then the full-scene golden goes green. Optionally have the gate launch under `setarch -R` (ASLR off) as an interim while (a) is pending.
  3. **Amend criterion**: accept the count-EXACT gate + the already-proven `--fail-red-audit` comparator value (co-location / bind-group-collapse detection, which is GPU/boot-free and does not depend on char-pose determinism) as the runnable gate, and treat the full-scene world-transform diff as diagnostic-only until the engine char-pose determinism item lands.
- No `MILO_ENGINE_PIN` bump. No engine-repo change (all S2 seams are rb3 `src/`).
