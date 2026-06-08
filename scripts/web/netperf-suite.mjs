#!/usr/bin/env node
/**
 * netperf-suite.mjs — network-conditioned web load/transition performance suite.
 *
 * WHY: localhost (unbounded) hides the real cost of the boot/menu fetches, which
 * are issued as *synchronous* XHRs that block the wasm main thread. The stall you
 * feel in production scales with network throughput, so we measure under emulated
 * conditions, not just on loopback.
 *
 * WHAT IT DOES: for each {network profile} x {scenario} it boots rb3-web headless,
 * throttles the network via CDP (Network.emulateNetworkConditions — applies to
 * localhost too), drives the scenario, and records on one timeline:
 *   - boot milestones (wasm-live, app-booted, first-screen, main_hub, …)
 *   - per-MENU-TRANSITION cost (wall time, bytes fetched, # requests, main-thread
 *     blocked ms, worst RAF gap) — this is the "menu transition murders perf" data
 *   - a network waterfall (every request: url, type, bytes, duration) from CDP,
 *     so sync-XHR serialization is visible
 *   - main-thread long tasks (>50ms freezes) and RAF gaps (user-visible jank)
 *   - (optional) a V8 .cpuprofile and a full DevTools timeline trace.json you can
 *     load in chrome://tracing or DevTools > Performance for deep inspection.
 *
 * NETWORK PROFILES (edit PROFILES below):
 *   low    =  50 Mbit/s, 30ms RTT   ("low end")
 *   normal = 200 Mbit/s, 15ms RTT   ("normal")
 *   local  = unbounded,   0ms RTT   ("high watermark" — loopback, no throttle)
 *
 * SCENARIOS:
 *   boot = cold boot to first interactive screen (fresh context => cold IDB cache)
 *   nav  = boot, skip intro, then drive main_hub -> song_select -> part_difficulty
 *          -> game, measuring EACH transition's stall (where on-demand milos load)
 *
 * USAGE:
 *   node scripts/web/netperf-suite.mjs                       # all profiles, both scenarios
 *   node scripts/web/netperf-suite.mjs --scenario boot       # boot only
 *   node scripts/web/netperf-suite.mjs --profiles low,local  # subset
 *   node scripts/web/netperf-suite.mjs --runs 3              # median over N runs
 *   node scripts/web/netperf-suite.mjs --trace --cpuprofile  # heavy inspectable traces
 *   node scripts/web/netperf-suite.mjs --out /tmp/rb3-web/netperf-A   # fixed out dir
 *
 * Artifacts land in <out>/ : per-run profile.json + network-waterfall.json
 * (+ boot.cpuprofile / trace.json when flagged), plus summary.json and REPORT.md.
 */
import { waitForServer, launchBrowser, createCapture, engineState, cleanup,
         outputDir, saveJson } from './lib/core.mjs';
import { resolve } from 'path';
import { writeFileSync, mkdirSync } from 'fs';

const mkdirp = (d) => { try { mkdirSync(d, { recursive: true }); } catch {} };

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

const mbit = (m) => Math.round((m * 1_000_000) / 8); // Mbit/s -> bytes/s (decimal)
const PROFILES = {
    low:    { label: '50 Mbit/s',  downBps: mbit(50),  upBps: mbit(10), latencyMs: 30 },
    normal: { label: '200 Mbit/s', downBps: mbit(200), upBps: mbit(50), latencyMs: 15 },
    local:  { label: 'unbounded',  downBps: -1,         upBps: -1,        latencyMs: 0  }, // -1 disables throttle
};

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8421'), 10) || 8421;
const SCENARIO = arg('scenario', 'both');           // boot | nav | both
const RUNS = parseInt(arg('runs', '1'), 10) || 1;
const WANT_TRACE = argv.includes('--trace');
const WANT_CPU = argv.includes('--cpuprofile');
const PROFILE_NAMES = (arg('profiles', 'low,normal,local')).split(',').map(s => s.trim()).filter(Boolean);
const OUT = outputDir('netperf', argv.includes('--out') ? arg('out') : null);

const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const median = (xs) => { const s = xs.filter(v => v != null).sort((a, b) => a - b); return s.length ? s[(s.length - 1) >> 1] : null; };

