# Web splash → main_hub stall: root cause + fix (HELD for verify)

**Status:** fix implemented + compiled, committed to `wt-websplash` (`e765cc74`).
**Blocked on:** the shared engine is currently non-compiling (concurrent FX/HUD session's
uncommitted WIP — `Rnd_Wgpu_RB3.cpp` calls `RndParticleSys::RelativeXfm()`, which exists only
in their uncommitted `Part.h`, not HEAD). Every worktree build fails until that is committed.
Once the engine compiles, run the verify steps below and merge.

> Coordination note: the concurrent session is also investigating this stall
> (`scripts/web/splash-diag.mjs`). This is our independent root-cause + fix; reconcile if they
> land one too.

## Root cause (confirmed by reading the code)
On web, menu keys are **not** raw-injected via `ExecButton`; `JoypadPoll → SendButtonMessages`
is meant to own them. But `rb3_game_input.cpp:1411-1424` has a Phase-2 **"double-fire guard"**
that suppresses the raw path, and the `SendButtonMessages` path does **not** fire the splash's
overshell add-user gate (`kSplashScreen_WaitOvershell`; `AddLocalUser` must populate `mUsers` +
fire `AddUserResultMsg` for `overshell_allowing_input` → TRUE — see
`rb3_netsession_native.cpp:84`). So Start/Confirm on splash do nothing and it stalls
(~290 frames stuck on `splash_screen` in the headless harness).

The verb path (`RB3GameInputInjectVerb` → `gPendingInject`, drained in `RB3GameInputPoll`) is a
**separate queue not subject to the double-fire guard**, and reliably advances the splash — it's
the exact path `/api/input` uses.

## The fix (one file, `#ifdef HX_WEB`)
`native/src/main_web.cpp`: a `WebSplashAdvanceHook()` called in `BOOT_RUNNING` right after
`sApp->RunOneFrame`. **Only while `TheUI.CurrentScreen()->Name() == "splash_screen"`**, it
edge-detects Start (bit 11 `kPad_Start`) and Confirm (bit 6 `kPad_X`) on `window._rb3Keys`
(own static prev-mask) and calls `RB3GameInputInjectVerb("start"/"confirm")` (declared via a
local `extern`, so no edit/include of the contested `rb3_game_input.cpp`). Scoped strictly to
splash so it never double-fires with `JoypadPoll` on other screens (where that path works).

## Orchestrator verify steps (once the engine compiles)
1. `cd .claude/worktrees/websplash && scripts/web/build.sh`
2. `python3 native/web/server.py --port 8785`
3. From the main repo (node_modules symlinked): `node scripts/web/splash-diag.mjs --port 8785`
4. PASS = `window.rb3CurrentScreen` goes `splash_screen → main_hub_screen` after Start(Space)+
   Confirm(Enter) holds; console shows `RB3 Web splash: Start edge -> inject verb start` /
   `... Confirm edge -> inject verb confirm`.
5. Native regression unaffected (no native code path changed). Merge `main_web.cpp` to master.
6. Cleanup: `git worktree remove --force .claude/worktrees/websplash`.
