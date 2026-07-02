#!/usr/bin/env node
/**
 * RB3 Web — W3c gameplay: drive the menu from splash → main_hub → song_select →
 * part_difficulty → game_screen (the gem track), all by keyboard, and let the
 * song stream + score logic run. Captures audio-decode evidence + in-game
 * screenshots.
 *
 * The web input path (rb3_game_input.cpp) replicates the native crossing: a
 * Confirm on song_select fires {music_library select_highlighted_node} (the
 * kept song-select aid, via rb3WebUseAids), and the part_difficulty screen is
 * crossed with REAL Enter/ArrowDown key presses against the actual
 * choose_part -> choose_diff overshell sub-views (window.rb3OvershellView),
 * mirroring keyboard-to-gameplay.mjs. The old synthetic
 * track:guitar -> overshell:end_override_flow verb sequence no longer exists.
 *
 * Usage:
 *   node scripts/web/w3c-gameplay-test.mjs [--port 8431] [--verbose]
 *                                          [--play-seconds 30]
 *
 * Output: scripts/web/results/web-w3c/gameplay/ + flow.json + console.jsonl
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port:        parseInt(argv[argv.indexOf('--port') + 1] || '8431', 10) || 8431,
    verbose:     argv.includes('--verbose'),
    playSeconds: parseInt(argv[argv.indexOf('--play-seconds') + 1] || '30', 10) || 30,
    debug:       argv.includes('--debug'),  // hit /?debug=true (native/web/server.py no-store debug build)
};

const BOOT_TIMEOUT_MS   = 300000;
const SPLASH_TIMEOUT_MS = 180000;
// The MOGG for 20th Century Boy is ~37MB; the on-demand sync XHR fetch +
// decrypt + decode can take a while. Be generous waiting for game_screen.
// +30s headroom vs the old direct-commit hack: the real choose_part ->
// choose_diff overshell crossing now takes longer than the removed synthetic
// track:guitar -> end_override_flow shortcut did.
const LOADSONG_TIMEOUT_MS = 270000;

const OUT_DIR = resolve(__dirname, 'results/web-w3c');
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
const getFrame = (page) => page.evaluate(() => window.rb3FrameCount || 0);
// The pad-0 overshell slot's current view symbol (choose_part_guitar,
// choose_diff, ready_to_play, ...) — published by main_web.cpp's
// PublishCurrentScreen, same probe keyboard-to-gameplay.mjs uses.
const getView = (page) => page.evaluate(() => window.rb3OvershellView || '?');

async function snap(page, label) {
    const path = resolve(OUT_DIR, 'gameplay', `${label}.png`);
    const a = await analyzeCanvas(page, path);
    const screen = await getScreen(page);
    const frame = await getFrame(page);
    flow.push({ label, screen, frame, painted: a.paintedPct, png: path });
    console.log(`  SNAP [${label}] screen='${screen}' frame=${frame} painted=${a.paintedPct}%`);
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

// One clean keypress: down -> hold (several frames) -> up -> gap (bit clears).
// A bare page.keyboard.press() (0ms between down/up) races the emscripten
// main loop's requestAnimationFrame poll of window._rb3Keys: if the down+up
// pair completes within the same JS tick, the engine's next frame can sample
// the bitmask AFTER it already cleared and see no edge at all — a real press
// that never happened as far as the game is concerned. Mirrors the `press()`
// helper in keyboard-to-gameplay.mjs (which does NOT use bare .press()).
async function press(page, key, holdMs = 220, gapMs = 400) {
    await page.keyboard.down(key);
    await new Promise(r => setTimeout(r, holdMs));
    await page.keyboard.up(key);
    await new Promise(r => setTimeout(r, gapMs));
}

// Same as waitScreen but polls the overshell sub-view (choose_part_guitar,
// choose_diff, ...) instead of the top-level screen name.
async function waitView(page, { targets = null, prefix = null, timeoutMs = 20000 }) {
    const deadline = Date.now() + timeoutMs;
    let v = await getView(page);
    while (Date.now() < deadline) {
        v = await getView(page);
        if (targets && targets.includes(v)) return v;
        if (prefix && v && v.startsWith(prefix)) return v;
        await new Promise(r => setTimeout(r, 250));
    }
    return v;
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
        if (opts.verbose || /web-input|web part-select|web song-select|screen:|FIRE|WAIT|GetSongStream|LoadSong|MOGG|mogg|decrypt|StreamReceiver|AudioDevice|AddSource|PumpAudio|track set|nofail|autohit|game_screen|VorbisReader/.test(text)) {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => { errors.push(err.message || String(err)); console.log(`  [PAGE_ERROR] ${err.message || err}`); });
    page.on('crash', () => { errors.push('Page crashed'); console.log('  [CRASH]'); });

    const url = `http://127.0.0.1:${opts.port}/${opts.debug ? '?debug=true' : ''}`;
    console.log(`Loading ${url} (W3c gameplay)`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await page.evaluate(() => {
        window.rb3WebUseAids = 1;
        window.rb3WebTargetSong = '20thcenturyboy';
    });

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

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // === Splash → main_hub ===
    console.log(`\n[splash→main_hub] Start then Confirm...`);
    await press(page, 'Space');
    s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
        await press(page, 'Enter');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
    }
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    await new Promise(r => setTimeout(r, 3000));
    s = await getScreen(page);
    await snap(page, '01_main_hub');

    // === main_hub → song_select ===
    if (s === 'main_hub_screen') {
        console.log(`\n[main_hub→song_select] Confirm chain...`);
        for (let i = 0; i < 5; i++) {
            await press(page, 'Enter');
            await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            const cur = await getScreen(page);
            console.log(`  Confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await new Promise(r => setTimeout(r, 1500));
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await new Promise(r => setTimeout(r, 4000));
    s = await getScreen(page);
    await snap(page, '02_song_select');
    console.log(`song_select reached: '${s}' songCount=${await getSongCount(page)}`);

    // === song_select → part_difficulty ===
    // "20th Century Boy" sorts first (alphabetical) in the library. The default
    // highlight is NOT the top row, so scroll all the way UP (ArrowUp clamps at
    // the top entry) to land the highlight on 20th Century Boy, THEN confirm
    // (music_library:select_highlighted_node picks the highlighted node).
    if (s === 'song_select_screen') {
        // The web song-select Confirm pins the highlight to the W3c target song
        // (20th Century Boy) via MusicLibrary::TryToSetHighlight before selecting
        // — deterministic, no blind list nav, no trap on a non-song header node.
        // Set the target explicitly (default is 20thcenturyboy anyway).
        await page.evaluate(() => {
            window.rb3WebUseAids = 1;
            window.rb3WebTargetSong = '20thcenturyboy';
        });
        const TARGET = '20thcenturyboy';
        await new Promise(r => setTimeout(r, 1000));
        await snap(page, '02b_song_top');
        console.log(`[song_select→part_difficulty] confirm pins '${TARGET}' + selects...`);
        for (let i = 0; i < 4; i++) {
            await press(page, 'Enter', 220, 400);
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
    await snap(page, '03_part_difficulty');
    console.log(`part_difficulty reached: '${s}'`);

    // === part_difficulty → game_screen (the finale) ===
    // Real key nav across the part + difficulty overshell sub-views
    // (choose_part_guitar -> choose_diff), mirroring keyboard-to-gameplay.mjs.
    // The old synthetic track:guitar -> overshell:end_override_flow autopilot
    // (armed by any Confirm on part_difficulty_screen) no longer exists; the
    // screen must actually be crossed via choose_part then choose_diff.
    if (s === 'part_difficulty_screen') {
        console.log(`\n[part_difficulty→game_screen] real key nav: choose_part -> choose_diff...`);
        const cp = await waitView(page, { prefix: 'choose_part', timeoutMs: 40000 });
        console.log(`  overshell view='${cp}' (${elapsed()}s)`);
        if (cp && cp.startsWith('choose_part')) {
            await press(page, 'Enter', 220, 600);
        }
        const cd = await waitView(page, { targets: ['choose_diff'], timeoutMs: 40000 });
        console.log(`  overshell view='${cd}' (${elapsed()}s)`);
        if (cd === 'choose_diff') {
            // Easy(0)/Medium(1)/Hard(2)/Expert(3) top-down, default focus Easy
            // — scroll down to Expert (same difficulty the old hard-coded
            // track:guitar shortcut always ended up on).
            for (let i = 0; i < 3; i++) {
                await press(page, 'ArrowDown', 180, 300);
            }
            await press(page, 'Enter', 220, 600);
        } else {
            console.log(`  WARN: overshell never reached choose_diff (view='${cd}')`);
        }
        console.log(`  waiting for game_screen (MOGG load may take a while)...`);
        const g = await waitScreen(page, { targets: ['game_screen'], timeoutMs: LOADSONG_TIMEOUT_MS });
        s = await getScreen(page);
        console.log(`  after crossing: screen='${s}' (${elapsed()}s)`);
        await snap(page, '04_after_crossing');
    }

    // === Let the song play; capture in-game frames ===
    if (s === 'game_screen' || (s && s !== 'part_difficulty_screen' && s !== 'song_select_screen')) {
        console.log(`\n[gameplay] letting song play ${opts.playSeconds}s, capturing frames...`);
        const startFrame = await getFrame(page);
        const startT = Date.now();
        for (let k = 0; k < opts.playSeconds; k += 5) {
            await new Promise(r => setTimeout(r, 5000));
            await snap(page, `05_gameplay_t${k+5}s`);
        }
        const endFrame = await getFrame(page);
        const dt = (Date.now() - startT) / 1000;
        console.log(`  played ${dt.toFixed(1)}s: frames ${startFrame}->${endFrame} (${((endFrame-startFrame)/dt).toFixed(1)} fps)`);
    }

    const finalScreen = await getScreen(page);
    console.log(`\n=== FINAL screen: '${finalScreen}' ===`);

    // Pull audio stats from the engine debug export.
    let audioStats = null;
    try {
        audioStats = await page.evaluate(() => {
            if (typeof window.rb3AudioStats === 'function') { window.rb3AudioStats(); return 'called rb3AudioStats()'; }
            return 'rb3AudioStats not available';
        });
    } catch { /* ignore */ }
    await new Promise(r => setTimeout(r, 500));

    const summary = {
        result: 'info',
        appBooted,
        final_screen: finalScreen,
        song_count: await getSongCount(page),
        final_frame: await getFrame(page),
        flow,
        deepest: finalScreen,
        audio_stats_call: audioStats,
        error_count: errors.length,
        errors: errors.slice(0, 30),
        log_count: logs.length,
    };
    writeFileSync(resolve(OUT_DIR, 'gameplay', 'flow.json'), JSON.stringify(summary, null, 2));
    writeFileSync(resolve(OUT_DIR, 'gameplay', 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(`\nFlow recorded → ${resolve(OUT_DIR, 'gameplay', 'flow.json')}`);
    for (const f of flow) console.log(`  ${f.label}: screen='${f.screen}' frame=${f.frame} painted=${f.painted}%`);
    process.exit(0);
} catch (e) {
    console.error(`Error: ${e.message}`);
    try {
        writeFileSync(resolve(OUT_DIR, 'gameplay', 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
        writeFileSync(resolve(OUT_DIR, 'gameplay', 'flow.json'), JSON.stringify({ result: 'error', message: e.message, flow, errors }, null, 2));
    } catch { /* ignore */ }
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); }
        catch { /* ignore */ }
    }
}
