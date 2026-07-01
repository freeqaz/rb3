#!/usr/bin/env node
/**
 * _framestall-steady-profile.mjs — STEADY-STATE main-thread jitter attribution
 * for the RB3 web build (frame-stall-2026-06-20 workflow).
 *
 * GOAL: drive to ~30s+ of steady gameplay (or song-select preview fallback),
 * capture on ONE timeline:
 *   (a) per-rAF inter-frame gaps (the jank metric) with timestamps
 *   (b) a CDP V8 CPU sampling profile over a CLEAN steady window (function-name
 *       self-time — works because the DEBUG -g2 wasm carries demangled C++ names)
 *   (c) the engine-side RB3_FRAME_TRACE JSONL (per-frame dt bucketed into
 *       loadPoll / sync-drain / fetch / dta / objLoad / prime / tex / mesh /
 *       vertUnpack / pipeline / inflate) read back from MEMFS
 *   (d) PerformanceObserver 'longtask' for >50ms blocks
 *
 * Then it (1) builds a frame-time histogram, (2) finds the SPIKE frames (>budget,
 * and >100ms), (3) attributes them: which RB3_FRAME_TRACE bucket dominates the
 * spike frames, and (4) aggregates CPU-profile self-time so spike causes get
 * function names. MUST run against the DEBUG build (--debug-build) for names.
 *
 * Usage:
 *   node scripts/web/_framestall-steady-profile.mjs [--port 8421]
 *        [--play-secs 40] [--out DIR] [--tag T] [--debug-build]
 *        [--budget 16.7]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PLAY_SECS = parseInt(arg('--play-secs', '40'), 10) || 40;
const BUDGET = parseFloat(arg('--budget', '16.7')) || 16.7;
const TAG = arg('--tag', `framestall-${Date.now()}`);
const OUT = arg('--out', `/home/free/code/milohax/rb3/docs/native/frame-stall-2026-06-20/cap/${TAG}`);
const DEBUG_BUILD = !argv.includes('--release-build'); // default debug for names
const TRACE_PATH = '/trace.jsonl';
const qs = `env=RB3_FRAME_TRACE=${encodeURIComponent(TRACE_PATH)}`;
const PAGE_URL = `http://127.0.0.1:${PORT}/${DEBUG_BUILD ? '?debug=true&' : '?'}${qs}`;
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
    window.__fs = { rafGaps: [], longtasks: [] };
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
async function pressKey(page, key, holdMs = 250) {
    try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(200); } catch {}
}

let browser;
const logs = [];
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

try {
    await waitForServer(PORT);
    console.log(`[fs] server up :${PORT} tag=${TAG} debugBuild=${DEBUG_BUILD}`);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps', '--disable-sync',
            '--autoplay-policy=no-user-gesture-required',
        ],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    page.on('console', msg => {
        const text = msg.text(); logs.push({ t: elapsed(), text });
        if (/env RB3_FRAME_TRACE|underrun|PumpAudio|boot error|frame trace/i.test(text))
            console.log(`  [${elapsed()}s] ${text.slice(0, 130)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));
    page.on('crash', () => console.log('  [CRASH] page crashed'));

    const client = await ctx.newCDPSession(page);
    await client.send('Profiler.enable');
    // 1000us = 1kHz sampling — fine-grained enough to resolve per-frame spikes.
    await client.send('Profiler.setSamplingInterval', { interval: 1000 });

    await page.addInitScript(installInstrumentation);
    console.log(`[fs] loading ${PAGE_URL}`);
    await page.goto(PAGE_URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    console.log('[fs] waiting for boot...');
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log(`[fs] booted (${elapsed()}s)`);

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    console.log(`[fs] screen: ${s} (${elapsed()}s)`);
    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) { await pressKey(page, 'Space', 300); s = await screenOf(page); }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await screenOf(page); await sleep(2000);

    if (s === 'splash_screen') {
        console.log('[fs] splash -> main_hub...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 8 && s === 'splash_screen'; i++) { await pressKey(page, 'Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
        if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await sleep(3000); s = await screenOf(page);
    console.log(`[fs] main_hub: ${s} (${elapsed()}s)`);

    let reachedSongSelect = false;
    if (s === 'main_hub_screen') {
        console.log('[fs] main_hub -> song_select...');
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
    console.log(`[fs] song_select: ${s} (${elapsed()}s) reached=${reachedSongSelect}`);

    let phase = 'unknown';
    let crashedDuringNav = false;
    page.on('crash', () => { crashedDuringNav = true; });
    if (reachedSongSelect) {
        console.log('[fs] song_select -> attempting gameplay...');
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'ArrowDown', 200);
        await sleep(2000);
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
            const c = await screenOf(page).catch(() => '');
            console.log(`[fs] gameplay not reached (screen=${c} crashed=${crashedDuringNav}); measuring PREVIEW`);
            phase = (c === 'song_select_screen') ? 'preview' : `stuck:${c || (crashedDuringNav ? 'crashed' : 'unknown')}`;
        }
    } else {
        phase = `nosongselect:${s}`;
    }
    console.log(`[fs] STEADY phase = ${phase} (${elapsed()}s)`);

    // Let the song-start LOAD burst settle (the mission's t=2-7s burst) before we
    // open the CLEAN steady window. ~6s of warmup so the CPU profile is steady.
    console.log('[fs] warmup 6s (let song-start burst settle)...');
    await sleep(6000);

    // ---- open CLEAN steady window: start CPU profile ----
    const steadyStart = await page.evaluate(() => performance.now());
    const steadyStartFrame = await page.evaluate(() => window.rb3FrameCount || 0);
    await client.send('Profiler.start');
    console.log(`[fs] CPU profile started; measuring ${PLAY_SECS}s steady (phase=${phase})...`);
    for (let i = 0; i < PLAY_SECS; i++) {
        await sleep(1000);
        if (phase === 'preview' && i > 0 && i % 6 === 0)
            await pressKey(page, (i / 6) % 2 === 0 ? 'ArrowDown' : 'ArrowUp', 160);
        if (i % 10 === 9) {
            const rg = await page.evaluate(() => (window.__fs && window.__fs.rafGaps ? window.__fs.rafGaps.length : 0)).catch(() => 0);
            console.log(`  [${i + 1}s] rafGaps=${rg} screen=${await screenOf(page)}`);
        }
    }
    const steadyEnd = await page.evaluate(() => performance.now());
    const steadyEndFrame = await page.evaluate(() => window.rb3FrameCount || 0);
    const prof = await client.send('Profiler.stop');
    console.log(`[fs] CPU profile stopped: ${prof.profile.samples.length} samples`);

    // ---- read back RB3_FRAME_TRACE JSONL from MEMFS ----
    // NOTE: use window.FS (the Emscripten runtime FS), NOT window.Module.FS —
    // accessing Module.FS triggers a trap-on-access stub that calls abort()
    // ('unreachable'). window.FS.readFile works directly.
    let traceText = '';
    try {
        traceText = await page.evaluate((p) => {
            try {
                if (window.FS && window.FS.readFile)
                    return window.FS.readFile(p, { encoding: 'utf8' });
            } catch (e) { return '__ERR__' + e.message; }
            return '__ERR__no FS';
        }, TRACE_PATH);
    } catch (e) { traceText = '__ERR__' + e.message; }

    const data = await page.evaluate(() => ({
        rafGaps: (window.__fs && window.__fs.rafGaps) || [],
        longtasks: (window.__fs && window.__fs.longtasks) || [],
        screen: window.rb3CurrentScreen || '',
        frameCount: window.rb3FrameCount || 0,
    }));

    // ============ ANALYSIS ============
    function pctile(a, p) { if (!a.length) return 0; const s = a.slice().sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.max(0, Math.ceil(p / 100 * s.length) - 1))]; }
    const inSteady = (t) => t >= steadyStart && t <= steadyEnd;
    const steadyGaps = data.rafGaps.filter(g => inSteady(g.t));
    const gv = steadyGaps.map(g => g.gap);
    const windowSecs = +((steadyEnd - steadyStart) / 1000).toFixed(2);

    // frame-time histogram (bucket gaps)
    const histBuckets = [8, 12, 16.7, 20, 25, 33, 50, 75, 100, 150, 250, 500, Infinity];
    const hist = histBuckets.map(() => 0);
    for (const g of gv) { for (let b = 0; b < histBuckets.length; b++) { if (g <= histBuckets[b]) { hist[b]++; break; } } }
    const jitter = {
        n: gv.length, windowSecs,
        p50: +pctile(gv, 50).toFixed(2), p90: +pctile(gv, 90).toFixed(2),
        p95: +pctile(gv, 95).toFixed(2), p99: +pctile(gv, 99).toFixed(2),
        max: gv.length ? +Math.max(...gv).toFixed(2) : 0,
        overBudget: gv.filter(g => g > BUDGET).length,
        over33: gv.filter(g => g > 33).length, over50: gv.filter(g => g > 50).length,
        over100: gv.filter(g => g > 100).length,
        pctOverBudget: gv.length ? +(100 * gv.filter(g => g > BUDGET).length / gv.length).toFixed(1) : 0,
        histLabels: histBuckets.map(b => b === Infinity ? '>500' : `<=${b}`),
        hist,
    };

    // ---- RB3_FRAME_TRACE: parse JSONL, attribute spikes ----
    let allFrames = [];
    if (traceText && !traceText.startsWith('__ERR__')) {
        for (const line of traceText.split('\n')) {
            const ln = line.trim();
            if (!ln || ln[0] === '#') continue;
            try { allFrames.push(JSON.parse(ln)); } catch {}
        }
    }
    // Slice to the steady window by wasm frame index (the trace has no perf.now;
    // align via window.rb3FrameCount captured at window open/close).
    const frames = allFrames.filter(f => f.f >= steadyStartFrame && f.f <= steadyEndFrame);
    // Buckets that sum toward dt. residue = dt - sum(buckets) = "uncounted" (the
    // steady per-frame work: Poll subsystems, char skinning, particle sim, GPU
    // submit — none are individually metered yet, so residue ~= that work).
    const bucketKeys = ['lp', 'lpu', 'fetchMs', 'dtaMs', 'objMs', 'primeMs', 'texMs', 'meshMs', 'unpackMs', 'pipeMs', 'inflMs'];
    function frameSummary(fr) {
        const sum = bucketKeys.reduce((a, k) => a + (fr[k] || 0), 0);
        return { dt: fr.dt || 0, sum, residue: (fr.dt || 0) - sum };
    }
    // overall trace stats (whole captured trace incl warmup; flag which are in steady)
    const traceDt = frames.map(f => f.dt || 0);
    const traceStats = {
        nFrames: frames.length,
        p50: +pctile(traceDt, 50).toFixed(2), p99: +pctile(traceDt, 99).toFixed(2),
        max: traceDt.length ? +Math.max(...traceDt).toFixed(2) : 0,
    };
    // spike frames in the trace (>budget). Attribute each to its dominant bucket.
    const spikeFrames = frames.filter(f => (f.dt || 0) > BUDGET);
    const big100 = frames.filter(f => (f.dt || 0) > 100);
    // aggregate bucket ms over spike frames + over ALL frames
    function aggBuckets(set) {
        const agg = {}; let dtSum = 0, residueSum = 0;
        for (const fr of set) {
            const fsum = frameSummary(fr);
            dtSum += fsum.dt; residueSum += fsum.residue;
            for (const k of bucketKeys) agg[k] = (agg[k] || 0) + (fr[k] || 0);
        }
        return { dtSum: +dtSum.toFixed(1), residueSum: +residueSum.toFixed(1), agg, n: set.length };
    }
    const spikeAgg = aggBuckets(spikeFrames);
    const allAgg = aggBuckets(frames);
    // worst single frames (for the t=62/71s 100ms spikes)
    const worstFrames = frames.slice().sort((a, b) => (b.dt || 0) - (a.dt || 0)).slice(0, 25)
        .map(f => ({ f: f.f, dt: +(f.dt || 0).toFixed(1), scr: f.scr, residue: +frameSummary(f).residue.toFixed(1),
            lpu: f.lpu, objMs: f.objMs, objWNm: f.objWNm, texMs: f.texMs, meshMs: f.meshMs, primeMs: f.primeMs,
            unpackMs: f.unpackMs, pipeMs: f.pipeMs, ld: f.ld, st: f.st, pend: f.pend }));

    // ---- CPU profile self-time aggregation by function ----
    const pn = prof.profile;
    const nodes = pn.nodes || [];
    const byId = new Map(); for (const n of nodes) byId.set(n.id, n);
    const selfUs = new Map();
    const samples = pn.samples || [];
    const deltas = pn.timeDeltas || [];
    let totalUs = 0;
    for (let i = 0; i < samples.length; i++) { const id = samples[i]; const dt = Math.max(0, deltas[i] || 0); totalUs += dt; selfUs.set(id, (selfUs.get(id) || 0) + dt); }
    const catOf = (f) => {
        const fn = f.functionName || '(anonymous)'; const url = f.url || '';
        if (fn === '(idle)') return 'idle'; if (fn === '(program)') return 'program'; if (fn === '(garbage collector)') return 'gc';
        if (/wasm/.test(url) || url.endsWith('.wasm')) return 'wasm';
        if (url.endsWith('.js')) return 'js-glue';
        if (!url && fn !== '(root)') return 'wasm';
        return 'other';
    };
    const fnAgg = new Map(); const catAgg = new Map();
    for (const [id, us] of selfUs) {
        const n = byId.get(id); if (!n) continue; const f = n.callFrame || n;
        const name = (f.functionName || '(anonymous)') + (f.url && f.url.endsWith('.js') ? ' @js' : '');
        fnAgg.set(name, (fnAgg.get(name) || 0) + us);
        const c = catOf(f); catAgg.set(c, (catAgg.get(c) || 0) + us);
    }
    const busyUs = totalUs - (catAgg.get('idle') || 0);
    const cpuTop = [...fnAgg].sort((a, b) => b[1] - a[1]).slice(0, 60)
        .map(([name, us]) => ({ name: name.slice(0, 90), ms: +(us / 1000).toFixed(1), pct: +(100 * us / Math.max(1, totalUs)).toFixed(2), pctBusy: +(100 * us / Math.max(1, busyUs)).toFixed(2) }));
    const cpuCats = [...catAgg].sort((a, b) => b[1] - a[1]).map(([c, us]) => ({ cat: c, ms: +(us / 1000).toFixed(0), pct: +(100 * us / Math.max(1, totalUs)).toFixed(1) }));

    const result = {
        tag: TAG, phase, debugBuild: DEBUG_BUILD, budgetMs: BUDGET,
        steadyWindow: { startPerfMs: +steadyStart.toFixed(1), endPerfMs: +steadyEnd.toFixed(1), secs: windowSecs, startFrame: steadyStartFrame, endFrame: steadyEndFrame, wasmFrames: steadyEndFrame - steadyStartFrame, wasmFps: +((steadyEndFrame - steadyStartFrame) / Math.max(0.01, windowSecs)).toFixed(1) },
        jitter,
        traceAvailable: !!(traceText && !traceText.startsWith('__ERR__')),
        traceErr: traceText.startsWith('__ERR__') ? traceText : null,
        traceStats, spikeCount: spikeFrames.length, big100Count: big100.length,
        spikeBucketAgg: spikeAgg, allBucketAgg: allAgg,
        worstFrames,
        cpu: { totalMs: +(totalUs / 1000).toFixed(0), busyMs: +(busyUs / 1000).toFixed(0), samples: samples.length, cats: cpuCats, top: cpuTop },
    };
    writeFileSync(resolve(OUT, 'summary.json'), JSON.stringify(result, null, 2));
    writeFileSync(resolve(OUT, 'profile.cpuprofile'), JSON.stringify(pn));
    writeFileSync(resolve(OUT, 'timeline.json'), JSON.stringify({ rafGaps: data.rafGaps, longtasks: data.longtasks, steadyWindow: result.steadyWindow }));
    if (traceText && !traceText.startsWith('__ERR__')) writeFileSync(resolve(OUT, 'frametrace.jsonl'), traceText);
    writeFileSync(resolve(OUT, 'console.json'), JSON.stringify(logs.slice(-400), null, 2));

    console.log('\n' + '='.repeat(74));
    console.log(`FRAME-STALL STEADY PROFILE — tag=${TAG} phase=${phase} debug=${DEBUG_BUILD}`);
    console.log(`steady=${windowSecs}s  budget=${BUDGET}ms  wasmFrames=${result.steadyWindow.wasmFrames} wasmFps=${result.steadyWindow.wasmFps} (frames ${steadyStartFrame}..${steadyEndFrame})`);
    console.log('-'.repeat(74));
    console.log(`JITTER (rAF gaps): n=${jitter.n} p50=${jitter.p50} p90=${jitter.p90} p95=${jitter.p95} p99=${jitter.p99} max=${jitter.max}ms`);
    console.log(`  overBudget(>${BUDGET})=${jitter.overBudget} (${jitter.pctOverBudget}%)  >33=${jitter.over33} >50=${jitter.over50} >100=${jitter.over100}`);
    console.log(`  hist: ${jitter.histLabels.map((l, i) => `${l}:${jitter.hist[i]}`).join(' ')}`);
    console.log('-'.repeat(74));
    console.log(`FRAME-TRACE available=${result.traceAvailable} frames=${traceStats.nFrames} p50=${traceStats.p50} p99=${traceStats.p99} max=${traceStats.max}ms`);
    if (result.traceErr) console.log(`  traceErr=${result.traceErr}`);
    console.log(`  spikes(>${BUDGET})=${spikeFrames.length}  big(>100)=${big100.length}`);
    console.log(`  spike bucket sums (ms over ${spikeAgg.n} spike frames): dtSum=${spikeAgg.dtSum} residue=${spikeAgg.residueSum}`);
    console.log(`    ${Object.entries(spikeAgg.agg).filter(([, v]) => v > 0.5).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k}=${v.toFixed(1)}`).join(' ')}`);
    console.log('  WORST FRAMES:');
    for (const w of worstFrames.slice(0, 12))
        console.log(`    f${w.f} dt=${w.dt}ms residue=${w.residue} scr=${w.scr} lpu=${w.lpu} obj=${w.objMs}(${w.objWNm}) tex=${w.texMs} mesh=${w.meshMs} prime=${w.primeMs} unpack=${w.unpackMs} ld=${w.ld} st=${w.st}`);
    console.log('-'.repeat(74));
    console.log(`CPU PROFILE: total=${result.cpu.totalMs}ms busy=${result.cpu.busyMs}ms samples=${samples.length}`);
    console.log(`  cats: ${cpuCats.map(c => `${c.cat}=${c.ms}ms(${c.pct}%)`).join(' ')}`);
    console.log('  TOP FUNCTIONS (self, %busy):');
    for (const f of cpuTop.slice(0, 30)) console.log(`    ${String(f.ms).padStart(7)}ms ${String(f.pctBusy).padStart(5)}%  ${f.name}`);
    console.log('='.repeat(74));
    console.log(`[fs] artifacts -> ${OUT}/`);
    process.exit(0);
} catch (e) {
    console.error('[fs] ERROR:', e.message);
    try { writeFileSync(resolve(OUT, 'error.json'), JSON.stringify({ error: e.message, stack: e.stack, logsTail: logs.slice(-60) }, null, 2)); } catch {}
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
