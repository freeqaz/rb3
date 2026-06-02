# RB3 Web Test Harness

Browser-automation scripts for the RB3 web port (`rb3-web`): boot smoke tests,
screen-flow navigation, and visual captures. All scripts run **headless with no
display server** (no `xvfb`, no system Chromium) via Playwright's bundled
Chromium driving WebGPU over ANGLE/Vulkan.

## No-xvfb headless WebGPU

The harness launches Chromium with `headless: !process.env.DISPLAY` and the
WebGPU/ANGLE flags below. With **no `DISPLAY` set** it runs fully headless and
WebGPU still works (SwiftShader/Vulkan under ANGLE — no X server needed). If a
developer *does* have a display, the same script opens a visible window for
debugging. The flags are centralized in `lib/core.mjs::launchBrowser`:

```
--no-sandbox
--enable-unsafe-webgpu
--use-angle=vulkan
--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration
--ozone-platform=x11
--mute-audio  (+ a few disable-* hardening flags)
```

Playwright + its Chromium are already vendored under `scripts/web/node_modules`
(`chromium-1223` lives in `~/.cache/ms-playwright`). **No install needed.**

## Quick start

```bash
# 1. Build + deploy rb3-web once (slow — brotli on a 28M wasm):
scripts/web/build.sh

# 2. Serve the deployed artifacts (serves /api/health for the harness):
python3 native/web/server.py --port 8421
# (in a worktree:  python3 native/web/server.py --port $(cat .worktree-port) )

# 3. Run the boot smoke test (purely headless — do NOT set DISPLAY/xvfb):
node scripts/web/smoke-test.mjs --port 8421
#   or:  npm --prefix scripts/web run web:smoke-test -- --port 8421
```

`smoke-test.mjs` exits **0** on success / **1** on failure and prints a
`PASS`/`FAIL` line. It reaches `main_hub_screen`, asserts the song DB populated
(`window.rb3SongCount > 0`), and that no `pageerror` / WASM-trap fired.

## RB3 screen flow

```
splash_screen → main_hub_screen → song_select_screen → part_difficulty_screen → game_screen
```

Engine state is published each frame to `window` globals (see
`native/src/main_web.cpp`): `rb3CurrentScreen`, `rb3FocusButton`,
`rb3OvershellView/Track/Diff`, `rb3SongCount`, `rb3FrameCount`, `rb3AppBooted`,
`rb3HighlightedSong/Type`, and `_rb3Keys`.

Input keymap (`native/src/rb3_game_input.cpp`): **Space = Start**, **Enter =
Confirm**, **ArrowUp/Down = nav**. The boot splash is advanced by the
`WebSplashAdvanceHook` (main_web.cpp), which routes splash Start/Confirm through
the direct-injection verb path; `navigateTo` sends Start then Confirm(s).

## `lib/core.mjs` API

Shared module every script imports from instead of duplicating launch/capture/nav.

| Export | Purpose |
| --- | --- |
| `parseArgs(spec)` | parse `--flag` / `--key value` argv against a spec |
| `waitForServer(port, ms?)` | poll `/api/health` until ready |
| `launchBrowser(port, {query?, viewport?})` | headless WebGPU Chromium → `{browser, context, page}` |
| `createCapture(page, {verbose?, filter?})` | wire console/pageerror/crash → `{logs, errors, waitForLog, elapsed, silenceMs}` |
| `engineState(page)` | snapshot all `window.rb3*` globals → `{screen, focus, songs, frame, booted, …}` |
| `waitForBoot(page, ms?)` | poll `rb3AppBooted >= 1` |
| `waitScreen(page, {targets?, from?, timeoutMs?})` | wait for screen name to match `targets` or change away from `from` |
| `pressKey(page, key, holdMs?)` | hold a key for several frames (rising-edge safe) + guard |
| `focusCanvas(page)` | click `#rb3-canvas` so keys land on the game |
| `navigateTo(page, capture, target?, {onScreen?})` | full splash→…→`target` nav (default `main_hub_screen`); `target` from `SCREENS.*` |
| `SCREENS` | `{SPLASH, MAIN_HUB, SONG_SELECT, PART_DIFFICULTY, GAME}` |
| `outputDir(name, explicit?)` | resolve/create an output dir (auto-timestamped under `/tmp/rb3-web/`) |
| `screenshot(page, dir, name)` | PNG the `#rb3-canvas` (not the page) → path / null |
| `captureCanvasStats(page, dir, name, threshold?)` | screenshot + decode `paintedPct` (assert not-black) |
| `saveLogs(logs, dir, name?)` / `saveJson(obj, dir, name?)` | write JSONL / JSON |
| `cleanup(browser)` | close with a 3s timeout guard |

## Writing a new test

```js
import {
    parseArgs, waitForServer, launchBrowser, createCapture,
    navigateTo, engineState, outputDir, screenshot, cleanup, SCREENS,
} from './lib/core.mjs';

const opts = parseArgs({ port: { type: 'number', default: 8421 }, verbose: { type: 'flag' } });
let browser;
try {
    await waitForServer(opts.port);
    const { browser: b, page } = await launchBrowser(opts.port);
    browser = b;
    const cap = createCapture(page, { verbose: opts.verbose });

    await navigateTo(page, cap, SCREENS.SONG_SELECT);   // drive to a screen

    const dir = outputDir('my-test', opts.out);
    await screenshot(page, dir, 'song_select');
    const st = await engineState(page);
    console.log(st.screen, st.songs);

    process.exit(0);
} catch (e) {
    console.error(e.message);
    process.exit(1);
} finally {
    await cleanup(browser);
}
```

To set a WASM env var (e.g. a debug flag) before `main()`, install an init
script and reload so `preRun` runs (see `menuhub-probe.mjs` / `quick-capture.mjs`):

```js
await page.addInitScript(() => {
    window.Module = window.Module || {};
    (window.Module.preRun = window.Module.preRun || []).push(() => {
        if (typeof ENV !== 'undefined') ENV.MENU_DBG = '1';
    });
});
await page.reload({ waitUntil: 'domcontentloaded', timeout: 30000 });
```

## Scripts using the shared lib

- **`smoke-test.mjs`** — boot smoke (this is the dc3-parity test). PASS/FAIL, exit 0/1.
- **`splash-diag.mjs`** — splash→main_hub stall diagnostic (hand-drives splash with key holds).
- **`menuhub-probe.mjs`** — main-hub `[MENU_DBG]` draw-position probe.
- **`quick-capture.mjs`** — quick main-hub screenshot (with `MILO_PROPANIM_DBG`).

The other `*.mjs` here are older one-off captures that still inline their own
launch/nav; port them to `lib/core.mjs` as you touch them.
`pixelmatch-test.mjs` (the former `smoke-test.mjs`) is the milo-render
pixel-diff regression tool — separate from the boot smoke.

## npm scripts (`package.json`)

```bash
npm --prefix scripts/web run web:smoke-test  -- --port 8421
npm --prefix scripts/web run web:splash-diag -- --port 8421
```
