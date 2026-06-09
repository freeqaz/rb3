#!/usr/bin/env node
/**
 * web-audio-rate-probe.mjs — read back the ACTUAL AudioContext sample rate the
 * browser gave us, vs the 44100 the engine requested. If ctx.sampleRate=48000
 * while the engine pushes 44100-rate PCM into the SAB with NO resampler, the
 * worklet plays it 48000/44100 = 1.0884x fast (the chipmunk bug).
 *
 * Usage: node web-audio-rate-probe.mjs [--port 8421] [--wait 60]
 */
import { launchBrowser, createCapture } from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const WAIT = parseInt(arg('--wait', '70'), 10) || 70;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
    const { browser, page } = await launchBrowser(PORT);
    const cap = createCapture(page, { filter: /AudioDevice|AudioWorklet|AudioContext/ });

    // Press keys periodically to satisfy the autoplay gesture + drive boot.
    let result = null;
    const deadline = Date.now() + WAIT * 1000;
    while (Date.now() < deadline) {
        await page.keyboard.press('Enter').catch(() => {});
        await sleep(1500);
        result = await page.evaluate(() => {
            const a = window._rb3Audio;
            if (!a) return { present: false };
            return {
                present: true,
                hasCtx: !!a.ctx,
                ctxSampleRate: a.ctx ? a.ctx.sampleRate : null,
                ctxState: a.ctx ? a.ctx.state : null,
                started: !!a.started,
                bufFrames: a.bufFrames || null,
                // baseLatency + outputLatency give extra context
                baseLatency: a.ctx ? a.ctx.baseLatency : null,
            };
        }).catch(() => null);
        if (result && result.present && result.hasCtx && result.ctxSampleRate) {
            console.log(`[probe] _rb3Audio present, ctx.sampleRate=${result.ctxSampleRate} state=${result.ctxState} started=${result.started}`);
            break;
        }
    }

    console.log('\n==== AUDIO RATE PROBE RESULT ====');
    console.log(JSON.stringify(result, null, 2));
    if (result && result.ctxSampleRate) {
        const r = result.ctxSampleRate / 44100;
        console.log(`ENGINE_REQUESTED=44100  BROWSER_ACTUAL=${result.ctxSampleRate}`);
        console.log(`PLAYBACK_RATIO (no-resampler) = ${result.ctxSampleRate}/44100 = ${r.toFixed(4)}`);
        console.log(r > 1.005 ? `=> CHIPMUNK: plays ${((r-1)*100).toFixed(1)}% too fast` :
                    r < 0.995 ? `=> too SLOW` : `=> CORRECT (rates match)`);
    } else {
        console.log('FAILED to read ctx.sampleRate (audio not initialized within wait window)');
    }
    console.log('=================================\n');

    await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
