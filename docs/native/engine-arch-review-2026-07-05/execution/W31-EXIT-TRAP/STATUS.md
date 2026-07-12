# W31-EXIT-TRAP — Lane C STATUS (append-only)

## STEP 0 (in flight) — symbolized backtrace of the bounded-boot teardown SIGSEGV
- Base SHA rb3 fd119705; engine pin b36bcfc (HEAD==pin, clean except untouchable M FxSendNative.cpp).
- Own build dir native/build-agent-W31-EXIT-TRAP (clang -O0 -g2). PLAN.md written.
- Charter target: bounded NON-HTTP boot rc=-11/139 (sometimes SIGABRT 134). Known class = Dawn/GPU device teardown at static-dtor order (W0.3.S1).
- Pre-read finding: a full teardown-ordering scheme ALREADY exists (RB3RegisterBandRndShutdown -> gBandRnd.Shutdown from Debug::Exit; AudioDevice::Suspend at frame-loop exit). On the clean bounded path Debug::Exit should already drop all wgpu refs before exit(0). Trap persists anyway -> backtrace must name the ACTUAL faulting chain before any fix (lint 10, no fix-first).

## Rider — WEB-YELLOW (W31-EXIT-TRAP sub-item, Sonnet, 2026-07-12)


**Verdict: CONFIRMED_ON_WEB.** The user's "floating yellow square" on the main
menu IS reproducible on the deployed web build. It does NOT reproduce on native
(coordinator W31-REPRO, `a_hubtop_*.png`) — this is a genuine web-specific
divergence, not a stale-build or non-16:9-window artifact.

