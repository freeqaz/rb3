# verify-dta-eval-crash — independent adversarial review (wave 6)

**Verdict:** `CONFIRM_WITH_RESIDUALS`
**Reviewer:** independent Opus, composed master build (HEAD `7a0b9595`, engine pin `15ce606`)
**Binary:** `native/build-native/rb3-native` (built Jun 15 19:29, AFTER fix `3d2356b8` @ 18:41)
**Fix under review:** rb3 `3d2356b8` — `native/src/rb3_http_handlers.cpp::HandleDtaEval`
(MILO_TRY/MILO_CATCH + sigsetjmp backstop + gCallStackPtr snapshot/restore + null-safe formatting)
**crashReproducesPostFix:** `true` — but ONLY for a pathological ≥~15 consecutive hard-fail
BURST. A single hard-fail and a normal interleaved inspection session NEVER crash. Per the
assignment's own gate (`crashReproducesPostFix=true` is a REJECT *only if a SINGLE/NORMAL
hard-fail SIGSEGVs or wedges*), the burst SIGSEGV is the documented residual, **not a REJECT.**

---

## What I confirmed WORKS (the actual reviewer-facing bug — fixed)

Booted rb3-native to `song_select_screen` headless on my port range (9521-9524) and ran the
exact reviewer repros via `/api/dta/eval`. **Every single hard-fail returned a graceful
400/200 — never a 500, never SIGSEGV-text, never a timeout — and a valid eval (`{+ 19 23}` → 42)
succeeded immediately after EACH one, proving the main thread never wedged / the malloc lock
is not held:**

| expr | result | after-fail `{+ 19 23}` |
|---|---|---|
| `{find no_such_dir no_such_key}` | `400` graceful | `200` → 42 ✅ |
| `{song_select_panel find live_lb.grp}` | `200` (null obj) | `200` → 42 ✅ |
| `{exists {song_select_panel find live_lb.grp}}` | `400` graceful | `200` → 42 ✅ |
| `{exists {song_select_panel}}` | `400` graceful | `200` → 42 ✅ |
| `{song_select_panel start_color_low r}` (Color sub-prop) | `200` graceful | `200` → 42 ✅ |
| `{start_color_low r}` | `200` graceful | `200` → 42 ✅ |
| `{exists live_lb.grp}` | `200` graceful | `200` → 42 ✅ |

Engine log over the whole single-fail sweep: every fail took the clean `MILO_FAIL for …`
(MILO_TRY) path, **0 `recovered from`** (signal backstop never even fired), 0 aborts, 0
OSFatal, 0 `main thread not polling`. The Modal/heap-abort path is fully avoided for the
common miss exactly as the impl claims.

**Normal interleaved inspection session — clean, twice.** 4 hard-fails interleaved with 5
good reads (`{+ ..}`, `{get_type live_lb.grp}`, `{music_library get_highlighted_node}`):
all graceful, server alive throughout, **0 coredumps** across both runs. This is the realistic
inspection workflow and it works end-to-end.

## What still crashes (the residual — sharpened vs the impl doc)

The documented "≥~12 back-to-back hard-fail burst can stall (~1/8)" residual is **real and
reproduces, but it is a hard SIGSEGV that kills the server, not merely a stall.** Two
independent burst runs each crashed after ~15-16 consecutive identical hard-fails
(`{find no_such_dir no_such_key}`), confirmed by two fresh systemd coredumps whose PID +
executable path + timestamp match my runs exactly:

- PID 2610307 @ 06:11:04 UTC — `native/build-native/rb3-native`, SIGSEGV (my verify.py burst)
- PID 2647317 @ 06:12:58 UTC — `native/build-native/rb3-native`, SIGSEGV (my threshold run, died at the 16th consecutive fail)

Both coredumps have the **identical 643-frame recursive stack**:
```
snprintf  <- FormatString::operator<<  <- MakeString<...>  <- MemPushHeap(int)
  <- Debug::Fail  <- DebugFailer::operator<<  <- MemPushHeap(int)  <- Debug::Fail  <- …(×~210)
```
i.e. a runaway `MemPushHeap → Debug::Fail → operator<< → MakeString → MemPushHeap → …` loop
that overflows the stack and SIGSEGVs inside `snprintf`.

### Why MILO_TRY can't catch this (root cause, verified in source)
`Debug::Fail` (`src/system/os/Debug.cpp:127`) calls **`MemPushHeap(x)` at line 162 BEFORE the
`mTry != 0` longjmp check at line 175.** For a single fail-hard the heap push succeeds and the
longjmp fires cleanly — which is why single/normal fails are graceful. But each bad eval LEAKS
its `parsed` DataArray (and in-flight DataNode/String temporaries) on the MILO_CATCH path — the
impl doc admits this explicitly. After ~15 consecutive leaks the "main" heap is exhausted/
corrupted, so `MemPushHeap` itself starts failing → it re-enters `Debug::Fail` at line 162
**before** the `mTry` branch is ever reached, so the handler's MILO_TRY scope is never consulted.
The recursion formats a message each level (`MakeString`→`snprintf`) and never unwinds → stack
overflow. This matches the impl's "complete fix needs an engine-side WARN-not-FAIL debug-eval
mode" note; the leak is the accelerant.

## Adjudication against the REJECT gate

- Single hard-fail: **clean** (7/7 distinct repros, 0 crashes, valid-eval-after each).
- Normal interleaved session: **clean** (2/2 runs, 0 coredumps).
- Burst ≥~15 consecutive hard-fails: **SIGSEGV** (2/2 runs crashed, identical signature).

The assignment's REJECT condition is a single/normal hard-fail crashing/wedging — which does
NOT occur. The fix is strictly + substantially better than baseline (baseline wedged on the
FIRST hard miss; this stays fully responsive through normal use and only dies under a
pathological back-to-back burst). Hence **CONFIRM_WITH_RESIDUALS**, not REJECT.

## Residual / follow-up (not blocking, for a future engine wave)
1. **Burst SIGSEGV** — the impl's residual is a hard crash, not a stall; the verify-doc/impl
   framing of "stall (~1/8)" understates it. The reviewer-facing trigger (a few isolated bad
   evals) is fixed; defending the burst needs the engine-side WARN-not-FAIL debug-eval mode the
   impl already scoped, plus releasing `parsed` on the catch path to stop the heap-exhaustion
   accelerant. (Re-touching the heap to free in the catch is what the impl deliberately avoided
   for safety — a bounded arena or a guarded free would close the leak.)
2. Hardening idea: a re-entrancy guard / depth cap in `Debug::Fail` (or skipping `MakeString`
   when `MemPushHeap` is already failing) would convert the stack-overflow SIGSEGV into a clean
   bounded fail — engine-side, out of scope for this native-only handler.

## Evidence
- `/tmp/rp6rev-dta-eval-crash/verify.py` — full reviewer-repro + burst harness; `results-9521.json`.
- `/tmp/rp6rev-dta-eval-crash/verify2.py` — threshold + normal-session harness;
  `res2-9522-normal.json`, `res2-9523-threshold.json` (died_at iter 16), `res2-9524-normal.json`.
- systemd coredumps 2610307 + 2647317 (`coredumpctl info <pid>`): identical 643-frame recursive
  Debug::Fail signature.
- Process hygiene: all my rb3-native instances were port-scoped (9521-9524) and killpg'd; no
  sibling (9425/9426/9502-9505/9511/9531) was ever signalled.
