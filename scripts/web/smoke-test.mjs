#!/usr/bin/env node
/**
 * RB3 Web — Boot smoke test.
 *
 * Regression guard for the boot → menu → song-DB path. Boots the App headless
 * (no xvfb — Playwright bundled Chromium + WebGPU/ANGLE-Vulkan), drives
 * splash_screen → main_hub_screen, and asserts:
 *
 *   1. main_hub_screen is reached (splash advance + overshell continue work —
 *      the WebSplashAdvanceHook verb path in native/src/main_web.cpp).
 *   2. The song DB populated: window.rb3SongCount > 0 (PublishSongCount reads
 *      TheSongMgr.GetRankedSongs — proves NativeContentMgr loaded songs.dta).
 *   3. No `pageerror` events fired (a WASM/JS exception during boot/nav).
 *   4. Defense in depth: no `function signature mismatch` / `call_indirect`
 *      WASM-trap signatures in the console.
 *
 * Exits 0 on success, 1 on failure.
 *
 * (The earlier `smoke-test.mjs` — a milo-render pixelmatch regression tool —
 *  is now `pixelmatch-test.mjs`; this file is the dc3-parity boot smoke.)
 *
 * Usage:
 *   # Start the server first (serves /api/health for waitForServer):
 *   python3 native/web/server.py --port $(cat .worktree-port) &
 *
 *   node scripts/web/smoke-test.mjs [--port 8421] [--verbose] [--out DIR]
 *   npm run web:smoke-test -- --port 8421
 *
 * See scripts/web/README.md for the no-xvfb workflow and lib API.
 */

import {
    parseArgs, waitForServer, launchBrowser, createCapture,
    navigateTo, engineState, outputDir, screenshot, saveLogs, saveJson, cleanup,
    SCREENS,
} from './lib/core.mjs';

const opts = parseArgs({
    port:    { type: 'number', default: 8421 },
    out:     { type: 'string' },
    verbose: { type: 'flag' },
});

let browser;
const failures = [];
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

try {
    await waitForServer(opts.port);
    const { browser: b, page } = await launchBrowser(opts.port);
    browser = b;

    const cap = createCapture(page, { verbose: opts.verbose });

    // Boot → splash → main_hub. The splash advance + overshell continue +
    // song-DB load all happen along this path.
    await navigateTo(page, cap, SCREENS.MAIN_HUB);

    // Give main_hub a couple frames so PublishSongCount runs at least once.
    await new Promise(r => setTimeout(r, 2000));

    const dir = outputDir('smoke-test', opts.out);
    const state = await engineState(page);
    await screenshot(page, dir, 'main_hub');
    saveLogs(cap.logs, dir);

    // 1. main_hub reached.
    if (state.screen !== SCREENS.MAIN_HUB) {
        failures.push(`expected screen '${SCREENS.MAIN_HUB}', got '${state.screen}'`);
    }

    // 2. Song DB populated.
    if (!(state.songs > 0)) {
        failures.push(`song DB empty (rb3SongCount=${state.songs}) — NativeContentMgr/songs.dta load suspect`);
    }

    // 3. No WASM/JS exceptions during boot/nav.
    for (const err of cap.errors) {
        failures.push(`pageerror: ${err}`);
        if (/function signature mismatch|call_indirect/i.test(err)) {
            failures.push('  ^ WASM vtable/signature regression suspected');
        }
    }

    // 4. Defense in depth: scan console for WASM trap signatures.
    const sigMatch = cap.logs.find(l =>
        /function signature mismatch|call_indirect type/i.test(l.text)
    );
    if (sigMatch) {
        failures.push(`WASM trap signature in console: ${sigMatch.text}`);
    }

    saveJson({
        result: failures.length === 0 ? 'pass' : 'fail',
        elapsed_s: Number(elapsed()),
        screen: state.screen,
        songs: state.songs,
        frame: state.frame,
        error_count: cap.errors.length,
        failures,
    }, dir);

    console.log('\n=== Smoke test ===');
    if (failures.length === 0) {
        console.log(`PASS — main_hub_screen reached, song DB populated (${state.songs} songs), no pageerror`);
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
    console.log(`Elapsed: ${elapsed()}s  Logs: ${dir}`);

    process.exit(failures.length === 0 ? 0 : 1);
} catch (e) {
    console.error(`Error: ${e.message}`);
    console.log(`\n=== Smoke test ===\nFAIL\n  - ${e.message}\nElapsed: ${elapsed()}s`);
    process.exit(1);
} finally {
    await cleanup(browser);
}
