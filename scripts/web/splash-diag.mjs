#!/usr/bin/env node
/**
 * splash-diag.mjs — focused diagnostic for the splash → main_hub stall.
 *
 * Boots the web App, reaches splash_screen, then drives Start/Confirm with
 * KEY HOLDS (keyboard.down → wait several frames → keyboard.up) instead of
 * instantaneous press(), polling window.rb3CurrentScreen + window.rb3FocusButton
 * + window._rb3Keys after each step. Captures the full console (incl. the
 * engine RB3 web-input / RB3 screen / SendButtonMessages trace) and screenshots.
 *
 * Refactored to import the shared harness (scripts/web/lib/core.mjs) — same
 * headless no-xvfb launch, console capture, key holds, and canvas screenshots.
 * Behavior is unchanged: it still hand-drives splash (NOT navigateTo) so it can
 * observe each step of the stall.
 *
 * Usage: node scripts/web/splash-diag.mjs [--port 8421]
 */
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import {
    parseArgs, launchBrowser, createCapture, engineState,
    pressKey, focusCanvas, screenshot, waitForBoot, waitScreen, cleanup,
} from './lib/core.mjs';

const __dirname = dirname(fileURLToPath(import.meta.url));
const opts = parseArgs({ port: { type: 'number', default: 8421 } });
const OUT = resolve(__dirname, 'results/splash-diag');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

let browser;
let capRef;
try {
    const { browser: b, page } = await launchBrowser(opts.port);
    browser = b;
    const cap = createCapture(page);
    capRef = cap;

    console.log('waiting for app boot + splash...');
    await waitForBoot(page);
    await waitScreen(page, { targets: ['splash_screen'], timeoutMs: 180000 });
    await sleep(2500);
    let s = await engineState(page);
    console.log(`SPLASH reached: ${JSON.stringify(s)}  (${cap.elapsed()}s)`);
    await focusCanvas(page);
    await sleep(300);
    await screenshot(page, OUT, '00-splash');

    // Step 1: hold Start (Space).
    console.log('\n--- HOLD Start (Space) ---');
    await pressKey(page, 'Space', 800);
    for (let i = 0; i < 8; i++) { await sleep(500); console.log(`  +${i}: ${JSON.stringify(await engineState(page))}`); }
    await screenshot(page, OUT, '01-after-start');

    // Step 2: hold Confirm (Enter) up to 8 times, watching for screen change.
    for (let k = 0; k < 8; k++) {
        console.log(`\n--- HOLD Confirm (Enter) #${k + 1} ---`);
        await pressKey(page, 'Enter', 700);
        for (let i = 0; i < 4; i++) { await sleep(500); }
        s = await engineState(page);
        console.log(`  after confirm #${k + 1}: ${JSON.stringify(s)}`);
        if (s.screen && s.screen !== 'splash_screen') { console.log('  >>> SCREEN CHANGED'); break; }
    }
    await screenshot(page, OUT, '02-after-confirms');

    const final = await engineState(page);
    console.log(`\n=== FINAL: ${JSON.stringify(final)} ===`);
    writeFileSync(resolve(OUT, 'console.jsonl'), cap.logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT, 'result.json'), JSON.stringify({ final }, null, 2));
} catch (e) {
    console.error('ERR', e.message);
    if (capRef) writeFileSync(resolve(OUT, 'console.jsonl'), capRef.logs.map(x => JSON.stringify(x)).join('\n') + '\n');
} finally {
    await cleanup(browser);
}
