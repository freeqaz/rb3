# task-dta-eval-crash — impl (wave 6, blocking crash bug)

**Status:** done (with a documented residual) · **verified:** the exact reviewer
repros no longer SIGSEGV/wedge in 7/8 runs · **needsEngine:** no (native-only TU)
**Branch:** `wt-task-dta-eval-crash` · **Commit:** `3d79dd61`
**File:** `native/src/rb3_http_handlers.cpp` (HX_NATIVE-only; Wii byte-identical)

---

## BUG

`/api/dta/eval` (the headless engine-state inspection route, `RB3_HTTP=1`) crashed
reviewers across multiple waves. Per `verify-songselect-ui.md` (lines 96–103):
`{song_select_panel find live_lb.grp}` returns a NULL object in some extracts,
then `{exists ...}` on it plus a Color sub-property read trip a SIGSEGV — "caught
inside the handler, non-fatal, server kept running" but returns garbage and trips
a latent SIGSEGV. Two named defects: (1) a miss/lookup returning NULL is then
dereferenced; (2) a Color sub-property read on a non-Color node crashes/returns
garbage.

## REPRO (native, headless)

Boot to `song_select_screen` (`scripts/native/song-select-capture.py` nav), then
POST the exprs to `/api/dta/eval`. Repro driver: `/tmp/rp6-dta-eval-crash/reviewer_repro.py`.

The crashing forms (confirmed under gdb on the **baseline** binary):

| expr | pre-fix result |
|---|---|
| `{find no_such_dir no_such_key}` | `500 "DTA eval crashed: SIGABRT/SIGSEGV"` |
| `{exists {song_select_panel}}` | abort via OSFatal (caught) |
| `{exists {song_select_panel find live_lb.grp}}` | `500 "DTA eval crashed: SIGSEGV"` |

`(start_color_low r)` / `{song_select_panel start_color_low r}` returned garbage
type nodes (`type:16`, `type:0`, `value:null`) — never a clean read.

## BACKTRACES (gdb, baseline binary)

Two precise backtraces pin the same root cause — a hard `MILO_FAIL` deep in the
engine reaching `Debug::Fail` with NO try-scope active, which on native rides the
full `Debug::Modal` abort path:

`{find no_such_dir no_such_key}` — heap abort while formatting the fail message:
```
abort  <- _MemFree <- String::~String <- Debug::Modal <- Debug::Fail
  <- DebugFailer::operator<< <- DataNode::Array <- DataArray::Array(1)
  <- DataFindExists <- DataFind <- DataArray::Execute <- DataNode::Evaluate
  <- DataArray::Evaluate(i) <- RB3HttpServer::HandleDtaEval
```
(`{find <dir> <key>}` evaluates arg 1 as an Array; `no_such_dir` is a Symbol →
`DataNode::Array` MILO_FAILs "Data … is not Array".)

`{exists {song_select_panel}}` — OSFatal:
```
abort <- OSFatal <- DebugModal <- AppDebugModal <- Debug::Modal <- Debug::Fail
  <- DebugFailer::operator<< <- DataArray::Node(1) <- DataArray::Sym(1)
  <- SongSelectPanel::Handle <- DataArray::Execute <- DataNode::Str
  <- DataArray::Str(1) <- DataExists <- DataArray::Execute <- …HandleDtaEval
```
(`exists`'s arg 1 `{song_select_panel}` executes the object handler with no
message → `Sym(1)` on a 1-element array → `Node(1)` out-of-range MILO_FAIL.)

## ROOT CAUSE

Not a single null-deref in the handler — a structural one. `DataExists`,
`DataFind`/`DataFindExists`, `DataNode::Str/Array/Sym` are *designed* to `MILO_FAIL`
on a missing key / wrong type (programmer-error assertions in DTA scripts). The
eval handler ran them with **no `MILO_TRY` scope** (`TheDebug.mTry == 0`), so
`Debug::Fail` (`src/system/os/Debug.cpp:127`) skipped its clean `mTry != 0`
longjmp branch (line 175) and fell into `Debug::Modal`, where:
- formatting the fail message constructs `String` temporaries on a heap the abort
  is already stressing → a glibc `_MemFree` double-free `abort()`, OR
- `AppDebugModal → OSFatal` aborts.

