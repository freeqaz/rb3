#!/usr/bin/env node
/**
 * firstframe-gate.mjs — Wave-5 regression gate for the game_screen first-frame hitch.
 *
 * Drives a full boot→gameplay journey (boot→splash→hub→song_select→song),
 * pulls the MEMFS RB3_FRAME_TRACE JSONL, locates the reveal frame (first frame
 * whose `scr` flips to `game_screen`), and asserts dt / counter / over100
 * thresholds. Exits 0 on PASS, non-zero on FAIL or error.
 *
 * Usage:
 *   node scripts/web/firstframe-gate.mjs [options]
 *
 * Options:
 *   --port <n>          Server port (default: 8446)
 *   --out <dir>         Output directory for trace + result JSON (default: /tmp/rb3-w5-gate)
 *   --mbps <n>          Network throttle Mbps (0 = unthrottled; default: 0)
 *   --rtt <n>           Network RTT ms for throttle (default: 0)
 *   --baseline          Use today's-code baseline thresholds (expect FAIL on current
 *                       build, verify the gate itself sees a bad number and would pass
 *                       the baseline). Exits 0 if reveal dt is in the baseline band
 *                       (600–1200 ms web) rather than the target band.
 *   --release           Use the release build (default: debug)
 *   --profile           Arm the CDP V8 sampling profiler at part_difficulty and
 *                       write firstframe.cpuprofile to --out. Requires the debug
 *                       build (wasm names need -g2). Adds ~3s post-game dwell.
 *   --8mbps             Shorthand for --mbps 8 --rtt 80 (wave-4 gate condition A)
 *   --4mbps             Shorthand for --mbps 4 --rtt 150 (wave-4 gate condition B)
 *   --verbose           Echo every console line from the browser
 *
 * Thresholds (TARGET — MET on current master, default warm ON):
 *   Unthrottled / debug or release:
 *     reveal dt <= 120 ms, over100 == 0
 *   As of the 2026-06-23 re-baseline this PASSES on default-ON master: the
 *   framestall venue-aware prewarm (86312dcf) lands the reveal frame ~96 ms.
 *   See research/10-wave6-rebaseline.md.
 *
 *   --baseline mode = the warm-OFF self-test. Run with RB3_GAMEWARM_OFF=1 +
 *   RB3_TEX_PREWARM_OFF=1 (the un-warmed build): reveal dt lands in
 *   [400, 1200] ms (measured ~410 ms), confirming the probe sees the drain.
 *
 * Reveal frame identification: first JSONL line with scr=="game_screen".
 * lpu / lp attribution: reported from the trace once T3 Loader.cpp wiring lands;
 * pre-T3 both are 0 (the gap this task fixes). Counters: objMs, texMs/texN,
 * meshMs/meshN from the same reveal frame.
 *
 * Over100 gate: counts rAF gaps > 100 ms in a 10s window around the reveal
 * frame (the JSPI-yield-vs-rAF-gap distinction matters; the plan §1.1 explains
 * why both dt and over100 must be gated).
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync, readFileSync } from 'fs';

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const has = (k) => argv.includes(`--${k}`);

// shorthand conditions
const COND_8MBPS = has('8mbps');
const COND_4MBPS = has('4mbps');

let PORT    = parseInt(arg('port', '8446'), 10);
let OUT     = arg('out', '/tmp/rb3-w5-gate');
let MBPS    = parseFloat(arg('mbps', COND_8MBPS ? '8' : (COND_4MBPS ? '4' : '0')));
let RTT     = parseFloat(arg('rtt',  COND_8MBPS ? '80' : (COND_4MBPS ? '150' : '0')));
const BASELINE = has('baseline');
const RELEASE  = has('release');
const PROFILE  = has('profile');
const VERBOSE  = has('verbose');

const THROTTLED = MBPS > 0;
const BUILD = RELEASE ? 'release' : 'debug';

mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// ---------------------------------------------------------------------------
// Thresholds
// ---------------------------------------------------------------------------

// TARGET thresholds. PASSES on current master (default warm ON) as of the
// 2026-06-23 re-baseline — the framestall venue-aware prewarm (86312dcf) lands
// the reveal frame ~96 ms. Over100 gate is the rAF freeze count in the
// start-song window (separate from dt, see plan §1.1).
const TARGET = {
    revealDtMaxMs: 120,   // reveal frame wall-clock dt (ms)
    over100Max:    0,     // rAF gaps > 100 ms in the start-song window
};

// BASELINE thresholds = the warm-OFF self-test band. Run --baseline with
// RB3_GAMEWARM_OFF=1 + RB3_TEX_PREWARM_OFF=1: the un-warmed reveal frame lands
// in [400, 1200] ms (measured ~410 ms), confirming the probe sees the drain.
const BASELINE_BAND = { minMs: 400, maxMs: 1200 };

// ---------------------------------------------------------------------------
// Journey navigation helpers
// ---------------------------------------------------------------------------

const stateOf = (page) => page.evaluate(() => ({
    screen: window.rb3CurrentScreen || '',
    frame:  window.rb3FrameCount    || 0,
    booted: window.rb3AppBooted     || 0,
    now:    performance.now(),
})).catch(() => ({ screen: '', frame: 0, booted: 0, now: 0 }));

const waitScreen = async (page, pred, timeoutMs, label) => {
    const dl = Date.now() + timeoutMs;
    let last = '';
    while (Date.now() < dl) {
        const s = await stateOf(page);
        if (s.screen !== last) {
            console.log(`  [nav] ${label}: '${s.screen}' frame=${s.frame}`);
            last = s.screen;
        }
        if (pred(s)) return s;
        await sleep(250);
    }
    console.warn(`  [nav] TIMEOUT waiting for ${label}`);
    return null;
};

const press = async (page, key, holdMs = 200, gapMs = 450) => {
    try {
        await page.keyboard.down(key);
        await sleep(holdMs);
        await page.keyboard.up(key);
        await sleep(gapMs);
    } catch {}
};

// ---------------------------------------------------------------------------
// rAF instrument injected into the page
// ---------------------------------------------------------------------------

function rafInstrument() {
    window.__wf = { perf0: performance.now(), raf: [], screenFirst: {} };
    const wf = window.__wf;
    let last = -1;
    const s = () => window.rb3CurrentScreen || '';
    function tick() {
        const t = performance.now();
        if (last >= 0) wf.raf.push([+t.toFixed(1), +(t - last).toFixed(1)]);
        last = t;
        const name = s();
        if (name && wf.screenFirst[name] === undefined) wf.screenFirst[name] = t;
        requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
}

// Count rAF gaps > 100 ms in perf.now window [p0, p1].
function over100In(raf, p0, p1) {
    return raf.filter(g => g[0] >= p0 && g[0] <= p1 && g[1] > 100).length;
}

// ---------------------------------------------------------------------------
// JSONL trace parsing
// ---------------------------------------------------------------------------

function parseTrace(text) {
    const lines = text.split('\n').filter(l => l.trim().startsWith('{'));
    const frames = [];
    for (const l of lines) {
        try { frames.push(JSON.parse(l)); } catch {}
    }
    return frames;
}

// Find the first frame with scr == "game_screen" — the reveal frame.
function findRevealFrame(frames) {
    return frames.find(f => f.scr === 'game_screen') || null;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

const T0 = Date.now();
console.log(`firstframe-gate: port=${PORT} build=${BUILD} mbps=${MBPS} rtt=${RTT} baseline=${BASELINE} profile=${PROFILE}`);

let browser, exitCode = 1;

try {
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox',
            '--enable-unsafe-webgpu',
            '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11',
            '--disable-extensions',
            '--disable-background-networking',
            '--mute-audio',
            '--autoplay-policy=no-user-gesture-required',
        ],
    });

    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const cdp  = await ctx.newCDPSession(page);

    // Cold cache
    await cdp.send('Storage.clearDataForOrigin', {
        origin: `http://127.0.0.1:${PORT}`, storageTypes: 'all',
    }).catch(() => {});

    // Network throttle (opt-in)
    if (THROTTLED) {
        await cdp.send('Network.enable');
        await cdp.send('Network.emulateNetworkConditions', {
            offline: false,
            latency: RTT,
            downloadThroughput: (MBPS * 1024 * 1024) / 8,
            uploadThroughput:   (MBPS * 1024 * 1024) / 8,
        });
        console.log(`throttle: ${MBPS} Mbps / ${RTT} ms RTT`);
    }

    // Console capture
    const logs = [];
    page.on('console', m => {
        logs.push({ t: Date.now() - T0, text: m.text() });
        if (VERBOSE || m.type() === 'error') console.log(`  [browser] ${m.text()}`);
    });
    page.on('pageerror', err => console.warn(`  [pageerror] ${err.message}`));

    // Inject rAF instrument before page load
    await page.addInitScript(rafInstrument);

    // Build the URL — always enable FRAME_TRACE via ?env=
    const ENVQ = encodeURIComponent('RB3_FRAME_TRACE=/trace.jsonl');
    const queryParts = [
        RELEASE ? '' : 'debug=true',
        `env=${ENVQ}`,
    ].filter(Boolean);
    const url = `http://127.0.0.1:${PORT}/?${queryParts.join('&')}`;

    console.log(`goto: ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 120000 });

    // ---- BOOT ----
    await waitScreen(page,
        s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s.screen),
        420000, 'boot');
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    await sleep(500);

    // ---- INTRO -> SPLASH ----
    for (let i = 0; i < 30; i++) {
        const sc = (await stateOf(page)).screen;
        if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
        await press(page, 'Space', 200, 600);
    }
    await waitScreen(page, s => ['splash_screen', 'main_hub_screen'].includes(s.screen), 180000, 'splash');
    await sleep(1200);

    // ---- SPLASH -> MAIN_HUB ----
    for (let i = 0; i < 18 && (await stateOf(page)).screen !== 'main_hub_screen'; i++) {
        await press(page, 'Space', 250, 600);
    }
    await waitScreen(page, s => s.screen === 'main_hub_screen', 180000, 'main_hub');
    await sleep(1500);

    // ---- MAIN_HUB -> SONG_SELECT ----
    for (let i = 0; i < 14 && (await stateOf(page)).screen !== 'song_select_screen'; i++) {
        await press(page, 'Enter', 220, 500);
    }
    await waitScreen(page, s => s.screen === 'song_select_screen', 90000, 'song_select');
    await sleep(3000);

    // Down a few rows to a non-default song for journey reproducibility
    for (let i = 0; i < 3; i++) await press(page, 'ArrowDown', 130, 250);
    await sleep(1000);

    // ---- SONG_SELECT -> PART_DIFFICULTY ----
    for (let i = 0; i < 6 && !['part_difficulty_screen', 'game_screen'].includes((await stateOf(page)).screen); i++) {
        await press(page, 'Enter', 220, 600);
    }
    console.log(`at: ${(await stateOf(page)).screen}`);

    // Optional CDP profiler: arm at part_difficulty
    let profilerArmed = false;
    if (PROFILE) {
        await cdp.send('Profiler.enable');
        await cdp.send('Profiler.setSamplingInterval', { interval: 200 }); // 200 µs
        await cdp.send('Profiler.start');
        profilerArmed = true;
        console.log('CDP profiler armed');
    }

    // Record perf.now just before the reveal transition
    const startSongPerf = await page.evaluate(() => performance.now());

    // ---- PART_DIFFICULTY -> GAME_SCREEN ----
    for (let i = 0; i < 6 && (await stateOf(page)).screen !== 'game_screen'; i++) {
        await press(page, 'Enter', 220, 700);
    }
    const reached = await waitScreen(page, s => s.screen === 'game_screen', 180000, 'game_screen');
    const reachedPerf = reached ? await page.evaluate(() => performance.now()) : null;
    console.log(`reached game_screen: ${!!reached}`);

    // Dwell a few frames so the trace flushes
    await sleep(PROFILE ? 3000 : 1500);
    const endDwellPerf = await page.evaluate(() => performance.now());

    // Stop profiler
    if (profilerArmed) {
        const { profile } = await cdp.send('Profiler.stop');
        const profilePath = `${OUT}/firstframe.cpuprofile`;
        writeFileSync(profilePath, JSON.stringify(profile));
        console.log(`CDP profile written: ${profilePath}`);
    }

    // ---- Collect rAF data ----
    const { raf, screenFirst } = await page.evaluate(() => window.__wf);

    // ---- Pull MEMFS trace ----
    let traceText = '';
    try {
        traceText = await page.evaluate(() => {
            try { return FS.readFile('/trace.jsonl', { encoding: 'utf8' }); }
            catch (e) { return 'ERR:' + String(e); }
        });
    } catch (e) { traceText = 'EVAL_ERR:' + String(e); }

    // ---- Persist raw outputs ----
    writeFileSync(`${OUT}/trace.jsonl`, traceText);
    writeFileSync(`${OUT}/raf.json`, JSON.stringify({ raf, screenFirst }));
    writeFileSync(`${OUT}/console.jsonl`, logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(`trace written: ${OUT}/trace.jsonl (${traceText.length} bytes)`);

    // ---- Analyse ----
    const frames = parseTrace(traceText);
    const reveal = findRevealFrame(frames);

    // rAF window: from the moment we started the song (just before part_difficulty
    // confirm) to 10s after the reveal.
    const winP0 = startSongPerf;
    const winP1 = reachedPerf !== null ? reachedPerf + 10000 : endDwellPerf;
    const over100 = over100In(raf, winP0, winP1);

    console.log('');
    console.log('=== firstframe-gate analysis ===');
    if (!reveal) {
        console.log('WARN: no game_screen frame found in trace — tracing may not be active');
        console.log(`trace lines: ${frames.length}, first: ${frames[0] ? JSON.stringify(frames[0]).slice(0,120) : 'none'}`);
    } else {
        console.log(`reveal frame: f=${reveal.f} dt=${reveal.dt.toFixed(1)}ms lp=${reveal.lp.toFixed(1)} lpu=${reveal.lpu.toFixed(1)}`);
        console.log(`  objMs=${reveal.objMs.toFixed(1)} (worst: ${reveal.objWNm} ${reveal.objWMs.toFixed(1)}ms)`);
        console.log(`  texMs=${reveal.texMs.toFixed(1)} texN=${reveal.texN} meshMs=${reveal.meshMs.toFixed(1)} meshN=${reveal.meshN}`);
        console.log(`  pipeMs=${reveal.pipeMs.toFixed(1)} pipeN=${reveal.pipeN} fetchMs=${reveal.fetchMs.toFixed(1)}`);
        if (reveal.unpackMs !== undefined) {
            console.log(`  unpackMs=${reveal.unpackMs.toFixed(1)} unpackN=${reveal.unpackN}`);
        }
    }
    console.log(`rAF over100 in start-song window: ${over100}`);
    console.log('');

    // ---- Gate decision ----
    const revealDt = reveal ? reveal.dt : null;
    let pass = false;
    let verdict = '';

    if (BASELINE) {
        // Baseline mode: confirm today's build is in the expected band (gate self-test).
        if (revealDt !== null && revealDt >= BASELINE_BAND.minMs && revealDt <= BASELINE_BAND.maxMs) {
            pass = true;
            verdict = `PASS (baseline): reveal dt=${revealDt.toFixed(1)}ms is in the baseline band [${BASELINE_BAND.minMs},${BASELINE_BAND.maxMs}]ms — gate self-test OK`;
        } else if (revealDt === null) {
            pass = false;
            verdict = 'FAIL (baseline): no reveal frame found in trace — cannot confirm baseline band';
        } else {
            pass = false;
            verdict = `FAIL (baseline): reveal dt=${revealDt.toFixed(1)}ms is outside baseline band [${BASELINE_BAND.minMs},${BASELINE_BAND.maxMs}]ms — unexpected`;
        }
    } else {
        // Target mode: assert post-T1/T2 improvement goals.
        const dtOk  = revealDt !== null && revealDt <= TARGET.revealDtMaxMs;
        const rafOk = over100 <= TARGET.over100Max;
        pass = dtOk && rafOk;
        if (!reveal) {
            verdict = 'FAIL: no reveal frame in trace';
        } else if (!dtOk) {
            verdict = `FAIL: reveal dt=${revealDt.toFixed(1)}ms > target ${TARGET.revealDtMaxMs}ms (T1/T2 not yet landed)`;
        } else if (!rafOk) {
            verdict = `FAIL: over100=${over100} > target ${TARGET.over100Max} rAF freezes`;
        } else {
            verdict = `PASS: reveal dt=${revealDt.toFixed(1)}ms <= ${TARGET.revealDtMaxMs}ms, over100=${over100}`;
        }
    }

    console.log(verdict);

    // Persist result
    const result = {
        build: BUILD, mbps: MBPS, rtt: RTT, baseline: BASELINE,
        reveal: reveal ? {
            f: reveal.f, dt: reveal.dt, lp: reveal.lp, lpu: reveal.lpu,
            objMs: reveal.objMs, objWMs: reveal.objWMs, objWNm: reveal.objWNm,
            texMs: reveal.texMs, texN: reveal.texN,
            meshMs: reveal.meshMs, meshN: reveal.meshN,
            pipeMs: reveal.pipeMs, pipeN: reveal.pipeN,
            fetchMs: reveal.fetchMs,
            unpackMs: reveal.unpackMs,
            unpackN: reveal.unpackN,
        } : null,
        over100, traceFrames: frames.length,
        pass, verdict,
        targets: TARGET, baselineBand: BASELINE_BAND,
    };
    writeFileSync(`${OUT}/result.json`, JSON.stringify(result, null, 2));
    console.log(`result written: ${OUT}/result.json`);

    exitCode = pass ? 0 : 1;

} catch (err) {
    console.error(`firstframe-gate ERROR: ${err.message || err}`);
    exitCode = 2;
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), sleep(3000)]); } catch {}
    }
}

console.log(`exit code: ${exitCode}`);
process.exit(exitCode);
