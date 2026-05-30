#!/usr/bin/env node
/**
 * RB3 Web — W7 HUD-bar capture.
 * Same nav flow as post-v2-capture / w5p3-capture; output redirected so
 * the post-fix shots sit alongside post-v2-sweep for side-by-side
 * comparison of the gameplay HUD digit slots and meter fills.
 *
 * Usage: node scripts/web/w8-meters-capture.mjs [--port 8580]
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port: parseInt(argv[argv.indexOf('--port') + 1] || '8719', 10) || 8580,
    verbose: argv.includes('--verbose'),
};

const BOOT_TIMEOUT_MS    = 300000;
const SPLASH_TIMEOUT_MS  = 180000;
const LOADSONG_TIMEOUT_MS = 240000;

const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR = resolve(REPO_ROOT, 'docs/sessions/web/screenshots/w8-meters');
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

async function analyzeCanvas(page, path) {
    try {
        await page.locator('#rb3-canvas').screenshot({ path });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(path));
        let painted = 0, totalR = 0, totalG = 0, totalB = 0;
        const total = png.width * png.height;
        for (let p = 0; p < png.data.length; p += 4) {
            const r = png.data[p], g = png.data[p+1], b = png.data[p+2];
            if (r > 12 || g > 12 || b > 12) painted++;
            totalR += r; totalG += g; totalB += b;
        }
        const avgR = (totalR / total).toFixed(0);
        const avgG = (totalG / total).toFixed(0);
        const avgB = (totalB / total).toFixed(0);
        return { paintedPct: Number((100 * painted / total).toFixed(2)), w: png.width, h: png.height, avgRGB: `${avgR},${avgG},${avgB}` };
    } catch (e) {
        return { error: e.message };
    }
}

let browser;
const logs = [];
const flow = [];
const errors = [];

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame  = (page) => page.evaluate(() => window.rb3FrameCount || 0);
const getSongCount = (page) => page.evaluate(() => window.rb3SongCount || 0);

async function snap(page, filename, label) {
    const path = resolve(OUT_DIR, filename);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    const frame = await getFrame(page);
    flow.push({ filename, label, screen, frame, painted: a.paintedPct, avgRGB: a.avgRGB, path });
    console.log(`  SNAP [${filename}] screen='${screen}' frame=${frame} painted=${a.paintedPct}% avgRGB=${a.avgRGB}`);
    return { path, ...a, screen, frame };
}

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

const elapsed = (() => { const t0 = Date.now(); return () => ((Date.now() - t0) / 1000).toFixed(2); })();

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
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ t: elapsed(), type: msg.type(), text });
        if (opts.verbose || /screen:|booted|SetGeom|num\d|score|Scoreboard|HUD/.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => { errors.push(err.message || String(err)); });
    page.on('crash', () => errors.push('Page crashed'));

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url}...`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    console.log('Waiting for rb3AppBooted...');
    let appBooted = 0, deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`App booted: ${appBooted} (${elapsed()}s)`);

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
    await snap(page, '01_splash.png', 'splash_screen');

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // Splash → main_hub
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    await snap(page, '02_main_hub.png', 'main_hub_screen');

    // main_hub → song_select
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
    s = await getScreen(page);
    await snap(page, '03_song_select.png', 'song_select_screen');

    // song_select → part_difficulty
    if (s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        for (let i = 0; i < 4; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 120));
            await page.keyboard.up('Enter');
            const ns = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
            const cur = await getScreen(page);
            if (cur === 'part_difficulty_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);
    await snap(page, '04_part_difficulty.png', 'part_difficulty_screen');

    // part_difficulty → game_screen
    if (s === 'part_difficulty_screen') {
        for (let i = 0; i < 5; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 150));
            await page.keyboard.up('Enter');
            await new Promise(r => setTimeout(r, 1200));
            const cur = await getScreen(page);
            if (cur !== 'part_difficulty_screen') { s = cur; break; }
        }
        console.log(`  waiting for game_screen (MOGG load)...`);
        await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
        s = await getScreen(page);
    }

    await snap(page, '05_game_screen_entry.png', 'game_screen_entry');
    await new Promise(r => setTimeout(r, 5000));
    await snap(page, '06_gameplay_t5s.png', 'gameplay_5s');
    await new Promise(r => setTimeout(r, 10000));
    await snap(page, '07_gameplay_t15s.png', 'gameplay_15s');
    await new Promise(r => setTimeout(r, 15000));
    await snap(page, '08_gameplay_t30s.png', 'gameplay_30s');

    const finalScreen = await getScreen(page);
    const finalFrame  = await getFrame(page);
    console.log(`\n=== FINAL screen='${finalScreen}' frame=${finalFrame} elapsed=${elapsed()}s ===`);

    const summary = {
        captured: new Date().toISOString(),
        commit: 'wt-web-w8-meters',
        engine_pin: 'e6c8f86',
        port: opts.port,
        appBooted,
        final_screen: finalScreen,
        final_frame: finalFrame,
        flow,
        errors: errors.slice(0, 10),
    };
    writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify(summary, null, 2));
    console.log(`\nSaved ${flow.length} screenshots to ${OUT_DIR}`);
    for (const f of flow) {
        console.log(`  ${f.filename}: screen='${f.screen}' frame=${f.frame} painted=${f.painted}% avgRGB=${f.avgRGB}`);
    }

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
