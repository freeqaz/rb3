#!/usr/bin/env node
/**
 * RB3 Web — W3c-nav: drive the menu from splash → main_hub → song_select →
 * (part/difficulty) → gem-track gameplay, all by keyboard.
 *
 * Boots the App (no ?milo=), waits for splash_screen, then walks a scripted
 * keypress sequence. After each press it polls window.rb3CurrentScreen and
 * window.rb3SongCount, screenshots whenever the screen name changes, and
 * records the full flow.
 *
 * Key map (rb3_game_input.cpp): Enter=Confirm, Space=Start, Esc=Cancel,
 * Tab=Option, Arrows/WASD=dpad, Q/E=PageUp/Dn.
 *
 * Usage:
 *   node scripts/web/w3cnav-test.mjs [--port 8431] [--verbose]
 *
 * Output: scripts/web/results/web-w3cnav/{song_select,gameplay}/ + flow.json
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

const BOOT_TIMEOUT_MS   = 300000;
const SPLASH_TIMEOUT_MS = 180000;

const OUT_DIR = resolve(__dirname, 'results/web-w3cnav');
mkdirSync(resolve(OUT_DIR, 'song_select'), { recursive: true });
mkdirSync(resolve(OUT_DIR, 'gameplay'), { recursive: true });

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

// Decode a canvas screenshot → painted% + center pixel.
async function analyzeCanvas(page, path) {
    try {
        await page.locator('#rb3-canvas').screenshot({ path });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(path));
        let painted = 0;
        const total = png.width * png.height;
        for (let p = 0; p < png.data.length; p += 4) {
            if (png.data[p] > 12 || png.data[p+1] > 12 || png.data[p+2] > 12) painted++;
        }
        return { paintedPct: Number((100 * painted / total).toFixed(2)), w: png.width, h: png.height };
    } catch (e) {
        return { error: e.message };
    }
}

let browser;
const logs = [];
const errors = [];
const flow = [];

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getSongCount = (page) => page.evaluate(() => window.rb3SongCount || 0);

async function snap(page, label, dir) {
    const path = resolve(OUT_DIR, dir, `${label}.png`);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    const songs = await getSongCount(page);
    flow.push({ label, screen, songs, painted: a.paintedPct, png: path });
    console.log(`  SNAP [${label}] screen='${screen}' songs=${songs} painted=${a.paintedPct}%`);
    return path;
}

// Wait for the screen name to become one of `targets` (or change away from
// `from`) within timeoutMs. Returns the final screen.
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
        if (opts.verbose || /web-input|screen:|FIRE|WAIT|SKIP|SongMgr|StartRefresh|ranked|NativeContentMgr|song_select|part_difficulty|goto_screen|set_state|SELECT/.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => { errors.push(err.message || String(err)); console.log(`  [PAGE_ERROR] ${err.message || err}`); });
    page.on('crash', () => { errors.push('Page crashed'); console.log('  [CRASH]'); });

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url} (W3c-nav menu→gameplay)`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Step 1: App boot.
    console.log('Waiting for rb3AppBooted...');
    let appBooted = 0, deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`App booted: ${appBooted} (${elapsed()}s)`);

    // Step 2: reach splash_screen.
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

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // === Splash → main_hub ===
    // Native flow: @start on splash (start.btn → overshell add-user), then
    // @confirm (overshell continue-without-profile → main_hub). The keyboard
    // sends Start (Space) then Confirm (Enter). Retry Confirm a few times to
    // ride the overshell settle.
    console.log(`\n[splash→main_hub] sending Start then Confirm presses...`);
    await page.keyboard.press('Space');           // splash start.btn
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');       // overshell continue
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    console.log(`After splash drive: screen='${s}' (${elapsed()}s)`);
    // main_hub may take time to load its venue; wait for it.
    if (s !== 'main_hub_screen') {
        s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);
    await snap(page, 'main_hub', 'song_select');
    console.log(`main_hub reached: '${s}'`);

    // === main_hub → song_select ===
    // main_hub.dta SELECT_MSG: mb_playnow.btn → kMainHubState_PlayNow (focus
    // pn_quickplay.btn) → Confirm → kMainHubState_Quickplay (focus
    // qp_quickplay.btn) → Confirm → set_override Waiting → song_select.
    // Each Confirm acts on the auto-focused button for the new state.
    if (s === 'main_hub_screen') {
        console.log(`\n[main_hub→song_select] Confirm chain (playnow→quickplay→quickplay)...`);
        for (let i = 0; i < 5; i++) {
            await page.keyboard.press('Enter');
            const ns = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            const cur = await getScreen(page);
            console.log(`  Confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        // song_select may transition through song_select_enter_screen first.
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') {
            s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
        }
    }
    await new Promise(r => setTimeout(r, 4000));
    s = await getScreen(page);
    const songCount = await getSongCount(page);
    await snap(page, 'song_select', 'song_select');
    console.log(`\nsong_select reached: '${s}' songCount=${songCount}`);

    // === song_select → part_difficulty → gameplay (stretch) ===
    if (s === 'song_select_screen') {
        console.log(`\n[song_select→gameplay] navigate list + confirm song...`);
        await page.keyboard.press('ArrowDown');
        await new Promise(r => setTimeout(r, 1000));
        await page.keyboard.press('Enter');       // confirm song → part_difficulty
        let ns = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
        console.log(`  after song confirm: screen='${ns}' (${elapsed()}s)`);
        await new Promise(r => setTimeout(r, 3000));
        await snap(page, 'part_difficulty', 'gameplay');
        ns = await getScreen(page);
        if (ns === 'part_difficulty_screen') {
            // Confirm part/difficulty → game_screen (gem track).
            for (let i = 0; i < 4; i++) {
                await page.keyboard.press('Enter');
                const g = await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: 10000 });
                const cur = await getScreen(page);
                console.log(`  part confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
                if (cur === 'game_screen') break;
                await new Promise(r => setTimeout(r, 1500));
            }
            const g = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 30000 });
            await new Promise(r => setTimeout(r, 4000));
            await snap(page, 'gameplay', 'gameplay');
            console.log(`  final gameplay screen: '${await getScreen(page)}'`);
        }
    }

    const finalScreen = await getScreen(page);
    console.log(`\n=== FINAL screen: '${finalScreen}' ===`);

    const summary = {
        result: 'info',
        appBooted,
        final_screen: finalScreen,
        song_count: await getSongCount(page),
        flow,
        deepest: finalScreen,
        error_count: errors.length,
        errors: errors.slice(0, 20),
        log_count: logs.length,
    };
    writeFileSync(resolve(OUT_DIR, 'flow.json'), JSON.stringify(summary, null, 2));
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(`\nFlow recorded → ${resolve(OUT_DIR, 'flow.json')}`);
    for (const f of flow) console.log(`  ${f.label}: screen='${f.screen}' songs=${f.songs} painted=${f.painted}%`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}`);
    try {
        writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
        writeFileSync(resolve(OUT_DIR, 'flow.json'), JSON.stringify({ result: 'error', message: e.message, flow, errors }, null, 2));
    } catch { /* ignore */ }
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); }
        catch { /* ignore */ }
    }
}
