#!/usr/bin/env node
/**
 * frame-stall-attribute.mjs — attribute web MAIN-THREAD frame stalls to engine/game
 * FUNCTIONS by intersecting a windowed CDP CPU profile with the longtask timeline.
 *
 * THE PROBLEM this solves: the audio under-run diagnosis proved each main-thread
 * longtask (381ms @song-start, ~114-154ms mid-gameplay) empties the SAB audio ring.
 * But "longtask" / "GC / asset-decode" is not actionable. This tool says WHICH C++
 * functions ran INSIDE each stall, with self-time ms — so we can decide what to defer.
 *
 * HOW: one browser session drives boot -> song_select -> game_screen, runs a CDP
 * `Profiler` sampling profile for the whole gameplay window, and captures
 * PerformanceObserver 'longtask' + per-rAF timestamps on the SAME timeline. After
 * the run we (a) bin the profile globally, (b) for the N worst longtasks, slice the
 * profile samples whose ts falls inside [start,start+dur] and aggregate self-time by
 * function — the per-stall attribution.
 *
 * The CDP sample ts and the page longtask ts are reconciled via a captured
 * (Date.now <-> CDP monotonic-us) offset pair, plus a page (performance.now<->Date.now).
 *
 * Usage:
 *   node scripts/web/frame-stall-attribute.mjs [--port 8421] [--play-secs 50]
 *        [--song 20thcenturyboy] [--out DIR] [--interval-us 200] [--top-stalls 8]
 *
 * Output: <out>/profile.cpuprofile (loadable in DevTools/speedscope),
 *         <out>/attribution.json, and a human summary to stdout.
 *
 * Reuses the proven nav path from audio-stall-measure.mjs.
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PLAY_SECS = parseInt(arg('--play-secs', '50'), 10) || 50;
const SONG = arg('--song', '20thcenturyboy');
const OUT = arg('--out', '/tmp/rb3-frame-stall');
const INTERVAL_US = parseInt(arg('--interval-us', '200'), 10) || 200; // 200us = 5kHz sampling
const TOP_STALLS = parseInt(arg('--top-stalls', '10'), 10) || 10;
const DEBUG_BUILD = argv.includes('--debug-build');
const PAGE_URL = `http://127.0.0.1:${PORT}/${DEBUG_BUILD ? '?debug=true' : ''}`;
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

// Page-side: capture longtasks + rAF gaps with performance.now() timestamps, plus
// a (performance.now <-> Date.now) offset so we can map to wall clock.
function installInstrumentation() {
    window.__fs = { longtasks: [], rafTs: [], marks: [], perfToDateOffset: Date.now() - performance.now() };
    const st = window.__fs;
    try {
        new PerformanceObserver(list => {
            for (const e of list.getEntries())
                st.longtasks.push({ start: +e.startTime.toFixed(2), dur: +e.duration.toFixed(2) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}
    let last = -1;
    const raf = (t) => {
        st.rafTs.push(+t.toFixed(2));
        last = t;
        requestAnimationFrame(raf);
    };
    requestAnimationFrame(raf);
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
        await sleep(250);
    }
    return s;
}

// ---- profile attribution helpers --------------------------------------------
// CDP Profiler.stop returns { nodes, samples, timeDeltas, startTime, endTime }.
// startTime/endTime are CDP monotonic microseconds. We compute per-sample absolute
// ts (us) by accumulating timeDeltas, then bin by id.
function buildSampleTimeline(prof) {
    const samples = prof.samples || [];
    const deltas = prof.timeDeltas || [];
    const ts = new Array(samples.length);
    let t = prof.startTime || 0;
    for (let i = 0; i < samples.length; i++) { t += (deltas[i] || 0); ts[i] = t; }
    return ts; // microseconds, CDP monotonic clock
}
function nodeName(node) {
    const f = node.callFrame || node;
    const fn = f.functionName || '(anonymous)';
    const url = f.url || '';
    if (fn === '(idle)') return '(idle)';
    if (fn === '(program)') return '(program)';
    if (fn === '(garbage collector)') return '(gc)';
    // dev -g2 wasm fns carry demangled C++ names with empty url.
    const tail = url ? ` @${url.split('/').pop()}` : '';
    return fn + tail;
}
function aggregateSelfTime(prof, sampleIdxFilter) {
    const byId = new Map();
    for (const n of (prof.nodes || [])) byId.set(n.id, n);
    const samples = prof.samples || [];
    const deltas = prof.timeDeltas || [];
    const selfUs = new Map();
    let total = 0;
    for (let i = 0; i < samples.length; i++) {
        if (sampleIdxFilter && !sampleIdxFilter(i)) continue;
        const id = samples[i];
        const dt = Math.max(0, deltas[i] || 0);
        const n = byId.get(id);
        if (!n) continue;
        const name = nodeName(n);
        selfUs.set(name, (selfUs.get(name) || 0) + dt);
        total += dt;
    }
    return { selfUs, total };
}
function topN(selfMap, n) {
    return [...selfMap].sort((a, b) => b[1] - a[1]).slice(0, n)
        .map(([name, us]) => ({ name, ms: +(us / 1000).toFixed(2) }));
}

let browser;
try {
    await waitForServer(PORT);
    console.log(`[fs] server up at :${PORT}`);
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
    const t0 = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);
    page.on('console', msg => {
        const text = msg.text();
        if (/underrun|adaptive|latency|stall|LONG|PumpAudio/i.test(text)) console.log(`  [${elapsed()}s] ${text.slice(0, 130)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));

    await page.addInitScript(installInstrumentation);
    console.log(`[fs] loading ${PAGE_URL}`);
    await page.goto(PAGE_URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    // boot
    console.log('[fs] waiting for boot...');
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log(`[fs] booted (${elapsed()}s)`);

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
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
    console.log(`[fs] main_hub: ${s} (${elapsed()}s)`);
    if (s === 'main_hub_screen') {
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        for (let i = 0; i < 5; i++) { await pressKey(page, 'Enter'); const cur = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 }); if (cur && cur !== 'main_hub_screen') { s = cur; break; } await sleep(1500); }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await sleep(4000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[fs] song_select: ${s} (${elapsed()}s)`);

    // -------- Start CDP profiler JUST BEFORE we trigger the song load --------
    // This captures the song-start burst (381/201ms stalls) + steady gameplay.
    const client = await ctx.newCDPSession(page);
    await client.send('Profiler.enable');
    await client.send('Profiler.setSamplingInterval', { interval: INTERVAL_US });
    // Capture clock-offset: page perf.now -> Date.now is stored page-side; CDP ts is
    // monotonic-us. We snapshot Date.now right around start; Profiler.start's first
    // sample startTime is CDP's monotonic. Reconcile via Runtime.evaluate timestamps.
    const beforeStart = Date.now();
    await client.send('Profiler.start');
    const afterStart = Date.now();
    const profStartWall = (beforeStart + afterStart) / 2;
    console.log(`[fs] CDP profiler started (interval=${INTERVAL_US}us) at wall ${profStartWall}`);

    // trigger song load
    await page.evaluate((song) => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = song; }, SONG).catch(() => {});
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(2000);
    for (let i = 0; i < 6; i++) { await pressKey(page, 'Enter', 150); await sleep(1200); const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => ''); if (cur === 'game_screen') { s = cur; break; } }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    console.log(`[fs] game_screen: ${s} (${elapsed()}s) — measuring ${PLAY_SECS}s`);
    const gameStartWall = Date.now();

    await sleep(PLAY_SECS * 1000);

    // -------- stop profiler + collect page data --------
    const prof = (await client.send('Profiler.stop')).profile;
    const pageData = await page.evaluate(() => {
        const st = window.__fs;
        const a = window._rb3Audio;
        return {
            longtasks: st.longtasks, rafTs: st.rafTs, perfToDateOffset: st.perfToDateOffset,
            audio: a ? { ctxRate: a.ctx ? a.ctx.sampleRate : null, bufFrames: a.bufFrames, underruns: a.underruns } : null,
        };
    });
    writeFileSync(`${OUT}/profile.cpuprofile`, JSON.stringify(prof));

    // -------- reconcile clocks --------
    // CDP sample ts (us, monotonic). prof.startTime ~ profStartWall (we measured the
    // wall time we called Profiler.start). Map cdp-us -> wall-ms via the start anchor.
    // longtask.start is page performance.now() ms; wall = start + perfToDateOffset.
    const cdpStartUs = prof.startTime || 0;
    const ts = buildSampleTimeline(prof); // per-sample cdp-us
    const cdpToWall = (cdpUs) => profStartWall + (cdpUs - cdpStartUs) / 1000; // -> wall ms
    const ltToWall = (lt) => lt.start + pageData.perfToDateOffset; // page perf.now ms -> wall ms

    // -------- global attribution (whole gameplay window) --------
    const all = aggregateSelfTime(prof);
    const idleUs = all.selfUs.get('(idle)') || 0;
    const gcUs = all.selfUs.get('(gc)') || 0;
    const busyUs = all.total - idleUs;

    // -------- per-stall attribution --------
    // Only consider longtasks during the gameplay window (after gameStartWall - 3s
    // to catch the song-start burst that lands just before game_screen settles).
    const windowStartWall = gameStartWall - 4000;
    const stalls = pageData.longtasks
        .map(lt => ({ ...lt, wallStart: ltToWall(lt) }))
        .filter(lt => lt.wallStart >= windowStartWall)
        .sort((a, b) => b.dur - a.dur)
        .slice(0, TOP_STALLS);

    const stallAttrib = stalls.map(lt => {
        const w0 = lt.wallStart, w1 = lt.wallStart + lt.dur;
        const filter = (i) => { const w = cdpToWall(ts[i]); return w >= w0 && w <= w1; };
        const { selfUs, total } = aggregateSelfTime(prof, filter);
        return {
            longtaskMs: +lt.dur.toFixed(1),
            tSinceGameStart: +((lt.wallStart - gameStartWall) / 1000).toFixed(2),
            sampledMs: +(total / 1000).toFixed(1),
            top: topN(selfUs, 12),
        };
    });

    const out = {
        meta: { port: PORT, song: SONG, playSecs: PLAY_SECS, intervalUs: INTERVAL_US, debugBuild: DEBUG_BUILD,
                profDurationS: +((ts[ts.length - 1] - cdpStartUs) / 1e6).toFixed(1), samples: (prof.samples || []).length },
        audio: pageData.audio,
        global: { totalS: +(all.total / 1e6).toFixed(2), busyS: +(busyUs / 1e6).toFixed(2),
                  idlePct: +(100 * idleUs / all.total).toFixed(1), gcPct: +(100 * gcUs / all.total).toFixed(1),
                  topBusy: topN(all.selfUs, 30) },
        stalls: stallAttrib,
    };
    writeFileSync(`${OUT}/attribution.json`, JSON.stringify(out, null, 2));

    // -------- human summary --------
    console.log('\n' + '='.repeat(78));
    console.log(`FRAME-STALL ATTRIBUTION  (song=${SONG}, ${PLAY_SECS}s gameplay, ${out.meta.samples} samples @${INTERVAL_US}us)`);
    console.log('='.repeat(78));
    if (out.audio && out.audio.underruns) {
        const u = out.audio.underruns;
        console.log(`AUDIO: underrunEvents=${u.underrunEvents} frames=${u.underrunFrames}/${u.totalFrames} (${u.totalFrames ? (100*u.underrunFrames/u.totalFrames).toFixed(2) : '?'}%)  ring=${out.audio.bufFrames}f rate=${out.audio.ctxRate}Hz`);
    }
    console.log(`PROFILE WINDOW: ${out.meta.profDurationS}s  busy=${out.global.busyS}s  idle=${out.global.idlePct}%  gc=${out.global.gcPct}%`);
    console.log('-'.repeat(78));
    console.log('TOP BUSY FUNCTIONS (self-time, whole gameplay window — the steady per-frame cost):');
    for (const f of out.global.topBusy.slice(0, 20)) console.log(`  ${String(f.ms).padStart(8)}ms  ${f.name.slice(0, 78)}`);
    console.log('-'.repeat(78));
    console.log(`TOP ${stalls.length} LONGTASKS (>50ms) with per-stall function attribution:`);
    for (const st of stallAttrib) {
        console.log(`\n  ▸ ${st.longtaskMs}ms longtask @t+${st.tSinceGameStart}s  (sampled ${st.sampledMs}ms inside):`);
        for (const f of st.top.slice(0, 8)) console.log(`        ${String(f.ms).padStart(7)}ms  ${f.name.slice(0, 70)}`);
    }
    console.log('\n' + '='.repeat(78));
    console.log(`Artifacts: ${OUT}/profile.cpuprofile  ${OUT}/attribution.json`);
} catch (e) {
    console.error('[fs] ERROR', e);
    process.exitCode = 1;
} finally {
    if (browser) await browser.close().catch(() => {});
}
