# verify-eval-burst-harden — independent adversarial review (wave 7 close-out)

**Verdict: CONFIRM** (`crashReproducesPostFix=false`)

Independent Opus reviewer, composed master `a36bfcf9` (fix `77b6a3fe`, engine pin `15ce606`,
no pin bump). All four assignment checks pass; the wave-6 dta-eval burst stack-overflow SIGSEGV
is eliminated, with hard attribution to the fix via a self-built pre-fix BEFORE binary, and Wii
byte-identity re-confirmed by my own rebuild + objdiff. Evidence under
`/tmp/rp7rev-eval-burst-harden/`.

## What landed (re-read, not trusted)

`77b6a3fe`, two rb3-side files, no engine change / no pin bump:
- **`native/src/rb3_http_handlers.cpp::HandleDtaEval`** — the `MILO_CATCH` block now frees the
  parsed `DataArray` (`if (parsed) { parsed->Release(); parsed = nullptr; }`, line 367) instead
  of leaking it. Removes the heap-exhaustion **accelerant**. No double-free: the try-success
  release (line 345), the catch release (367), and the parse-failure release (275 → `goto
  cleanup`) are mutually exclusive paths.
- **`src/system/os/Debug.cpp::Debug::Fail`** — `#ifdef HX_NATIVE` block (lines 161-204) that (1)
  takes the clean `mTry != 0` longjmp **before** `MemPushHeap` (kills the recursion's first link
  for the caught case), plus (2) a `static int sFailDepth` re-entrancy guard that breaks the loop
  (longjmp-if-try else return) on genuine re-entry. The inner Wii-structural longjmp branch hand-
  decrements `sFailDepth` (longjmp skips the RAII dtor). All additions gated — Wii path unchanged.

## (a) Attribution — pre-fix BEFORE binary reproduces the burst crash

Built a clean pre-fix binary from the fix's PARENT commit `e83e2c79` in a cold-ish worktree
(`tools/setup-worktree.sh rp7rev-eval-prefix e83e2c79`; configured with the main build's clang +
`Dawn_DIR`; verified `sFailDepth` absent + the "re-touching the heap is avoided" leak comment
present). Fired `{find no_such_dir no_such_key}` consecutively against `song_select_screen`:

- **DIED at burst iter 15** — `RemoteDisconnected` = process gone (`prefix-9713.json`).
- Process PID 2898369 GONE; **systemd coredump SIGSEGV, the pre-fix worktree binary, 77.1M**.
- Backtrace = the exact documented unbounded recursion (saved `prefix-crash-backtrace.txt`):
  ```
  snprintf  <-  FormatString::operator<<  <-  MakeString
    <-  MemPushHeap(int)          (rb3-native + 0x44398e)
    <-  Debug::Fail(const char*)  (rb3-native + 0x47cb40)
    <-  DebugFailer::operator<<   (rb3-native + 0x2d2df0)
    <-  MemPushHeap  <-  Debug::Fail  <-  DebugFailer::operator<<  <-  ...   (repeats to overflow)
  ```
  Symbol offsets match the impl doc's `baseline-crash-signature.txt`. The FIRST hard-fail was a
  graceful 400 (`first_fail_graceful=true`) — i.e. the wave-6 single-fail fix IS present in the
  pre-fix binary; only the BURST overwhelms it. This nails attribution: the crash is the thing
  `77b6a3fe` fixes, and ~15 consecutive hard-fails is the documented threshold.

## (b) Fixed master binary — burst no longer crashes; main thread not wedged

Freshly-built `/home/free/code/milohax/rb3/native/build-native/rb3-native` (mtime 22:25:57, just
after the fix commit 22:25:46; BuildID dd05748). Three independent boots, my own harness
(`rev_burst.py`), each ≥100 consecutive hard-fails:

| boot / port | form | n | died_at | post valid `{+ 19 23}` | post single hard-fail |
|---|---|---|---|---|---|
| 9711 | mixed (7 forms) | 120 | None | **42** | graceful 400 |
| 9712 | `{find no_such_dir no_such_key}` (the exact accelerant) | 150 | None | **42** | graceful 400 |
| 9714 | mixed (7 forms) | 100 | None | **42** | graceful 400 |

- 0 SIGSEGV, 0 deaths, **0 coredumps** across all three (the session's ONLY coredump is the
  pre-fix repro). Each catch-path fired hundreds of `parsed->Release()` with no heap corruption.
- A VALID eval (`{+ 19 23}` → **42**) succeeds after every burst → main thread is responsive, not
  wedged. (`fixed-9711.json`, `fixed-single-9712.json`, `fixed-mixed-9714.json`.)

## (c) Normal single hard-fails still graceful (wave-6 fix not regressed)

Every run's first hard-fail returned a graceful **400** (`first_fail_graceful=true`), and every
post-burst single hard-fail returned a graceful **400** with a clean message (`DTA eval failed:
Data no_such_dir is not Array …`) — never 500/SIGSEGV/timeout. Wave-6 single-fail behavior intact.

## (d) Wii byte-identical — verified by my own rebuild

`tools/ninja-locked build/SZBE69_B8/src/system/os/Debug.o` then
`objdiff-cli diff -u system/os/Debug Fail__5DebugFPCc`:
**raw 100.0% / normalized 100.0% / fuzzy 100.0%, diff_score 0/10600, target_size == base_size ==
424**. The `#ifdef HX_NATIVE` gating is correct — the Wii MWCC build compiles the original code
verbatim. Matches the impl claim exactly.

## Residuals / notes

- None blocking. The fix is debug-tool-only (`/api/dta/eval`), not a gameplay path. The depth
  guard + fast-path are structurally sound; the previously-mused engine WARN-not-FAIL mode is
  unnecessary for this crash (recursion is structurally gone). Pre-fix worktree torn down; large
  coredump + engine logs trimmed.
- The mixed-form burst exercises forms that legitimately *succeed* (`{exists …}`, `{find_obj
  …}` return valid int/null-object), so the single-form `{find …}` 150-burst is the more
  stringent accelerant test — it passed.

— reviewer evidence: `/tmp/rp7rev-eval-burst-harden/` (`rev_burst.py`, `*.json`,
`prefix-crash-backtrace.txt`).
