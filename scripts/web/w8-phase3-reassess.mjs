#!/usr/bin/env node
/**
 * RB3 Web — W8 Phase 3 reassessment capture.
 *
 * Targets the three previously-flagged "dim" screens:
 *   03_song_select     — song-row titles (primary dim complaint)
 *   04_part_difficulty — instrument labels
 *   07_gameplay_t15s   — HUD digit area (post W7-HUD SetGeomOwner fix)
 *
 * Saves screenshots to docs/sessions/web/screenshots/w8-phase3-reassess/
 * along with per-screen brightness analysis comparing to the Phase 3 threshold.
 *
 * Usage:
 *   node scripts/web/w8-phase3-reassess.mjs [--port 8430]
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port: parseInt(argv[argv.indexOf('--port') + 1] || '8430', 10) || 8430,
    verbose: argv.includes('--verbose'),
};

const BOOT_TIMEOUT_MS     = 300000;
const SPLASH_TIMEOUT_MS   = 180000;
const LOADSONG_TIMEOUT_MS = 240000;

const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR   = resolve(REPO_ROOT, 'docs/sessions/web/screenshots/w8-phase3-reassess');
mkdirSync(OUT_DIR, { recursive: true });

// Brightness thresholds from Phase 3 investigation
const BRIGHT_THRESHOLD = 0.6;   // > 0.6 normalised → "bright"
const DIM_THRESHOLD    = 0.4;   // < 0.4 normalised → "truly dim"
const BRIGHT_BYTE      = Math.round(BRIGHT_THRESHOLD * 255);  // 153
const DIM_BYTE         = Math.round(DIM_THRESHOLD    * 255);  // 102

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

/**
 * Full brightness analysis for a captured PNG.
 * Returns per-screen metrics suitable for the Phase 3 verdict.
 */
async function analyzeCanvas(page, path) {
    try {
        await page.locator('#rb3-canvas').screenshot({ path });
        const { PNG } = await import('pngjs');
        const png = PNG.sync.read(readFileSync(path));
        const { width, height, data } = png;
        const total = width * height;

        // Overall stats
        let painted = 0, totalR = 0, totalG = 0, totalB = 0;
        let brightPx = 0, dimPx = 0;

        for (let p = 0; p < data.length; p += 4) {
            const r = data[p], g = data[p+1], b = data[p+2];
            const luma = 0.299 * r + 0.587 * g + 0.114 * b;
            if (r > 12 || g > 12 || b > 12) painted++;
            totalR += r; totalG += g; totalB += b;
            if (luma >= BRIGHT_BYTE) brightPx++;
            if (luma <= DIM_BYTE)   dimPx++;
        }

        const avgR = Math.round(totalR / total);
        const avgG = Math.round(totalG / total);
        const avgB = Math.round(totalB / total);
        const avgLuma = 0.299 * avgR + 0.587 * avgG + 0.114 * avgB;

        // Horizontal stripe analysis — 40px bands, measuring bright-pixel %
        const stripeH = 40;
        const stripes = [];
        for (let y = 0; y < height; y += stripeH) {
            const rowEnd = Math.min(y + stripeH, height);
            let stripeR = 0, stripeG = 0, stripeB = 0, stripeBright = 0;
            const stripePx = rowEnd * width - y * width;
            for (let row = y; row < rowEnd; row++) {
                for (let x = 0; x < width; x++) {
                    const p = (row * width + x) * 4;
                    const r = data[p], g = data[p+1], b = data[p+2];
                    const luma = 0.299 * r + 0.587 * g + 0.114 * b;
                    stripeR += r; stripeG += g; stripeB += b;
                    if (luma >= BRIGHT_BYTE) stripeBright++;
                }
            }
            const sAvgR = Math.round(stripeR / stripePx);
            const sAvgG = Math.round(stripeG / stripePx);
            const sAvgB = Math.round(stripeB / stripePx);
            const sAvgL = (0.299*sAvgR + 0.587*sAvgG + 0.114*sAvgB).toFixed(1);
            const sBrightPct = ((100 * stripeBright) / stripePx).toFixed(2);
            stripes.push({ y, sAvgRGB: `${sAvgR},${sAvgG},${sAvgB}`, sAvgLuma: sAvgL, brightPct: sBrightPct });
        }

        return {
            w: width, h: height,
            paintedPct:   Number((100 * painted  / total).toFixed(2)),
            brightPct:    Number((100 * brightPx / total).toFixed(2)),
            dimPct:       Number((100 * dimPx    / total).toFixed(2)),
            avgRGB:       `${avgR},${avgG},${avgB}`,
            avgLuma:      avgLuma.toFixed(1),
            stripes,
        };
    } catch (e) {
        return { error: e.message };
    }
}