### A9 deploy-freshness check
- `native/web/build/release/` mtime 2026-07-12 05:35 (rb3-web.wasm/.js + .br/.gz),
  built from base SHA `fd119705` (committed 05:46:42Z; deploy postdates the
  coordinator's 05:17 rebuild noted in W31-REPRO/NOTES.md). Deploy contains the
  current tree. Stale-build hypothesis from W31-REPRO stays dead — this is a
  real web-only bug, not a stale asset.

### Repro path
Harness: shared `scripts/web/lib/core.mjs` (`launchBrowser`/`navigateTo`/
`pressKey`/`engineState`, per A11), release build served via
`native/web/server.py --port 8720` (own free port, killed after). Screenshots
1280x720, headless chromium.

1. splash -> `main_hub_screen` (`navigateTo`, ~4s settle). Engine state:
   `focus='mb_playnow.btn' overshell='options'` — a "Player 1" controller/
   profile flyout (RETURN / PLAY ON XBOX LIVE / CHARACTERS / OPTIONS / DROP
   OUT) is AUTO-OPEN over the hub on first arrival (web-only; native repro's
   `a_hubtop_00_focused.png` shows PLAY NOW itself highlighted directly, no
   flyout). This flyout's own yellow highlight is clean and contained — NOT
   the bug (evidence/web-yellow/crop_player1_panel_clean.png).
2. Pressed Cancel (Escape -> `kAction_Cancel` per `rb3_game_input.cpp`) twice
   to dismiss the flyout. Engine state moved `overshell: 'options' ->
   'joined_default'` — `joined_default` is the engine's own default
   `OvershellDir::mSlotView` construction value (`src/system/bandobj/
   OvershellDir.cpp:9`), i.e. this is the NORMAL/default hub-browsing state,
   not a harness artifact.
3. In `joined_default`, at hub top-level with PLAY NOW focused: a solid flat
   yellow-green rectangle floats in open scene space over the lead
   character's torso (screen-space approx x395-718, y292-328), fully
   detached from the PLAY NOW text row — no border, no icon, not aligned to
   any list item (evidence/web-yellow/confirmed_floating_square_playnow.png,
   crop evidence/web-yellow/crop_floating_square_playnow.png).
4. Pressed ArrowDown (focus -> `mb_career.btn`, CAREER text tints green as
   expected): the floating rectangle does NOT move — identical position and
   size, still detached, now not even under any highlighted row
   (evidence/web-yellow/confirmed_floating_square_career_static.png, crop
   evidence/web-yellow/crop_floating_square_career.png). This is the smoking
   gun: a real tracking highlight would follow focus; this quad is static
   and unrelated to list layout — matches the user's "floating yellow
   square" description exactly.

### Disposition
- CONFIRMED on the web release build, at hub top-level (`main_hub_screen`,
  `overshell='joined_default'`), reached via ordinary keyboard nav (arrows +
  cancel) — not a synthetic-only path. NOT reproduced on native (coordinator
  W31-REPRO verdict stands unchanged).
- Per lane charter this rider is CAPTURE-ONLY — no fix landed this wave.
  Recommend a fresh W32-menu item: web-only floating highlight-quad, likely a
  leftover/mispositioned highlight-mesh instance surviving the
  `options`->`joined_default` overshell transition (screen-space quad not
  repositioned or hidden when the flyout closes). Needs its own STEP-0
  diagnosis; candidate surfaces `src/system/bandobj/OvershellDir.cpp` /
  MainHubPanel highlight-mesh code, web-vs-native render-hook divergence not
  yet isolated (out of scope for this rider — read-only per charter).
- Evidence (gitignored, on disk):
  `execution/W31-EXIT-TRAP/evidence/web-yellow/` —
  confirmed_floating_square_playnow.png, confirmed_floating_square_career_static.png,
  crop_floating_square_playnow.png, crop_floating_square_career.png,
  hub_initial_options_panel.png, crop_player1_panel_clean.png,
  before/after_cancel_*.png raw captures.

## STEP 0 RESULT + FIX (fix applied, verify in progress)
- SYMBOLIZED BACKTRACE captured (evidence/step0_backtrace_symbolized.txt). rc=139 x5/5 baseline.
- ROOT CAUSE: gBandRnd+1432 == BandRnd::mComposeDiffView. BandRnd::Shutdown() (run from Debug::Exit exit-callback, "BandRnd: Shutdown complete" prints) nulls every OTHER wgpu handle + mGpu.Shutdown() (-> "device lost reason 2"), but TWO late-added GPU-handle clusters were never added to Shutdown:
    (1) compose / C8-RTT cluster (mComposeDiffView / mComposeShader / mComposeBGL / mComposePL / mComposeUB / mComposePipelines), Rnd_Wgpu_RB3.h:371-380;
    (2) billboard-particle cluster (mPartShader / mPartTexBGL / mPartPL / mPartVB / mPartIB / mPartPipelines), Rnd_Wgpu_RB3.h:387-395.
  A surviving handle (mComposeDiffView) transitively held the LAST strong ref to the Dawn Device/Adapter/Instance, so mGpu.Shutdown() (mDevice=nullptr) did NOT actually retire the device -- real teardown deferred to ~BandRnd during libc static-dtor phase (exit(0)) where dropping the last ref jumps into the torn-down libvulkan ICD -> SIGSEGV. Whole-class enumeration (gdb static member walk) confirmed these two are the ONLY unreleased GPU clusters; all others are covered by explicit nulls / mBloom.Terminate() / mHaloBloom.Terminate() / mPipelines.Terminate() / uniform-ring Release() / mGpu.Shutdown().
- FIX: release BOTH clusters inside BandRnd::Shutdown() while Dawn is alive, ahead of mGpu.Shutdown(). Non-behavioral ordering fix; NO new flags. Engine-only (owned gfx teardown surface).
- ITERATIVE PROOF: after the compose-only fix the bt MOVED (mComposeDiffView->~TextureView became mPartShader->~ShaderModule @+1536) -- direct evidence the compose leak was real and closed; then added the Part cluster.
- VERIFY BLOCKED (transient, concurrent): rb3-native EXECUTABLE link blocked by Lane D untracked native/src/rb3_shardprobe_native.cpp:89 (Matrix3 vs Hmx::Matrix3 compile error) in the shared source tree. My engine change compiles clean into libmilo-engine.a. Waiting for Lane D fix, retrying.

## RESULT — DONE (engine commit 0083bad)
- FIX committed: milo-native-engine 0083bad "W31-EXIT-TRAP: release compose + particle GPU clusters in BandRnd::Shutdown()". Only src/platform/Rnd_Wgpu_RB3.cpp staged; FxSendNative.cpp left untouched.
- PIN: NOT bumped by lane (charter rule). Coordinator must bump MILO_ENGINE_PIN b36bcfc -> 0083bad at close-out.
- ACCEPTANCE (own tree, native/build-agent-W31-EXIT-TRAP):
    * bounded non-HTTP 5-frame boot rc=0 -> 10/10 (baseline 139 5/5). Iterative proof: compose-only fix moved the bt from mComposeDiffView->~TextureView to mPartShader->~ShaderModule, then the particle-cluster add cleared it entirely.
    * rb3-tests -> 116 PASSED / 7 SKIPPED (env-gated real-capture fixtures) / 0 FAILED.
    * drawlog-golden.py --fixed-clock --bin <mine> -> PASS (frame=60 count=792; 281 known-residual divergences within bound, non-blocking).
    * lineup-gate.py --bin <mine> -> PASS (img/segA/ratioB/countC/pin all PASS across coop_g_n03 x2 + coop_g_b x2).
- A7: did NOT touch drawlog-golden.py:183-190,234-237 or song-end-test.py:269 (coordinator-owned tolerance removal, post-merge, now unblockable since rc=0 holds). No new flags (non-behavioral ordering fix). Lint 9 flavor-membership: Rnd_Wgpu_RB3.cpp compiles into rb3-native (gfx backend flavor=rb3) — verified by the successful build + boot.
- RIDER (web yellow-highlight capture): DEFERRED as secondary Sonnet side-task; primary exit-trap deliverable complete.
