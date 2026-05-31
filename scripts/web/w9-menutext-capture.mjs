#!/usr/bin/env node
/**
 * W9 — menu-text brightness/quality capture.
 * Captures main_hub + song_select, measures brightness, and compares against
 * the retail Wii reference. Output dir overridable via --out.
 *
 *   node scripts/web/w9-menutext-capture.mjs --port 8838 --out <dir-name>
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port: parseInt(argv[argv.indexOf('--port') + 1] || '8838', 10) || 8838,
    out: argv[argv.indexOf('--out') + 1] || 'w9-menutext',
};
const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR = resolve(REPO_ROOT, 'docs/sessions/web/screenshots', opts.out);
mkdirSync(OUT_DIR, { recursive: true });

const BRIGHT_BYTE = 153, DIM_BYTE = 102;

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
                if (r.statusCode === 200) return res(); retry();
            }).on('error', retry);
        };
        const retry = () => { if (Date.now() > deadline) return rej(new Error('no server')); setTimeout(check, 300); };
        check();
    });
}

function statsFromPNG(png) {
    const { width, height, data } = png;
    const total = width * height;
    let painted = 0, tR = 0, tG = 0, tB = 0, bright = 0, dim = 0;
    for (let p = 0; p < data.length; p += 4) {
        const r = data[p], g = data[p+1], b = data[p+2];
        const luma = 0.299*r + 0.587*g + 0.114*b;
        if (r > 12 || g > 12 || b > 12) painted++;
        tR += r; tG += g; tB += b;
        if (luma >= BRIGHT_BYTE) bright++;
        if (luma <= DIM_BYTE) dim++;
    }
    const avgR = Math.round(tR/total), avgG = Math.round(tG/total), avgB = Math.round(tB/total);
    return {
        w: width, h: height,
        paintedPct: Number((100*painted/total).toFixed(2)),
        brightPct: Number((100*bright/total).toFixed(2)),
        dimPct: Number((100*dim/total).toFixed(2)),
        avgRGB: `${avgR},${avgG},${avgB}`,
        avgLuma: Number((0.299*avgR + 0.587*avgG + 0.114*avgB).toFixed(1)),
    };
}

async function analyzeCanvas(page, path) {
    await page.locator('#rb3-canvas').screenshot({ path });
    const { PNG } = await import('pngjs');
    return statsFromPNG(PNG.sync.read(readFileSync(path)));
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame  = (page) => page.evaluate(() => window.rb3FrameCount || 0);

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
let browser;
async function snap(page, filename, label) {
    const path = resolve(OUT_DIR, filename);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page), frame = await getFrame(page);
    flow.push({ filename, label, screen, frame, ...a, path });
    console.log(`  SNAP [${filename}] screen='${screen}' frame=${frame} painted=${a.paintedPct}% bright=${a.brightPct}% dim=${a.dimPct}% avgRGB=${a.avgRGB} avgLuma=${a.avgLuma}`);
}

try {
    await waitForServer(opts.port);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--mute-audio'],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    page.on('console', (msg) => { if (/screen:|booted|GPU ready|preferred surface/.test(msg.text())) console.log(`  [console] ${msg.text()}`); });

    await page.goto(`http://127.0.0.1:${opts.port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    let deadline = Date.now() + 300000, appBooted = 0;
    while (Date.now() < deadline) { appBooted = await page.evaluate(() => window.rb3AppBooted || 0); if (appBooted >= 1) break; await new Promise(r => setTimeout(r, 500)); }
    console.log(`App booted: ${appBooted}`);

    let s = '';
    deadline = Date.now() + 180000;
    while (Date.now() < deadline) { s = await getScreen(page); if (s === 'splash_screen') break; await new Promise(r => setTimeout(r, 500)); }
    await new Promise(r => setTimeout(r, 2000));

    // splash → main_hub
    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) { await page.keyboard.press('Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3500));

    console.log('\n--- main_hub ---');
    await snap(page, '02_main_hub.png', 'main_hub_screen');

    // main_hub → song_select
    s = await getScreen(page);
    if (s === 'main_hub_screen') {
        for (let i = 0; i < 5; i++) {
            await page.keyboard.press('Enter');
            await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            const cur = await getScreen(page);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 4000));

    console.log('\n--- song_select ---');
    await snap(page, '03_song_select.png', 'song_select_screen');

    writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify({ captured: new Date().toISOString(), port: opts.port, out: opts.out, flow }, null, 2));
    console.log(`\nSaved ${flow.length} screenshots to ${OUT_DIR}`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}\n${e.stack}`);
    try { writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify({ result: 'error', message: e.message, flow }, null, 2)); } catch {}
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {} }
}
