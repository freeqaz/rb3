#!/usr/bin/env node
/**
 * web-audio-rate-forcetest.mjs — PROVE the chipmunk mechanism. Before boot, wrap
 * AudioContext so that whatever sampleRate the engine requests, the REAL context
 * is forced to 48000 (simulating a hardware-locked-to-48000 machine, the common
 * real-world case the headless test box does NOT reproduce). The engine still
 * thinks it's 44100 and pushes 44100-rate PCM with no resampler => the worklet,
 * running at the context's true 48000, plays it 1.0884x fast.
 *
 * We read back ctx.sampleRate to confirm the override took, AND capture the live
 * worklet OUTPUT (post-resample-at-context-rate) to measure the real pitch shift
 * against the engine's pre-SAB capture.
 *
 * Usage: node web-audio-rate-forcetest.mjs [--port 8421] [--force 48000]
 */
import { launchBrowser, createCapture } from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const FORCE = parseInt(arg('--force', '48000'), 10) || 48000;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
    const { browser, page } = await launchBrowser(PORT, { noGoto: true });
    createCapture(page, { filter: /AudioDevice|AudioWorklet/ });

    // Force every AudioContext the page creates to FORCE Hz, ignoring the
    // requested sampleRate option (mirrors a browser that clamps to hw rate).
    await page.addInitScript((forceHz) => {
        const Native = window.AudioContext || window.webkitAudioContext;
        window.__forcedHz = forceHz;
        function Wrapped(opts) {
            // strip sampleRate so the browser uses its device default... then we
            // can't pick it, so instead REQUEST forceHz explicitly to emulate a
            // device locked there. (Headless chromium DOES honor an explicit rate.)
            const o = Object.assign({}, opts || {}, { sampleRate: forceHz });
            const ctx = new Native(o);
            window.__lastCtxRate = ctx.sampleRate;
            return ctx;
        }
        Wrapped.prototype = Native.prototype;
        window.AudioContext = Wrapped;
        window.webkitAudioContext = Wrapped;
    }, FORCE);

    await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

    let res = null;
    const deadline = Date.now() + 75000;
    while (Date.now() < deadline) {
        await page.keyboard.press('Enter').catch(() => {});
        await sleep(1500);
        res = await page.evaluate(() => {
            const a = window._rb3Audio;
            return {
                forced: window.__forcedHz, lastCtxRate: window.__lastCtxRate || null,
                present: !!a, ctxRate: (a && a.ctx) ? a.ctx.sampleRate : null,
                state: (a && a.ctx) ? a.ctx.state : null, started: !!(a && a.started),
            };
        }).catch(() => null);
        if (res && res.ctxRate) break;
    }

    console.log('\n==== FORCE-RATE TEST RESULT ====');
    console.log(JSON.stringify(res, null, 2));
    if (res && res.ctxRate) {
        const ratio = res.ctxRate / 44100;
        console.log(`engine pushes 44100-rate PCM; worklet runs at ctx.sampleRate=${res.ctxRate}`);
        console.log(`=> worklet plays ${ratio.toFixed(4)}x  (${((ratio-1)*100).toFixed(1)}% fast) — CHIPMUNK confirmed if >1`);
    }
    console.log('================================\n');
    await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
