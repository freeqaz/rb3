// W32-WEB-YELLOW STEP-0: reach overshell=joined_default (flyout closed) and
// capture the floating yellow-green orphan quad over the torso; then focus-travel
// both directions to show the quad is STATIC (A10 replay). Backspace = Cancel
// (kPad_Circle) collapses options -> joined_default.
import {
    launchBrowser, createCapture, navigateTo, engineState, pressKey,
    focusCanvas, screenshot, cleanup, SCREENS,
} from './lib/core.mjs';
import { mkdirSync } from 'fs';

const port = parseInt(process.argv[2] || '39555', 10);
const dir  = process.argv[3] || '/tmp/w32y';
mkdirSync(dir, { recursive: true });

let browser;
try {
    const { browser: b, page } = await launchBrowser(port, { query: 'debug=true' });
    browser = b;
    const cap = createCapture(page);
    await navigateTo(page, cap, SCREENS.MAIN_HUB);
    await new Promise(r => setTimeout(r, 3000));
    await focusCanvas(page);

    let st = await engineState(page);
    console.log('STATE@hub:', JSON.stringify({screen:st.screen, focus:st.focus, overshell:st.overshell}));

    // Collapse options -> joined_default: press Backspace (Cancel) up to 4x until
    // overshell != options.
    for (let i = 0; i < 4; i++) {
        if (st.overshell !== 'options') break;
        await pressKey(page, 'Backspace', 300);
        await new Promise(r => setTimeout(r, 1500));
        st = await engineState(page);
        console.log(`after Backspace#${i}: overshell='${st.overshell}' focus='${st.focus}'`);
    }
    await new Promise(r => setTimeout(r, 1500));
    await screenshot(page, dir, 'joined_00_default');

    // A10 replay: focus travel both directions. In joined_default the main-menu
    // focus should move (ArrowDown/Up). Quad must stay static.
    const seq = [['01','ArrowDown'],['02','ArrowDown'],['03','ArrowUp'],['04','ArrowUp']];
    for (const [i, key] of seq) {
        await pressKey(page, key, 300);
        await new Promise(r => setTimeout(r, 1200));
        st = await engineState(page);
        console.log(`STATE@${key}#${i}: focus='${st.focus}' overshell='${st.overshell}'`);
        await screenshot(page, dir, `joined_${i}_${key}`);
    }
    console.log('DONE');
} catch (e) {
    console.error('ERR', e.stack || e.message);
} finally {
    await cleanup(browser);
}
