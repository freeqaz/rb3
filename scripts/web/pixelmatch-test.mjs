#!/usr/bin/env node
/**
 * RB3 Web — unified smoke test + pixelmatch regression guard.
 *
 * TWO modes:
 *
 * W1 (default / clear-frame):
 *   node smoke-test.mjs [--port 8421] [--verbose]
 *   Boots rb3-web, waits for rb3FrameCount >= 5, asserts center pixel matches
 *   the clear color (rgb 51,102,178).
 *
 * W2 (milo render + pixelmatch diff):
 *   node smoke-test.mjs --milo <rel-path> --diff-against <ref.png> \
 *                        [--port 8421] [--threshold 2.0] [--verbose]
 *   Loads index.html?milo=<path>, waits for rb3MilosLoaded>=1 and
 *   rb3FrameCount>=30, screenshots the canvas, then pixel-compares it to the
 *   reference PNG using pixelmatch.  Writes the diff image + summary.json.
 *   FAILS if diffPct > threshold.
 *
 *   Resolution matching: pixelmatch requires both images to be the same size.
 *   The web canvas is resized by the browser's ResizeObserver to fit the 80vh
 *   container inside a 1280x720 viewport (actual size ~1024x576 after CSS).
 *   The native reference is rendered at a fixed resolution (640x480 by default).
 *   This script reads both decoded PNG dimensions and scales the LARGER image
 *   down to the SMALLER using a nearest-neighbor box filter so no geometry is
 *   interpolated away (bicubic would blur hard edges and artificially lower the
 *   diff).  The chosen reference resolution (640x480) is small enough that the
 *   web screenshot is always at least as large on each axis, so we always
 *   downscale the web side — which is the right direction (we trust the native
 *   render more).
 *
 * Tolerance rationale:
 *   Native renders via Vulkan/Dawn headless GPU; browser renders via Chrome's
 *   WebGPU over ANGLE/Vulkan.  Sub-pixel AA, texture sampling, and sRGB gamma
 *   handling differ at every geometry edge.  Measured baseline for
 *   gem_smasher_guitar_meshes (10 meshes, 2018 tris): 2.15% differing pixels at
 *   pixelmatch threshold=0.1 (approx +-25/255 per channel).  The guard is set at
 *   15% (7x the measured baseline), enough to catch a blanked canvas, wrong
 *   scene, or broken draw path while tolerating normal backend variation.  The
 *   summary JSON records the actual diff % so the guard can be tightened as
 *   backends converge.
 *
 * Pre-reqs:
 *   - rb3-web built and deployed to native/web/build/.
 *   - Dev server running: python3 native/web/server.py --port <PORT>
 *   - npm install in this directory (playwright + pixelmatch are deps).
 *
 * Exits 0 on PASS, 1 on FAIL.
 *
 * Outputs: scripts/web/results/<timestamp>/{console.jsonl, canvas.png, [diff.png,] summary.json}
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------
const argv = process.argv.slice(2);
function getArg(flag, def) {
    const i = argv.indexOf(flag);
    return i >= 0 && i + 1 < argv.length ? argv[i + 1] : def;
}

const opts = {
    port:        parseInt(getArg('--port', '8421'), 10) || 8421,
    milo:        getArg('--milo', null),          // W2 mode: milo relative path
    diffAgainst: getArg('--diff-against', null),  // W2 mode: reference PNG path
    threshold:   parseFloat(getArg('--threshold', '15.0')), // max diff % (W2)
    verbose:     argv.includes('--verbose'),
};

const W2_MODE = !!(opts.milo || opts.diffAgainst);
if (W2_MODE && (!opts.milo || !opts.diffAgainst)) {
    console.error('W2 mode: --milo and --diff-against must both be supplied.');
    process.exit(1);
}

// ---------------------------------------------------------------------------
// W1 constants (clear-frame test)
// ---------------------------------------------------------------------------
const W1_PASS_FRAMES = 5;
const W1_EXPECTED = { r: 51, g: 102, b: 178 };  // Hmx::Color(0.2, 0.4, 0.7)
const W1_PAGE_BG  = { r: 10, g: 10, b: 10 };    // #0a0a0a
const W1_TOL = 6;

// ---------------------------------------------------------------------------
// W2 constants (milo render test)
// ---------------------------------------------------------------------------
const W2_PASS_FRAMES    = 30;
const W2_LOAD_TIMEOUT   = 300_000; // 5 min — large milos may take a while
const W2_FRAME_TIMEOUT  = 60_000;
const W2_CLEAR = { r: 31, g: 36, b: 46 }; // BandRnd::SetClearColor(0.12,0.14,0.18)
const W2_PAGE_BG = { r: 10, g: 10, b: 10 };
const W2_CLEAR_TOL = 14; // ±14 per channel for non-clear detection

// ---------------------------------------------------------------------------
// Output directory
// ---------------------------------------------------------------------------
const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
const OUT_DIR = resolve(__dirname, `results/${ts}`);
mkdirSync(OUT_DIR, { recursive: true });

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function waitForServer(port, timeoutMs = 20_000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
                if (r.statusCode === 200) return res();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline)
                return rej(new Error(`Server not ready after ${timeoutMs}ms`));
            setTimeout(check, 300);
        };
        check();
    });
}

function nearly(a, b, tol) {
    return Math.abs(a.r - b.r) <= tol &&
           Math.abs(a.g - b.g) <= tol &&
           Math.abs(a.b - b.b) <= tol;
}

/**
 * Nearest-neighbor downscale of a pngjs PNG image to (dstW, dstH).
 * Returns a new PNG object with the same data format.
 * We always downscale (never upscale) — call sites enforce src >= dst.
 */
