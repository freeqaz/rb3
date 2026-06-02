// Draw-time MENU_DBG probe capture for the main-hub button-stacking bug.
// Boots splash -> main_hub, sets MENU_DBG=1 so the engine's BandRnd::DrawMesh
// logs each hub button/label mesh's world position + projected screen-Y, then
// dumps the unique [MENU_DBG] lines and saves a screenshot.
//
// Refactored to import the shared harness (scripts/web/lib/core.mjs):
// headless no-xvfb launch, console capture, navigateTo(main_hub), screenshot.
// Behavior unchanged.
//
// Usage: node scripts/web/menuhub-probe.mjs [port] [out.png]
import {
    launchBrowser, createCapture, navigateTo, engineState, cleanup, SCREENS,
} from './lib/core.mjs';

const port = parseInt(process.argv[2] || '8710', 10);
const out = process.argv[3] || '/tmp/menuhub_probe.png';

let browser;
try {
    // Set MENU_DBG=1 in the WASM env BEFORE main() runs.
    const { browser: b, page } = await launchBrowser(port);
    browser = b;
    await page.addInitScript(() => {
        window.Module = window.Module || {};
        window.Module.preRun = window.Module.preRun || [];
        window.Module.preRun.push(function () {
            if (typeof ENV !== 'undefined') ENV.MENU_DBG = '1';
        });
    });
    // The init script must be installed before navigation; reload so preRun runs.
    await page.reload({ waitUntil: 'domcontentloaded', timeout: 30000 });

    const cap = createCapture(page);
    const menuLines = () => cap.logs.filter(l => l.text.includes('[MENU_DBG]')).map(l => l.text);

    console.log('navigating splash -> main_hub...');
    await navigateTo(page, cap, SCREENS.MAIN_HUB);
    await new Promise(r => setTimeout(r, 5000));

    const st = await engineState(page);
    console.log(`screen='${st.screen}' frame=${st.frame}`);

    await page.locator('#rb3-canvas').screenshot({ path: out });
    console.log(`saved ${out}`);

    console.log('\n===== MENU_DBG (engine dedups by world x,z) =====');
    const seen = new Set();
    const all = menuLines();
    for (const l of all) {
        const clean = l.replace(/^.*\[MENU_DBG\]/, '[MENU_DBG]');
        if (seen.has(clean)) continue;
        seen.add(clean);
        console.log(clean);
    }
    console.log(`===== ${seen.size} unique lines, ${all.length} total =====`);
} catch (e) {
    console.error('ERR', e.message);
} finally {
    await cleanup(browser);
}
