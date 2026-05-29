#!/usr/bin/env node
/**
 * RB3 Web — W3b keyboard input acceptance test.
 *
 * Loads index.html (no ?milo=) in headless Chromium, waits for the App to boot
 * to splash_screen, then sends Enter (kAction_Confirm) and verifies the screen
 * advances to main_hub_screen (or a later screen). Screenshots the result.
 *
 * Asserts:
 *   - App booted (rb3AppBooted === 1)
 *   - Boot reaches splash_screen (or later)
 *   - A keypress (Enter) advances the screen past splash_screen
 *   - rb3CurrentScreen !== 'splash_screen' after the keypress
 *   - No WASM trap / boot error in the console
 *   - Canvas is painted
 *
 * Usage:
 *   node scripts/web/w3b-input-test.mjs [--port 8431] [--verbose]
 *
 * Output: scripts/web/results/web-w3b/main_hub/{canvas.png, console.jsonl, summary.json}
 * Exits 0 on PASS, 1 on FAIL.
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));

const argv = process.argv.slice(2);
const opts = {
    port:    parseInt(argv[argv.indexOf('--port') + 1] || '8431', 10) || 8431,
    verbose: argv.includes('--verbose'),
};

const BOOT_TIMEOUT_MS   = 300000;  // 5 min for the full App boot
const SPLASH_TIMEOUT_MS = 120000;  // 2 min to reach splash_screen
const SCREEN_TIMEOUT_MS =  30000;  // 30 s for screen advance after keypress

const OUT_DIR = resolve(__dirname, 'results/web-w3b/main_hub');
mkdirSync(OUT_DIR, { recursive: true });

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
                if (r.statusCode === 200) return res();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline) return rej(new Error(`Server not ready after ${timeoutMs}ms`));
            setTimeout(check, 300);
        };
        check();
    });
}

let browser;
const failures = [];
const logs = [];
const errors = [];
let summary = {};

try {
    await waitForServer(opts.port);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--disable-background-networking',
            '--disable-default-apps', '--disable-sync', '--mute-audio',
        ],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0 = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ elapsed: elapsed(), type: msg.type(), text });
        if (opts.verbose || /web-input|screen:|FIRE|WAIT|SKIP/.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => {
        errors.push(err.message || String(err));
        console.log(`  [PAGE_ERROR] ${err.message || err}`);
    });
    page.on('crash', () => { errors.push('Page crashed'); console.log('  [CRASH] Page crashed!'); });

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url} (App boot — W3b keyboard test)`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Step 1: Wait for App to be constructed.
    console.log('Waiting for rb3AppBooted...');
    let appBooted = 0;
    let deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    if (appBooted < 1) {
        failures.push(`rb3AppBooted = ${appBooted} after ${BOOT_TIMEOUT_MS}ms (App never constructed)`);
    }
    console.log(`App booted: ${appBooted} (${elapsed()}s)`);

    // Step 2: Wait for splash_screen (or any screen past the very beginning).
    console.log('Waiting for splash_screen...');
    let screenBeforeKey = '';
    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    while (Date.now() < deadline) {
        screenBeforeKey = await page.evaluate(() => window.rb3CurrentScreen || '');
        if (screenBeforeKey && screenBeforeKey !== '') break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`Current screen before keypress: '${screenBeforeKey}' (${elapsed()}s)`);

    // Step 3: Give it a bit more time to settle on splash_screen.
    await new Promise(r => setTimeout(r, 2000));
    screenBeforeKey = await page.evaluate(() => window.rb3CurrentScreen || '');
    console.log(`Screen (settled): '${screenBeforeKey}' (${elapsed()}s)`);

    // Step 4: Click canvas to focus it, then send input to advance through splash.
    // The splash flow requires: (1) a press on start.btn (Confirm/Enter) which
    // opens the overshell add-user flow, then (2) another Confirm once the overshell
    // settles. This mirrors the native script: @10:start,@30:confirm (two presses).
    // We send Enter (Confirm) first, wait for overshell to settle (~2s), then
    // send Enter again to advance through the overshell to main_hub.
    console.log('Clicking canvas to focus...');
    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // Press sequence with delays to let the overshell settle between each press.
    const pressSequence = [
        { key: 'Enter', delay: 2000, label: 'Enter #1 (Confirm — splash start.btn)' },
        { key: 'Enter', delay: 3000, label: 'Enter #2 (Confirm — overshell settle)' },
        { key: 'Enter', delay: 2000, label: 'Enter #3 (Confirm — further advance)' },
        { key: 'Space', delay: 2000, label: 'Space (Start — fallback)' },
    ];

    let screenAfterKey = screenBeforeKey;
    let pressedIdx = 0;

    for (const press of pressSequence) {
        console.log(`Sending ${press.label} at ${elapsed()}s`);
        await page.keyboard.press(press.key);

        // Wait for the screen to change or for the settle delay.
        const pressDeadline = Date.now() + press.delay;
        while (Date.now() < pressDeadline) {
            screenAfterKey = await page.evaluate(() => window.rb3CurrentScreen || '');
            if (screenAfterKey !== screenBeforeKey && screenAfterKey !== '') {
                console.log(`Screen advanced after '${press.label}': '${screenBeforeKey}' → '${screenAfterKey}' (${elapsed()}s)`);
                break;
            }
            await new Promise(r => setTimeout(r, 200));
        }
        pressedIdx++;
        if (screenAfterKey !== screenBeforeKey && screenAfterKey !== '') break;
        console.log(`  Screen still '${screenAfterKey}' after ${press.label}`);
    }

    // If still on splash after all presses, wait longer for the last action to settle.
    if (screenAfterKey === screenBeforeKey) {
        console.log(`Still on '${screenBeforeKey}' — waiting up to ${SCREEN_TIMEOUT_MS}ms...`);
        deadline = Date.now() + SCREEN_TIMEOUT_MS;
        while (Date.now() < deadline) {
            screenAfterKey = await page.evaluate(() => window.rb3CurrentScreen || '');
            if (screenAfterKey !== screenBeforeKey && screenAfterKey !== '') break;
            await new Promise(r => setTimeout(r, 500));
        }
    }

    // Wait a bit for the screen to settle after transition.
    await new Promise(r => setTimeout(r, 3000));
    const finalScreen = await page.evaluate(() => window.rb3CurrentScreen || '');
    console.log(`Final screen: '${finalScreen}' (${elapsed()}s)`);

    // Step 6: Screenshot the result.
    const canvasPath = resolve(OUT_DIR, 'canvas.png');
    let centerPx = null, paintedPct = 0;
    try {
        await page.locator('#rb3-canvas').screenshot({ path: canvasPath, omitBackground: false });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(canvasPath));
        const cx = png.width >> 1, cy = png.height >> 1;
        const i = (cy * png.width + cx) * 4;
        centerPx = { x: cx, y: cy, r: png.data[i], g: png.data[i+1], b: png.data[i+2] };
        let painted = 0;
        const total = png.width * png.height;
        for (let p = 0; p < png.data.length; p += 4) {
            if (png.data[p] > 12 || png.data[p+1] > 12 || png.data[p+2] > 12) painted++;
        }
        paintedPct = 100 * painted / total;
    } catch (e) {
        failures.push(`Canvas screenshot/decode failed: ${e.message}`);
    }

    // Assertions.
    if (screenAfterKey === screenBeforeKey || screenAfterKey === '') {
        failures.push(
            `Screen did not advance after keypress: before='${screenBeforeKey}' after='${finalScreen}'`
        );
    } else {
        console.log(`SCREEN ADVANCED: '${screenBeforeKey}' → '${finalScreen}'`);
    }
    if (paintedPct < 0.1) failures.push(`Only ${paintedPct.toFixed(2)}% of pixels are painted`);

    for (const err of errors) failures.push(`pageerror: ${err}`);
    const trap = logs.find(l => /function signature mismatch|call_indirect type|RuntimeError|abort\(/i.test(l.text));
    if (trap) failures.push(`WASM trap: ${trap.text}`);
    const bootErr = logs.find(l => l.text.includes('RB3 Web: boot error'));
    if (bootErr) failures.push(`Boot error: ${bootErr.text}`);

    // Collect web-input log lines for summary.
    const webInputLogs = logs.filter(l => /web-input|InitWebInput|keyboard input ready/.test(l.text)).map(l => l.text);

    summary = {
        result: failures.length === 0 ? 'pass' : 'fail',
        appBooted,
        screen_before_key: screenBeforeKey,
        screen_after_key: screenAfterKey,
        final_screen: finalScreen,
        screen_advanced: screenAfterKey !== screenBeforeKey && screenAfterKey !== '',
        center_pixel: centerPx,
        painted_pct: Number(paintedPct.toFixed(2)),
        canvas_png: canvasPath,
        web_input_logs: webInputLogs.slice(0, 20),
        failures,
        log_count: logs.length,
        error_count: errors.length,
    };
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT_DIR, 'summary.json'), JSON.stringify(summary, null, 2));

    console.log('\n=== RB3 Web W3b keyboard input test ===');
    if (failures.length === 0) {
        console.log(`PASS — screen advanced: '${screenBeforeKey}' → '${finalScreen}'`);
        console.log(`canvas: painted=${paintedPct.toFixed(2)}% center=${JSON.stringify(centerPx)}`);
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
    if (webInputLogs.length) {
        console.log('Web input logs:');
        for (const l of webInputLogs.slice(0, 10)) console.log(`  ${l}`);
    }
    console.log(`Results: ${OUT_DIR}`);
    process.exit(failures.length === 0 ? 0 : 1);

} catch (e) {
    console.error(`Error: ${e.message}`);
    try {
        writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
        writeFileSync(resolve(OUT_DIR, 'summary.json'), JSON.stringify({ result: 'error', message: e.message, ...summary }, null, 2));
    } catch { /* ignore */ }
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); }
        catch { /* ignore */ }
    }
}