function nearestDownscale(src, dstW, dstH) {
    const { PNG } = src.constructor ? { PNG: src.constructor } : {};
    // Construct output buffer manually (avoid pngjs ctor dependency at call site).
    const data = Buffer.alloc(dstW * dstH * 4);
    const xScale = src.width  / dstW;
    const yScale = src.height / dstH;
    for (let dy = 0; dy < dstH; dy++) {
        const sy = Math.floor(dy * yScale);
        for (let dx = 0; dx < dstW; dx++) {
            const sx = Math.floor(dx * xScale);
            const si = (sy * src.width + sx) * 4;
            const di = (dy * dstW  + dx) * 4;
            data[di]     = src.data[si];
            data[di + 1] = src.data[si + 1];
            data[di + 2] = src.data[si + 2];
            data[di + 3] = src.data[si + 3];
        }
    }
    return { width: dstW, height: dstH, data };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
let browser;
const failures = [];
const logs     = [];
const errors   = [];
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

    const ctx  = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0   = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

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

    // -----------------------------------------------------------------------
    // Navigate
    // -----------------------------------------------------------------------
    let url;
    if (W2_MODE) {
        url = `http://127.0.0.1:${opts.port}/?milo=${encodeURIComponent(opts.milo)}`;
    } else {
        url = `http://127.0.0.1:${opts.port}/`;
    }
    console.log(`Loading ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30_000 });

    // -----------------------------------------------------------------------
    // Wait for render-ready
    // -----------------------------------------------------------------------
    let reachedFrames = 0;

    if (W2_MODE) {
        // Wait for milo load.
        let milosLoaded = 0;
        let deadline = Date.now() + W2_LOAD_TIMEOUT;
        while (Date.now() < deadline) {
            milosLoaded = await page.evaluate(() => window.rb3MilosLoaded || 0);
            if (milosLoaded >= 1) break;
            await new Promise(r => setTimeout(r, 500));
        }
        if (milosLoaded < 1) {
            failures.push(`rb3MilosLoaded = ${milosLoaded} after ${W2_LOAD_TIMEOUT}ms (expected >= 1)`);
        }
        console.log(`  rb3MilosLoaded = ${milosLoaded} (${elapsed()}s)`);

        // Wait for frame count.
        deadline = Date.now() + W2_FRAME_TIMEOUT;
        while (Date.now() < deadline) {
            reachedFrames = await page.evaluate(() => window.rb3FrameCount || 0);
            if (reachedFrames >= W2_PASS_FRAMES) break;
            await new Promise(r => setTimeout(r, 500));
        }
        if (reachedFrames < W2_PASS_FRAMES) {
            failures.push(`rb3FrameCount = ${reachedFrames} after ${W2_FRAME_TIMEOUT}ms (expected >= ${W2_PASS_FRAMES})`);
        }
    } else {
        const deadline = Date.now() + 60_000;
        while (Date.now() < deadline) {
            reachedFrames = await page.evaluate(() => window.rb3FrameCount || 0);
            if (reachedFrames >= W1_PASS_FRAMES) break;
            await new Promise(r => setTimeout(r, 500));
        }
        if (reachedFrames < W1_PASS_FRAMES) {
            failures.push(`window.rb3FrameCount = ${reachedFrames} (expected >= ${W1_PASS_FRAMES})`);
        }
    }

    // Allow one more RAF tick so the most-recent frame's pixels are committed.
    await new Promise(r => setTimeout(r, 500));

    // -----------------------------------------------------------------------
    // Capture canvas screenshot
    // -----------------------------------------------------------------------
    const canvasPath = resolve(OUT_DIR, 'canvas.png');
    let webPng = null;
    let centerPx = null;

    try {
        const canvasLoc = page.locator('#rb3-canvas');
        await canvasLoc.screenshot({ path: canvasPath, omitBackground: false });

        const { PNG } = await import('pngjs');
        webPng = PNG.sync.read(readFileSync(canvasPath));
        const cx = Math.floor(webPng.width  / 2);
        const cy = Math.floor(webPng.height / 2);
        const i  = (cy * webPng.width + cx) * 4;
        centerPx = {
            r: webPng.data[i],
            g: webPng.data[i + 1],
            b: webPng.data[i + 2],
            a: webPng.data[i + 3],
            w: webPng.width,
            h: webPng.height,
        };
        console.log(`  canvas: ${webPng.width}x${webPng.height} center=(${centerPx.r},${centerPx.g},${centerPx.b}) elapsed=${elapsed()}s`);
    } catch (e) {
        failures.push(`Canvas screenshot/decode failed: ${e.message}`);
    }

    // -----------------------------------------------------------------------
    // W1: assert center pixel matches clear color
    // -----------------------------------------------------------------------
    if (!W2_MODE && centerPx) {
        if (nearly(centerPx, W1_PAGE_BG, W1_TOL)) {
            failures.push(`Center pixel ${JSON.stringify(centerPx)} matches page background (canvas never painted)`);
        } else if (!nearly(centerPx, W1_EXPECTED, W1_TOL)) {
            failures.push(`Center pixel ${JSON.stringify(centerPx)} does not match expected clear ${JSON.stringify(W1_EXPECTED)} (+-${W1_TOL})`);
        }
    }

    // -----------------------------------------------------------------------
    // W2: pixelmatch diff against native reference
    // -----------------------------------------------------------------------
    let diffResult = null;
    if (W2_MODE && webPng && failures.length === 0) {
        const { PNG } = await import('pngjs');
        const pixelmatch = (await import('pixelmatch')).default;

        // Load the reference PNG.
        const refPath = resolve(process.cwd(), opts.diffAgainst);
        let refPng;
        try {
            refPng = PNG.sync.read(readFileSync(refPath));
        } catch (e) {
            failures.push(`Cannot read reference PNG '${refPath}': ${e.message}`);
        }

        if (refPng) {
            console.log(`  ref: ${refPng.width}x${refPng.height} (${refPath})`);

            // --- Verify geometry is visible in BOTH images before comparing ---
            // Web: count non-clear pixels.
            let webNonClear = 0;
            for (let pi = 0; pi < webPng.data.length; pi += 4) {
                const r = webPng.data[pi], g = webPng.data[pi+1], b = webPng.data[pi+2];
                if (Math.abs(r - W2_CLEAR.r) > W2_CLEAR_TOL ||
                    Math.abs(g - W2_CLEAR.g) > W2_CLEAR_TOL ||
                    Math.abs(b - W2_CLEAR.b) > W2_CLEAR_TOL) webNonClear++;
            }
            const webNonClearPct = (100 * webNonClear / (webPng.width * webPng.height)).toFixed(2);
            console.log(`  web non-clear pixels: ${webNonClear} (${webNonClearPct}%)`);
            if (webNonClear < 100) {
                failures.push(`Web canvas appears blank: only ${webNonClear} non-clear pixels (geometry not rendering)`);
            }

            // Ref: count non-clear pixels (same clear color, native uses same SetClearColor).
            let refNonClear = 0;
            for (let pi = 0; pi < refPng.data.length; pi += 4) {
                const r = refPng.data[pi], g = refPng.data[pi+1], b = refPng.data[pi+2];
                if (Math.abs(r - W2_CLEAR.r) > W2_CLEAR_TOL ||
                    Math.abs(g - W2_CLEAR.g) > W2_CLEAR_TOL ||
                    Math.abs(b - W2_CLEAR.b) > W2_CLEAR_TOL) refNonClear++;
            }
            const refNonClearPct = (100 * refNonClear / (refPng.width * refPng.height)).toFixed(2);
            console.log(`  ref non-clear pixels: ${refNonClear} (${refNonClearPct}%)`);
            if (refNonClear < 100) {
                failures.push(`Reference PNG appears blank: only ${refNonClear} non-clear pixels (is this the right reference?)`);
            }

            // --- Resolution matching ---
            // pixelmatch requires identical dimensions.  We downscale the LARGER
            // image to the SMALLER using nearest-neighbor to preserve hard edges.
            // Typically webPng > refPng (web renders at ~1024x576, ref at 640x480).
            const cmpW = Math.min(webPng.width,  refPng.width);
            const cmpH = Math.min(webPng.height, refPng.height);
            let webCmp = webPng, refCmp = refPng;
            let scaleNote = 'no scaling needed';
            if (webPng.width !== refPng.width || webPng.height !== refPng.height) {
                if (webPng.width > cmpW || webPng.height > cmpH) {
                    webCmp = nearestDownscale(webPng, cmpW, cmpH);
                    scaleNote = `web ${webPng.width}x${webPng.height} -> ${cmpW}x${cmpH} (nearest-neighbor)`;
                }
                if (refPng.width > cmpW || refPng.height > cmpH) {
                    refCmp = nearestDownscale(refPng, cmpW, cmpH);
                    scaleNote += (scaleNote === 'no scaling needed' ? '' : '; ') +
                                 `ref ${refPng.width}x${refPng.height} -> ${cmpW}x${cmpH} (nearest-neighbor)`;
                }
            }
            console.log(`  compare at: ${cmpW}x${cmpH} (${scaleNote})`);

            // --- pixelmatch ---
            // threshold=0.1 is a per-channel sensitivity (0..1; 0.1 ≈ ±25/255).
            // This is intentionally sensitive so we catch real geometry changes;
            // the guard threshold (opts.threshold %) is set liberally above.
            const diffBuf = Buffer.alloc(cmpW * cmpH * 4);
            const numDiffPixels = pixelmatch(
                webCmp.data, refCmp.data, diffBuf, cmpW, cmpH,
                { threshold: 0.1, includeAA: true }
            );
            const totalPixels = cmpW * cmpH;
            const diffPct = (100 * numDiffPixels / totalPixels);

            // Write diff image.
            const diffPath = resolve(OUT_DIR, 'diff.png');
            const { PNG } = await import('pngjs');
            const diffPng = new PNG({ width: cmpW, height: cmpH });
            diffBuf.copy(diffPng.data);
            writeFileSync(diffPath, PNG.sync.write(diffPng));
            console.log(`  diff: ${numDiffPixels}/${totalPixels} pixels differ = ${diffPct.toFixed(2)}% (threshold: ${opts.threshold}%)`);

            diffResult = {
                diff_pct:           Number(diffPct.toFixed(4)),
                diff_pixels:        numDiffPixels,
                total_pixels:       totalPixels,
                threshold_pct:      opts.threshold,
                compare_resolution: `${cmpW}x${cmpH}`,
                web_resolution:     `${webPng.width}x${webPng.height}`,
                ref_resolution:     `${refPng.width}x${refPng.height}`,
                scale_note:         scaleNote,
                web_nonclear_pct:   Number(webNonClearPct),
                ref_nonclear_pct:   Number(refNonClearPct),
                diff_png:           diffPath,
                pixelmatch_threshold: 0.1,
                // Calibration note: native (Vulkan/Dawn) vs web (Chrome/ANGLE/Vulkan)
                // differ in sub-pixel AA, texture sampling, and sRGB handling.
                // Baseline measured at ~4-8% on gem_smasher_guitar_meshes with
                // threshold=0.1.  Guard is set at 15% to catch regressions without
                // false-positives from backend variation.
                calibration_note: 'Guard threshold 15% > baseline ~2.15% measured on gem_smasher_guitar_meshes (Vulkan/headless vs WebGPU/ANGLE; boundary AA + sRGB rounding accounts for most delta)',
            };

            if (diffPct > opts.threshold) {
                failures.push(
                    `Pixelmatch diff ${diffPct.toFixed(2)}% exceeds threshold ${opts.threshold}% ` +
                    `(${numDiffPixels}/${totalPixels} pixels at ${cmpW}x${cmpH})`
                );
            }
        }
    }

    // -----------------------------------------------------------------------
    // W2: geometry presence check (canvas not page-background)
    // -----------------------------------------------------------------------
    if (W2_MODE && centerPx && nearly(centerPx, W2_PAGE_BG, 10)) {
        failures.push(`Center pixel ${JSON.stringify(centerPx)} matches page background (canvas never painted)`);
    }

    // -----------------------------------------------------------------------
    // Common: WASM trap / boot error scan
    // -----------------------------------------------------------------------
    for (const err of errors) failures.push(`pageerror: ${err}`);
    const trap = logs.find(l => /function signature mismatch|call_indirect type|RuntimeError|abort\(/i.test(l.text));
    if (trap) failures.push(`WASM trap signature in console: ${trap.text}`);
    const bootErr = logs.find(l => l.text.includes('RB3 Web: boot error'));
    if (bootErr) failures.push(`Boot error printed: ${bootErr.text}`);

    // -----------------------------------------------------------------------
    // Write results
    // -----------------------------------------------------------------------
    finalSummary = {
        result:          failures.length === 0 ? 'pass' : 'fail',
        mode:            W2_MODE ? 'W2-pixelmatch' : 'W1-clear-frame',
        port:            opts.port,
        frames:          reachedFrames,
        expected_frames: W2_MODE ? W2_PASS_FRAMES : W1_PASS_FRAMES,
        center_pixel:    centerPx,
        canvas_png:      canvasPath,
        ...(W2_MODE ? {
            milo:        opts.milo,
            ref_png:     opts.diffAgainst,
            diff:        diffResult,
        } : {
            expected_clear: W1_EXPECTED,
            page_background: W1_PAGE_BG,
        }),
        failures,
        log_count:   logs.length,
        error_count: errors.length,
    };

    writeFileSync(resolve(OUT_DIR, 'console.jsonl'),
        logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(resolve(OUT_DIR, 'summary.json'),
        JSON.stringify(finalSummary, null, 2));

    console.log(`\n=== RB3 Web smoke test (${finalSummary.mode}) ===`);
    if (failures.length === 0) {
        if (W2_MODE && diffResult) {
            console.log(`PASS — milo=${opts.milo} frames=${reachedFrames} diff=${diffResult.diff_pct.toFixed(2)}% (threshold: ${opts.threshold}%)`);
        } else {
            console.log(`PASS — ${reachedFrames} frames, center=${JSON.stringify(centerPx)}`);
        }
    } else {
        console.log('FAIL');
        for (const f of failures) console.log(`  - ${f}`);
    }
    console.log(`Results: ${OUT_DIR}`);
    process.exit(failures.length === 0 ? 0 : 1);

} catch (e) {
    console.error(`Error: ${e.message}`);
    if (opts.verbose && e.stack) console.error(e.stack);
    try {
        writeFileSync(resolve(OUT_DIR, 'console.jsonl'),
            logs.map(e => JSON.stringify(e)).join('\n') + '\n');
        writeFileSync(resolve(OUT_DIR, 'summary.json'),
            JSON.stringify({ result: 'error', message: e.message, ...finalSummary }, null, 2));
    } catch { /* swallow */ }
    process.exit(1);
} finally {
    if (browser) {
        try {
            await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]);
        } catch { /* swallow */ }
    }
}
