#!/usr/bin/env node
/**
 * _framestall-songstart.mjs — capture a CDP CPU profile + timeline trace across
 * the SONG-START transition (part_difficulty → game_screen + first ~8s of song)
 * on the rb3-web build, to attribute the big main-thread longtasks to concrete
 * wasm/JS function stacks.
 *
 * Loads the ?debug=true build (-g2, demangled C++ names in the CPU profile).
 *
 * Drives boot → splash → main_hub → song_select → part_difficulty → game_screen,
 * arms Profiler.start + Tracing.start RIGHT BEFORE the final Enter that kicks the
 * song load, then samples for --profile-secs. Saves:
 *   songstart.cpuprofile   (V8 CPU profile; open in DevTools or analyze-cpuprofile.mjs)
 *   songstart.trace.json   (chrome tracing; disabled-by-default-devtools.timeline)
 *   songstart.meta.json    (longtasks + RAF gaps with wall-clock + profile-relative t,
 *                           screen transitions, marks)
 *
 * Usage:
 *   node scripts/web/_framestall-songstart.mjs --port 8533 --profile-secs 9
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8533'), 10);
const PROFILE_SECS = parseFloat(arg('--profile-secs', '9'));
const OUT = arg('--out', '/tmp/framestall-out');
const SONG = arg('--song', '20thcenturyboy');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

function waitForServer(port, timeoutMs = 20000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => http.get(`http://127.0.0.1:${port}/api/health`, r => {
            if (r.statusCode === 200) return res(); retry();
        }).on('error', retry);
        const retry = () => Date.now() > deadline ? rej(new Error('Server not ready')) : setTimeout(check, 300);
        check();
    });
}

function installInstrumentation() {
    window.__fs = { rafGaps: [], longtasks: [], screens: [], marks: [] };
    const st = window.__fs;
    try {
        new PerformanceObserver(list => {
            for (const e of list.getEntries())
                st.longtasks.push({ start: +e.startTime.toFixed(2), dur: +e.duration.toFixed(2) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}
    let lastRaf = -1;
    const raf = (t) => {
        if (lastRaf >= 0) st.rafGaps.push({ t: +t.toFixed(2), gap: +(t - lastRaf).toFixed(2) });
        lastRaf = t;
        requestAnimationFrame(raf);
    };
    requestAnimationFrame(raf);
    // watch screen transitions
    let lastScreen = '';
    setInterval(() => {
        const s = window.rb3CurrentScreen || '';
        if (s !== lastScreen) { st.screens.push({ t: +performance.now().toFixed(1), screen: s }); lastScreen = s; }
    }, 50);
}

async function pressKey(page, key, holdMs = 250) {
    try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(200); } catch {}
}
async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) {
    const deadline = Date.now() + timeoutMs; let s = '';
    while (Date.now() < deadline) {
        s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await sleep(200);
    }
    return s;
}

let browser;
const logs = [];
try {
    await waitForServer(PORT);
    console.log(`[fs] server up :${PORT}`);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--disable-extensions', '--disable-background-networking',
            '--disable-default-apps', '--disable-sync',
            '--autoplay-policy=no-user-gesture-required',
        ],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0 = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

    page.on('console', msg => {
        const text = msg.text();
        logs.push({ t: elapsed(), text });
        if (/song|load|game_screen|MILO|underrun|stall|error|Error/i.test(text))
            console.log(`  [${elapsed()}s] ${text.slice(0, 130)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));

    await page.addInitScript(installInstrumentation);

    // --no-domlog: neuter the on-page #console log mirror (appendChild + scrollHeight
    // read = forced sync layout per console line). Isolates true engine cost.
    if (argv.includes('--no-domlog')) {
        await page.addInitScript(() => {
            const kill = () => {
                const d = document.getElementById('console');
                // Detach from the document so appendChild/scrollHeight reads on it no
                // longer trigger a full-document relayout (the layout-thrash source).
                if (d && d.parentNode) d.parentNode.removeChild(d);
            };
            if (document.readyState !== 'loading') kill();
            document.addEventListener('DOMContentLoaded', kill);
            // Also poll briefly since the inline <script> may define log() after DCL.
            let n = 0; const iv = setInterval(() => { kill(); if (++n > 40) clearInterval(iv); }, 100);
        });
    }

    // CDP session for Profiler + Tracing.
    const client = await ctx.newCDPSession(page);

    console.log(`[fs] loading ?debug=true (g2 symbols)`);
    await page.goto(`http://127.0.0.1:${PORT}/?debug=true`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    // boot
    {
        const deadline = Date.now() + 300000;
        while (Date.now() < deadline) {
            const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
            if (b >= 1) break; await sleep(500);
        }
    }
    console.log(`[fs] booted ${elapsed()}s`);

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    console.log(`[fs] screen ${s} ${elapsed()}s`);
    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) { await pressKey(page, 'Space', 300); s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => ''); }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    await sleep(2000);

    if (s === 'splash_screen') {
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 8 && s === 'splash_screen'; i++) { await pressKey(page, 'Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
        if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await sleep(3000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[fs] main_hub ${s} ${elapsed()}s`);

    if (s === 'main_hub_screen') {
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        for (let i = 0; i < 5; i++) {
            await pressKey(page, 'Enter');
            const cur = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await sleep(1500);
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await sleep(4000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[fs] song_select ${s} ${elapsed()}s`);

    // arm target song; go to part_difficulty
    await page.evaluate((song) => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = song; }, SONG).catch(() => {});
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    await pressKey(page, 'ArrowDown', 200);
    await sleep(1500);
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(2500);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[fs] part_difficulty ${s} ${elapsed()}s`);

    const STEADY_DELAY = parseFloat(arg('--steady-delay', '0'));

    const traceEvents = [];
    const startProfilers = async () => {
        await client.send('Profiler.enable');
        await client.send('Profiler.setSamplingInterval', { interval: 200 }); // 200us = 5kHz
        await client.send('Profiler.start');
        client.on('Tracing.dataCollected', (d) => { for (const e of d.value) traceEvents.push(e); });
        await client.send('Tracing.start', {
            transferMode: 'ReportEvents',
            traceConfig: {
                recordMode: 'recordAsMuchAsPossible',
                includedCategories: [
                    'devtools.timeline',
                    'disabled-by-default-devtools.timeline',
                    'disabled-by-default-devtools.timeline.frame',
                    'v8.execute', 'v8', 'blink.user_timing', 'latencyInfo',
                ],
            },
        });
    };

    let armWall, armPerf;
    const kickSong = async () => {
        for (let i = 0; i < 6; i++) {
            await pressKey(page, 'Enter', 150);
            const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
            if (cur === 'game_screen') { s = cur; break; }
            await sleep(800);
        }
    };

    if (STEADY_DELAY > 0) {
        // STEADY-STATE mode: kick the song, wait for the load to settle, THEN profile.
        console.log(`[fs] STEADY mode: kicking song, settling ${STEADY_DELAY}s before profiling...`);
        await kickSong();
        s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 60000 });
        console.log(`[fs] game_screen ${s}; settling...`);
        await sleep(STEADY_DELAY * 1000);
        await startProfilers();
        armWall = Date.now();
        armPerf = await page.evaluate(() => performance.now());
        await page.evaluate(() => { performance.mark('fs:song_kick'); });
        console.log(`[fs] profiling ${PROFILE_SECS}s of STEADY gameplay`);
    } else {
        // SONG-START mode: arm right before the kick.
        console.log(`[fs] arming Profiler + Tracing, then kicking song load...`);
        await startProfilers();
        armWall = Date.now();
        armPerf = await page.evaluate(() => performance.now());
        await page.evaluate(() => { performance.mark('fs:song_kick'); });
        await kickSong();
    }

    // keep profiling for the full window regardless of when game_screen lands
    const profileDeadline = armWall + PROFILE_SECS * 1000;
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: Math.max(2000, profileDeadline - Date.now()) });
    console.log(`[fs] game_screen ${s} at +${((Date.now() - armWall) / 1000).toFixed(2)}s after arm`);

    while (Date.now() < profileDeadline) await sleep(100);

    // ===== STOP =====
    const prof = await client.send('Profiler.stop');
    await client.send('Tracing.end');
    await new Promise((res) => { client.once('Tracing.tracingComplete', res); setTimeout(res, 5000); });

    const meta = await page.evaluate(() => window.__fs);
    const screenNow = await page.evaluate(() => window.rb3CurrentScreen || '');
    console.log(`[fs] stopped. profile nodes=${prof.profile.nodes.length} samples=${prof.profile.samples.length} traceEvents=${traceEvents.length} screen=${screenNow}`);

    writeFileSync(resolve(OUT, 'songstart.cpuprofile'), JSON.stringify(prof.profile));
    writeFileSync(resolve(OUT, 'songstart.trace.json'), JSON.stringify({ traceEvents }));
    writeFileSync(resolve(OUT, 'songstart.meta.json'), JSON.stringify({
        armWall, armPerf, profileSecs: PROFILE_SECS,
        screens: meta.screens, longtasks: meta.longtasks, rafGaps: meta.rafGaps,
        finalScreen: screenNow, song: SONG, logs,
    }, null, 2));
    console.log(`[fs] wrote ${OUT}/songstart.{cpuprofile,trace.json,meta.json}`);
    process.exit(0);
} catch (e) {
    console.error('[fs] ERROR:', e.message, e.stack);
    writeFileSync(resolve(OUT, 'error.json'), JSON.stringify({ error: e.message, stack: e.stack }));
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
