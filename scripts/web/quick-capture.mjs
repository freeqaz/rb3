// quick-capture.mjs — boot splash -> main_hub, screenshot the hub.
// Sets MILO_PROPANIM_DBG=1 so the engine logs prop-anim reveal targets, and
// echoes only the interesting console lines (errors / main_hub / btn / anim).
//
// Refactored to import the shared harness (scripts/web/lib/core.mjs):
// headless no-xvfb launch, filtered console capture, navigateTo(main_hub).
// Behavior unchanged.
//
// Usage: node scripts/web/quick-capture.mjs [port] [out.png]
import {
    launchBrowser, createCapture, navigateTo, engineState, cleanup, SCREENS,
} from './lib/core.mjs';

const port = parseInt(process.argv[2] || '8903', 10);
const out = process.argv[3] || '/tmp/main_hub_capture.png';

let browser;
try {
    const { browser: b, page } = await launchBrowser(port);
    browser = b;

    // Set MILO_PROPANIM_DBG=1 in the WASM env BEFORE main() runs.
    await page.addInitScript(() => {
        window.Module = window.Module || {};
        window.Module.preRun = window.Module.preRun || [];
        window.Module.preRun.push(function () {
            if (typeof ENV !== 'undefined') ENV.MILO_PROPANIM_DBG = '1';
        });
    });
    await page.reload({ waitUntil: 'domcontentloaded', timeout: 30000 });

    // Echo only the interesting lines, matching the original filter.
    const cap = createCapture(page, {
        filter: /error|fail|ERR|main_hub|btn|reveal|anim|PROPANIM|PK target|BTNPROBE/i,
    });

    console.log('navigating splash -> main_hub...');
    await navigateTo(page, cap, SCREENS.MAIN_HUB);
    await new Promise(r => setTimeout(r, 5000));

    const st = await engineState(page);
    console.log(`screen='${st.screen}' frame=${st.frame}`);

    await page.locator('#rb3-canvas').screenshot({ path: out });
    console.log(`saved ${out}`);
} catch (e) {
    console.error('ERR', e.message);
} finally {
    await cleanup(browser);
}
