#!/usr/bin/env node
/**
 * W4b — IndexedDB asset cache timing harness.
 *
 * Boots the web build, drives the keyboard menu to song_select, and times
 * each milestone. Reads window.__rb3CacheStats to report hit/miss/byte
 * counters. Use `--mode cold` to clear IDB before the run; `--mode warm`
 * runs against an already-populated IDB.
 *
 * Output: scripts/web/results/web-w4b/<mode>/{flow.json,console.jsonl}
 *
 * Usage:
 *   node scripts/web/w4b-cache-timing.mjs --port 8432 --mode cold
 *   node scripts/web/w4b-cache-timing.mjs --port 8432 --mode warm
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, rmSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port: parseInt(argv[argv.indexOf('--port') + 1] || '8432', 10) || 8432,
    mode: argv[argv.indexOf('--mode') + 1] || 'cold',  // 'cold' | 'warm'
    verbose: argv.includes('--verbose'),
    profileDir: argv[argv.indexOf('--profile-dir') + 1] ||
        '/tmp/rb3-w4b-profile',
    eToE: argv.includes('--e2e'),  // run all the way to game_screen
};

const BOOT_TIMEOUT_MS = 300000;
const SPLASH_TIMEOUT_MS = 180000;
const LOADSONG_TIMEOUT_MS = 240000;

const OUT_DIR = resolve(__dirname, 'results/web-w4b', opts.mode);
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

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getSongCount = (page) => page.evaluate(() => window.rb3SongCount || 0);
const getFrame = (page) => page.evaluate(() => window.rb3FrameCount || 0);
const getIdb = (page) => page.evaluate(() => ({
    ready: window.__rb3IdbReady || 0,
    version: window.__rb3IdbVersion || '',
    rows: window.__rb3IdbCache ? window.__rb3IdbCache.size : 0,
    stats: window.__rb3CacheStats || {},
}));

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

const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

const milestones = {};
function mark(name) {
    milestones[name] = parseFloat(elapsed());
    console.log(`  [${milestones[name].toFixed(2)}s] ${name}`);
}

let browser, context;

try {
    await waitForServer(opts.port);

    // Cold mode wipes the persistent profile (and thus IDB) before launch.
    if (opts.mode === 'cold') {
        try { rmSync(opts.profileDir, { recursive: true, force: true }); } catch {}
    }

    // Persistent context so IDB survives between runs (the whole point).
    context = await chromium.launchPersistentContext(opts.profileDir, {
        headless: !process.env.DISPLAY,
        viewport: { width: 1280, height: 720 },
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--disable-background-networking',
            '--disable-default-apps', '--disable-sync', '--mute-audio',
        ],
    });
    browser = context.browser();
    const page = context.pages()[0] || await context.newPage();

    const logs = [];
    const errors = [];
    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ elapsed: elapsed(), type: msg.type(), text });
        if (text.includes('rb3-idb') || text.includes('RUNTIME_INIT_OK') ||
            text.includes('assets ready') || text.includes('GPU ready')) {
            console.log(`  [${elapsed()}s] ${text}`);
        } else if (opts.verbose) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => { errors.push(err.message || String(err)); });
    page.on('crash', () => { errors.push('Page crashed'); });

    mark('test_start');
    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url} (mode=${opts.mode})`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
    mark('dom_loaded');

    console.log('Waiting for rb3AppBooted...');
    let appBooted = 0, deadline = Date.now() + BOOT_TIMEOUT_MS;
    let assetsReadyMark = null;
    let gpuReadyMark = null;
    let runtimeInitMark = null;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        // sample timing markers based on console log content
        const recent = logs.slice(-20).map(l => l.text).join('|');
        if (!runtimeInitMark && recent.includes('RUNTIME_INIT_OK')) {
            runtimeInitMark = parseFloat(elapsed());
            console.log(`  [${runtimeInitMark.toFixed(2)}s] (mark) runtime_init`);
        }
        if (!assetsReadyMark && recent.includes('assets ready')) {
            assetsReadyMark = parseFloat(elapsed());
            console.log(`  [${assetsReadyMark.toFixed(2)}s] (mark) assets_ready`);
        }
        if (!gpuReadyMark && recent.includes('GPU ready')) {
            gpuReadyMark = parseFloat(elapsed());
            console.log(`  [${gpuReadyMark.toFixed(2)}s] (mark) gpu_ready`);
        }
        await new Promise(r => setTimeout(r, 250));
    }
    mark('app_booted');
    if (runtimeInitMark) milestones.runtime_init = runtimeInitMark;
    if (assetsReadyMark) milestones.assets_ready = assetsReadyMark;
    if (gpuReadyMark) milestones.gpu_ready = gpuReadyMark;

    let idbAfterBoot = await getIdb(page);
    console.log(`  IDB after boot: ready=${idbAfterBoot.ready} rows=${idbAfterBoot.rows} version=${idbAfterBoot.version}`);

    // Splash → main_hub → song_select.
    console.log('Waiting for splash_screen...');
    let s = '';
    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    mark('splash_seen');
    await new Promise(r => setTimeout(r, 2000));
    s = await getScreen(page);

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // splash → main_hub
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    mark('main_hub');

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
    mark('song_select');
    const songCount = await getSongCount(page);
    const idbAtSongSelect = await getIdb(page);
    console.log(`  song_select reached: '${s}' songs=${songCount}`);
    console.log(`  IDB stats: hits=${idbAtSongSelect.stats.hits} misses=${idbAtSongSelect.stats.misses} bytesCached=${idbAtSongSelect.stats.bytesFromCache} bytesFetched=${idbAtSongSelect.stats.bytesFetched} puts=${idbAtSongSelect.stats.puts} rows=${idbAtSongSelect.rows}`);

    let finalState = { screen: s, frame: await getFrame(page) };

    // Optional: --e2e drives all the way to game_screen so cache also covers
    // the song-load path (MOGG, venue milos).
    if (opts.eToE && s === 'song_select_screen') {
        await page.evaluate(() => { window.rb3WebTargetSong = '20thcenturyboy'; });
        await new Promise(r => setTimeout(r, 1000));
        for (let i = 0; i < 4; i++) {
            await page.keyboard.down('Enter');
            await new Promise(r => setTimeout(r, 120));
            await page.keyboard.up('Enter');
            await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
            const cur = await getScreen(page);
            if (cur === 'part_difficulty_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
        mark('part_difficulty');
        if (s === 'part_difficulty_screen') {
            for (let i = 0; i < 5; i++) {
                await page.keyboard.down('Enter');
                await new Promise(r => setTimeout(r, 150));
                await page.keyboard.up('Enter');
                await new Promise(r => setTimeout(r, 1200));
                const cur = await getScreen(page);
                if (cur !== 'part_difficulty_screen') { s = cur; break; }
            }
            await waitScreen(page, { targets: ['game_screen'], from: 'part_difficulty_screen', timeoutMs: LOADSONG_TIMEOUT_MS });
            s = await getScreen(page);
            mark('game_screen');
        }
        finalState = { screen: s, frame: await getFrame(page) };
    }

    const idbFinal = await getIdb(page);
    const summary = {
        mode: opts.mode,
        e2e: opts.eToE,
        port: opts.port,
        final: finalState,
        song_count: songCount,
        milestones,
        idb: {
            ready: idbAfterBoot.ready,
            version: idbFinal.version,
            rows_at_boot: idbAfterBoot.rows,
            rows_final: idbFinal.rows,
        },
        cache_stats: idbFinal.stats,
        errors_count: errors.length,
        errors: errors.slice(0, 10),
    };
    writeFileSync(resolve(OUT_DIR, 'flow.json'), JSON.stringify(summary, null, 2));
    writeFileSync(resolve(OUT_DIR, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');

    console.log('\n=== W4b TIMING SUMMARY ===');
    console.log(`mode=${opts.mode}  e2e=${opts.eToE}  final_screen=${finalState.screen}`);
    Object.entries(milestones).forEach(([k, v]) => console.log(`  ${k.padEnd(20)} ${v.toFixed(2)}s`));
    const st = idbFinal.stats || {};
    console.log(`cache hits=${st.hits||0} misses=${st.misses||0} bytesCached=${((st.bytesFromCache||0)/1048576).toFixed(2)}MB bytesFetched=${((st.bytesFetched||0)/1048576).toFixed(2)}MB puts=${st.puts||0} writeErrors=${st.writeErrors||0}`);
    console.log(`idb version=${idbFinal.version} rows=${idbFinal.rows}`);
    console.log(`output: ${OUT_DIR}/flow.json`);

    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}`);
    try {
        writeFileSync(resolve(OUT_DIR, 'flow.json'), JSON.stringify({ result: 'error', message: e.message, milestones }, null, 2));
    } catch {}
    process.exit(1);
} finally {
    if (context) {
        try { await Promise.race([context.close(), new Promise(r => setTimeout(r, 3000))]); }
        catch {}
    }
}