// ---------------------------------------------------------------------------
// Page instrumentation (installed BEFORE navigation; shares performance.now())
// ---------------------------------------------------------------------------
function instrument() {
    window.__np = { longtasks: [], rafGaps: [], milestones: {}, screenLog: [] };
    const np = window.__np;
    try {
        new PerformanceObserver((list) => {
            for (const e of list.getEntries())
                np.longtasks.push({ start: +e.startTime.toFixed(1), dur: +e.duration.toFixed(1) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}
    let last = -1, lastScreen = '';
    const mark = (k) => { if (np.milestones[k] === undefined) np.milestones[k] = +performance.now().toFixed(1); };
    function tick(t) {
        if (last >= 0) np.rafGaps.push({ t: +t.toFixed(1), gap: +(t - last).toFixed(1) });
        last = t;
        if ((window.rb3FrameCount || 0) > 0) mark('wasmLive');
        if ((window.rb3AppBooted || 0) >= 1) mark('appBooted');
        const s = window.rb3CurrentScreen || '';
        if (s) mark('firstScreen');
        if (s && s !== lastScreen) { np.screenLog.push({ t: +performance.now().toFixed(1), screen: s }); lastScreen = s; }
        if (s === 'intro_movie_screen') mark('intro');
        if (s === 'splash_screen') mark('splash');
        if (s === 'main_hub_screen') mark('mainHub');
        if (s === 'song_select_screen') mark('songSelect');
        if (s === 'part_difficulty_screen') mark('partDiff');
        if (s === 'game_screen') mark('game');
        requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
}

const pnow = (page) => page.evaluate(() => performance.now()).catch(() => 0);
const screenOf = (page) => page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');

// ---------------------------------------------------------------------------
// CDP: network emulation + waterfall capture
// ---------------------------------------------------------------------------
async function setupNetwork(cdp, profile) {
    await cdp.send('Network.enable');
    await cdp.send('Network.emulateNetworkConditions', {
        offline: false,
        latency: profile.latencyMs,
        downloadThroughput: profile.downBps,
        uploadThroughput: profile.upBps,
    });
}

// Record every request with NODE-side wall timestamps (Date.now()) so we can
// bucket requests into menu-transition windows without mixing clock domains.
function trackNetwork(cdp) {
    const reqs = new Map(); // requestId -> {url, type, t0, bytes, t1, status}
    cdp.on('Network.requestWillBeSent', (e) => {
        reqs.set(e.requestId, {
            url: (e.request.url || '').replace(/^https?:\/\/[^/]+/, ''),
            type: e.type || 'Other', method: e.request.method,
            t0: Date.now(), bytes: 0, t1: null, status: null,
        });
    });
    cdp.on('Network.responseReceived', (e) => { const r = reqs.get(e.requestId); if (r) r.status = e.response.status; });
    cdp.on('Network.dataReceived', (e) => { const r = reqs.get(e.requestId); if (r) r.bytes += e.dataLength || 0; });
    cdp.on('Network.loadingFinished', (e) => {
        const r = reqs.get(e.requestId);
        if (r) { r.t1 = Date.now(); if (e.encodedDataLength) r.bytes = e.encodedDataLength; }
    });
    return {
        all: () => [...reqs.values()],
        // Requests SENT within [a,b] node-ms.
        inWindow: (a, b) => [...reqs.values()].filter(r => r.t0 >= a && r.t0 <= b),
    };
}

// Roll a request list into {reqs, bytes, xhr, durMs, slowest[]}.
function rollupNet(list) {
    const bytes = list.reduce((s, r) => s + (r.bytes || 0), 0);
    const xhr = list.filter(r => r.type === 'XHR').length;
    const durMs = list.reduce((s, r) => s + (r.t1 && r.t0 ? r.t1 - r.t0 : 0), 0);
    const slowest = list.filter(r => r.t1).map(r => ({ url: r.url, type: r.type, bytes: r.bytes, ms: r.t1 - r.t0 }))
        .sort((a, b) => b.ms - a.ms).slice(0, 12);
    return { reqs: list.length, bytes, xhr, durMs, slowest };
}

// Window helper: snapshot perf clock + net + jank around an async action.
async function measureWindow(page, net, fn) {
    const pStart = await pnow(page);
    const nStart = Date.now();
    const result = await fn();
    const pEnd = await pnow(page);
    const nEnd = Date.now();
    const data = await page.evaluate(([a, b]) => {
        const np = window.__np || { longtasks: [], rafGaps: [] };
        const lt = np.longtasks.filter(t => t.start >= a && t.start <= b);
        const gaps = np.rafGaps.filter(g => g.t >= a && g.t <= b).map(g => g.gap);
        return {
            blockedMs: +lt.reduce((s, t) => s + Math.max(0, t.dur - 50), 0).toFixed(0),
            longCount: lt.length,
            maxGap: gaps.length ? +Math.max(...gaps).toFixed(0) : 0,
        };
    }, [pStart, pEnd]);
    const netRoll = rollupNet(net.inWindow(nStart, nEnd));
    return { wallMs: Math.round(nEnd - nStart), ...data, net: netRoll, result };
}

// ---------------------------------------------------------------------------
// Scenario drivers
// ---------------------------------------------------------------------------
const press = async (page, key, hold = 220) => {
    try {
        await page.keyboard.down(key); await sleep(hold); await page.keyboard.up(key); await sleep(180);
    } catch {}
};
async function waitScreenPred(page, pred, timeoutMs, label) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        if (pred(await screenOf(page))) return true;
        await sleep(250);
    }
    console.log(`    [nav] timeout waiting for ${label} (at '${await screenOf(page)}')`);
    return false;
}

// boot: wait to first interactive screen. Returns milestone snapshot.
async function driveBoot(page) {
    await waitScreenPred(page, s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s), 120000, 'boot');
    await sleep(1500); // tail tasks
}

// nav: skip intro, then measure each transition. Mirrors web-song-preview-audio.mjs.
async function driveNav(page, net) {
    const transitions = [];
    await waitScreenPred(page, s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s), 120000, 'boot');
    try { await page.locator('#rb3-canvas').click({ force: true }); } catch {}

    // T1: (intro/splash) -> main_hub
    const t1 = await measureWindow(page, net, async () => {
        for (let i = 0; i < 16 && (await screenOf(page)) !== 'main_hub_screen'; i++) await press(page, 'Space', 250);
        return waitScreenPred(page, s => s === 'main_hub_screen', 30000, 'main_hub');
    });
    transitions.push({ label: 'boot->main_hub', ...t1 });
    if (!t1.result) return transitions;

    // T2: main_hub -> song_select  (loads song list / overshell milos)
    const t2 = await measureWindow(page, net, async () => {
        for (let i = 0; i < 12 && (await screenOf(page)) !== 'song_select_screen'; i++) await press(page, 'Enter');
        return waitScreenPred(page, s => s === 'song_select_screen', 40000, 'song_select');
    });
    transitions.push({ label: 'main_hub->song_select', ...t2 });
    if (!t2.result) return transitions;
    await sleep(1500);

    // T3: song_select -> part_difficulty  (selecting a song)
    const t3 = await measureWindow(page, net, async () => {
        await press(page, 'ArrowDown'); await sleep(800); await press(page, 'Enter');
        return waitScreenPred(page, s => s === 'part_difficulty_screen', 60000, 'part_difficulty');
    });
    transitions.push({ label: 'song_select->part_difficulty', ...t3 });
    if (!t3.result) return transitions;

    // T4: part_difficulty -> game  (loads track/gameplay milos — track_shared, trackpanel)
    const t4 = await measureWindow(page, net, async () => {
        for (let i = 0; i < 5 && (await screenOf(page)) !== 'game_screen'; i++) { await press(page, 'Enter'); await sleep(1200); }
        return waitScreenPred(page, s => s === 'game_screen', 120000, 'game');
    });
    transitions.push({ label: 'part_difficulty->game', ...t4 });
    return transitions;
}

// ---------------------------------------------------------------------------
// Single run
// ---------------------------------------------------------------------------
async function runOne(profileName, scenario, idx) {
    const profile = PROFILES[profileName];
    const runDir = resolve(OUT, `${profileName}-${scenario}-run${idx}`);
    const tag = `[${profileName}/${scenario}#${idx}]`;
    console.log(`\n${tag} launching (net=${profile.label}, latency=${profile.latencyMs}ms)`);

    const { browser, context, page, url } = await launchBrowser(PORT, { noGoto: true });
    const capture = createCapture(page, { filter: /never|ERROR|crash/i });
    await page.addInitScript(instrument);

    const cdp = await context.newCDPSession(page);
    await setupNetwork(cdp, profile);
    const net = trackNetwork(cdp);

    let traceChunks = null;
    if (WANT_TRACE) {
        traceChunks = [];
        cdp.on('Tracing.dataCollected', (e) => { if (e.value) traceChunks.push(...e.value); });
        await cdp.send('Tracing.start', {
            traceConfig: { includedCategories: [
                'devtools.timeline', 'disabled-by-default-devtools.timeline',
                'disabled-by-default-devtools.timeline.frame', 'v8.execute', 'blink.user_timing',
                'loading', 'disabled-by-default-v8.cpu_profiler',
            ] },
            transferMode: 'ReportEvents',
        });
    }
    if (WANT_CPU) {
        await cdp.send('Profiler.enable');
        await cdp.send('Profiler.setSamplingInterval', { interval: 200 });
        await cdp.send('Profiler.start');
    }

    const tNav = Date.now();
    let transitions = [];
    let error = null;
    try {
        await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
        if (scenario === 'nav') transitions = await driveNav(page, net);
        else await driveBoot(page);
    } catch (e) { error = e.message; console.log(`${tag} ERROR: ${e.message}`); }

    // Stop profilers / trace.
    let cpuPath = null, tracePath = null;
    if (WANT_CPU) {
        try {
            const { profile: cpu } = await cdp.send('Profiler.stop');
            cpuPath = resolve(runDir, 'boot.cpuprofile');
            mkdirp(runDir); writeFileSync(cpuPath, JSON.stringify(cpu));
        } catch {}
    }
    if (WANT_TRACE) {
        try {
            const done = new Promise(r => cdp.once('Tracing.tracingComplete', r));
            await cdp.send('Tracing.end'); await done;
            tracePath = resolve(runDir, 'trace.json');
            mkdirp(runDir); writeFileSync(tracePath, JSON.stringify({ traceEvents: traceChunks }));
        } catch {}
    }

    // Pull page-side instrumentation.
    const pageData = await page.evaluate(() => {
        const np = window.__np || { longtasks: [], rafGaps: [], milestones: {}, screenLog: [] };
        return {
            milestones: np.milestones, screenLog: np.screenLog,
            longCount: np.longtasks.length,
            totalBlockedMs: +np.longtasks.reduce((s, t) => s + Math.max(0, t.dur - 50), 0).toFixed(0),
            maxGap: np.rafGaps.length ? +Math.max(...np.rafGaps.map(g => g.gap)).toFixed(0) : 0,
            bootPhases: window.rb3BootPhaseLog || [],
        };
    }).catch(() => ({ milestones: {}, screenLog: [], longCount: 0, totalBlockedMs: 0, maxGap: 0, bootPhases: [] }));
    const finalState = await engineState(page).catch(() => ({}));

    const netAll = net.all();
    const netRoll = rollupNet(netAll);
    await cleanup(browser);

    const report = {
        profile: profileName, profileLabel: profile.label, latencyMs: profile.latencyMs,
        scenario, idx, error,
        milestones: pageData.milestones, bootPhases: pageData.bootPhases,
        screenLog: pageData.screenLog, finalScreen: finalState.screen, finalFrame: finalState.frame,
        jank: { longCount: pageData.longCount, totalBlockedMs: pageData.totalBlockedMs, maxGapMs: pageData.maxGap },
        network: netRoll,
        transitions: transitions.map(t => ({
            label: t.label, reached: !!t.result, wallMs: t.wallMs, blockedMs: t.blockedMs,
            longCount: t.longCount, maxGapMs: t.maxGap,
            reqs: t.net.reqs, bytes: t.net.bytes, xhr: t.net.xhr,
            slowest: t.net.slowest.slice(0, 5),
        })),
    };
    mkdirp(runDir);
    saveJson(report, runDir, 'profile.json');
    saveJson({ requests: netAll.map(r => ({ url: r.url, type: r.type, bytes: r.bytes, ms: r.t1 && r.t0 ? r.t1 - r.t0 : null, status: r.status })) }, runDir, 'network-waterfall.json');
    if (cpuPath) console.log(`${tag} cpuprofile -> ${cpuPath}`);
    if (tracePath) console.log(`${tag} trace -> ${tracePath} (load in DevTools > Performance)`);

    // One-line live summary.
    const M = pageData.milestones;
    const s = (v) => v === undefined ? 'n/a' : `${(v / 1000).toFixed(1)}s`;
    console.log(`${tag} boot: firstScreen=${s(M.firstScreen)} appBooted=${s(M.appBooted)}  net=${(netRoll.bytes/1e6).toFixed(1)}MB/${netRoll.reqs}req(${netRoll.xhr}xhr)  blocked=${pageData.totalBlockedMs}ms`);
    for (const t of report.transitions)
        console.log(`${tag}   ${t.label.padEnd(30)} ${String(t.wallMs).padStart(6)}ms  ${(t.bytes/1e6).toFixed(1)}MB/${t.reqs}req  blocked=${t.blockedMs}ms  maxGap=${t.maxGapMs}ms ${t.reached ? '' : '(NOT REACHED)'}`);
    return report;
}

// ---------------------------------------------------------------------------
// Matrix + aggregate report
// ---------------------------------------------------------------------------
(async () => {
    console.log(`[netperf] waiting for server :${PORT}`);
    await waitForServer(PORT, 20000);
    const scenarios = SCENARIO === 'both' ? ['boot', 'nav'] : [SCENARIO];
    console.log(`[netperf] profiles=${PROFILE_NAMES.join(',')} scenarios=${scenarios.join(',')} runs=${RUNS} out=${OUT}`);

    const all = []; // flat list of run reports
    for (const scenario of scenarios) {
        for (const pn of PROFILE_NAMES) {
            if (!PROFILES[pn]) { console.log(`  skip unknown profile '${pn}'`); continue; }
            for (let i = 1; i <= RUNS; i++) {
                try { all.push(await runOne(pn, scenario, i)); }
                catch (e) { console.log(`  run ${pn}/${scenario}#${i} crashed: ${e.message}`); }
            }
        }
    }

    // Aggregate by (scenario, profile): median across runs.
    const agg = {};
    for (const r of all) {
        const key = `${r.scenario}|${r.profile}`;
        (agg[key] = agg[key] || []).push(r);
    }
    const summary = { generated: new Date().toISOString(), out: OUT, profiles: PROFILES, rows: [] };
    for (const [key, runs] of Object.entries(agg)) {
        const [scenario, profile] = key.split('|');
        const M = (k) => median(runs.map(r => r.milestones[k]));
        const row = {
            scenario, profile, profileLabel: PROFILES[profile].label,
            firstScreenMs: M('firstScreen'), appBootedMs: M('appBooted'), mainHubMs: M('mainHub'),
            songSelectMs: M('songSelect'), gameMs: M('game'),
            netBytes: median(runs.map(r => r.network.bytes)), netReqs: median(runs.map(r => r.network.reqs)),
            netXhr: median(runs.map(r => r.network.xhr)),
            blockedMs: median(runs.map(r => r.jank.totalBlockedMs)), maxGapMs: median(runs.map(r => r.jank.maxGapMs)),
            transitions: scenario === 'nav' ? medianTransitions(runs) : [],
        };
        summary.rows.push(row);
    }
    saveJson(summary, OUT, 'summary.json');
    writeReport(summary, all);
    printMatrix(summary);
    process.exit(0);
})().catch(e => { console.error('[netperf] FATAL', e); process.exit(2); });

function medianTransitions(runs) {
    const labels = runs[0]?.transitions.map(t => t.label) || [];
    return labels.map(label => {
        const ts = runs.map(r => r.transitions.find(t => t.label === label)).filter(Boolean);
        return {
            label,
            wallMs: median(ts.map(t => t.wallMs)), bytes: median(ts.map(t => t.bytes)),
            reqs: median(ts.map(t => t.reqs)), xhr: median(ts.map(t => t.xhr)),
            blockedMs: median(ts.map(t => t.blockedMs)), maxGapMs: median(ts.map(t => t.maxGapMs)),
            reached: ts.every(t => t.reached),
        };
    });
}

function printMatrix(summary) {
    const s = (v) => v == null ? '  n/a' : `${(v / 1000).toFixed(1)}s`;
    const mb = (v) => v == null ? ' n/a' : `${(v / 1e6).toFixed(1)}MB`;
    console.log('\n' + '='.repeat(78));
    console.log('NETPERF SUMMARY (median across runs)');
    const order = { local: 0, normal: 1, low: 2 };
    const rows = summary.rows.slice().sort((a, b) => (a.scenario.localeCompare(b.scenario)) || (order[a.profile] - order[b.profile]));
    let lastScenario = '';
    for (const r of rows) {
        if (r.scenario !== lastScenario) {
            console.log('-'.repeat(78));
            console.log(`SCENARIO: ${r.scenario}`);
            if (r.scenario === 'boot')
                console.log(`  ${'profile'.padEnd(20)} ${'firstScreen'.padStart(12)} ${'appBooted'.padStart(10)} ${'network'.padStart(14)} ${'blocked'.padStart(9)}`);
            lastScenario = r.scenario;
        }
        if (r.scenario === 'boot') {
            console.log(`  ${(r.profile + ' (' + r.profileLabel + ')').padEnd(20)} ${s(r.firstScreenMs).padStart(12)} ${s(r.appBootedMs).padStart(10)} ${(mb(r.netBytes) + '/' + r.netReqs + 'r').padStart(14)} ${(r.blockedMs + 'ms').padStart(9)}`);
        } else {
            console.log(`  ${(r.profile + ' (' + r.profileLabel + ')')}:`);
            for (const t of r.transitions)
                console.log(`      ${t.label.padEnd(30)} ${s(t.wallMs).padStart(8)}  ${mb(t.bytes).padStart(7)}/${String(t.reqs).padStart(2)}r(${t.xhr}xhr)  blocked=${(t.blockedMs + 'ms').padStart(7)}  maxGap=${t.maxGapMs}ms ${t.reached ? '' : '(NOT REACHED)'}`);
        }
    }
    console.log('='.repeat(78));
    console.log(`Full data + traces: ${summary.out}  (REPORT.md, summary.json, per-run dirs)`);
}

function writeReport(summary, all) {
    const s = (v) => v == null ? 'n/a' : `${(v / 1000).toFixed(1)}s`;
    const mb = (v) => v == null ? 'n/a' : `${(v / 1e6).toFixed(1)} MB`;
    const L = [];
    L.push(`# RB3-web network-conditioned performance\n`);
    L.push(`Generated ${summary.generated}\n`);
    L.push(`Profiles: ` + Object.entries(summary.profiles).map(([k, v]) => `**${k}** ${v.label} (${v.latencyMs}ms RTT)`).join(', ') + `\n`);
    const boots = summary.rows.filter(r => r.scenario === 'boot');
    if (boots.length) {
        L.push(`## Boot (cold, to first interactive screen)\n`);
        L.push(`| profile | first screen | app booted | network | main-thread blocked |`);
        L.push(`|---|---|---|---|---|`);
        for (const r of boots.sort((a, b) => order(a) - order(b)))
            L.push(`| ${r.profile} (${r.profileLabel}) | ${s(r.firstScreenMs)} | ${s(r.appBootedMs)} | ${mb(r.netBytes)} / ${r.netReqs} req (${r.netXhr} xhr) | ${r.blockedMs} ms |`);
        L.push('');
    }
    const navs = summary.rows.filter(r => r.scenario === 'nav');
    if (navs.length) {
        L.push(`## Menu transitions (per-transition stall)\n`);
        for (const r of navs.sort((a, b) => order(a) - order(b))) {
            L.push(`### ${r.profile} (${r.profileLabel})\n`);
            L.push(`| transition | wall | network | blocked | max RAF gap |`);
            L.push(`|---|---|---|---|---|`);
            for (const t of r.transitions)
                L.push(`| ${t.label}${t.reached ? '' : ' ⚠️not reached'} | ${s(t.wallMs)} | ${mb(t.bytes)} / ${t.reqs} req (${t.xhr} xhr) | ${t.blockedMs} ms | ${t.maxGapMs} ms |`);
            L.push('');
        }
    }
    L.push(`## Notes\n`);
    L.push(`- "main-thread blocked" = sum of (long task − 50ms): time the tab was frozen and unresponsive.`);
    L.push(`- "max RAF gap" = worst single frame stall (user-visible hitch).`);
    L.push(`- Network throttling is applied via CDP \`Network.emulateNetworkConditions\` (covers localhost).`);
    L.push(`- Each run uses a fresh browser context, so the IndexedDB asset cache is COLD (worst case).`);
    L.push(`- Per-run details + waterfalls in each \`<profile>-<scenario>-runN/\` dir.`);
    const path = resolve(summary.out, 'REPORT.md');
    writeFileSync(path, L.join('\n') + '\n');
    console.log(`[report] ${path}`);
    function order(r) { return ({ local: 0, normal: 1, low: 2 })[r.profile] ?? 9; }
}
