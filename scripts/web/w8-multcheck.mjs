#!/usr/bin/env node
/**
 * RB3 Web — multiplier-label verification.
 * Navigates splash -> ... -> gameplay (same flow as w7-hud-capture), then
 * captures the full canvas + a brightened crop of the lower HUD where the
 * streak-meter "xN" multiplier label sits, at song start and after a brief
 * gameplay window. Console MULT_DBG logs are echoed so the rendered label can
 * be cross-checked against the computed multiplier.
 *
 * Usage: node scripts/web/w8-multcheck.mjs --port 8999
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const port = parseInt(argv[argv.indexOf('--port') + 1] || '8999', 10) || 8999;

const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR = resolve(REPO_ROOT, 'docs/sessions/web/screenshots/w8-multcheck');
mkdirSync(OUT_DIR, { recursive: true });

const BOOT_TIMEOUT_MS = 300000;
const SPLASH_TIMEOUT_MS = 180000;
const LOADSONG_TIMEOUT_MS = 240000;

function waitForServer(p, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${p}/api/health`, (r) => {
                if (r.statusCode === 200) return res();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline) return rej(new Error('server not ready'));
            setTimeout(check, 300);
        };
        check();
    });
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');

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

// Brighten + crop a saved PNG to the lower HUD band.
async function cropBright(srcPath, dstPath, region) {
    const { PNG } = await import('pngjs');
    const png = PNG.sync.read(readFileSync(srcPath));
    const { x, y, w, h } = region;
    const out = new PNG({ width: w, height: h });
    for (let j = 0; j < h; j++) {
        for (let i = 0; i < w; i++) {
            const sp = ((y + j) * png.width + (x + i)) * 4;
            const dp = (j * w + i) * 4;
            // 3x gain, clamp
            out.data[dp]   = Math.min(255, png.data[sp]   * 3);
            out.data[dp+1] = Math.min(255, png.data[sp+1] * 3);
            out.data[dp+2] = Math.min(255, png.data[sp+2] * 3);
            out.data[dp+3] = 255;
        }
    }
    writeFileSync(dstPath, PNG.sync.write(out));
}

const logs = [];
let browser;
const elapsed = (() => { const t0 = Date.now(); return () => ((Date.now() - t0) / 1000).toFixed(2); })();

try {
    await waitForServer(port);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
               '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
               '--ozone-platform=x11', '--mute-audio'],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    page.on('console', (msg) => {
        const t = msg.text();
        logs.push(t);
        if (/MULT_DBG/.test(t)) console.log(`  [${elapsed()}s] ${t}`);
    });

    await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    let deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        if ((await page.evaluate(() => window.rb3AppBooted || 0)) >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    let s = '';
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    await new Promise(r => setTimeout(r, 2000));
    await page.locator('#rb3-canvas').click({ force: true });

    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));

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

    if (s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        for (let i = 0; i < 4; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 120));
            await page.keyboard.up('Enter');
            await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
            if ((await getScreen(page)) === 'part_difficulty_screen') break;
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);

    if (s === 'part_difficulty_screen') {
        for (let i = 0; i < 5; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 150));
            await page.keyboard.up('Enter');
            await new Promise(r => setTimeout(r, 1200));
            if ((await getScreen(page)) !== 'part_difficulty_screen') break;
        }
        await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
    }

    // Wait until the real game_screen is showing, then capture at "song start".
    await waitScreen(page, { targets: ['game_screen'], timeoutMs: LOADSONG_TIMEOUT_MS });
    await new Promise(r => setTimeout(r, 1500));

    const full0 = resolve(OUT_DIR, 'start_full.png');
    await page.locator('#rb3-canvas').screenshot({ path: full0 });
    // Lower HUD band (streak meter / xN label sits low-center on the track).
    await cropBright(full0, resolve(OUT_DIR, 'start_hud_bright.png'), { x: 380, y: 470, w: 520, h: 230 });
    console.log(`  CAPTURED song-start at screen='${await getScreen(page)}'`);

    // Let a streak build so the xN should appear (>=10 notes -> x2).
    await new Promise(r => setTimeout(r, 18000));
    const full1 = resolve(OUT_DIR, 'ramped_full.png');
    await page.locator('#rb3-canvas').screenshot({ path: full1 });
    await cropBright(full1, resolve(OUT_DIR, 'ramped_hud_bright.png'), { x: 380, y: 470, w: 520, h: 230 });
    console.log(`  CAPTURED ramped at screen='${await getScreen(page)}'`);

    const multLogs = logs.filter(t => /MULT_DBG/.test(t));
    writeFileSync(resolve(OUT_DIR, 'mult-logs.txt'), multLogs.join('\n'));
    console.log(`\nSaved ${multLogs.length} MULT_DBG lines + crops to ${OUT_DIR}`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}\n${e.stack}`);
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {} }
}
