// W32-WEB-YELLOW STEP-0 repro: boot web debug build -> main_hub (joined_default),
// screenshot the floating yellow-green quad, then ArrowDown/ArrowUp to show the
// REAL focus highlight moves while the orphan quad stays static.
// Usage: node scripts/web/_w32_yellow_repro.mjs <port> <outdir>
import {
    launchBrowser, createCapture, navigateTo, engineState, pressKey,
    focusCanvas, screenshot, cleanup, SCREENS,
} from './lib/core.mjs';

const port = parseInt(process.argv[2] || '39555', 10);
const dir  = process.argv[3] || '/tmp/w32y';
import { mkdirSync } from 'fs';
mkdirSync(dir, { recursive: true });

let browser;
try {
    const { browser: b, page } = await launchBrowser(port, { query: 'debug=true' });
    browser = b;
    const cap = createCapture(page);

    console.log('navigating splash -> main_hub...');
    await navigateTo(page, cap, SCREENS.MAIN_HUB);
    await new Promise(r => setTimeout(r, 4000));

    const st0 = await engineState(page);
    console.log('STATE@hub:', JSON.stringify(st0));
    await screenshot(page, dir, 'hub_00_initial');

    // Focus travel: ArrowDown x2, ArrowUp x2. Screenshot each so we can see the
    // real focus highlight move while the orphan quad stays put (A10 replay).
    await focusCanvas(page);
    for (const [i, key] of [['01','ArrowDown'],['02','ArrowDown'],['03','ArrowUp'],['04','ArrowUp']]) {
        await pressKey(page, key, 300);
        await new Promise(r => setTimeout(r, 1200));
        const st = await engineState(page);
        console.log(`STATE@${key}#${i}:`, JSON.stringify({screen:st.screen, focus:st.focus, overshell:st.overshell}));
        await screenshot(page, dir, `hub_${i}_${key}`);
    }
    console.log('DONE');
} catch (e) {
    console.error('ERR', e.stack || e.message);
} finally {
    await cleanup(browser);
}
