#!/usr/bin/env node
/**
 * loadperf-profile.mjs — deep web boot/load profiler (hard data for stutters).
 *
 * Boots rb3-web headless (no xvfb) and captures, correlated on one timeline:
 *   1. A V8 CPU profile (CDP Profiler) -> .cpuprofile, loadable in Chrome
 *      DevTools (Performance > Load profile) or https://speedscope.app — the
 *      authoritative "where did the main-thread time go" flame graph.
 *   2. Long Tasks (PerformanceObserver 'longtask') — every >50ms main-thread
 *      block, each attributed to the boot phase active at that moment.
 *   3. requestAnimationFrame gaps — the user-visible "tab froze for N ms" metric.
 *   4. Resource Timing — every network fetch (wasm, DTA, milos, textures) with
 *      transfer size + duration; flags the slowest and the wasm download.
 *   5. Boot-phase milestones — wasm-live, app-booted, first-screen, splash,
 *      main_hub — as ms from navigation, so stalls can be blamed on a phase.
 *
 * Writes a JSON blob + a human summary, and (default) a .cpuprofile.
 *
 * Usage:
 *   node scripts/web/loadperf-profile.mjs [--port 8421] [--secs 40] [--nav]
 *        [--no-cpuprofile] [--out <dir>]
 *   --nav  also drive splash->main_hub->song_select->game to profile song-load.
 */
import { waitForServer, launchBrowser, createCapture, engineState, navigateTo,
         outputDir, saveJson, cleanup, SCREENS } from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8421'), 10) || 8421;
