#!/usr/bin/env node
/**
 * audio-jitter-profile.mjs — wave-10 unified, timestamp-aligned audio-jitter +
 * GC-causation profiler for the RB3 web build.
 *
 * THE CAUSATION TEST: in ONE browser session, drive to sustained song audio
 * (gameplay if reachable, else song_select PREVIEW) and capture, on ONE timeline:
 *   (a) requestAnimationFrame inter-frame gaps  (the freeze proxy)
 *   (b) PerformanceObserver 'longtask' (>50ms main-thread blocks)
 *   (c) JS heap / wasm heap (HEAPU8.length) / __rb3IdbCache size+bytes  (~every 300ms)
 *   (d) per-0.5s underrun stats from window._rb3Audio.underruns
 *   (e) V8 GC events via CDP Tracing (v8 / disabled-by-default-v8.gc / devtools.timeline)
 *
 * All page-side timestamps are performance.now() (page clock). CDP trace event ts
 * are microseconds on the monotonic clock; we capture a (performance.now <-> CDP ts)
 * offset pair so GC events can be aligned to the page rAF/underrun timeline.
 *
 * Usage:
 *   node scripts/web/audio-jitter-profile.mjs [--port 8421] [--play-secs 50]
 *        [--out DIR] [--tag w10-webcap-1]
 *
 * Reuses logic from audio-stall-measure.mjs + _songlib-mem.mjs. Standalone (no
 * lib import needed for the CDP path); navigation mirrors core.mjs / stall-measure.
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PLAY_SECS = parseInt(arg('--play-secs', '50'), 10) || 50;
const TAG = arg('--tag', `w10-webcap-${Date.now()}`);
const OUT = arg('--out', `/home/free/code/milohax/rb3/docs/native/audio-perf-loop/baselines/${TAG}`);
// Load the no-store debug build (?debug=true) instead of the HTTP-cached release
// build at `/`. Use when only the debug wasm carries the change under test.
const DEBUG_BUILD = argv.includes('--debug-build');
const PAGE_URL = `http://127.0.0.1:${PORT}/${DEBUG_BUILD ? '?debug=true' : ''}`;
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
            ? rej(new Error('Server not ready')) : setTimeout(check, 300);
        check();
    });
}

// ---- page-side instrumentation: rAF gaps + longtasks + heap samples + underrun samples
function installInstrumentation() {
    window.__jit = {
        rafGaps: [],          // {t (perf.now of THIS frame), gap}
        longtasks: [],        // {start, dur}  (perf.now-relative startTime)
        heap: [],             // {t, jsUsed, jsTotal, jsLimit, wasm, cacheN, cacheBytes}
        underruns: [],        // {t, events, frames, quanta, total}  (cumulative snapshots)
        clockPairs: [],       // {perf, epoch}  perf.now <-> Date.now pairs for alignment sanity
    };
    const st = window.__jit;

    // capture a perf.now <-> Date.now pair immediately (and again later) for clock alignment.
    const pair = () => st.clockPairs.push({ perf: +performance.now().toFixed(3), epoch: Date.now() });
    pair();

    // longtasks
    try {
        new PerformanceObserver(list => {
            for (const e of list.getEntries())
                st.longtasks.push({ start: +e.startTime.toFixed(2), dur: +e.duration.toFixed(2) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}

    // rAF gaps — record the frame's own perf.now AND the gap, so gaps align to the timeline.
    let lastRaf = -1;
    const raf = (t) => {
        if (lastRaf >= 0) st.rafGaps.push({ t: +t.toFixed(2), gap: +(t - lastRaf).toFixed(2) });
        lastRaf = t;
        requestAnimationFrame(raf);
    };
    requestAnimationFrame(raf);

    // heap sampler ~ every 300ms (JS heap + wasm heap + idbCache)
    const heapSample = () => {
        const m = performance.memory || {};
        let cacheN = 0, cacheBytes = 0;
        try { const c = window.__rb3IdbCache; if (c) { cacheN = c.size; c.forEach(v => cacheBytes += (v.byteLength || 0)); } } catch {}
        let wasm = 0;
        try { wasm = window.HEAPU8 ? window.HEAPU8.length : (window.Module && window.Module.HEAPU8 ? window.Module.HEAPU8.length : 0); } catch {}
        st.heap.push({
            t: +performance.now().toFixed(2),
            jsUsed: m.usedJSHeapSize | 0, jsTotal: m.totalJSHeapSize | 0, jsLimit: m.jsHeapSizeLimit | 0,
            wasm, cacheN, cacheBytes,
        });
        setTimeout(heapSample, 300);
    };
    setTimeout(heapSample, 300);

    // underrun sampler ~ every 0.5s (aligned with the worklet postMessage cadence)
    const urSample = () => {
        const a = window._rb3Audio;
        if (a && a.underruns) {
            const u = a.underruns;
            st.underruns.push({
                t: +performance.now().toFixed(2),
                events: u.underrunEvents | 0, frames: u.underrunFrames | 0,
                quanta: u.totalQuanta | 0, total: u.totalFrames | 0,
                // Per-window ring low-water mark (frames). Dips here while events
                // stays 0 are the near-misses the underrun counter is blind to.
                minDepth: u.minRingDepthFrames | 0,
            });
        }
        setTimeout(urSample, 500);
    };
    setTimeout(urSample, 1000);
}

function collect(page) {
    return page.evaluate(() => {
        const st = window.__jit || {};
        const a = window._rb3Audio;
        const audio = a ? { ctxRate: a.ctx ? a.ctx.sampleRate : null, bufFrames: a.bufFrames || null, underruns: a.underruns || null } : null;
        // refresh a clock pair at collect time too
        try { st.clockPairs && st.clockPairs.push({ perf: +performance.now().toFixed(3), epoch: Date.now() }); } catch {}
        return {
            rafGaps: st.rafGaps || [], longtasks: st.longtasks || [], heap: st.heap || [],
            underruns: st.underruns || [], clockPairs: st.clockPairs || [],
            audio, screen: window.rb3CurrentScreen || '', frameCount: window.rb3FrameCount || 0,
        };
    });
}

async function pressKey(page, key, holdMs = 250) {
    try {
        await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(200);
    } catch {}
}
const screenOf = (p) => p.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) {
    const deadline = Date.now() + timeoutMs; let s = '';
    while (Date.now() < deadline) {
        s = await screenOf(page);
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await sleep(250);
    }
    return s;
}

let browser;
const logs = [];
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

// ---- CDP trace event accumulator ----
const traceEvents = [];

try {
    await waitForServer(PORT);
    console.log(`[jit] server up :${PORT}, tag=${TAG}`);

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps', '--disable-sync',
            '--autoplay-policy=no-user-gesture-required', // do NOT --mute-audio: worklet must run
        ],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    page.on('console', msg => {
        const text = msg.text();
        logs.push({ t: elapsed(), text });
        if (/audio|underrun|PumpAudio|AudioDevice|latency|stall|GROW|SHRINK/i.test(text))
            console.log(`  [${elapsed()}s] ${text.slice(0, 130)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));
    page.on('crash', () => console.log('  [CRASH] page crashed'));

    // CDP session for GC tracing.
    const client = await ctx.newCDPSession(page);
    client.on('Tracing.dataCollected', (d) => { if (d && d.value) for (const ev of d.value) traceEvents.push(ev); });

    await page.addInitScript(installInstrumentation);

    console.log(`[jit] loading ${PAGE_URL}${DEBUG_BUILD ? ' (debug build)' : ''}`);
    await page.goto(PAGE_URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    // boot
    console.log('[jit] waiting for boot...');
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log(`[jit] booted (${elapsed()}s)`);

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    console.log(`[jit] screen: ${s} (${elapsed()}s)`);
    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) { await pressKey(page, 'Space', 300); s = await screenOf(page); }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await screenOf(page); await sleep(2000);

    // splash -> main_hub
    if (s === 'splash_screen') {
        console.log('[jit] splash -> main_hub...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 8 && s === 'splash_screen'; i++) { await pressKey(page, 'Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
        if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await sleep(3000); s = await screenOf(page);
    console.log(`[jit] main_hub: ${s} (${elapsed()}s)`);

    // main_hub -> song_select
    let reachedSongSelect = false;
    if (s === 'main_hub_screen') {
        console.log('[jit] main_hub -> song_select...');
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
    await sleep(3000); s = await screenOf(page);
    reachedSongSelect = (s === 'song_select_screen');
    console.log(`[jit] song_select: ${s} (${elapsed()}s) reached=${reachedSongSelect}`);

    // ---- start CDP GC tracing NOW (just before the steady-audio measurement window) ----
    // Categories: v8 + disabled-by-default-v8.gc (GC pauses) + devtools.timeline (RunTask markers for alignment).
    console.log('[jit] starting CDP Tracing (v8.gc)...');
    // NOTE: deliberately OMIT 'disabled-by-default-v8.gc_stats' — enabling it makes V8
    // run V8.GC_OBJECT_DUMP_STATISTICS *inside* every MajorGC (60-140ms each), which is a
    // pure tracing observer-effect that inflates the measured MajorGC pause ~10-20x. We
    // only need the canonical MajorGC/MinorGC slices from 'disabled-by-default-v8.gc'.
    await client.send('Tracing.start', {
        transferMode: 'ReportEvents',
        bufferUsageReportingInterval: 1000,
        traceConfig: {
            recordMode: 'recordContinuously',
            includedCategories: [
                'v8', 'disabled-by-default-v8.gc',
                'devtools.timeline',
                'blink.user_timing',
            ],
        },
    });
    // capture a CDP-ts <-> page perf.now alignment marker: emit a user-timing mark, then read perf.now.
    const alignMark = await page.evaluate(() => {
        const name = '__jit_align_' + Math.random().toString(36).slice(2);
        performance.mark(name);
        return { name, perf: performance.now(), epoch: Date.now() };
    });
    console.log(`[jit] align mark=${alignMark.name} perf=${alignMark.perf.toFixed(1)} (${elapsed()}s)`);

    let phase = 'unknown';
    // Try to navigate to gameplay; if it crashes / fails, fall back to PREVIEW in song_select.
    let crashedDuringNav = false;
    page.on('crash', () => { crashedDuringNav = true; });

    if (reachedSongSelect) {
        // Kick a preview by scrolling (preview audio = same decode->mix->resample->SAB pump path).
        console.log('[jit] song_select: kicking PREVIEW (scroll) + attempting gameplay...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'ArrowDown', 200);
        await sleep(3000);
        // Attempt gameplay nav. If it crashes (known main_hub->song_select / song_select->game instability),
        // we'll detect the crash / failure and stay measuring preview.
        await page.evaluate(() => { try { window.rb3WebUseAids = 1; window.rb3WebTargetSong = '20thcenturyboy'; } catch {} }).catch(() => {});
        let curScreen = await screenOf(page);
        try {
            await pressKey(page, 'Enter', 220);
            curScreen = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
        } catch {}
        if (!crashedDuringNav && curScreen === 'part_difficulty_screen') {
            await sleep(2000);
            for (let i = 0; i < 5; i++) {
                await pressKey(page, 'Enter', 150); await sleep(1200);
                const c = await screenOf(page).catch(() => '');
                if (c === 'game_screen') break;
            }
            const g = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 60000 });
            if (g === 'game_screen' && !crashedDuringNav) phase = 'gameplay';
        }
        if (phase !== 'gameplay') {
            // Fall back to preview. Make sure we're still in song_select and re-kick a preview.
            const c = await screenOf(page).catch(() => '');
            console.log(`[jit] gameplay not reached (screen=${c} crashed=${crashedDuringNav}); measuring PREVIEW`);
            if (c === 'song_select_screen') {
                await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
                // gently scroll to re-trigger preview hovers during the window
                phase = 'preview';
            } else {
                phase = `stuck:${c || (crashedDuringNav ? 'crashed' : 'unknown')}`;
            }
        }
    } else {
        phase = `nosongselect:${s}`;
    }
    console.log(`[jit] STEADY phase = ${phase} (${elapsed()}s)`);

    // Mark the start of the steady measurement window on the page clock.
    const steadyStart = await page.evaluate(() => performance.now());

    // ---- STEADY measurement window: PLAY_SECS of audio ----
    console.log(`[jit] measuring ${PLAY_SECS}s of steady audio (phase=${phase})...`);
    for (let i = 0; i < PLAY_SECS; i++) {
        await sleep(1000);
        // In preview mode, nudge the cursor every ~6s to keep a fresh preview streaming.
        if (phase === 'preview' && i > 0 && i % 6 === 0) {
            await pressKey(page, (i / 6) % 2 === 0 ? 'ArrowDown' : 'ArrowUp', 160);
        }
        if (i % 10 === 9) {
            const snap = await collect(page).catch(() => null);
            if (snap && snap.audio && snap.audio.underruns) {
                const u = snap.audio.underruns;
                console.log(`  [${i + 1}s] ur events=${u.underrunEvents} frames=${u.underrunFrames}/${u.totalFrames} minDepth=${u.minRingDepthFrames | 0} rafGaps=${snap.rafGaps.length} lt=${snap.longtasks.length} heap=${snap.heap.length}`);
            } else {
                console.log(`  [${i + 1}s] (no audio underrun obj yet) screen=${snap?.screen}`);
            }
        }
    }
    const steadyEnd = await page.evaluate(() => performance.now());

    // capture another align mark at the end (drift check)
    const alignMark2 = await page.evaluate(() => {
        const name = '__jit_align2_' + Math.random().toString(36).slice(2);
        performance.mark(name);
        return { name, perf: performance.now(), epoch: Date.now() };
    });

    // ---- stop tracing, drain events ----
    console.log('[jit] stopping CDP Tracing...');
    const tracingComplete = new Promise(res => client.once('Tracing.tracingComplete', res));
    await client.send('Tracing.end');
    await Promise.race([tracingComplete, sleep(15000)]);
    await sleep(500);
    console.log(`[jit] collected ${traceEvents.length} trace events`);

    const data = await collect(page);

    // ============ ANALYSIS ============
    // Align CDP trace ts (microseconds, monotonic) to page perf.now (ms).
    // Find our align user-timing marks in the trace; their 'ts' (us) maps to alignMark.perf (ms).
    function findMark(evs, name) {
        // blink.user_timing emits an instant/complete event with args.data.name or the 'name' field.
        for (const e of evs) {
            if (e.cat && e.cat.includes('blink.user_timing')) {
                const nm = (e.args && e.args.data && e.args.data.name) || e.name;
                if (nm === name) return e.ts; // microseconds
            }
            if (e.name === name) return e.ts;
        }
        return null;
    }
    let traceTsForAlign = findMark(traceEvents, alignMark.name);
    let traceTsForAlign2 = findMark(traceEvents, alignMark2.name);
    // offset: page_perf_ms ≈ (trace_ts_us - offsetUs) / 1000  → offsetUs = trace_ts_us - perf_ms*1000
    let offsetUs = null, offsetUs2 = null, driftMsPerSec = null;
    if (traceTsForAlign != null) offsetUs = traceTsForAlign - alignMark.perf * 1000;
    if (traceTsForAlign2 != null) offsetUs2 = traceTsForAlign2 - alignMark2.perf * 1000;
    if (offsetUs != null && offsetUs2 != null) {
        const dPerf = (alignMark2.perf - alignMark.perf) / 1000; // sec
        driftMsPerSec = dPerf > 0 ? ((offsetUs2 - offsetUs) / 1000) / dPerf : 0;
    }
    const usableOffset = offsetUs != null ? offsetUs : (offsetUs2 != null ? offsetUs2 : null);
    const traceTsToPerfMs = (tsUs) => usableOffset != null ? (tsUs - usableOffset) / 1000 : null;

    // Extract GC events. ONLY the canonical top-level stop-the-world slices count as a
    // main-thread pause: 'MajorGC' (mark-compact) and 'MinorGC' (scavenge). Every other
    // 'V8.GC_*' slice is a NESTED sub-phase of one of these (or a background-thread slice)
    // and must NOT be counted separately (it would multiply the total many-fold). We keep
    // ALL gc-cat events in `gcEventsAll` for inspection but build pause stats from the
    // canonical pair only.
    const gcEventsAll = [];
    const gcEvents = []; // canonical pauses only
    for (const e of traceEvents) {
        if (e.ph !== 'X' || typeof e.dur !== 'number') continue;
        const isGcCat = e.cat && (e.cat.includes('v8.gc') || (e.cat.includes('devtools.timeline') && /GC/.test(e.name || '')));
        if (!isGcCat && !/^V8\.GC|^MajorGC$|^MinorGC$/.test(e.name || '')) continue;
        const perfMs = traceTsToPerfMs(e.ts);
        gcEventsAll.push({ name: e.name, ts: e.ts, durMs: +(e.dur / 1000).toFixed(3), perfMs });
        if (e.name === 'MajorGC') gcEvents.push({ name: 'MajorGC', kind: 'major', ts: e.ts, durMs: e.dur / 1000, perfMs });
        else if (e.name === 'MinorGC') gcEvents.push({ name: 'MinorGC', kind: 'minor', ts: e.ts, durMs: e.dur / 1000, perfMs });
    }
    gcEvents.sort((a, b) => a.ts - b.ts);
    gcEventsAll.sort((a, b) => a.ts - b.ts);

    function pctile(arr, p) {
        if (!arr.length) return 0;
        const a = arr.slice().sort((x, y) => x - y);
        const idx = Math.min(a.length - 1, Math.max(0, Math.ceil(p / 100 * a.length) - 1));
        return a[idx];
    }

    // Steady-window filter on page clock.
    const inSteady = (t) => t >= steadyStart && t <= steadyEnd;
    const steadyRafGaps = data.rafGaps.filter(g => inSteady(g.t));
    const gapVals = steadyRafGaps.map(g => g.gap);
    const steadyLongtasks = data.longtasks.filter(lt => inSteady(lt.start));
    const steadyHeap = data.heap.filter(h => inSteady(h.t));
    const steadyUnder = data.underruns.filter(u => inSteady(u.t));
    const steadyGc = gcEvents.filter(g => g.perfMs != null && inSteady(g.perfMs));

    // 1. jitter distribution
    const jitter = {
        n: gapVals.length,
        p50: +pctile(gapVals, 50).toFixed(2), p95: +pctile(gapVals, 95).toFixed(2),
        p99: +pctile(gapVals, 99).toFixed(2), max: gapVals.length ? +Math.max(...gapVals).toFixed(2) : 0,
        gapsOver33: gapVals.filter(g => g > 33).length,
        gapsOver50: gapVals.filter(g => g > 50).length,
        gapsOver100: gapVals.filter(g => g > 100).length,
        windowSecs: +((steadyEnd - steadyStart) / 1000).toFixed(2),
    };

    // 2. underruns/sec across steady region
    let underrunPerSec = null, framesPerSec = null, urDelta = null, framesDelta = null;
    if (steadyUnder.length >= 2) {
        const first = steadyUnder[0], last = steadyUnder[steadyUnder.length - 1];
        const dt = (last.t - first.t) / 1000;
        urDelta = last.events - first.events; framesDelta = last.frames - first.frames;
        underrunPerSec = dt > 0 ? +(urDelta / dt).toFixed(3) : null;
        framesPerSec = dt > 0 ? +(framesDelta / dt).toFixed(1) : null;
    }

    // 2b. ring low-water mark (the near-miss series the underrun counter is blind to).
    // Each sample is the smallest ring depth the worklet saw in its ~0.5s window.
    let minDepthStats = null;
    const depthVals = steadyUnder.map(u => u.minDepth | 0).filter(d => d > 0);
    if (depthVals.length) {
        minDepthStats = {
            samples: depthVals.length,
            min: Math.min(...depthVals),
            p5: +pctile(depthVals, 5).toFixed(0),
            p50: +pctile(depthVals, 50).toFixed(0),
            max: Math.max(...depthVals),
            // windows where the ring dipped below ~5ms @48k (240 frames) — true near-misses
            dipsUnder240: depthVals.filter(d => d < 240).length,
            last: steadyUnder.length ? (steadyUnder[steadyUnder.length - 1].minDepth | 0) : null,
        };
    }

    // 3. heap slope (linear fit over steady region) in MB/min
    function slopeMBperMin(samples, field) {
        if (samples.length < 3) return null;
        const xs = samples.map(s => s.t / 1000); // sec
        const ys = samples.map(s => (s[field] || 0) / 1048576); // MB
        const n = xs.length;
        const mx = xs.reduce((a, b) => a + b, 0) / n, my = ys.reduce((a, b) => a + b, 0) / n;
        let num = 0, den = 0;
        for (let i = 0; i < n; i++) { num += (xs[i] - mx) * (ys[i] - my); den += (xs[i] - mx) ** 2; }
        if (den === 0) return null;
        return +((num / den) * 60).toFixed(3); // MB per minute
    }
    const heapSlope = {
        jsMBperMin: slopeMBperMin(steadyHeap, 'jsUsed'),
        wasmMBperMin: slopeMBperMin(steadyHeap, 'wasm'),
        idbMBperMin: steadyHeap.length >= 3 ? +((slopeMBperMin(steadyHeap, 'cacheBytes')) || 0).toFixed(3) : null,
        jsFirstMB: steadyHeap.length ? +(steadyHeap[0].jsUsed / 1048576).toFixed(1) : null,
        jsLastMB: steadyHeap.length ? +(steadyHeap[steadyHeap.length - 1].jsUsed / 1048576).toFixed(1) : null,
        wasmFirstMB: steadyHeap.length ? +(steadyHeap[0].wasm / 1048576).toFixed(1) : null,
        wasmLastMB: steadyHeap.length ? +(steadyHeap[steadyHeap.length - 1].wasm / 1048576).toFixed(1) : null,
        idbFirstMB: steadyHeap.length ? +(steadyHeap[0].cacheBytes / 1048576).toFixed(1) : null,
        idbLastMB: steadyHeap.length ? +(steadyHeap[steadyHeap.length - 1].cacheBytes / 1048576).toFixed(1) : null,
        idbN: steadyHeap.length ? steadyHeap[steadyHeap.length - 1].cacheN : null,
        samples: steadyHeap.length,
    };

    // 4. CRUX — GC coincidence with big gaps and with underrun increments.
    const COINC_MS = 50;
    const bigGaps = steadyRafGaps.filter(g => g.gap > 33);
    let bigGapsNearGc = 0;
    for (const g of bigGaps) {
        // the gap spans [g.t - g.gap, g.t]; a GC anywhere within ±COINC of that span counts.
        const lo = g.t - g.gap - COINC_MS, hi = g.t + COINC_MS;
        if (steadyGc.some(gc => {
            const gcStart = gc.perfMs, gcEnd = gc.perfMs + gc.durMs;
            return gcEnd >= lo && gcStart <= hi;
        })) bigGapsNearGc++;
    }
    // underrun-increment windows: find consecutive steadyUnder samples where events grew, check GC nearby.
    let urIncrements = 0, urIncrNearGc = 0;
    for (let i = 1; i < steadyUnder.length; i++) {
        const d = steadyUnder[i].events - steadyUnder[i - 1].events;
        if (d > 0) {
            urIncrements++;
            const lo = steadyUnder[i - 1].t - COINC_MS, hi = steadyUnder[i].t + COINC_MS;
            if (steadyGc.some(gc => gc.perfMs >= lo && gc.perfMs <= hi)) urIncrNearGc++;
        }
    }
    const majorGc = steadyGc.filter(g => g.kind === 'major');
    const minorGc = steadyGc.filter(g => g.kind === 'minor');
    const gcStats = {
        totalSteady: steadyGc.length,
        major: majorGc.length, minor: minorGc.length,
        majorDurMs: { p50: +pctile(majorGc.map(g => g.durMs), 50).toFixed(2), max: majorGc.length ? +Math.max(...majorGc.map(g => g.durMs)).toFixed(2) : 0, sum: +majorGc.reduce((a, b) => a + b.durMs, 0).toFixed(2) },
        minorDurMs: { p50: +pctile(minorGc.map(g => g.durMs), 50).toFixed(2), max: minorGc.length ? +Math.max(...minorGc.map(g => g.durMs)).toFixed(2) : 0, sum: +minorGc.reduce((a, b) => a + b.durMs, 0).toFixed(2) },
        gcDurMaxMs: steadyGc.length ? +Math.max(...steadyGc.map(g => g.durMs)).toFixed(2) : 0,
    };
    const crux = {
        coincWindowMs: COINC_MS,
        bigGaps_over33: bigGaps.length,
        bigGapsNearGc, bigGapFracNearGc: bigGaps.length ? +(bigGapsNearGc / bigGaps.length).toFixed(3) : null,
        urIncrements, urIncrNearGc, urIncrFracNearGc: urIncrements ? +(urIncrNearGc / urIncrements).toFixed(3) : null,
        gc: gcStats,
    };

    // ---- emit ----
    const result = {
        tag: TAG, phase, box_loadavg: null, playSecs: PLAY_SECS,
        ctxRate: data.audio?.ctxRate || null, bufFrames: data.audio?.bufFrames || null,
        finalUnderruns: data.audio?.underruns || null,
        steadyWindow: { startPerfMs: +steadyStart.toFixed(1), endPerfMs: +steadyEnd.toFixed(1), secs: jitter.windowSecs },
        clockAlign: { offsetUs, offsetUs2, driftMsPerSec, traceEvents: traceEvents.length, gcEventsTotal: gcEvents.length, gcEventsSteady: steadyGc.length, alignMark, alignMark2 },
        jitter, underruns: { perSec: underrunPerSec, framesPerSec, eventsDelta: urDelta, framesDelta, samples: steadyUnder.length },
        minDepth: minDepthStats,
        heapSlope, crux,
    };

    writeFileSync(resolve(OUT, 'summary.json'), JSON.stringify(result, null, 2));
    writeFileSync(resolve(OUT, 'timeline.json'), JSON.stringify({
        rafGaps: data.rafGaps, longtasks: data.longtasks, heap: data.heap, underruns: data.underruns,
        gcEvents, gcEventsAll, steadyWindow: result.steadyWindow, clockAlign: result.clockAlign,
    }, null, 2));
    writeFileSync(resolve(OUT, 'console.json'), JSON.stringify(logs.slice(-400), null, 2));

    console.log('\n' + '='.repeat(72));
    console.log(`WAVE-10 AUDIO JITTER PROFILE — tag=${TAG} phase=${phase}`);
    console.log(`ctxRate=${result.ctxRate} bufFrames=${result.bufFrames} steady=${jitter.windowSecs}s`);
    console.log(`clock align offsetUs=${offsetUs} drift=${driftMsPerSec}ms/s  traceEvents=${traceEvents.length} gcAll=${gcEvents.length} gcSteady=${steadyGc.length}`);
    console.log('-'.repeat(72));
    console.log(`1. JITTER (rAF gaps, steady): n=${jitter.n} p50=${jitter.p50} p95=${jitter.p95} p99=${jitter.p99} max=${jitter.max}ms`);
    console.log(`   gaps>33ms=${jitter.gapsOver33}  >50ms=${jitter.gapsOver50}  >100ms=${jitter.gapsOver100}`);
    console.log(`2. UNDERRUNS: ${underrunPerSec}/s events, ${framesPerSec} frames/s  (Δevents=${urDelta} Δframes=${framesDelta} over ${steadyUnder.length} samples)`);
    if (minDepthStats) {
        console.log(`2b. RING LOW-WATER (frames): min=${minDepthStats.min} p5=${minDepthStats.p5} p50=${minDepthStats.p50} max=${minDepthStats.max} | dips<240(~5ms)=${minDepthStats.dipsUnder240} last=${minDepthStats.last}`);
    } else {
        console.log(`2b. RING LOW-WATER: no minDepth samples (field absent — old worklet?)`);
    }
    console.log(`3. HEAP SLOPE (steady): JS=${heapSlope.jsMBperMin} MB/min (${heapSlope.jsFirstMB}->${heapSlope.jsLastMB}), wasm=${heapSlope.wasmMBperMin} MB/min (${heapSlope.wasmFirstMB}->${heapSlope.wasmLastMB}), idb=${heapSlope.idbMBperMin} MB/min (${heapSlope.idbFirstMB}->${heapSlope.idbLastMB} n=${heapSlope.idbN})`);
    console.log(`4. CRUX (±${COINC_MS}ms): bigGaps(>33)=${crux.bigGaps_over33} nearGC=${crux.bigGapsNearGc} frac=${crux.bigGapFracNearGc}`);
    console.log(`   urIncrements=${crux.urIncrements} nearGC=${crux.urIncrNearGc} frac=${crux.urIncrFracNearGc}`);
    console.log(`   GC steady: total=${gcStats.totalSteady} major=${gcStats.major} minor=${gcStats.minor} | majorDur p50=${gcStats.majorDurMs.p50} max=${gcStats.majorDurMs.max} | minorDur p50=${gcStats.minorDurMs.p50} max=${gcStats.minorDurMs.max} | gcDurMax=${gcStats.gcDurMaxMs}ms`);
    console.log('='.repeat(72));
    console.log(`[jit] artifacts -> ${OUT}/{summary,timeline,console}.json`);
    process.exit(0);
} catch (e) {
    console.error('[jit] ERROR:', e.message);
    try { writeFileSync(resolve(OUT, 'error.json'), JSON.stringify({ error: e.message, stack: e.stack, traceEvents: traceEvents.length, logsTail: logs.slice(-60) }, null, 2)); } catch {}
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
