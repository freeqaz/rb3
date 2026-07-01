# task-eval-burst-harden — impl (wave 7 close-out)

**Status:** done · **verified:** baseline burst SIGSEGV reproduced + eliminated (2×80 +
100-burst, 0 crashes/coredumps); single/normal hard-fails still graceful (no wave-6 regression)
**needsEngine:** NO — both files are rb3-repo files (no `../milo-native-engine` change, no pin bump)
**Branch:** `wt-task-eval-burst-harden` · **Commit:** `<see structured result>`
**Wii byte-identical:** YES — `Debug::Fail` objdiff raw 100.0%, diff_score 0 after the edit
**Files:**
- `native/src/rb3_http_handlers.cpp` (HX_NATIVE-only TU; not in the Wii MWCC build)
- `src/system/os/Debug.cpp` (shared Wii-matched; the change is fully `#ifdef HX_NATIVE`-gated)

---

## BUG (the wave-6 dta-eval residual, upgraded from "stall" to a HARD crash)

`/api/dta/eval` (the `RB3_HTTP=1` headless engine-state inspection route) survives a single or
normal-interleaved hard-fail eval (wave-6 fix `3d2356b8`), but a pathological BURST of ~15-16
consecutive hard-fail evals (e.g. `{find no_such_dir no_such_key}` repeated) **SIGSEGVs the whole
process** via an unbounded recursion that overflows the C stack.

## REPRO ON BASELINE (confirmed FIRST, as the reviewer did)

Built the pre-fix binary in this worktree, booted to `song_select_screen` headless, then fired
back-to-back `{find no_such_dir no_such_key}` evals (`/tmp/rp7-eval-burst-harden/burst.py`):

- **Died at burst iter 14** (`RemoteDisconnected` → `ConnectionRefused` = process gone).
- systemd coredump **PID 1708112, SIGSEGV**, identical 643-frame recursive stack the reviewer saw.
- Symbolized (addr2line on the baseline binary) the repeating 3-frame cycle:
  ```
  snprintf <- FormatString::operator<< <- MakeString<...>
    <- MemPushHeap(int)            (0x44398e)
    <- Debug::Fail(char const*)    (0x47cb40)
    <- DebugFailer::operator<<     (0x2d2df0)
    <- MemPushHeap(int) <- Debug::Fail <- ...   (cycle ~210x -> stack overflow inside snprintf)
  ```
  (saved: `/tmp/rp7-eval-burst-harden/baseline-crash-signature.txt`)

## ROOT CAUSE (verified in source)

Two interacting defects, exactly as the wave-6 review nailed:

1. **`src/system/os/Debug.cpp` Debug::Fail calls `MemPushHeap(x)` (line 162 on master) BEFORE the
   `mTry != 0` longjmp check (line 175).** `MemPushHeap` (`src/system/utl/MemMgr.cpp:700`) asserts
   `MILO_ASSERT(s.mSize + 1 < DIM(s.mStack), 0x607)` (line 703). When the per-thread heap-stack
   bookkeeping overflows, that assert *itself* calls `Debug::Fail` → which hits `MemPushHeap` again
   (line 162) → asserts again → `Debug::Fail` again → **never reaches the `mTry` longjmp at line
   175**, so the handler's `MILO_TRY` scope is never consulted. Each level formats a message
   (`DebugFailer::operator<< → MakeString → snprintf`), so the stack overflows and SIGSEGVs.

2. **The `/api/dta/eval` handler LEAKED its parsed `DataArray` on the `MILO_CATCH` path.** The
   wave-6 code explicitly chose to let `parsed` leak ("re-touching the heap is avoided for safety").
   Each bad eval leaked one `DataArray` (+ in-flight DataNode/String temporaries); ~15 consecutive
   leaks pushed the main heap / heap-stack bookkeeping to the overflow point in (1) → the burst
   threshold. The leak is the *accelerant*; (1) is the actual SIGSEGV mechanism.

## FIX (two-part, both rb3-side)

### (a) NATIVE — `native/src/rb3_http_handlers.cpp::HandleDtaEval`, the `MILO_CATCH` block
Release the parsed `DataArray` on the fail path instead of leaking it:
```cpp
if (parsed) { parsed->Release(); parsed = nullptr; }
```
Safe because the `MILO_TRY` longjmp is a CLEAN unwind — it fires at `Debug.cpp:175` BEFORE
`Debug::Modal`, so the allocator is fully intact and unlocked at `MILO_CATCH` (the wave-6 "avoid
re-touching the heap" worry only applied to the signal-backstop / Modal-abort path, which this path
never takes). `gCallStackPtr` is already restored to its pre-eval snapshot immediately above the
release, so `~DataArray`'s bookkeeping sees normal call-stack state. This removes the accelerant —
on its own it pushes the burst threshold far out under realistic use.

