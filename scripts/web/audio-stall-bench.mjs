#!/usr/bin/env node
/**
 * audio-stall-bench.mjs — STALL-RESILIENCE CURVE benchmark for the RB3 web audio
 * architecture. Extends audio-stall-measure.mjs from "passively observe stalls"
 * to "ACTIVELY INJECT controlled main-thread longtasks and measure how the audio
 * pipeline holds up". This is the objective A/B yardstick for comparing audio
 * architectures (current main-thread-pump baseline vs any off-main-thread MVP).
 *
 * WHY THIS IS THE RIGHT TEST
 * --------------------------
 * Today the audio PRODUCER (AudioDevice_Web.cpp PumpAudio -> MixSources -> resample
 * -> SAB ring write) runs ONCE PER requestAnimationFrame on the MAIN THREAD, while
 * the CONSUMER (audio-worklet.js) drains the SAB ring on the real-time audio clock.
 * A main-thread longtask longer than the buffered ring depth empties the ring and
 * the worklet pads silence (the audible "click"/under-run). So the failure mode is
 * exactly: main-thread stall > ring depth. We reproduce it deterministically by
 * busy-looping the main thread for a configurable duration on a configurable
 * cadence, and read the under-run rate straight from the worklet's instrumentation
 * (window._rb3Audio.underruns, posted ~every 0.5s of audio).
 *
 * THE INJECTION
 * -------------
 * A self-rescheduling rAF hook on the MAIN THREAD busy-spins (Date.now() loop, no
 * yields) for `stallMs`, every `intervalMs` of wall time. Because the engine's
 * PumpAudio is driven from the same rAF loop (App::RunOneFrame), the stall lands
 * right on the producer — i.e. it withholds exactly the cycle that would have
 * refilled the ring, which is the real-world jank we are defending against. We
 * sweep `stallMs` over a configurable set (default 0,50,100,200,400 ms) and hold
 * each step for `--step-secs` while recording the under-run DELTA for that window
 * only (cumulative counters are differenced per step, and the injector is reset to
 * 0 between steps to let the ring recover).
 *
 * THE OUTPUT — "stall-resilience curve"
 * -------------------------------------
 *   for each injected stall size:
 *     - under-run rate           = silence-padded frames / total frames in window (%)
 *     - under-run events/sec      = worklet quanta that had to pad, per second
 *     - worst audible gap (ms)    = largest single contiguous starvation, estimated
 *                                   from the biggest per-window under-run-frame burst
 *     - min ring depth (frames/ms)= worklet low-water mark (how close to empty)
 *     - observed main-thread jank = measured rAF gap p99/max during the window
 *                                   (sanity: did the injected stall actually land?)
 *
 * This curve is the BASELINE every off-main-thread MVP must beat: a stall-immune
 * producer should hold ~0% under-runs even as injected stall grows past the ring
 * depth, whereas the current main-thread pump degrades monotonically.
 *
 * USAGE
 *   node scripts/web/audio-stall-bench.mjs \
 *        [--port 8421] [--stalls 0,50,100,200,400] [--step-secs 12] \
 *        [--interval 250] [--out /tmp/rb3-audio-stall-bench] [--song 20thcenturyboy]
 *
 *   --port       dev-server port (default 8421)
 *   --stalls     comma list of injected stall sizes in ms (default 0,50,100,200,400)
 *   --step-secs  seconds to hold each stall size and measure (default 12)
 *   --interval   ms between injected stalls within a step (default 250 -> 4 stalls/s)
 *   --song       aid-target song shortname (default 20thcenturyboy)
 *   --out        output dir (default /tmp/rb3-audio-stall-bench)
 *
 * OUTPUT FILES
 *   <out>/curve.json   machine-readable curve + per-window samples + console logs
 *   <out>/curve.csv    stall_ms,underrun_pct,events_per_sec,worst_gap_ms,min_ring_ms,raf_p99_ms,raf_max_ms
 *   stdout             the human-readable curve table + VERDICT
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const STALLS = arg('--stalls', '0,50,100,200,400').split(',').map(s => parseInt(s, 10)).filter(n => n >= 0);
const STEP_SECS = parseInt(arg('--step-secs', '12'), 10) || 12;
const INTERVAL_MS = parseInt(arg('--interval', '250'), 10) || 250;
const SONG = arg('--song', '20thcenturyboy');
const OUT = arg('--out', '/tmp/rb3-audio-stall-bench');
// Build to load. The fast iteration path (scripts/web/build.sh --debug) deploys
// only build/debug/, which the server serves at /?debug=true. Default to debug so
// the bench loads the build you just built; pass --release if you built release/.
const BUILD = arg('--build', 'debug'); // 'debug' | 'release'
// Optional ?env= passthrough (semicolon-separated RB3_* flags), e.g.
//   --env RB3_WEB_OFFMAIN_MIX=1
// Bridged to wasm getenv via native/web/rb3_pre.js's ?env= seam.
const ENV_PARAM = arg('--env', '');
let URL_SUFFIX = BUILD === 'release' ? '/' : '/?debug=true';
if (ENV_PARAM) {
    URL_SUFFIX += (URL_SUFFIX.includes('?') ? '&' : '?') + 'env=' + encodeURIComponent(ENV_PARAM);
}
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

// ---- page-side instrumentation + stall injector (installed before page load) ----
// Exposes window.__bench with controllable injection + raw sample buffers, plus a
// per-window snapshot/reset API the node side drives between sweep steps.
function installBench() {
    const B = window.__bench = {
        stallMs: 0,           // current injected stall (node sets via __benchSetStall)
        intervalMs: 250,      // min wall-ms between injected stalls
        injCount: 0,          // # stalls actually injected (proof the hook fired)
        injBusyMs: 0,         // total measured busy-spin time (proof it really blocked)
        _lastInj: 0,
        rafGaps: [],          // [ms] every rAF gap (cleared per window)
        longtasks: [],        // {start,dur} >50ms (cleared per window)
        urSamples: [],        // {t,events,frames,quanta,total,minDepth} worklet snapshots
    };

    // Long tasks (>50ms) — sanity that our injected stall registers as a longtask.
    try {
        new PerformanceObserver(list => {
            for (const e of list.getEntries())
                B.longtasks.push({ start: +e.startTime.toFixed(1), dur: +e.duration.toFixed(1) });
        }).observe({ entryTypes: ['longtask'] });
    } catch (e) {}

    // The injector + rAF-gap recorder live on ONE rAF loop so the busy-spin lands
    // on the same cadence the engine's PumpAudio runs (App::RunOneFrame is rAF-driven).
    let lastRaf = -1;
    const raf = (t) => {
        if (lastRaf >= 0) B.rafGaps.push(+(t - lastRaf).toFixed(1));
        lastRaf = t;
        // Inject a controlled main-thread longtask: busy-spin for stallMs, but no
        // more often than every intervalMs of wall time.
        if (B.stallMs > 0) {
            const now = Date.now();
            if (now - B._lastInj >= B.intervalMs) {
                B._lastInj = now;
                const until = now + B.stallMs;
                let spins = 0;
                // Pure busy loop — does NOT yield, so the main thread (and thus the
                // producer pump) is fully blocked for stallMs, exactly like a real
                // longtask. The trivial arithmetic keeps the JIT from eliding it.
                while (Date.now() < until) { spins++; if (spins < 0) break; }
                B.injCount++;
                B.injBusyMs += (Date.now() - now);
            }
        }
        requestAnimationFrame(raf);
    };
    requestAnimationFrame(raf);

    // Poll the worklet's cumulative under-run counters every 0.25s (the worklet
    // posts ~every 0.5s; oversampling is harmless and improves window alignment).
    const poll = () => {
        const a = window._rb3Audio;
        if (a && a.underruns) {
            const u = a.underruns;
            B.urSamples.push({
                t: performance.now(),
                events: u.underrunEvents | 0,
                frames: u.underrunFrames | 0,
                quanta: u.totalQuanta | 0,
                total: u.totalFrames | 0,
                minDepth: (u.minRingDepthFrames | 0) || 0,
            });
        }
        setTimeout(poll, 250);
    };
    setTimeout(poll, 500);

    // Node-driven controls.
    window.__benchSetStall = (ms, interval) => {
        window.__bench.stallMs = ms | 0;
        if (interval) window.__bench.intervalMs = interval | 0;
        window.__bench._lastInj = 0; // allow an immediate first injection
    };
    // Snapshot the CURRENT cumulative state + buffered samples, then clear the
    // per-window buffers so the next window starts fresh. Returns everything the
    // node side needs to compute the delta for the window that just ended.
    window.__benchSnapshot = () => {
        const B = window.__bench;
        const a = window._rb3Audio;
        const cur = a && a.underruns ? {
            events: a.underruns.underrunEvents | 0,
            frames: a.underruns.underrunFrames | 0,
            quanta: a.underruns.totalQuanta | 0,
            total: a.underruns.totalFrames | 0,
            minDepth: (a.underruns.minRingDepthFrames | 0) || 0,
        } : null;
        const out = {
            cur,
            rafGaps: B.rafGaps.slice(),
            longtasks: B.longtasks.slice(),
            urSamples: B.urSamples.slice(),
            injCount: B.injCount,
            injBusyMs: B.injBusyMs,
            ctxRate: a && a.ctx ? a.ctx.sampleRate : null,
            bufFrames: a ? (a.bufFrames || 0) : 0,
            screen: window.rb3CurrentScreen || '',
        };
        // Reset per-window accumulators.
        B.rafGaps.length = 0;
        B.longtasks.length = 0;
        B.urSamples.length = 0;
        B.injCount = 0;
        B.injBusyMs = 0;
        return out;
    };
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

function pct(sortedAsc, p) {
    if (!sortedAsc.length) return 0;
    const i = Math.max(0, Math.min(sortedAsc.length - 1, Math.floor(p / 100 * sortedAsc.length) - 1));
    return sortedAsc[i];
}

// Estimate the worst contiguous audible gap (ms) in a window from the per-poll
// under-run-FRAME deltas. Each poll covers ~0.25-0.5s; the biggest single-poll
// jump in underrunFrames is the worst burst of silence we padded in that span.
// frames / ctxRate * 1000 = ms of audible silence. This is a conservative proxy
// for the largest single dropout (real contiguous gap <= sum across the poll).
function worstGapMs(urSamples, ctxRate) {
    if (urSamples.length < 2 || !ctxRate) return 0;
    let maxBurst = 0;
    for (let i = 1; i < urSamples.length; i++) {
        const d = urSamples[i].frames - urSamples[i - 1].frames;
        if (d > maxBurst) maxBurst = d;
    }
    return +(maxBurst / ctxRate * 1000).toFixed(1);
}

let browser;
const logs = [];

try {
    await waitForServer(PORT);
    console.log(`[bench] server up at :${PORT}`);
    console.log(`[bench] sweep stalls=[${STALLS.join(', ')}] ms  step=${STEP_SECS}s  interval=${INTERVAL_MS}ms  song=${SONG}`);

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps', '--disable-sync',
            // do NOT --mute-audio — the AudioWorklet must run.
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
        if (/audio|underrun|AudioWorklet|ring|PumpAudio/i.test(text))
            console.log(`  [${elapsed()}s] ${text.slice(0, 120)}`);
    });
    page.on('pageerror', e => console.log(`  [PAGEERROR] ${e.message}`));

    await page.addInitScript(installBench);

    console.log(`[bench] loading http://127.0.0.1:${PORT}${URL_SUFFIX} (build=${BUILD})`);
    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    console.log('[bench] waiting for app boot...');
    {
        const deadline = Date.now() + 300000;
        while (Date.now() < deadline) {
            const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
            if (b >= 1) break;
            await sleep(500);
        }
    }
    console.log(`[bench] booted (${elapsed()}s)`);

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    console.log(`[bench] screen: ${s} (${elapsed()}s)`);

    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) {
        await pressKey(page, 'Space', 300);
        s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    await sleep(2000);

    if (s === 'splash_screen') {
        console.log('[bench] splash -> main_hub...');
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
    console.log(`[bench] main_hub: ${s} (${elapsed()}s)`);

    if (s === 'main_hub_screen') {
        console.log('[bench] main_hub -> song_select...');
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
    await sleep(3000);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    console.log(`[bench] song_select: ${s} (${elapsed()}s)`);

    // -> game_screen for MOGG playback (the real, sustained song stream).
    console.log(`[bench] launching song '${SONG}' -> game_screen...`);
    await page.evaluate((song) => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = song; }, SONG).catch(() => {});
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(2500);
    for (let i = 0; i < 6; i++) {
        await pressKey(page, 'Enter', 150);
        await sleep(1200);
        const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (cur === 'game_screen') { s = cur; break; }
    }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    console.log(`[bench] game_screen: ${s} (${elapsed()}s)`);
    if (s !== 'game_screen') {
        throw new Error(`never reached game_screen (stuck at '${s}') — cannot run the sweep without sustained song audio`);
    }

    // Let the song stabilize + ring prime before the first measurement window.
    console.log('[bench] stabilizing song audio (6s) before sweep...');
    await sleep(6000);
    await page.evaluate(() => window.__benchSnapshot()); // discard the warmup window

    // ---- THE SWEEP ----
    const rows = [];
    const windows = [];
    for (const stallMs of STALLS) {
        // Arm the injector and clear accumulators for a clean window.
        await page.evaluate(([ms, iv]) => window.__benchSetStall(ms, iv), [stallMs, INTERVAL_MS]);
        await page.evaluate(() => window.__benchSnapshot()); // reset window buffers at arm time
        console.log(`[bench] --- stall=${stallMs}ms for ${STEP_SECS}s (inject every ${INTERVAL_MS}ms) ---`);

        await sleep(STEP_SECS * 1000);

        const snap = await page.evaluate(() => window.__benchSnapshot());
        // Disarm + recover between steps so the ring refills before the next size.
        await page.evaluate(() => window.__benchSetStall(0));

        // Compute the window delta from the urSamples buffer (first->last in window).
        const us = snap.urSamples;
        const ctxRate = snap.ctxRate || 44100;
        let dFrames = 0, dEvents = 0, dTotal = 0;
        // minDepth = the TRUE STEM-RING fill low-water for the window (in ctx
        // frames), taken straight from the worklet's per-window minRingDepthFrames
        // samples (u.minDepth). The stem ring fills to ~7-8 s (~320k-360k frames),
        // far deeper than the 32768-frame output ring.
        //
        // BUGFIX (deepring): we used to seed `minDepth = snap.bufFrames` (= the
        // 32768-frame OUTPUT/SFX ring) and then take min over the raw stem samples.
        // But every raw stem minDepth is ~320k-360k >> 32768, so the min was ALWAYS
        // pinned at bufFrames -> 32768/44100 = 743 ms, a structural clamp that made
        // the off-main stem ring look ~10x shallower than it really is. Seed instead
        // from the actual samples (no bufFrames floor) so the column reports the real
        // stem-ring depth. See docs/native/audio-thread-2026-06-20/08-DIAGNOSIS-*.md.
        let minDepth = 0;
        if (us.length >= 2) {
            dFrames = us[us.length - 1].frames - us[0].frames;
            dEvents = us[us.length - 1].events - us[0].events;
            dTotal = us[us.length - 1].total - us[0].total;
            let seeded = false;
            for (const u of us) {
                if (u.minDepth > 0) {
                    if (!seeded || u.minDepth < minDepth) { minDepth = u.minDepth; seeded = true; }
                }
            }
        }
        const urPct = dTotal > 0 ? (100 * dFrames / dTotal) : 0;
        const eventsPerSec = dEvents / STEP_SECS;
        const wGap = worstGapMs(us, ctxRate);
        const minDepthMs = ctxRate ? +(minDepth / ctxRate * 1000).toFixed(1) : 0;

        const gapsAsc = snap.rafGaps.slice().sort((a, b) => a - b);
        const rafP99 = pct(gapsAsc, 99);
        const rafMax = gapsAsc.length ? gapsAsc[gapsAsc.length - 1] : 0;
        const ltMax = snap.longtasks.reduce((m, lt) => Math.max(m, lt.dur), 0);

        const row = {
            stallMs,
            underrunPct: +urPct.toFixed(3),
            eventsPerSec: +eventsPerSec.toFixed(2),
            underrunFrames: dFrames,
            underrunEvents: dEvents,
            totalFrames: dTotal,
            worstGapMs: wGap,
            minRingFrames: minDepth,
            minRingMs: minDepthMs,
            rafP99Ms: +rafP99.toFixed(1),
            rafMaxMs: +rafMax.toFixed(1),
            ltMaxMs: +ltMax.toFixed(1),
            injCount: snap.injCount,
            injBusyMs: snap.injBusyMs,
            ctxRate,
            bufFrames: snap.bufFrames,
        };
        rows.push(row);
        windows.push({ stallMs, urSamples: us, rafGapCount: snap.rafGaps.length });

        console.log(`  underrun=${row.underrunPct.toFixed(2)}%  events/s=${row.eventsPerSec.toFixed(1)}  worstGap=${row.worstGapMs}ms  minRing=${row.minRingMs}ms  rafP99=${row.rafP99Ms}ms rafMax=${row.rafMaxMs}ms  injected=${row.injCount}x (busy ${row.injBusyMs}ms, ltMax ${row.ltMaxMs}ms)`);

        // Recovery gap so the ring is full again before the next, bigger stall.
        await sleep(3000);
        await page.evaluate(() => window.__benchSnapshot());
    }

    // ---- REPORT ----
    const ctxRate = rows[0]?.ctxRate || 44100;
    const bufFrames = rows[0]?.bufFrames || 0;
    const ringMs = bufFrames ? (bufFrames / ctxRate * 1000).toFixed(0) : '?';
    console.log('\n' + '='.repeat(92));
    console.log('STALL-RESILIENCE CURVE — RB3 web audio (CURRENT main-thread-pump architecture)');
    console.log(`ctxRate=${ctxRate}Hz  ring=${bufFrames} frames (~${ringMs}ms)  step=${STEP_SECS}s  inject-interval=${INTERVAL_MS}ms`);
    console.log('-'.repeat(92));
    console.log('  stall(ms) | underrun% | events/s | worstGap(ms) | minRing(ms) | rafP99 | rafMax | injected');
    console.log('  ' + '-'.repeat(88));
    for (const r of rows) {
        console.log(
            '  ' + String(r.stallMs).padStart(8) +
            '  | ' + r.underrunPct.toFixed(2).padStart(8) +
            '  | ' + r.eventsPerSec.toFixed(1).padStart(7) +
            '  | ' + String(r.worstGapMs).padStart(11) +
            '  | ' + String(r.minRingMs).padStart(10) +
            '  | ' + String(r.rafP99Ms).padStart(5) +
            '  | ' + String(r.rafMaxMs).padStart(5) +
            '  | ' + String(r.injCount) + 'x/' + r.injBusyMs + 'ms'
        );
    }
    console.log('-'.repeat(92));

    // Verdict: where does the current architecture start dropping audio?
    const firstBreak = rows.find(r => r.stallMs > 0 && r.underrunPct >= 0.5);
    const cleanStalls = rows.filter(r => r.stallMs > 0 && r.underrunPct < 0.5).map(r => r.stallMs);
    // The real stall budget is the STEM-ring fill (minRing), NOT the 32768-frame
    // output ring (bufFrames). With OFFMAIN the stem ring fills to ~7-8 s, so the
    // single-freeze budget is multi-second; the output ring (~743 ms) only bounds
    // the output-quantum latency, not the music stall budget.
    const stemRingMs = rows.reduce((m, r) => Math.max(m, r.minRingMs || 0), 0);
    console.log('VERDICT (baseline to beat):');
    console.log(`  STEM-ring fill ~${stemRingMs.toFixed(0)}ms is the single-freeze stall budget (off-main music). ` +
                `(output ring ${ringMs}ms only bounds output-quantum latency, NOT the music budget.)`);
    if (firstBreak) {
        console.log(`  AUDIBLE under-runs begin at injected stall = ${firstBreak.stallMs}ms ` +
                    `(${firstBreak.underrunPct.toFixed(2)}% frames padded, ${firstBreak.eventsPerSec.toFixed(1)} events/s, worst gap ${firstBreak.worstGapMs}ms).`);
    } else {
        console.log(`  no injected stall in [${STALLS.join(',')}] produced >=0.5% under-runs ` +
                    `— either the ring absorbed them all or the injection didn't land (check 'injected' column).`);
    }
    if (cleanStalls.length)
        console.log(`  CLEAN (<0.5% under-run) up to: ${Math.max(...cleanStalls)}ms injected stall.`);
    console.log(`  An off-main-thread producer MVP must hold ~0% under-runs across the WHOLE sweep ` +
                `(incl. ${Math.max(...STALLS)}ms) to win.`);
    console.log('='.repeat(92));

    // ---- write artifacts ----
    const result = {
        meta: {
            port: PORT, song: SONG, stalls: STALLS, stepSecs: STEP_SECS, intervalMs: INTERVAL_MS,
            ctxRate, bufFrames, ringMs: +ringMs, architecture: 'main-thread-pump (140ms floor baseline)',
            ts: new Date().toISOString(),
        },
        curve: rows,
        windows,
        consoleLogs: logs.slice(-400),
    };
    writeFileSync(resolve(OUT, 'curve.json'), JSON.stringify(result, null, 2));

    const csvLines = ['stall_ms,underrun_pct,events_per_sec,worst_gap_ms,min_ring_ms,raf_p99_ms,raf_max_ms,injected_count,inj_busy_ms'];
    for (const r of rows)
        csvLines.push([r.stallMs, r.underrunPct, r.eventsPerSec, r.worstGapMs, r.minRingMs, r.rafP99Ms, r.rafMaxMs, r.injCount, r.injBusyMs].join(','));
    writeFileSync(resolve(OUT, 'curve.csv'), csvLines.join('\n') + '\n');

    console.log(`[bench] curve -> ${OUT}/curve.json  +  ${OUT}/curve.csv`);
    process.exit(0);
} catch (e) {
    console.error('[bench] ERROR:', e.message);
    writeFileSync(resolve(OUT, 'error.json'), JSON.stringify({ error: e.message, stack: e.stack, logs: logs.slice(-200) }, null, 2));
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), sleep(3000)]); } catch {}
    }
}
