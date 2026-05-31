#!/usr/bin/env node
/**
 * RB3 Web — read-only runtime log TRIAGE.
 * Drives splash -> main_hub -> song_select -> part_difficulty -> gameplay (>=30s)
 * and captures EVERY distinct console line + pageerrors. No filtering. Buckets
 * by frequency so per-frame spam is distinguishable from one-shot events.
 *
 * Usage: node scripts/web/triage-capture.mjs --port 8585 --play-seconds 35
 * Output: /tmp/rb3-triage/console.full.jsonl + summary.json + screenshots
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const port = parseInt(argv[argv.indexOf('--port') + 1] || '8585', 10) || 8585;
const playSeconds = parseInt(argv[argv.indexOf('--play-seconds') + 1] || '35', 10) || 35;

const OUT_DIR = '/tmp/rb3-triage';
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

const logs = [];          // {elapsed, type, frame, screen, text}
const errors = [];        // pageerror messages
const phaseMarkers = [];  // {phase, frame, elapsed}
let browser;
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);
let curFrame = 0, curScreen = '';

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
        logs.push({ elapsed: elapsed(), type: msg.type(), frame: curFrame, screen: curScreen, text: msg.text() });
    });
    page.on('pageerror', (err) => {
        const m = err.stack || err.message || String(err);
        errors.push({ elapsed: elapsed(), frame: curFrame, screen: curScreen, message: m });
        console.log(`  [PAGE_ERROR ${elapsed()}s] ${(err.message || err).toString().slice(0, 200)}`);
    });
    page.on('crash', () => { errors.push({ elapsed: elapsed(), message: 'PAGE CRASHED' }); console.log('  [CRASH]'); });
    page.on('requestfailed', (req) => {
        logs.push({ elapsed: elapsed(), type: 'requestfailed', frame: curFrame, screen: curScreen,
                    text: `REQFAIL ${req.method()} ${req.url()} :: ${req.failure()?.errorText}` });
    });

    const mark = (phase) => { phaseMarkers.push({ phase, frame: curFrame, screen: curScreen, elapsed: elapsed() }); console.log(`[phase] ${phase} screen='${curScreen}' frame=${curFrame} (${elapsed()}s)`); };
    const refresh = async () => { try { curFrame = await getFrame(page); curScreen = await getScreen(page); } catch {} };

    await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    mark('page_loaded');

    let deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        if ((await page.evaluate(() => window.rb3AppBooted || 0)) >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    await refresh(); mark('app_booted');

    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    let s = '';
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    await new Promise(r => setTimeout(r, 2000));
    await refresh(); mark('splash');
    await page.locator('#rb3-canvas').click({ force: true });

    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    await refresh(); mark('main_hub');
    await page.locator('#rb3-canvas').screenshot({ path: `${OUT_DIR}/01_main_hub.png` }).catch(() => {});

    if (curScreen === 'main_hub_screen') {
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
    await refresh(); mark('song_select');
    await page.locator('#rb3-canvas').screenshot({ path: `${OUT_DIR}/02_song_select.png` }).catch(() => {});

    if (curScreen === 'song_select_screen') {
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
    await refresh(); mark('part_difficulty');
    await page.locator('#rb3-canvas').screenshot({ path: `${OUT_DIR}/03_part_difficulty.png` }).catch(() => {});

    if (curScreen === 'part_difficulty_screen') {
        for (let i = 0; i < 5; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 150));
            await page.keyboard.up('Enter');
            await new Promise(r => setTimeout(r, 1200));
            if ((await getScreen(page)) !== 'part_difficulty_screen') break;
        }
        await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
    }
    await waitScreen(page, { targets: ['game_screen'], timeoutMs: LOADSONG_TIMEOUT_MS });
    await new Promise(r => setTimeout(r, 1500));
    await refresh(); mark('gameplay_start');
    await page.locator('#rb3-canvas').screenshot({ path: `${OUT_DIR}/04_gameplay_start.png` }).catch(() => {});

    // Gameplay window: snapshot frame counts to measure per-frame spam.
    const gStart = { frame: curFrame, logIdx: logs.length, t: Date.now() };
    for (let k = 0; k < playSeconds; k += 5) {
        await new Promise(r => setTimeout(r, 5000));
        await refresh();
        await page.locator('#rb3-canvas').screenshot({ path: `${OUT_DIR}/05_gameplay_t${k + 5}s.png` }).catch(() => {});
    }
    await refresh(); mark('gameplay_end');
    const gEnd = { frame: curFrame, logIdx: logs.length, t: Date.now() };
    const framesDrawn = gEnd.frame - gStart.frame;
    const logsInGame = gEnd.logIdx - gStart.logIdx;

    // ---- Aggregate: normalize each line to a template and count ----
    const norm = (t) => t
        .replace(/0x[0-9a-fA-F]+/g, '0xHEX')
        .replace(/\b\d+\.\d+\b/g, 'N.N')
        .replace(/\b\d+\b/g, 'N');
    const agg = new Map();
    for (const e of logs) {
        const key = norm(e.text);
        let a = agg.get(key);
        if (!a) { a = { key, type: e.type, count: 0, firstElapsed: e.elapsed, sample: e.text,
                        inGame: 0, frames: new Set() }; agg.set(key, a); }
        a.count++;
        a.frames.add(e.frame);
        if (logs.indexOf(e) >= gStart.logIdx) a.inGame++;
    }
    const aggArr = [...agg.values()].map(a => ({
        key: a.key, type: a.type, count: a.count, inGameCount: a.inGame,
        distinctFrames: a.frames.size, firstElapsed: a.firstElapsed, sample: a.sample,
        // perFrame heuristic: appears across many distinct frames during gameplay
        perFrameDuringGameplay: a.inGame > 0 && a.frames.size > 5 && (framesDrawn > 0 ? a.inGame / framesDrawn : 0) > 0.3 ? true : false,
    })).sort((x, y) => y.count - x.count);

    const summary = {
        port, playSeconds,
        final_screen: curScreen,
        framesDrawnInGame: framesDrawn,
        logsInGame, totalLogs: logs.length,
        pageErrors: errors.length,
        phaseMarkers,
        distinctTemplates: aggArr.length,
        templates: aggArr,
        errors,
    };
    writeFileSync(`${OUT_DIR}/console.full.jsonl`, logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    writeFileSync(`${OUT_DIR}/summary.json`, JSON.stringify(summary, null, 2));
    console.log(`\n=== DONE === final='${curScreen}' totalLogs=${logs.length} distinct=${aggArr.length} pageErrors=${errors.length} framesInGame=${framesDrawn}`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}\n${e.stack}`);
    try {
        writeFileSync(`${OUT_DIR}/console.full.jsonl`, logs.map(x => JSON.stringify(x)).join('\n') + '\n');
        writeFileSync(`${OUT_DIR}/summary.json`, JSON.stringify({ result: 'error', message: e.message, phaseMarkers, errors, totalLogs: logs.length }, null, 2));
    } catch {}
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {} }
}