### (b) ENGINE-CLASS — `src/system/os/Debug.cpp::Debug::Fail` (HX_NATIVE-gated re-entrancy guard)
> The task framed this as "ENGINE", but the native build compiles **rb3's own**
> `src/system/os/Debug.cpp` (via `native/CMakeLists.txt` `ENGINE_OS = GLOB src/system/os/*.cpp`);
> there is NO separate `../milo-native-engine` Debug.cpp. So this is an rb3-repo edit, HX_NATIVE-
> gated → **no engine pin bump**.

Added a native-only block (gated, so Wii codegen is unchanged) at the top of the function body:
1. **Caught-fail fast path:** `if (!mNoDebug && MainThread() && mTry != 0) { mTry--; TheDebugFailMsg
   = msg; longjmp(TheDebugJump, 1); }` — take the clean longjmp BEFORE touching `MemPushHeap`. The
   matching `MemPopHeap` never runs on the longjmp path anyway (longjmp skips it), so the push is
   pure overhead AND it is the recursion's first link → removing it kills the loop for the common
   caught case. This is the structural fix: even with a leak, `Debug::Fail` no longer recurses.
2. **Depth guard:** `static int sFailDepth`; `if (sFailDepth > 0) { … longjmp-if-try / else return; }`
   then `sFailDepth++` with an RAII `FailDepthGuard { ~() { sFailDepth--; } }`. Belt-and-suspenders:
   if any OTHER call inside `Debug::Fail` re-fails (a `MakeString`/heap fail with no try-scope), the
   re-entry returns/longjmps instead of recursing → converts a would-be stack-overflow SIGSEGV into
   a bounded clean fail. The now-effectively-dead inner `mTry` longjmp (`Debug.cpp:~219`) hand-
   decrements `sFailDepth` first (longjmp skips the RAII dtor) to keep the guard from wedging at >0.

## MATCH-NEUTRALITY (Wii byte-identical)

- `native/src/rb3_http_handlers.cpp`: HX_NATIVE-only TU, not in the Wii MWCC build — byte-identical
  by construction (no Wii-compiled source touched).
- `src/system/os/Debug.cpp`: all additions are inside `#ifdef HX_NATIVE`. The Wii build (neither
  HX_WEB nor HX_NATIVE) skips both blocks and sees the original `… return; #endif #endif static int
  x = MemFindHeap("main"); MemPushHeap(x); …` sequence verbatim.
- **Proof:** rebuilt the Wii `Debug.o` (`tools/ninja-locked build/SZBE69_B8/src/system/os/Debug.o`)
  and objdiff'd `Fail__5DebugFPCc` → **raw_match 100.0%, normalized 100.0%, diff_score 0** (target
  424B == base 424B). No engine repo file changed; engine worktree stays clean.

## VERIFICATION (`/tmp/rp7-eval-burst-harden/`)

| run | binary | result |
|---|---|---|
| baseline burst (50) | pre-fix | **died_at=14** SIGSEGV + coredump PID 1708112 (reproduced the crash) |
| fixed burst (60)    | post-fix | died_at=None, completed, alive, post-good+post-graceful OK, NO coredump |
| final burst A (80)  | post-fix | died_at=None, completed, alive, no coredump |
| final burst B (80)  | post-fix | died_at=None, completed, alive, no coredump (independent boot) |
| verify_full (final) | post-fix | 7/7 reviewer repros graceful + valid-after; interleaved clean; **100-consecutive burst completed**; diverse-form 60-burst completed; final good eval=42; final single hard-fail graceful 400 |

- **No-crash:** 60 + 80 + 80 + 100 consecutive hard-fails across 4 boots, 0 SIGSEGV, 0 coredumps
  (the only coredump in the whole session is the baseline repro).
- **No main-thread wedge:** every post-burst `{+ 19 23}` returned 42; server stayed responsive.
- **No wave-6 regression:** all 7 distinct reviewer hard-fail forms still return graceful 200/400
  (never 500/SIGSEGV/timeout), and a valid eval succeeds after each.
- Harnesses: `burst.py`, `verify_full.py`; results `fixed-9612.json`, `final-{A,B}-*.json`,
  `verify_full-final-9616.json`; `baseline-crash-signature.txt`.

## LANDING NOTES

- **Two rb3-repo files, no engine change, no pin bump.** Cherry-pick the single wt-branch commit;
  rebuild `rb3-native`. Touches `native/src/rb3_http_handlers.cpp` (the wave-6 dta-eval file — no
  other open task touches it) and `src/system/os/Debug.cpp` (HX_NATIVE-gated; rebuild + objdiff
  `Fail__5DebugFPCc` to re-confirm 100% after any conflicting header churn).
- The `Debug::Fail` fast-path is the real fix (it kills the recursion structurally); the parsed-
  release closes the leak accelerant; the depth guard is the backstop. All three compose; keeping
  all three is the robust answer (the reviewer asked for both the leak fix AND the re-entrancy
  guard).
- Closes the "Open after wave 6 → dta-eval burst SIGSEGV" item. The broader engine WARN-not-FAIL
  debug-eval mode the wave-6 doc mused about is now UNNECESSARY for this crash — the recursion is
  gone — though it would still be a nice-to-have for cleaner 400 messages on exotic fail forms.
