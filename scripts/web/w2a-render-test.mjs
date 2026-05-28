#!/usr/bin/env node
/**
 * RB3 Web — W2a milo-render acceptance check.
 *
 * Loads index.html?milo=<path> in headless Chromium, waits for
 * window.rb3MilosLoaded >= 1 && window.rb3FrameCount >= 30, screenshots the
 * canvas, and reports the center pixel + a few sampled pixels. Asserts the
 * canvas shows geometry (NOT just the clear color and NOT the page background).
 *
 * This is the minimal stand-in the W2 doc calls for until W2b task 7 extends
 * smoke-test.mjs with --milo / --diff-against / pixelmatch.
 *
 * Usage:
 *   node scripts/web/w2a-render-test.mjs --milo ui/track/gen/gem_smasher_guitar.milo_xbox [--port 8421] [--verbose]
 *
 * Output: scripts/web/results/w2a/<milo-basename>/{canvas.png, console.jsonl, summary.json}
 * Exits 0 on PASS, 1 on FAIL.
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname, basename } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));

const argv = process.argv.slice(2);
const opts = {
    port:    parseInt(argv[argv.indexOf('--port') + 1] || '8421', 10) || 8421,
    milo:    argv[argv.indexOf('--milo') + 1] || 'ui/track/gen/gem_smasher_guitar.milo_xbox',
    verbose: argv.includes('--verbose'),
};

const PASS_FRAMES = 30;
const LOAD_TIMEOUT_MS = 300000; // 5 min for large milos
const FRAME_TIMEOUT_MS = 60000;
// Clear color from BandRnd::SetClearColor(0.12, 0.14, 0.18) -> ~(31, 36, 46).
const CLEAR = { r: 31, g: 36, b: 46 };
const PAGE_BG = { r: 10, g: 10, b: 10 };  // #0a0a0a
const TOL = 10;

const miloBase = basename(opts.milo).replace(/\.milo_\w+$/, '');
const OUT_DIR = resolve(__dirname, `results/w2a/${miloBase}`);
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

    const url = `http://127.0.0.1:${opts.port}/?milo=${encodeURIComponent(opts.milo)}`;
    console.log(`Loading ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Wait for milo to finish loading.
    let milosLoaded = 0;
    let deadline = Date.now() + LOAD_TIMEOUT_MS;
    while (Date.now() < deadline) {
        milosLoaded = await page.evaluate(() => window.rb3MilosLoaded || 0);
        if (milosLoaded >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    if (milosLoaded < 1) failures.push(`rb3MilosLoaded = ${milosLoaded} after ${LOAD_TIMEOUT_MS}ms (expected >= 1)`);

    // Wait for the render loop to advance past PASS_FRAMES.
    let frames = 0;
    deadline = Date.now() + FRAME_TIMEOUT_MS;
    while (Date.now() < deadline) {
        frames = await page.evaluate(() => window.rb3FrameCount || 0);
        if (frames >= PASS_FRAMES) break;
        await new Promise(r => setTimeout(r, 500));
    }
    if (frames < PASS_FRAMES) failures.push(`rb3FrameCount = ${frames} after ${FRAME_TIMEOUT_MS}ms (expected >= ${PASS_FRAMES})`);

    await new Promise(r => setTimeout(r, 500));

    // Capture + decode the canvas.
    const canvasPath = resolve(OUT_DIR, 'canvas.png');
    let centerPx = null, samples = [], nonClear = 0, total = 0;
    try {
        await page.locator('#rb3-canvas').screenshot({ path: canvasPath, omitBackground: false });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(canvasPath));
        const px = (x, y) => { const i = (y * png.width + x) * 4; return { x, y, r: png.data[i], g: png.data[i+1], b: png.data[i+2], a: png.data[i+3] }; };
        const cx = png.width >> 1, cy = png.height >> 1;
        centerPx = px(cx, cy);
        samples = [px(cx, cy), px(cx >> 1, cy), px(cx + (cx >> 1), cy), px(cx, cy >> 1), px(cx, cy + (cy >> 1))];
        // Count pixels that differ from the clear color (= rendered geometry).
        total = png.width * png.height;
        for (let i = 0; i < png.data.length; i += 4) {
            const r = png.data[i], g = png.data[i+1], b = png.data[i+2];
            if (Math.abs(r - CLEAR.r) > 14 || Math.abs(g - CLEAR.g) > 14 || Math.abs(b - CLEAR.b) > 14) nonClear++;
        }
    } catch (e) {
        failures.push(`Canvas screenshot/decode failed: ${e.message}`);
    }

    // Geometry must be visible: a meaningful fraction of pixels differ from the
    // clear color, and the canvas is NOT the page background.
    const nonClearPct = total ? (100 * nonClear / total) : 0;
    if (centerPx && nearly(centerPx, PAGE_BG)) failures.push(`Center pixel ${JSON.stringify(centerPx)} matches page background (canvas never painted)`);
    if (nonClearPct < 0.5) failures.push(`Only ${nonClearPct.toFixed(2)}% of pixels differ from clear color (expected >= 0.5% — geometry not visible)`);

    for (const err of errors) failures.push(`pageerror: ${err}`);
    const trap = logs.find(l => /function signature mismatch|call_indirect type|RuntimeError|abort\(/i.test(l.text));
    if (trap) failures.push(`WASM trap signature in console: ${trap.text}`);
    const bootErr = logs.find(l => l.text.includes('RB3 Web: boot error'));
    if (bootErr) failures.push(`Boot error printed: ${bootErr.text}`);
    const stubHits = logs.filter(l => l.text.includes('[rb3-stub]')).map(l => l.text);

    summary = {
        result: failures.length === 0 ? 'pass' : 'fail',
        milo: opts.milo,
        milosLoaded, frames, expected_frames: PASS_FRAMES,
        center_pixel: centerPx,
        sampled_pixels: samples,
        nonclear_pct: Number(nonClearPct.toFixed(2)),
        clear_color: CLEAR, page_background: PAGE_BG,
        canvas_png: canvasPath,
        stub_hits: [...new Set(stubHits)],
        failures,
        log_count: logs.length, error_count: errors.length,
    };
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT_DIR, 'summary.json'), JSON.stringify(summary, null, 2));

    console.log('\n=== RB3 Web W2a render test ===');
    if (failures.length === 0) {
        console.log(`PASS — milo=${opts.milo} milosLoaded=${milosLoaded} frames=${frames} nonClear=${nonClearPct.toFixed(2)}% center=${JSON.stringify(centerPx)}`);
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
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