The existing `sigsetjmp` guard caught the signal so the *process* survived, but:
1. it returned a 500 `"DTA eval crashed: SIGSEGV"` (indistinguishable from a real
   server fault — the reviewer's "returns garbage");
2. **recovering from a glibc malloc-abort leaves the allocator lock held → the
   MAIN THREAD wedges on its next `malloc`** (every later eval `"Command timed
   out (main thread not polling)"`). This is the reviewers' "latent SIGSEGV
   across waves" — a recovered eval poisoning the next one.

Also: the recovery skipped the in-flight `~DataCallStackFrame` pops
(`DataArray.cpp:45`), leaving `gCallStackPtr` advanced at stale `DataArray*` →
the next eval's `DataCallStackFrame` ctor / `DataAppendStackTrace` walks freed
pointers.

## FIX (native/src/rb3_http_handlers.cpp, HX_NATIVE-only)

`HandleDtaEval`:
1. **Wrap the parse-eval-format in `MILO_TRY {…} MILO_CATCH(failMsg) {…}`.**
   `MILO_TRY` sets `TheDebug.mTry`, so `Debug::Fail` takes its clean longjmp-back
   branch **before** `Debug::Modal` — **no Modal, no heap abort, no OSFatal, no
   signal, no main-thread wedge.** We land in `MILO_CATCH` with the engine's fail
   message and return a graceful `400 "DTA eval failed: <msg>"`.
2. **Snapshot/restore engine globals** (`gCallStackPtr`, `TheDebug.mTry`,
   `TheDebug.mFailing`): the longjmp (and the sigsetjmp backstop's siglongjmp)
   skip the `~DataCallStackFrame` pops, so we reset `gCallStackPtr` in the catch
   and the signal path → no stale-pointer corruption bleeds into the next eval.
3. **Null-safe `kDataObject` formatting** (already report "null" if `GetObj()` is
   null — a missing `find`/`find_obj` yields a null object node).
4. **Signal handler kept as a backstop** for genuine non-MILO_FAIL faults (stack
   overflow, a real null deref) — its recovery now restores state and returns a
   graceful 400 too (no more alarming 500).

Why MILO_TRY over signal-handler-only (both were tried): a signal-handler-only
recovery from the `{find …}` *heap abort* leaves the allocator locked → the main
thread wedges on the FIRST hard find (0% post-crash success). MILO_TRY avoids
Modal entirely, so the heap is never aborted — proven by the engine logs: in the
clean runs all 3 hard-fail evals took the MILO_TRY path (`MILO_FAIL for …`) with
**ZERO signal-handler recoveries**.

## EVIDENCE (`/tmp/rp6-dta-eval-crash/`)

- `gdb-find-fixed.log`, `gdb-segv-a.log` — the two baseline backtraces.
- BEFORE (`rb3-native-baseline`, HEAD handler): every hard-fail → `500 "DTA eval
  crashed: SIGSEGV"` (`reviewer_repro.py` on baseline, captured in the run logs).
- AFTER (fixed): every reviewer repro graceful —
  - `{exists {… find live_lb.grp}}` → `400 "DTA eval failed: Data live_lb.grp is
    not String"` (was SIGSEGV)
  - `{find no_such_dir no_such_key}` → `400 "DTA eval failed: Data no_such_dir is
    not Array"` (was SIGABRT)
  - `{exists {song_select_panel}}` → `400 "DTA eval failed: Array doesn't have
    node 1"` (was OSFatal abort)
  - `{exists live_lb.grp}` → clean `int 0`; valid evals after all crashers
    (`{+ 19 23}` → 42, header read → 0, `find live_lb.grp` → object) all work.
- Repro-run scoreboard (10-eval reviewer sequence): **7/8 runs fully clean**
  (all evals graceful + post-crash valid evals correct + server up). 1/8 wedged
  on the 3rd back-to-back hard-fail (see residual).

## VERIFICATION

- `reviewer_repro.py` (the exact reviewer sequence) → `RESULT: PASS
  (valid_after_ok=True)` on 7/8 independent boots; server alive throughout.
- Engine log of a PASS run: 3 `MILO_FAIL for` (clean MILO_TRY), **0
  `recovered from`** (no signal handler invoked) — the abort is fully avoided.

## RESIDUAL (honest)

Under a **pathological back-to-back hard-fail burst** (≥~12 consecutive
fail-hard evals, or unlucky timing on the 3rd), the MILO_TRY longjmp's
unreleasable in-flight engine temporaries (DataNode/String allocations inside the
recursive `DataArray::Execute`, the `START_AUTO_TIMER` stack object) accumulate
and occasionally stall the main thread (`"main thread not polling"`); ~1/8 runs
of the 10-eval sequence wedged. A realistic inspection session (a handful of bad
evals interleaved with reads) is clean. This residual is **strictly better than
baseline** (baseline's signal-handler heap-abort recovery wedged on the FIRST
hard find). A complete fix would require an engine-side change — e.g. a debug-eval
mode where the fail-hard DataFuncs (`find`, `Array/Str/Sym` accessors) WARN +
return `kDataUnhandled` instead of `MILO_FAIL` (mirroring `find_exists`). Out of
scope for a native-only debug-tool fix; logged here for a future engine wave.

## LANDING NOTES

- **One file, native-only:** `native/src/rb3_http_handlers.cpp`. `native/src/` is
  NOT in the Wii MWCC build (`config/SZBE69_B8/` has no reference; the TU is gated
  by HX_NATIVE/RB3_HTTP). **Wii byte-identical by construction** — no Wii-compiled
  source touched, no objdiff delta possible.
- Adds `#include "os/Debug.h"` (MILO_TRY/MILO_CATCH + `TheDebug`). `gCallStackPtr`
  is already `extern` in `obj/Data.h` (already included).
- Match-improvement vs ifdef: N/A (no Wii source). No `#ifdef` toggling needed —
  the whole TU is native.
- Conflicts / land order: none. The wave-5 `songselect-ui` fix touched
  `SongStatusMgr.cpp`/`BandSongMgr.cpp`/`SongSelectPanel.cpp` — different files.
  No other wave-6 task touches `rb3_http_handlers.cpp`. Cherry-pick `3d79dd61`
  standalone; rebuild `rb3-native`; no engine pin bump.
- Build: `cmake --build native/build-native --target rb3-native -j` (clean, ~exit 0).
