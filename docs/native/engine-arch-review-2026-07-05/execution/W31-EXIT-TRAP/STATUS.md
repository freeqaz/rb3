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
