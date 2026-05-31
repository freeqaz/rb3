#!/usr/bin/env node
/**
 * Magenta-cast triage: drive into gameplay, screenshot the venue/band, and
 * quantify the per-channel cast (mean R/G/B). A magenta cast => mean R and B
 * high, mean G suppressed. Also dumps console logs that mention material /
 * texture / colour diagnostics.
 *
 * Usage: node scripts/web/magenta-capture.mjs [--port 8581] [--play-seconds 20] [--tag v0]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port:        parseInt(argv[argv.indexOf('--port') + 1] || '8581', 10) || 8581,
    playSeconds: parseInt(argv[argv.indexOf('--play-seconds') + 1] || '20', 10) || 20,
    tag:         (argv[argv.indexOf('--tag') + 1] || 'v0'),
};
const BOOT_TIMEOUT_MS = 300000, SPLASH_TIMEOUT_MS = 180000, LOADSONG_TIMEOUT_MS = 240000;
const OUT_DIR = resolve(__dirname, 'results/magenta', opts.tag);
mkdirSync(OUT_DIR, { recursive: true });

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => http.get(`http://127.0.0.1:${port}/api/health`, (r) => (r.statusCode === 200 ? res() : retry())).on('error', retry);
        const retry = () => (Date.now() > deadline ? rej(new Error('server not ready')) : setTimeout(check, 300));
        check();
    });
}

async function analyzeCanvas(page, path) {
    await page.locator('#rb3-canvas').screenshot({ path });
    const { PNG } = await import('pngjs');
    const { readFileSync } = await import('fs');
    const png = PNG.sync.read(readFileSync(path));
    let r = 0, g = 0, b = 0, painted = 0, magenta = 0;
    const total = png.width * png.height;
    for (let p = 0; p < png.data.length; p += 4) {
        const R = png.data[p], G = png.data[p+1], B = png.data[p+2];
        r += R; g += G; b += B;
        if (R > 12 || G > 12 || B > 12) painted++;
        // magenta heuristic: R and B both notably above G
        if (R > 40 && B > 40 && R > G + 25 && B > G + 25) magenta++;
    }
    return {
        meanR: +(r/total).toFixed(1), meanG: +(g/total).toFixed(1), meanB: +(b/total).toFixed(1),
        paintedPct: +(100*painted/total).toFixed(2), magentaPct: +(100*magenta/total).toFixed(2),
        w: png.width, h: png.height,
    };
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame = (page) => page.evaluate(() => window.rb3FrameCount || 0);
const flow = [], logs = [];

async function snap(page, label) {
    const path = resolve(OUT_DIR, `${label}.png`);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    flow.push({ label, screen, ...a, png: path });
    console.log(`  SNAP [${label}] screen='${screen}' mean=(${a.meanR},${a.meanG},${a.meanB}) painted=${a.paintedPct}% magenta=${a.magentaPct}%`);
    return path;
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

let browser;
try {
    await waitForServer(opts.port);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: ['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11','--disable-extensions','--mute-audio'],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0 = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);
    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ elapsed: elapsed(), type: msg.type(), text });
        if (/FRAME CAPTURE|DRAW|mat=|tex\[|color=|MAGENTA|PostProc|ColorXfm|venue|Venue|usingFallback|fallback|couldn't find|BlackTex|WhiteTex|diffuse/.test(text))
            console.log(`  [${elapsed()}s] ${text}`);
    });
    page.on('pageerror', (err) => console.log(`  [PAGE_ERROR] ${err.message || err}`));

    await page.goto(`http://127.0.0.1:${opts.port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    let appBooted = 0, deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) { appBooted = await page.evaluate(() => window.rb3AppBooted || 0); if (appBooted >= 1) break; await new Promise(r => setTimeout(r, 500)); }
    console.log(`App booted: ${appBooted} (${elapsed()}s)`);
    let s = ''; deadline = Date.now() + SPLASH_TIMEOUT_MS;
    while (Date.now() < deadline) { s = await getScreen(page); if (s === 'splash_screen') break; await new Promise(r => setTimeout(r, 500)); }
    await new Promise(r => setTimeout(r, 2000));
    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) { await page.keyboard.press('Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page); await snap(page, '01_main_hub');

    if (s === 'main_hub_screen') {
        // main_hub has internal focus sub-states (playnow -> quickplay -> ...)
        // that don't change the screen string. Press Enter repeatedly with a
        // pause until the screen string actually changes off main_hub.
        for (let i = 0; i < 10; i++) {
            await page.keyboard.down('Enter'); await new Promise(r => setTimeout(r, 130)); await page.keyboard.up('Enter');
            await new Promise(r => setTimeout(r, 1800));
            const cur = await getScreen(page);
            console.log(`  main_hub Enter #${i+1}: screen='${cur}'`);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 4000));
    s = await getScreen(page); await snap(page, '02_song_select');

    if (s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        for (let i = 0; i < 4; i++) { await page.keyboard.down('Enter'); await new Promise(r => setTimeout(r, 120)); await page.keyboard.up('Enter'); await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 }); const cur = await getScreen(page); if (cur === 'part_difficulty_screen') { s = cur; break; } await new Promise(r => setTimeout(r, 1500)); }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page); await snap(page, '03_part_difficulty');

    if (s === 'part_difficulty_screen') {
        for (let i = 0; i < 5; i++) { await page.keyboard.down('Enter'); await new Promise(r => setTimeout(r, 150)); await page.keyboard.up('Enter'); await new Promise(r => setTimeout(r, 1200)); const cur = await getScreen(page); if (cur !== 'part_difficulty_screen') { s = cur; break; } }
        await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
        s = await getScreen(page); await snap(page, '04_after_crossing');
    }

    if (s === 'game_screen' || (s && s !== 'part_difficulty_screen' && s !== 'song_select_screen')) {
        for (let k = 0; k < opts.playSeconds; k += 5) {
            await new Promise(r => setTimeout(r, 5000));
            await snap(page, `05_gameplay_t${k+5}s`);
            // Arm a one-shot FrameCapture so the next frame dumps per-draw
            // mat/tex/colour info to the console (stderr).
            if (k === 5 || k === 10) {
                console.log(`  >>> arming rb3_capture_frame() at t=${k+5}s`);
                await page.evaluate(() => { if (window.Module && window.Module._rb3_capture_frame) window.Module._rb3_capture_frame(); else if (typeof _rb3_capture_frame === 'function') _rb3_capture_frame(); });
                await new Promise(r => setTimeout(r, 1500));
            }
        }
    }

    writeFileSync(resolve(OUT_DIR, 'flow.json'), JSON.stringify({ final_screen: await getScreen(page), flow }, null, 2));
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(`\n=== FINAL screen: '${await getScreen(page)}' ===`);
    for (const f of flow) console.log(`  ${f.label}: screen='${f.screen}' mean=(${f.meanR},${f.meanG},${f.meanB}) magenta=${f.magentaPct}%`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}`);
    try { writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n'); } catch {}
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {} }
}
