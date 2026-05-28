#!/usr/bin/env node
/**
 * RB3 Web — W1 clear-frame smoke test.
 *
 * Boots rb3-web in headless Chromium, waits for window.rb3FrameCount ≥ 5,
 * captures a canvas screenshot, and asserts the center pixel matches the
 * clear color (rgb(51,102,178) from Hmx::Color(0.2, 0.4, 0.7)) — NOT the
 * page background (#0a0a0a, rgb(10,10,10)).
 *
 * Pre-reqs:
 *   - rb3-web built (`scripts/web/build.sh`).
 *   - Dev server running on PORT (`python3 native/web/server.py --port 8421`).
 *   - Playwright installed (`npm install` in this dir).
 *
 * Usage:
 *   node scripts/web/smoke-test.mjs [--port 8421] [--verbose]
 *
 * Exits 0 on PASS, 1 on FAIL.
 *
 * Outputs `scripts/web/results/<timestamp>/{console.jsonl, canvas.png, summary.json}`.
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync, statSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));

// ----- arg parsing -----
const argv = process.argv.slice(2);
const opts = {
    port:    parseInt(argv[argv.indexOf('--port') + 1] || '8421', 10) || 8421,
    verbose: argv.includes('--verbose'),
};

const PASS_FRAMES = 5;
const EXPECTED = { r: 51, g: 102, b: 178 };  // Hmx::Color(0.2, 0.4, 0.7) → 8-bit
const PAGE_BG = { r: 10, g: 10, b: 10 };     // #0a0a0a
const TOL = 6;  // allow ±6 per channel for color-space rounding

// ----- output dir -----
const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
const OUT_DIR = resolve(__dirname, `results/${ts}`);
mkdirSync(OUT_DIR, { recursive: true });

// ----- server readiness -----
function waitForServer(port, timeoutMs = 15000) {
    return new Promise((resolve, reject) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (res) => {
                if (res.statusCode === 200) return resolve();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline)
                return reject(new Error(`Server not ready after ${timeoutMs}ms`));
            setTimeout(check, 300);
        };
        check();
    });
}

function nearly(a, b, tol = TOL) {
    return Math.abs(a.r - b.r) <= tol &&
           Math.abs(a.g - b.g) <= tol &&
           Math.abs(a.b - b.b) <= tol;
}

let browser;
const failures = [];
const logs = [];
const errors = [];
let finalSummary = {};

try {
    await waitForServer(opts.port);

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox',
            '--enable-unsafe-webgpu',
            '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11',
            '--disable-extensions',
            '--disable-background-networking',
            '--disable-default-apps',
            '--disable-sync',
            '--mute-audio',
        ],
    });

    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    const startTime = Date.now();
    const elapsed = () => ((Date.now() - startTime) / 1000).toFixed(2);

    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ elapsed: elapsed(), type: msg.type(), text });
        if (opts.verbose) console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
    });
    page.on('pageerror', (err) => {
        const text = err.message || String(err);
        errors.push(text);
        console.log(`  [PAGE_ERROR] ${text}`);
    });
    page.on('crash', () => {
        errors.push('Page crashed');
        console.log('  [CRASH] Page crashed!');
    });

    await page.goto(`http://127.0.0.1:${opts.port}/`, {
        waitUntil: 'domcontentloaded',
        timeout: 30000,
    });

    // Wait for window.rb3FrameCount ≥ PASS_FRAMES (or 60s timeout).
    const FRAME_TIMEOUT_MS = 60000;
    const deadline = Date.now() + FRAME_TIMEOUT_MS;
    let reachedFrames = 0;
    while (Date.now() < deadline) {
        reachedFrames = await page.evaluate(() => window.rb3FrameCount || 0);
        if (reachedFrames >= PASS_FRAMES) break;
        await new Promise(r => setTimeout(r, 500));
    }

    if (reachedFrames < PASS_FRAMES) {
        failures.push(`window.rb3FrameCount = ${reachedFrames} after ${FRAME_TIMEOUT_MS}ms (expected ≥ ${PASS_FRAMES})`);
    }

    // Allow one more RAF tick so the most recent frame's pixels are committed.
    await new Promise(r => setTimeout(r, 500));

    // Read the canvas pixels via Playwright's locator.screenshot — this uses
    // the browser's surface-capture pipeline which (unlike canvas.toDataURL /
    // ctx2d.drawImage(canvas)) DOES preserve the last-presented WebGPU frame.
    // The center pixel we sample MUST come from this readback too (re-doing
    // drawImage(canvas) in-page returns transparent), so we decode the PNG
    // ourselves in Node — pixelmatch already pulls pngjs as a dep.
    const canvasPath = resolve(OUT_DIR, 'canvas.png');
    let centerPx = null;
    try {
        const canvasLoc = page.locator('#rb3-canvas');
        await canvasLoc.screenshot({ path: canvasPath, omitBackground: false });
        const pngBuf = readFileSync(canvasPath);
        // Decode the PNG via pngjs (pulled in by pixelmatch).
        const { PNG } = await import('pngjs');
        const png = PNG.sync.read(pngBuf);
        const cx = Math.floor(png.width / 2);
        const cy = Math.floor(png.height / 2);
        const i = (cy * png.width + cx) * 4;
        centerPx = {
            r: png.data[i + 0],
            g: png.data[i + 1],
            b: png.data[i + 2],
            a: png.data[i + 3],
            w: png.width,
            h: png.height,
        };
    } catch (e) {
        failures.push(`Canvas screenshot/decode failed: ${e.message}`);
    }

    // Pixel-color assertions.
    if (centerPx) {
        if (nearly(centerPx, PAGE_BG)) {
            failures.push(`Center pixel ${JSON.stringify(centerPx)} matches page background (canvas never painted clear color)`);
        } else if (!nearly(centerPx, EXPECTED)) {
            failures.push(`Center pixel ${JSON.stringify(centerPx)} does not match expected clear ${JSON.stringify(EXPECTED)} (±${TOL})`);
        }
    }

    // Defense-in-depth: scan logs for WASM trap signatures.
    for (const err of errors) {
        failures.push(`pageerror: ${err}`);
    }
    const sigMatch = logs.find(l => /function signature mismatch|call_indirect type|RuntimeError/i.test(l.text));
    if (sigMatch) {
        failures.push(`WASM trap signature in console: ${sigMatch.text}`);
    }
    const bootErr = logs.find(l => l.text.includes('RB3 Web: boot error'));
    if (bootErr) {
        failures.push(`Boot error printed: ${bootErr.text}`);
    }

    // ----- summary -----
    finalSummary = {
        result: failures.length === 0 ? 'pass' : 'fail',
        port: opts.port,
        frames: reachedFrames,
        expected_frames: PASS_FRAMES,
        center_pixel: centerPx,
        expected_clear: EXPECTED,
        page_background: PAGE_BG,
        canvas_png: canvasPath,
        failures,
        log_count: logs.length,
        error_count: errors.length,
    };

    writeFileSync(resolve(OUT_DIR, 'console.jsonl'),
        logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT_DIR, 'summary.json'),
        JSON.stringify(finalSummary, null, 2));

    console.log('\n=== RB3 Web smoke test ===');
    if (failures.length === 0) {
        console.log(`PASS — ${reachedFrames} frames, center pixel ${JSON.stringify(centerPx)}`);
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
    console.log(`Results: ${OUT_DIR}`);
    process.exit(failures.length === 0 ? 0 : 1);
} catch (e) {
    console.error(`Error: ${e.message}`);
    if (e.stack && opts.verbose) console.error(e.stack);
    try {
        writeFileSync(resolve(OUT_DIR, 'console.jsonl'),
            logs.map(e => JSON.stringify(e)).join('\n') + '\n');
        writeFileSync(resolve(OUT_DIR, 'summary.json'),
            JSON.stringify({ result: 'error', message: e.message, ...finalSummary }, null, 2));
    } catch { /* swallow secondary write failures */ }
    process.exit(1);
} finally {
    if (browser) {
        try {
            await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]);
        } catch { /* swallow */ }
    }
}