const getScreen     = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame      = (page) => page.evaluate(() => window.rb3FrameCount    || 0);
const getSongCount  = (page) => page.evaluate(() => window.rb3SongCount     || 0);

const elapsed = (() => { const t0 = Date.now(); return () => ((Date.now() - t0) / 1000).toFixed(2); })();

async function waitScreen(page, { targets = null, from = null, timeoutMs = 20000 }) {
    const deadline = Date.now() + timeoutMs;
    let s = await getScreen(page);
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await new Promise(r => setTimeout(r, 250));
    }
    return s;
}

const flow = [];
const errors = [];
let browser;

async function snap(page, filename, label) {
    const path = resolve(OUT_DIR, filename);
    const a    = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    const frame  = await getFrame(page);
    const entry  = { filename, label, screen, frame, ...a, path };
    flow.push(entry);
    console.log(
        `  SNAP [${filename}] screen='${screen}' frame=${frame}` +
        ` painted=${a.paintedPct}% bright=${a.brightPct}% dim=${a.dimPct}%` +
        ` avgRGB=${a.avgRGB} avgLuma=${a.avgLuma}`
    );
    return entry;
}

try {
    console.log(`Waiting for server on port ${opts.port}...`);
    await waitForServer(opts.port);
    console.log('Server ready.');

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--disable-background-networking',
            '--disable-default-apps', '--disable-sync', '--mute-audio',
        ],
    });
    const ctx  = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    page.on('console', (msg) => {
        const text = msg.text();
        if (opts.verbose || /screen:|booted|rb3App|WASM/.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => errors.push(err.message || String(err)));
    page.on('crash',     ()    => errors.push('Page crashed'));

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url}...`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Wait for app boot
    console.log('Waiting for rb3AppBooted...');
    let appBooted = 0;
    let deadline  = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`App booted: ${appBooted} (${elapsed()}s)`);

    // splash_screen
    console.log('Waiting for splash_screen...');
    let s = '';
    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    await new Promise(r => setTimeout(r, 2000));
    s = await getScreen(page);
    console.log(`Screen settled: '${s}' (${elapsed()}s)`);

    // splash → main_hub
    console.log('\n[splash→main_hub]...');
    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen')
        s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);
    console.log(`main_hub: '${s}' (${elapsed()}s)`);

    // main_hub → song_select
    if (s === 'main_hub_screen') {
        console.log('\n[main_hub→song_select]...');
        for (let i = 0; i < 5; i++) {
            await page.keyboard.press('Enter');
            await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            const cur = await getScreen(page);
            console.log(`  Confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen')
            s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 4000));
    s = await getScreen(page);

    // TARGET 1: song_select — song-row titles
    console.log('\n--- TARGET 1: song_select (song-row brightness) ---');
    const songSnap = await snap(page, '03_song_select.png', 'song_select_screen');
    console.log(`song_select: '${s}' songCount=${await getSongCount(page)} (${elapsed()}s)`);

    // song_select → part_difficulty
    if (s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        console.log('[song_select→part_difficulty]...');
        for (let i = 0; i < 4; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 120));
            await page.keyboard.up('Enter');
            const ns = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
            const cur = await getScreen(page);
            console.log(`  song confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur === 'part_difficulty_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);

    // TARGET 2: part_difficulty — instrument labels
    console.log('\n--- TARGET 2: part_difficulty (instrument labels) ---');
    const partSnap = await snap(page, '04_part_difficulty.png', 'part_difficulty_screen');
    console.log(`part_difficulty: '${s}' (${elapsed()}s)`);

    // part_difficulty → game_screen
    if (s === 'part_difficulty_screen') {
        console.log('\n[part_difficulty→game_screen]...');
        for (let i = 0; i < 5; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 150));
            await page.keyboard.up('Enter');
            await new Promise(r => setTimeout(r, 1200));
            const cur = await getScreen(page);
            console.log(`  part confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur !== 'part_difficulty_screen') { s = cur; break; }
        }
        const g = await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
        s = await getScreen(page);
        console.log(`  after crossing: screen='${s}' (${elapsed()}s)`);
    }

    // Wait for gameplay rendering to settle
    await new Promise(r => setTimeout(r, 15000));
    s = await getScreen(page);

    // TARGET 3: gameplay at t=15s — HUD digit area
    console.log('\n--- TARGET 3: gameplay_t15s (HUD digit area) ---');
    const hudSnap = await snap(page, '07_gameplay_t15s.png', 'gameplay_15s');
    console.log(`gameplay_t15s: '${s}' (${elapsed()}s)`);

    // ---- Verdict ----
    console.log('\n=== PHASE 3 BRIGHTNESS VERDICT ===');

    const verdicts = [];
    for (const { filename, brightPct, dimPct, avgRGB, avgLuma } of [songSnap, partSnap, hudSnap]) {
        const status = brightPct >= 5 ? 'BRIGHT'
                     : dimPct   >= 80 ? 'DIM'
                     : 'MIXED';
        verdicts.push({ filename, status, brightPct, dimPct, avgRGB, avgLuma });
        console.log(`  ${filename}: ${status}  bright=${brightPct}% dim=${dimPct}% avgRGB=${avgRGB} avgLuma=${avgLuma}`);
    }

    // Compare against w7-phase3 baseline if available
    const baselinePath = resolve(REPO_ROOT, 'docs/sessions/web/screenshots/w7-phase3/capture-summary.json');
    let baseline = null;
    try { baseline = JSON.parse(readFileSync(baselinePath, 'utf8')); } catch { /* no baseline */ }

    if (baseline) {
        console.log('\n--- vs w7-phase3 baseline ---');
        for (const bEntry of baseline.flow) {
            const cur = flow.find(f => f.filename === bEntry.filename);
            if (!cur) continue;
            console.log(`  ${bEntry.filename}: avgRGB ${bEntry.avgRGB} → ${cur.avgRGB}  bright ${cur.brightPct}%`);
        }
    }

    // Summary
    const overallBright = verdicts.every(v => v.status === 'BRIGHT' || v.status === 'MIXED');
    const overallDim    = verdicts.some(v => v.status === 'DIM');
    const overallStatus = overallDim ? 'STILL DIM' : 'RESOLVED';
    console.log(`\nOVERALL VERDICT: ${overallStatus}`);

    const summary = {
        captured: new Date().toISOString(),
        commit: '3daf7100',
        engine_pin: 'e6c8f86',
        port: opts.port,
        appBooted,
        flow,
        verdicts,
        overallStatus,
        errors: errors.slice(0, 10),
    };
    writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify(summary, null, 2));
    console.log(`\nSaved ${flow.length} screenshots to ${OUT_DIR}`);
    process.exit(0);

} catch (e) {
    console.error(`Error: ${e.message}\n${e.stack}`);
    try {
        writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify({ result: 'error', message: e.message, flow, errors }, null, 2));
    } catch { /* ignore */ }
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch { }
    }
}