const SECS = parseFloat(arg('secs', '40')) || 40;
const NAV = argv.includes('--nav');
const CPUPROFILE = !argv.includes('--no-cpuprofile');
const OUT = outputDir('loadperf', argv.includes('--out') ? arg('out') : null);
// Loader-knob URL params (main_web.cpp ApplyUrlLoaderEnv -> setenv), so the boot
// trade-off can be A/B'd without rebuilds: --loader-yield 50 --loader-budget 8.
const Q = [];
if (arg('loader-yield', null)) Q.push(`loaderYieldMs=${arg('loader-yield')}`);
if (arg('loader-budget', null)) Q.push(`loaderBudgetMs=${arg('loader-budget')}`);
const QUERY = Q.join('&');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// Installed BEFORE navigation: records longtasks, RAF gaps, and boot milestones
// against performance.now() (t=0 ~ navigationStart) so everything shares a clock.
function instrument() {
    window.__lp = { longtasks: [], rafGaps: [], milestones: {}, marks: [] };
    const lp = window.__lp;

    // Long tasks (>50ms main-thread blocks).
    try {
        new PerformanceObserver((list) => {
            for (const e of list.getEntries()) {
                lp.longtasks.push({ start: +e.startTime.toFixed(1), dur: +e.duration.toFixed(1),
                    attr: (e.attribution && e.attribution[0] && e.attribution[0].name) || '' });
            }
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}

    // User timing marks (if the build emits performance.mark('rb3:...')).
    try {
        new PerformanceObserver((list) => {
            for (const e of list.getEntries()) lp.marks.push({ name: e.name, t: +e.startTime.toFixed(1) });
        }).observe({ entryTypes: ['mark', 'measure'] });
    } catch (e) {}

    // RAF gaps + boot milestones (polled each frame).
    let last = -1;
    const mark = (k) => { if (lp.milestones[k] === undefined) lp.milestones[k] = +performance.now().toFixed(1); };
    function tick(t) {
        if (last >= 0) lp.rafGaps.push(+(t - last).toFixed(1));
        last = t;
        if ((window.rb3FrameCount || 0) > 0) mark('wasmLive');
        if ((window.rb3AppBooted || 0) >= 1) mark('appBooted');
        const s = window.rb3CurrentScreen || '';
        if (s) mark('firstScreen');
        if (s === 'splash_screen') mark('splash');
        if (s === 'main_hub_screen') mark('mainHub');
        if (s === 'song_select_screen') mark('songSelect');
        if (s === 'game_screen') mark('game');
        requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
}

const phaseAt = (ms, milestones) => {
    // Return the most recent milestone name at time `ms`.
    const order = ['navStart', 'wasmLive', 'appBooted', 'firstScreen', 'splash', 'mainHub', 'songSelect', 'game'];
    const ts = { navStart: 0, ...milestones };
    let phase = 'preboot';
    for (const k of order) if (ts[k] !== undefined && ms >= ts[k]) phase = k;
    return phase;
};

(async () => {
    console.log(`[profile] waiting for server :${PORT}`);
    await waitForServer(PORT, 20000);

    const { browser, context, page, url } = await launchBrowser(PORT, { noGoto: true, query: QUERY });
    const capture = createCapture(page, { filter: /GPU ready|App constructed|booted|assets ready|FRAME-INSTRUMENT/i });
    await page.addInitScript(instrument);

    // CDP CPU profile across the whole boot.
    let cdp = null;
    if (CPUPROFILE) {
        cdp = await context.newCDPSession(page);
        await cdp.send('Profiler.enable');
        await cdp.send('Profiler.setSamplingInterval', { interval: 150 }); // microseconds
        await cdp.send('Profiler.start');
    }

    const tNav = Date.now();
    console.log(`[profile] navigating ${url} (profiling ${SECS}s, nav=${NAV}, cpuprofile=${CPUPROFILE})`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    if (NAV) {
        try { await navigateTo(page, capture, SCREENS.GAME); }
        catch (e) { console.log('  [nav] stopped:', e.message); }
    } else {
        // Passive boot measurement: stop once the first real screen appears (boot
        // is done). intro_movie_screen holds 68s once it plays, so don't wait for
        // main_hub here — time-to-first-screen / time-to-appBooted is the boot
        // metric. Capture a couple extra seconds past first screen for tail tasks.
        const deadline = Date.now() + SECS * 1000;
        while (Date.now() < deadline) {
            const s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
            if (s === 'main_hub_screen' || s === 'intro_movie_screen' || s === 'splash_screen') {
                await sleep(2500); break;
            }
            await sleep(300);
        }
    }

    // Stop CPU profile, save.
    let cpuPath = null;
    if (cdp) {
        const { profile } = await cdp.send('Profiler.stop');
        const { writeFileSync } = await import('fs');
        const { resolve } = await import('path');
        cpuPath = resolve(OUT, 'boot.cpuprofile');
        writeFileSync(cpuPath, JSON.stringify(profile));
        console.log(`[profile] cpuprofile -> ${cpuPath}  (load in DevTools Performance or speedscope.app)`);
    }

    // Pull the instrumentation + resource timing out of the page.
    const data = await page.evaluate(() => {
        const lp = window.__lp || { longtasks: [], rafGaps: [], milestones: {}, marks: [] };
        const res = performance.getEntriesByType('resource').map(r => ({
            name: r.name.replace(location.origin, ''),
            type: r.initiatorType,
            start: +r.startTime.toFixed(1),
            dur: +r.duration.toFixed(1),
            transfer: r.transferSize || 0,
            encoded: r.encodedBodySize || 0,
            decoded: r.decodedBodySize || 0,
        }));
        const nav = performance.getEntriesByType('navigation')[0];
        lp.bootPhases = window.rb3BootPhaseLog || [];
        return { lp, res, nav: nav ? {
            domContentLoaded: +nav.domContentLoadedEventEnd.toFixed(1),
            loadEvent: +nav.loadEventEnd.toFixed(1),
            responseEnd: +nav.responseEnd.toFixed(1),
        } : null };
    });

    const finalState = await engineState(page).catch(() => ({}));
    await cleanup(browser);

    // ---- Analysis ----
    const { lp, res, nav } = data;
    const M = lp.milestones;
    const longtasks = lp.longtasks.sort((a, b) => b.dur - a.dur);
    const gaps = lp.rafGaps.slice().sort((a, b) => b - a);
    const totalBlocking = lp.longtasks.reduce((s, t) => s + Math.max(0, t.dur - 50), 0);
    const over = (th) => gaps.filter(g => g > th).length;

    // Network rollups.
    const byType = {};
    for (const r of res) {
        const k = r.type || 'other';
        byType[k] = byType[k] || { count: 0, transfer: 0, decoded: 0, dur: 0 };
        byType[k].count++; byType[k].transfer += r.transfer; byType[k].decoded += r.decoded; byType[k].dur += r.dur;
    }
    const wasm = res.find(r => /\.wasm$/.test(r.name));
    const slowestRes = res.slice().sort((a, b) => b.dur - a.dur).slice(0, 12);
    const totalTransfer = res.reduce((s, r) => s + r.transfer, 0);

    const report = {
        url, nav, milestones: M, finalScreen: finalState.screen, finalFrame: finalState.frame,
        timeToMainHub: M.mainHub, timeToFirstScreen: M.firstScreen, timeToAppBooted: M.appBooted,
        timeToWasmLive: M.wasmLive,
        longtasks: { count: lp.longtasks.length, totalBlockingMs: +totalBlocking.toFixed(0),
            worst: longtasks.slice(0, 15).map(t => ({ ...t, phase: phaseAt(t.start, M) })) },
        rafGaps: { samples: gaps.length, maxMs: gaps[0] || 0, over100: over(100), over250: over(250),
            over500: over(500), over1000: over(1000), worst: gaps.slice(0, 10) },
        network: { requests: res.length, totalTransferBytes: totalTransfer, byType,
            wasm: wasm ? { dur: wasm.dur, transfer: wasm.transfer, decoded: wasm.decoded } : null,
            slowest: slowestRes },
        marks: lp.marks,
    };
    saveJson(report, OUT, 'profile.json');

    // ---- Human summary ----
    const ms = (v) => v === undefined ? '   n/a' : `${(v / 1000).toFixed(2)}s`;
    const kb = (b) => `${(b / 1024).toFixed(0)}KB`;
    console.log('='.repeat(70));
    if (lp.bootPhases && lp.bootPhases.length) {
        console.log(`[profile] BOOT PHASE TIMELINE (ms, and delta from prev):`);
        let prev = 0;
        for (const [name, t] of lp.bootPhases) {
            console.log(`    ${name.padEnd(18)} ${(t/1000).toFixed(2)}s   (+${((t-prev)/1000).toFixed(2)}s)`);
            prev = t;
        }
        console.log('-'.repeat(70));
    }
    console.log(`[profile] BOOT MILESTONES (ms from navigation):`);
    console.log(`    wasm live:    ${ms(M.wasmLive)}    app booted: ${ms(M.appBooted)}`);
    console.log(`    first screen: ${ms(M.firstScreen)}  splash:     ${ms(M.splash)}`);
    console.log(`    main_hub:     ${ms(M.mainHub)}    ${NAV ? `song_sel: ${ms(M.songSelect)}  game: ${ms(M.game)}` : ''}`);
    console.log(`    final: screen='${finalState.screen}' frame=${finalState.frame}`);
    console.log('-'.repeat(70));
    console.log(`[profile] MAIN-THREAD BLOCKING (long tasks > 50ms):`);
    console.log(`    count=${report.longtasks.count}  total blocking time=${report.longtasks.totalBlockingMs}ms`);
    console.log(`    worst tasks (ms @ phase):`);
    for (const t of report.longtasks.worst.slice(0, 10))
        console.log(`      ${String(t.dur).padStart(7)}ms  @${(t.start/1000).toFixed(2)}s  phase=${t.phase}${t.attr ? `  (${t.attr})` : ''}`);
    console.log('-'.repeat(70));
    console.log(`[profile] RAF GAPS (user-visible freezes):`);
    console.log(`    max=${report.rafGaps.maxMs.toFixed(0)}ms  >100ms:${report.rafGaps.over100}  >250ms:${report.rafGaps.over250}  >500ms:${report.rafGaps.over500}  >1s:${report.rafGaps.over1000}`);
    console.log(`    worst 10: ${report.rafGaps.worst.map(g => g.toFixed(0)).join(', ')}`);
    console.log('-'.repeat(70));
    console.log(`[profile] NETWORK: ${report.network.requests} requests, ${(totalTransfer/1024/1024).toFixed(1)}MB transferred`);
    if (wasm) console.log(`    wasm: ${kb(wasm.transfer)} over wire, ${kb(wasm.decoded)} decoded, ${wasm.dur.toFixed(0)}ms`);
    for (const [k, v] of Object.entries(byType).sort((a,b)=>b[1].transfer-a[1].transfer))
        console.log(`    ${k.padEnd(12)} ${String(v.count).padStart(4)} reqs  ${kb(v.transfer).padStart(8)}  ${v.dur.toFixed(0)}ms total`);
    console.log(`    slowest fetches:`);
    for (const r of slowestRes.slice(0, 8))
        console.log(`      ${r.dur.toFixed(0).padStart(6)}ms  ${kb(r.transfer).padStart(7)}  ${r.name.slice(0, 60)}`);
    if (cpuPath) console.log(`[profile] CPU profile: ${cpuPath}`);
    console.log('='.repeat(70));
    process.exit(0);
})().catch(e => { console.error('[profile] ERROR', e); process.exit(2); });
