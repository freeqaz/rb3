#!/usr/bin/env node
/**
 * audio-stall-measure.mjs — measure frame-stall distribution and audio underrun
 * correlation during sustained song audio on the RB3 web build.
 *
 * Drives: boot → splash → main_hub → song_select (preview streams) →
 *         part_difficulty → game_screen (MOGG plays)
 *
 * Instruments:
 *   1. requestAnimationFrame gaps (user-visible freeze proxy)
 *   2. PerformanceObserver 'longtask' (>50ms main-thread blocks)
 *   3. Per-second underrun stats from window._rb3Audio.underruns
 *   4. rb3_audio_stats() console output (ring health)
 *   5. Continuous per-quanteunderrun log from the worklet's 0.5s postMessage
 *
 * Usage:
 *   node scripts/web/audio-stall-measure.mjs [--port 8421] [--play-secs 40]
 *
 * Output: /tmp/rb3-audio-stall/result.json + human summary
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PLAY_SECS = parseInt(arg('--play-secs', '40'), 10) || 40;
const OUT = '/tmp/rb3-audio-stall';
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

function waitForServer(port, timeoutMs = 20000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => http.get(`http://127.0.0.1:${port}/api/health`, r => {
            if (r.statusCode === 200) return res();
            retry();
        }).on('error', retry);
        const retry = () => Date.now() > deadline
            ? rej(new Error('Server not ready'))
            : setTimeout(check, 300);
        check();
    });
}

// Injected into the page before navigation: collects all stall data.
function installInstrumentation() {
    window.__stall = {
        rafGaps: [],             // [ms] every RAF gap
        longtasks: [],           // {start, dur} >50ms tasks
        underrunSamples: [],     // {t, events, frames, quanta, total} per 0.5s worklet report
        audioStatsCalls: [],     // raw rb3AudioStats console lines captured separately
        marks: [],               // performance.mark entries
    };
    const st = window.__stall;

    // 1. Long tasks (>50ms main-thread blocks).
    try {
        new PerformanceObserver(list => {
            for (const e of list.getEntries())
                st.longtasks.push({ start: +e.startTime.toFixed(1), dur: +e.duration.toFixed(1) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}

    // 2. RAF gaps.
    let lastRaf = -1;
    const raf = (t) => {
        if (lastRaf >= 0) st.rafGaps.push(+(t - lastRaf).toFixed(1));
        lastRaf = t;
        requestAnimationFrame(raf);
    };
    requestAnimationFrame(raf);

    // 3. Poll _rb3Audio.underruns every 0.5s (aligned with worklet postMessage cadence).
    // Store cumulative snapshots so we can compute per-second deltas.
    const pollUnderruns = () => {
        const a = window._rb3Audio;
        if (a && a.underruns) {
            const u = a.underruns;
            st.underrunSamples.push({
                t: performance.now(),
                events: u.underrunEvents | 0,
                frames: u.underrunFrames | 0,
                quanta: u.totalQuanta | 0,
                total: u.totalFrames | 0,
            });
        }
        setTimeout(pollUnderruns, 500);
    };
    setTimeout(pollUnderruns, 1000);  // start after 1s to let audio init
}

// Returns a snapshot of the stall data collected so far.
function collectData(page) {
    return page.evaluate(() => {
        const st = window.__stall || { rafGaps: [], longtasks: [], underrunSamples: [] };
        // Also pull the audio object state
        const a = window._rb3Audio;
        const audio = a ? {
            ctxRate: a.ctx ? a.ctx.sampleRate : null,
            bufFrames: a.bufFrames || null,
            underruns: a.underruns || null,
        } : null;
        return { ...st, audio, frameCount: window.rb3FrameCount || 0, screen: window.rb3CurrentScreen || '' };
    });
}

async function pressKey(page, key, holdMs = 250) {
    try {
        await page.keyboard.down(key);
        await sleep(holdMs);
        await page.keyboard.up(key);
        await sleep(200);
    } catch {}
}

async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) {
    const deadline = Date.now() + timeoutMs;
    let s = '';
    while (Date.now() < deadline) {
        s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await sleep(250);
    }
    return s;
}

let browser;
const logs = [];

try {
    await waitForServer(PORT);
    console.log(`[stall] server up at :${PORT}`);

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps',
            '--disable-sync',
            // NOTE: do NOT --mute-audio — we need the AudioWorklet to run
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
        if (/audio|underrun|PumpAudio|AudioDevice|AudioWorklet|rb3_audio_stats|stall|LONG/i.test(text))
            console.log(`  [${elapsed()}s] ${text.slice(0, 120)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));

    // Install instrumentation BEFORE page load.
    await page.addInitScript(installInstrumentation);

    console.log(`[stall] loading http://127.0.0.1:${PORT}/`);
    await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Click canvas to enable AudioContext (needs user gesture in some modes).
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    // Wait for app boot.
    console.log('[stall] waiting for app boot...');
    {
        const deadline = Date.now() + 300000;
        while (Date.now() < deadline) {
            const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
            if (b >= 1) break;
            await sleep(500);
        }
    }
    console.log(`[stall] booted (${elapsed()}s)`);

    // Wait for splash_screen.
    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    console.log(`[stall] screen: ${s} (${elapsed()}s)`);

    // Skip intro movie if needed.
    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) {
        await pressKey(page, 'Space', 300);
        s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    await sleep(2000);

    // splash → main_hub
    if (s === 'splash_screen') {
        console.log('[stall] splash → main_hub...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 8 && s === 'splash_screen'; i++) {
            await pressKey(page, 'Enter');
            s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
        }
        if (s !== 'main_hub_screen')
            s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await sleep(3000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[stall] main_hub reached: ${s} (${elapsed()}s)`);

    // main_hub → song_select
    if (s === 'main_hub_screen') {
        console.log('[stall] main_hub → song_select...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        for (let i = 0; i < 5; i++) {
            await pressKey(page, 'Enter');
            const cur = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
            if (cur && cur !== 'main_hub_screen') { s = cur; break; }
            await sleep(1500);
        }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen')
            s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await sleep(4000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[stall] song_select reached: ${s} (${elapsed()}s)`);

    // ---- PHASE A: measure preview audio stalls (song_select) ----
    // Scroll down to land on a song and let preview stream.
    console.log('[stall] scrolling to song (preview phase)...');
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    await pressKey(page, 'ArrowDown', 200);
    await sleep(8000);  // let preview start and stream
    const previewSnapshot = await collectData(page);
    // Call rb3_audio_stats
    await page.evaluate(() => {
        if (typeof Module !== 'undefined' && Module._rb3_audio_stats) Module._rb3_audio_stats();
        else if (typeof _rb3_audio_stats !== 'undefined') _rb3_audio_stats();
    }).catch(() => {});
    await sleep(500);
    console.log(`[stall] preview snapshot: ${previewSnapshot.underrunSamples.length} underrun samples, screen=${previewSnapshot.screen}`);

    // ---- PHASE B: navigate to game_screen for MOGG playback ----
    console.log('[stall] navigating to game_screen...');
    await page.evaluate(() => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = '20thcenturyboy'; }).catch(() => {});
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(3000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[stall] part_difficulty reached: ${s} (${elapsed()}s)`);

    for (let i = 0; i < 5; i++) {
        await pressKey(page, 'Enter', 150);
        await sleep(1200);
        const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (cur === 'game_screen') { s = cur; break; }
    }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    console.log(`[stall] game_screen reached: ${s} (${elapsed()}s)`);

    // ---- PHASE C: measure stalls during MOGG playback ----
    console.log(`[stall] measuring for ${PLAY_SECS}s during gameplay...`);
    const gameStart = Date.now();
    const periodicSamples = [];

    for (let i = 0; i < PLAY_SECS; i++) {
        await sleep(1000);
        const snap = await collectData(page).catch(() => null);
        if (snap) periodicSamples.push({ t: i + 1, ...snap });
        // Call audio stats every 10s
        if (i % 10 === 9) {
            await page.evaluate(() => {
                if (typeof Module !== 'undefined' && Module._rb3_audio_stats) Module._rb3_audio_stats();
            }).catch(() => {});
        }
        process.stdout.write(`  [${i+1}s] `);
        if (snap && snap.audio && snap.audio.underruns) {
            const u = snap.audio.underruns;
            process.stdout.write(`underruns=${u.underrunEvents} frames=${u.underrunFrames}/${u.totalFrames} (${u.totalFrames > 0 ? (100*u.underrunFrames/u.totalFrames).toFixed(1) : '?'}%)  `);
        }
        if (snap) process.stdout.write(`rafGaps=${snap.rafGaps.length}  longtasks=${snap.longtasks.length}`);
        process.stdout.write('\n');
    }

    const gameDuration = (Date.now() - gameStart) / 1000;
    const finalData = await collectData(page);

    // ---- Analysis ----
    const gaps = finalData.rafGaps || [];
    const longtasks = finalData.longtasks || [];
    const underrunSamples = finalData.underrunSamples || [];
    const audio = finalData.audio;

    // Separate boot-phase gaps from steady-state
    // We instrumented from page load; game_screen is reached around elapsed ~elapsed_s.
    // Approximate: the last PLAY_SECS * 1.05 * 60fps = steady-state gaps
    const STEADY_GAP_COUNT = Math.floor(PLAY_SECS * 60);
    const steadyGaps = gaps.slice(-Math.min(STEADY_GAP_COUNT, gaps.length));
    const allGapsSorted = steadyGaps.slice().sort((a, b) => b - a);

    function pct(arr, p) {
        if (!arr.length) return 0;
        const i = Math.max(0, Math.floor(p / 100 * arr.length) - 1);
        return arr[i];
    }
    const sortedAsc = allGapsSorted.slice().reverse();

    console.log('\n' + '='.repeat(70));
    console.log('WEB FRAME-STALL DISTRIBUTION (steady-state RAF gaps)');
    console.log(`Samples: ${steadyGaps.length} RAF gaps from last ${PLAY_SECS}s of game playback`);
    console.log(`p50=${pct(sortedAsc, 50).toFixed(1)}ms  p90=${pct(sortedAsc, 90).toFixed(1)}ms  p99=${pct(sortedAsc, 99).toFixed(1)}ms  max=${allGapsSorted[0]?.toFixed(1) || 'n/a'}ms`);

    const buckets = [
        [0, 16, '<16ms'],
        [16, 50, '16-50ms'],
        [50, 100, '50-100ms'],
        [100, 300, '100-300ms'],
        [300, 1000, '300-1000ms'],
        [1000, Infinity, '>1000ms'],
    ];
    console.log('Histogram:');
    for (const [lo, hi, lbl] of buckets) {
        const n = steadyGaps.filter(g => g >= lo && g < hi).length;
        const pct_ = steadyGaps.length > 0 ? (100 * n / steadyGaps.length).toFixed(1) : '?';
        console.log(`  [${lbl.padStart(10)}] : ${String(n).padStart(5)}  (${pct_}%)`);
    }

    console.log('-'.repeat(70));
    console.log('LONG TASKS (>50ms main-thread blocks):');
    const steadyLT = longtasks.filter(lt => lt.start > (longtasks.reduce((mx, t) => Math.max(mx, t.start), 0) - PLAY_SECS * 1000));
    console.log(`  Total longtasks: ${longtasks.length}  (last ${PLAY_SECS}s: ~${steadyLT.length})`);
    const top10lt = longtasks.slice().sort((a, b) => b.dur - a.dur).slice(0, 10);
    console.log('  Worst longtasks (ms):');
    for (const lt of top10lt)
        console.log(`    ${lt.dur.toFixed(0).padStart(7)}ms  @${(lt.start / 1000).toFixed(2)}s`);

    console.log('-'.repeat(70));
    console.log('AUDIO UNDERRUN SUMMARY:');
    if (audio && audio.underruns) {
        const u = audio.underruns;
        console.log(`  underrunEvents=${u.underrunEvents}  underrunFrames=${u.underrunFrames}  totalFrames=${u.totalFrames}`);
        const pctUR = u.totalFrames > 0 ? (100 * u.underrunFrames / u.totalFrames).toFixed(2) : '?';
        console.log(`  underrun rate: ${pctUR}% of frames silence-padded`);
        console.log(`  ctx sampleRate=${audio.ctxRate}  bufFrames=${audio.bufFrames}  bufDepth=${audio.bufFrames && audio.ctxRate ? (audio.bufFrames / audio.ctxRate * 1000).toFixed(0) : '?'}ms`);
    }

    // Per-second underrun delta (show where bursts happen)
    if (underrunSamples.length >= 2) {
        console.log('  Per-second underrun deltas (events/frames per ~1s window):');
        for (let i = 1; i < Math.min(underrunSamples.length, 60); i++) {
            const prev = underrunSamples[i - 1], cur = underrunSamples[i];
            const dEvents = cur.events - prev.events;
            const dFrames = cur.frames - prev.frames;
            const dt = ((cur.t - prev.t) / 1000).toFixed(1);
            if (dEvents > 0 || i < 5)
                console.log(`    t=${(cur.t / 1000).toFixed(1)}s  dt=${dt}s  +events=${dEvents}  +frames=${dFrames}`);
        }
    }

    // Correlation: do longtask spikes coincide with underrun bursts?
    // Find longtasks > 100ms and check if an underrun sample nearby shows a delta
    console.log('-'.repeat(70));
    console.log('CORRELATION: big stalls vs underrun bursts');
    const bigStalls = longtasks.filter(lt => lt.dur >= 100);
    console.log(`  Longtasks >=100ms: ${bigStalls.length}`);
    for (const lt of bigStalls) {
        // Find underrun samples within 2s of this stall
        const near = underrunSamples.filter(u => Math.abs(u.t - lt.start) < 2000);
        if (near.length >= 2) {
            const dFrames = near[near.length - 1].frames - near[0].frames;
            const dEvents = near[near.length - 1].events - near[0].events;
            console.log(`  stall ${lt.dur.toFixed(0)}ms @${(lt.start/1000).toFixed(2)}s → +${dEvents} underrun events, +${dFrames} frames in 2s window`);
        } else {
            console.log(`  stall ${lt.dur.toFixed(0)}ms @${(lt.start/1000).toFixed(2)}s → no underrun data nearby`);
        }
    }

    // Buffer depth recommendation
    console.log('-'.repeat(70));
    const p99gap = pct(sortedAsc, 99);
    const maxGap = allGapsSorted[0] || 0;
    const sampleRate = audio?.ctxRate || 44100;
    const p99bufMs = p99gap;
    const maxBufMs = maxGap;
    console.log('BUFFER DEPTH RECOMMENDATION:');
    console.log(`  p99 stall: ${p99gap.toFixed(0)}ms → need >=${Math.ceil(p99gap / 1000 * sampleRate)} frames (${(p99gap/1000).toFixed(3)}s) of pre-buffered audio`);
    console.log(`  max stall: ${maxGap.toFixed(0)}ms → need >=${Math.ceil(maxGap / 1000 * sampleRate)} frames (${(maxGap/1000).toFixed(3)}s) of pre-buffered audio`);
    console.log(`  VERDICT: ${maxGap > 1000 ? 'FREEZE-TYPE (>1s stall; buffer cannot save — loader fix needed)' : maxGap > 300 ? 'MEDIUM FREEZE (300ms-1s; need ~0.5-1s buffer)' : maxGap > 100 ? 'SPIKE-TYPE (100-300ms; a 0.5s buffer likely absorbs it)' : 'LIGHT SPIKE-TYPE (<100ms; a 100-200ms buffer absorbs p99)'}`);

    console.log('='.repeat(70));

    // Save results
    const result = {
        steadyGaps: { n: steadyGaps.length, p50: pct(sortedAsc, 50), p90: pct(sortedAsc, 90), p99: pct(sortedAsc, 99), max: allGapsSorted[0] || 0 },
        longtasks: { total: longtasks.length, worst: top10lt },
        audio: audio,
        underrunSamples,
        periodicSamples: periodicSamples.map(s => ({ t: s.t, frameCount: s.frameCount, screen: s.screen, underruns: s.audio?.underruns || null })),
        consoleLogs: logs,
        playDurationSecs: gameDuration,
    };
    writeFileSync(resolve(OUT, 'result.json'), JSON.stringify(result, null, 2));
    console.log(`[stall] results → ${OUT}/result.json`);
    process.exit(0);
} catch (e) {
    console.error('[stall] ERROR:', e.message);
    writeFileSync(resolve(OUT, 'error.json'), JSON.stringify({ error: e.message, stack: e.stack }, null, 2));
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), sleep(3000)]); } catch {}
    }
}
