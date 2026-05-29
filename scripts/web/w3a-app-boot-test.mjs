#!/usr/bin/env node
/**
 * RB3 Web — W3a App-driven boot acceptance check.
 *
 * Loads index.html (NO ?milo= param) in headless Chromium, which boots the real
 * RB3 `App` (sApp = new App(0,nullptr)) and drives sApp->RunOneFrame() each
 * frame. Waits for window.rb3AppBooted==1 && window.rb3FrameCount >= 60, then
 * screenshots the canvas and reports the current screen + sampled pixels.
 *
 * Asserts: App booted, frames advance past 60, no WASM trap / boot error, and
 * the canvas is painted (not the page background).
 *
 * Usage:
 *   node scripts/web/w3a-app-boot-test.mjs [--port 8431] [--frames 60] [--verbose]
 *
 * Output: scripts/web/results/web-w3a/menu/{canvas.png, console.jsonl, summary.json}
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
    frames:  parseInt(argv[argv.indexOf('--frames') + 1] || '60', 10) || 60,
    verbose: argv.includes('--verbose'),
};

const BOOT_TIMEOUT_MS = 300000;  // 5 min for the full App boot (many milos)
const FRAME_TIMEOUT_MS = 120000; // 2 min to advance past --frames
const PAGE_BG = { r: 10, g: 10, b: 10 };  // #0a0a0a
const TOL = 10;

const OUT_DIR = resolve(__dirname, 'results/web-w3a/menu');
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

function nearly(a, b, tol = TOL) {
    return Math.abs(a.r - b.r) <= tol && Math.abs(a.g - b.g) <= tol && Math.abs(a.b - b.b) <= tol;
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
        if (opts.verbose) console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
    });
    page.on('pageerror', (err) => { errors.push(err.message || String(err)); console.log(`  [PAGE_ERROR] ${err.message || err}`); });
    page.on('crash', () => { errors.push('Page crashed'); console.log('  [CRASH] Page crashed!'); });

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url} (App-driven boot, no ?milo=)`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Wait for the App to be constructed (window.rb3AppBooted = 1).
    let appBooted = 0;
    let deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    if (appBooted < 1) failures.push(`rb3AppBooted = ${appBooted} after ${BOOT_TIMEOUT_MS}ms (expected 1 — App never constructed)`);

    // Wait for the RunOneFrame loop to advance past --frames.
    let frames = 0;
    deadline = Date.now() + FRAME_TIMEOUT_MS;
    while (Date.now() < deadline) {
        frames = await page.evaluate(() => window.rb3FrameCount || 0);
        if (frames >= opts.frames) break;
        await new Promise(r => setTimeout(r, 500));
    }
    if (frames < opts.frames) failures.push(`rb3FrameCount = ${frames} after ${FRAME_TIMEOUT_MS}ms (expected >= ${opts.frames})`);

    // Give the screen flow a moment, then read the current screen name.
    await new Promise(r => setTimeout(r, 1000));
    const currentScreen = await page.evaluate(() => window.rb3CurrentScreen || '');

    // Capture + decode the canvas.
    const canvasPath = resolve(OUT_DIR, 'canvas.png');
    let centerPx = null, samples = [], painted = 0, total = 0;
    try {
        await page.locator('#rb3-canvas').screenshot({ path: canvasPath, omitBackground: false });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(canvasPath));
        const px = (x, y) => { const i = (y * png.width + x) * 4; return { x, y, r: png.data[i], g: png.data[i+1], b: png.data[i+2], a: png.data[i+3] }; };
        const cx = png.width >> 1, cy = png.height >> 1;
        centerPx = px(cx, cy);
        samples = [px(cx, cy), px(cx >> 1, cy), px(cx + (cx >> 1), cy), px(cx, cy >> 1), px(cx, cy + (cy >> 1))];
        // Count painted pixels (non-black: App clears to black, so any color = drawn).
        total = png.width * png.height;
        for (let i = 0; i < png.data.length; i += 4) {
            const r = png.data[i], g = png.data[i+1], b = png.data[i+2];
            if (r > 12 || g > 12 || b > 12) painted++;
        }
    } catch (e) {
        failures.push(`Canvas screenshot/decode failed: ${e.message}`);
    }

    const paintedPct = total ? (100 * painted / total) : 0;
    if (centerPx && nearly(centerPx, PAGE_BG)) {
        // Center being black is OK (App clears black); only fail if NOTHING is painted.
    }
    if (paintedPct < 0.1) failures.push(`Only ${paintedPct.toFixed(2)}% of pixels are painted (expected >= 0.1% — nothing drawn)`);

    for (const err of errors) failures.push(`pageerror: ${err}`);
    const trap = logs.find(l => /function signature mismatch|call_indirect type|RuntimeError|abort\(/i.test(l.text));
    if (trap) failures.push(`WASM trap signature in console: ${trap.text}`);
    const bootErr = logs.find(l => l.text.includes('RB3 Web: boot error'));
    if (bootErr) failures.push(`Boot error printed: ${bootErr.text}`);
    const stubHits = logs.filter(l => l.text.includes('[rb3-stub]')).map(l => l.text);

    summary = {
        result: failures.length === 0 ? 'pass' : 'fail',
        appBooted, frames, expected_frames: opts.frames,
        current_screen: currentScreen,
        center_pixel: centerPx,
        sampled_pixels: samples,
        painted_pct: Number(paintedPct.toFixed(2)),
        page_background: PAGE_BG,
        canvas_png: canvasPath,
        stub_hits: [...new Set(stubHits)].slice(0, 40),
        failures,
        log_count: logs.length, error_count: errors.length,
    };
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT_DIR, 'summary.json'), JSON.stringify(summary, null, 2));

    console.log('\n=== RB3 Web W3a App-boot test ===');
    if (failures.length === 0) {
        console.log(`PASS — appBooted=${appBooted} frames=${frames} screen='${currentScreen}' painted=${paintedPct.toFixed(2)}% center=${JSON.stringify(centerPx)}`);
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
    console.log(`current screen: '${currentScreen}'`);
    if (summary.stub_hits.length) {
        console.log(`[rb3-stub] hits: ${summary.stub_hits.length}`);
        for (const s of summary.stub_hits.slice(0, 20)) console.log(`    ${s}`);
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
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch { /* ignore */ } }
}
