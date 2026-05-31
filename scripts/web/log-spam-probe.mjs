#!/usr/bin/env node
/**
 * RB3 Web — console-spam + FPS probe.
 *
 * Drives the app splash -> main_hub -> song_select -> part_difficulty ->
 * game_screen, and at two checkpoints (main_hub and gameplay) measures:
 *   - console messages/sec, bucketed by a 40-char text prefix (ranked)
 *   - FPS, from window.rb3FrameCount deltas over the same window
 *
 * Usage: node scripts/web/log-spam-probe.mjs [--port 8763] [--window 5]
 * Output: prints a ranked report; writes JSON to /tmp/log-spam-<label>.json
 */
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const PORT = parseInt(argv[argv.indexOf('--port') + 1] || '8763', 10) || 8763;
const WINDOW_S = parseInt(argv[argv.indexOf('--window') + 1] || '5', 10) || 5;

const BOOT_TIMEOUT_MS = 300000;
const SPLASH_TIMEOUT_MS = 180000;
const LOADSONG_TIMEOUT_MS = 240000;

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
            if (r.statusCode === 200) return res(); retry();
        }).on('error', retry);
        const retry = () => { if (Date.now() > deadline) return rej(new Error('server not ready')); setTimeout(check, 300); };
        check();
    });
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame = (page) => page.evaluate(() => window.rb3FrameCount || 0);

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

// Shared console buffer; each entry { t, type, text }
let consoleBuf = [];
let capturing = false;

async function measure(page, label) {
    // reset buffer, start counting, sample frames across the window
    consoleBuf = [];
    capturing = true;
    const f0 = await getFrame(page);
    const t0 = Date.now();
    await new Promise(r => setTimeout(r, WINDOW_S * 1000));
    const f1 = await getFrame(page);
    const dt = (Date.now() - t0) / 1000;
    capturing = false;

    const entries = consoleBuf.slice();
    const total = entries.length;
    const msgsPerSec = total / dt;
    const fps = (f1 - f0) / dt;

    // bucket by 40-char prefix
    const buckets = new Map();
    for (const e of entries) {
        const key = e.text.slice(0, 40).replace(/\s+/g, ' ');
        const b = buckets.get(key) || { count: 0, type: e.type };
        b.count++;
        buckets.set(key, b);
    }
    const ranked = [...buckets.entries()]
        .map(([prefix, b]) => ({ prefix, count: b.count, perSec: +(b.count / dt).toFixed(1), type: b.type }))
        .sort((a, b) => b.count - a.count);

    const screen = await getScreen(page);
    const report = { label, screen, windowSec: +dt.toFixed(2), totalMsgs: total,
        msgsPerSec: +msgsPerSec.toFixed(1), frames: f1 - f0, fps: +fps.toFixed(1), ranked };

    console.log(`\n===== MEASURE [${label}] screen='${screen}' window=${dt.toFixed(2)}s =====`);
    console.log(`  msgs/sec = ${msgsPerSec.toFixed(1)}  (total ${total})    FPS = ${fps.toFixed(1)}  (${f1 - f0} frames)`);
    console.log(`  top offenders (prefix x40 -> count, per-sec):`);
    for (const r of ranked.slice(0, 20)) {
        console.log(`    ${String(r.perSec).padStart(7)}/s  x${String(r.count).padStart(5)}  [${r.type}] ${JSON.stringify(r.prefix)}`);
    }
    writeFileSync(`/tmp/log-spam-${label}.json`, JSON.stringify(report, null, 2));
    return report;
}

let browser;
try {
    await waitForServer(PORT);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--mute-audio'],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    page.on('console', (m) => { if (capturing) consoleBuf.push({ t: Date.now(), type: m.type(), text: m.text() }); });

    console.log(`Loading http://127.0.0.1:${PORT}/`);
    await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    let appBooted = 0, deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) { appBooted = await page.evaluate(() => window.rb3AppBooted || 0); if (appBooted >= 1) break; await new Promise(r => setTimeout(r, 500)); }
    console.log(`booted=${appBooted}`);

    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    let s = '';
    while (Date.now() < deadline) { s = await getScreen(page); if (s === 'splash_screen') break; await new Promise(r => setTimeout(r, 500)); }
    await new Promise(r => setTimeout(r, 2000));

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // splash -> main_hub
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) { await page.keyboard.press('Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));

    // MEASURE at main_hub
    await measure(page, 'menu');

    // main_hub -> song_select
    s = await getScreen(page);
    if (s === 'main_hub_screen') {
        for (let i = 0; i < 5; i++) { await page.keyboard.press('Enter'); await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 }); const c = await getScreen(page); if (c && c !== 'main_hub_screen') { s = c; break; } await new Promise(r => setTimeout(r, 1500)); }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));

    // song_select -> part_difficulty
    s = await getScreen(page);
    if (s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        for (let i = 0; i < 4; i++) { await page.keyboard.down('Enter'); await new Promise(r => setTimeout(r, 120)); await page.keyboard.up('Enter'); const ns = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 }); if ((await getScreen(page)) === 'part_difficulty_screen') { s = 'part_difficulty_screen'; break; } await new Promise(r => setTimeout(r, 1500)); }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 2000));

    // part_difficulty -> game_screen
    s = await getScreen(page);
    if (s === 'part_difficulty_screen') {
        for (let i = 0; i < 5; i++) { await page.keyboard.down('Enter'); await new Promise(r => setTimeout(r, 150)); await page.keyboard.up('Enter'); await new Promise(r => setTimeout(r, 1200)); const c = await getScreen(page); if (c !== 'part_difficulty_screen') { s = c; break; } }
        // Wait for the *real* game_screen, not the transient tv3_* load screens.
        s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: LOADSONG_TIMEOUT_MS });
    }
    console.log(`\nreached screen='${s}'`);

    // Let gameplay settle so streaming/decoding/character-load is past its burst
    // and we sample steady-state rendering.
    await new Promise(r => setTimeout(r, 8000));

    // MEASURE during gameplay
    await measure(page, 'gameplay');

    console.log('\nDONE');
    process.exit(0);
} catch (e) {
    console.error('Error:', e.message);
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {} }
}
