#!/usr/bin/env node
/**
 * W7-V4 album art capture — splash → main_hub → song_select, then snap
 * the highlighted-song album-art panel. Hits the same fix path as the
 * post-V2 sweep's 03_song_select.png so the result is directly comparable
 * against the ground-truth `yt_qRagnZCIMzk_song_select_album_art.png`.
 *
 * Usage:
 *   node scripts/web/w7-v4-album-art-capture.mjs --port 8957
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port: parseInt(argv[argv.indexOf('--port') + 1] || '8957', 10) || 8957,
    verbose: argv.includes('--verbose'),
};

const BOOT_TIMEOUT_MS   = 300000;
const SPLASH_TIMEOUT_MS = 180000;

const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR = resolve(REPO_ROOT, 'docs/sessions/web/screenshots/w7-album-art');
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
        return {
            paintedPct: Number((100 * painted / total).toFixed(2)),
            w: png.width, h: png.height,
            avgRGB: `${(totalR/total)|0},${(totalG/total)|0},${(totalB/total)|0}`,
        };
    } catch (e) {
        return { error: e.message };
    }
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame  = (page) => page.evaluate(() => window.rb3FrameCount || 0);
const getSongCount = (page) => page.evaluate(() => window.rb3SongCount || 0);

async function snap(page, filename, label) {
    const path = resolve(OUT_DIR, filename);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    const frame  = await getFrame(page);
    console.log(`  SNAP [${filename}] label=${label} screen='${screen}' frame=${frame} painted=${a.paintedPct}% avgRGB=${a.avgRGB}`);
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

let browser;
const fetchLog = [];

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

    // Log all album-art-relevant fetches
    page.on('response', (resp) => {
        const u = resp.url();
        if (/_keep\.png|album|art/i.test(u)) {
            fetchLog.push({ t: elapsed(), status: resp.status(), url: u });
            if (opts.verbose) console.log(`  [fetch ${resp.status()}] ${u}`);
        }
    });
    page.on('console', (msg) => {
        const text = msg.text();
        if (opts.verbose || /album|_keep|404|WARN|texture/i.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });

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
    console.log(`Screen settled: '${s}' (${elapsed()}s)`);
    await snap(page, '01_splash.png', 'splash_screen');

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // Splash → main_hub
    console.log(`\n[splash→main_hub]...`);
    await page.keyboard.press('Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await page.keyboard.press('Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);
    await snap(page, '02_main_hub.png', 'main_hub_screen');
    console.log(`main_hub: '${s}' (${elapsed()}s)`);

    // main_hub → song_select
    if (s === 'main_hub_screen') {
        console.log(`\n[main_hub→song_select]...`);
        for (let i = 0; i < 5; i++) {
            await page.keyboard.press('Enter');
            await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            const cur = await getScreen(page);
            console.log(`  Confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    // Give the refresh_top trigger time to set tex_file on album_art.pic
    // and let TheLoadMgr poll the FileLoader to completion.
    await new Promise(r => setTimeout(r, 6000));
    s = await getScreen(page);
    await snap(page, '03_song_select.png', 'song_select_screen');
    console.log(`song_select: '${s}' songCount=${await getSongCount(page)} (${elapsed()}s)`);

    // Read current highlight + try scrolling
    const initHl = await page.evaluate(() => ({
        song: window.rb3HighlightedSong || '',
        type: window.rb3HighlightedType || -1,
    }));
    console.log(`Initial highlighted: song='${initHl.song}' type=${initHl.type}`);

    // Keep pressing ArrowDown until we land on a kNodeSong (type=4) row. The
    // first few presses walk past function-rows (SETLISTS/PARTY SHUFFLE/RANDOM
    // SONG, all type=5) and a `123` SubheaderSortNode (type=2) before reaching
    // `20thcenturyboy` (type=4 song). Bail after 30 attempts.
    let songHl = initHl;
    for (let i = 0; i < 30; i++) {
        await page.keyboard.press('ArrowDown');
        await new Promise(r => setTimeout(r, 250));
        songHl = await page.evaluate(() => ({
            song: window.rb3HighlightedSong || '',
            type: window.rb3HighlightedType || -1,
        }));
        if (songHl.type === 4) break;  // kNodeSong
    }
    await new Promise(r => setTimeout(r, 5000));
    console.log(`After ArrowDown loop: song='${songHl.song}' type=${songHl.type}`);
    await snap(page, '03b_song_select_on_song.png', `song_select_on_song_${songHl.song}`);

    // Advance further to a different song
    for (let i = 0; i < 5; i++) {
        await page.keyboard.press('ArrowDown');
        await new Promise(r => setTimeout(r, 250));
        const h = await page.evaluate(() => ({
            song: window.rb3HighlightedSong || '',
            type: window.rb3HighlightedType || -1,
        }));
        if (h.type === 4 && h.song !== songHl.song) {
            songHl = h;
            break;
        }
    }
    await new Promise(r => setTimeout(r, 5000));
    console.log(`After more ArrowDown: song='${songHl.song}' type=${songHl.type}`);
    await snap(page, '03c_song_select_next_song.png', `song_select_next_song_${songHl.song}`);

    console.log(`\nAlbum-art-related fetches (${fetchLog.length}):`);
    for (const f of fetchLog.slice(0, 30)) {
        console.log(`  ${f.t}s ${f.status} ${f.url}`);
    }
    const ok = fetchLog.filter(f => f.status === 200).length;
    const bad = fetchLog.filter(f => f.status >= 400).length;
    console.log(`\nSummary: ${ok} ok, ${bad} fail`);

    writeFileSync(resolve(OUT_DIR, 'capture-summary.json'), JSON.stringify({
        captured: new Date().toISOString(),
        port: opts.port,
        appBooted,
        fetchLog,
    }, null, 2));

    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}\n${e.stack}`);
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); } catch {}
    }
}
