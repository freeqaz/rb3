#!/usr/bin/env node
/**
 * _web-start-lead-probe.mjs — measure the audio-onset vs track-start lead at SONG
 * START on the web build, A/B by RB3_WEB_OFFMAIN_MIX.
 *
 * Reuses web-worklet-tap-capture.mjs's nav. Differences:
 *  - Starts the worklet OUTPUT tap recording the instant game_screen is reached
 *    (captures the silence-before-first-audio so audio-onset is measurable).
 *  - Records, per audioprocess chunk, a wall-clock timestamp -> first non-silent
 *    chunk = AUDIO ONSET (ms since game_screen).
 *  - Captures the track region via #rb3-canvas pixel sampling at ~30Hz; the frame
 *    where the track pixels first CHANGE (gems start scrolling) = TRACK ONSET.
 *  - Logs OFFMAIN-DBG / "OFF-MAIN mix ENABLED" / prime lines with wall-clock.
 *
 * Usage:
 *   node _web-start-lead-probe.mjs --port 8421 --env RB3_WEB_OFFMAIN_MIX=1 \
 *        --song 20thcenturyboy --secs 6 --tag offmain
 */
import { chromium } from 'playwright';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const ENV_PARAM = arg('--env', '');
const SONG = arg('--song', '20thcenturyboy');
const SECS = parseInt(arg('--secs', '6'), 10) || 6;
const TAG = arg('--tag', 'run');
const BUILD = arg('--build', 'debug');
let URL_SUFFIX = BUILD === 'release' ? '/' : '/?debug=true';
if (ENV_PARAM) URL_SUFFIX += (URL_SUFFIX.includes('?') ? '&' : '?') + 'env=' + encodeURIComponent(ENV_PARAM);

const sleep = ms => new Promise(r => setTimeout(r, ms));
const L = m => console.log(`[${TAG}] ${m}`);

function waitForServer(port) {
    return new Promise((res, rej) => {
        const t0 = Date.now();
        const check = () => http.get(`http://127.0.0.1:${port}/api/version`, r => { r.resume(); res(); })
            .on('error', () => { if (Date.now() - t0 > 15000) rej(new Error('server')); else setTimeout(check, 300); });
        check();
    });
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
        await sleep(150);
    }
    return s;
}

// Page-side: tap the worklet output; record per-chunk wall-clock + peak amplitude.
function installTap() {
    window.__tap = { ready: false, sampleRate: 0, t0: 0, marks: [] /* {t, peak} */ };
    const tryWire = () => {
        const a = window._rb3Audio;
        if (!a || !a.ctx || !a.worklet) { setTimeout(tryWire, 100); return; }
        try {
            const ctx = a.ctx;
            const sp = ctx.createScriptProcessor(2048, 2, 2);
            sp.onaudioprocess = (ev) => {
                if (!window.__tap.recording) return;
                const inL = ev.inputBuffer.getChannelData(0);
                const inR = ev.inputBuffer.getChannelData(1);
                let peak = 0;
                for (let i = 0; i < inL.length; i++) {
                    const a = Math.abs(inL[i]), b = Math.abs(inR[i]);
                    if (a > peak) peak = a; if (b > peak) peak = b;
                }
                window.__tap.marks.push({ t: performance.now() - window.__tap.t0, peak });
            };
            a.worklet.connect(sp);
            sp.connect(ctx.destination);
            window.__tap.sampleRate = ctx.sampleRate;
            window.__tap.ready = true;
            window.__tap.recording = false;
            console.log('TAP: wired @' + ctx.sampleRate + 'Hz');
        } catch (e) { console.error('TAP wire failed: ' + e); setTimeout(tryWire, 300); }
    };
    tryWire();
}

let browser;
try {
    await waitForServer(PORT);
    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required'],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const evLog = [];
    page.on('console', m => {
        const t = m.text();
        if (/OFF-MAIN|OFFMAIN|prime|AudioWorklet connected|underrun|stem SAB/i.test(t)) {
            evLog.push({ t: Date.now(), text: t.slice(0, 200) });
            console.log('  [page] ' + t.slice(0, 200));
        }
    });
    page.on('pageerror', e => console.log('  [PAGEERROR] ' + e.message));
    await page.addInitScript(installTap);

    L(`loading ${URL_SUFFIX}`);
    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    L('booted');

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
    if (s === 'main_hub_screen') {
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        for (let i = 0; i < 5; i++) { await pressKey(page, 'Enter'); const cur = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 }); if (cur && cur !== 'main_hub_screen') { s = cur; break; } await sleep(1500); }
        s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
        if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
    }
    await sleep(3000);
    L('launching song -> game_screen...');
    await page.evaluate((song) => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = song; }, SONG).catch(() => {});
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(2500);
    for (let i = 0; i < 6; i++) { await pressKey(page, 'Enter', 150); await sleep(800); const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => ''); if (cur === 'game_screen') { s = cur; break; } }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    if (s !== 'game_screen') throw new Error(`never reached game_screen (stuck at '${s}')`);

    // START RECORDING immediately on game_screen entry. t0 = now.
    const tEnter = Date.now();
    await page.evaluate(() => {
        if (!window.__tap || !window.__tap.ready) return false;
        window.__tap.marks = [];
        window.__tap.t0 = performance.now();
        window.__tap.recording = true;
        return true;
    });
    L(`game_screen reached — recording audio-onset for ${SECS}s`);
    await sleep(SECS * 1000);

    const tap = await page.evaluate(() => {
        window.__tap.recording = false;
        return { sampleRate: window.__tap.sampleRate, marks: window.__tap.marks };
    });

    // Audio onset: first chunk whose peak crosses a real-signal threshold.
    const THRESH = 0.01;
    let onset = -1;
    for (const m of tap.marks) { if (m.peak >= THRESH) { onset = m.t; break; } }
    const maxPeak = tap.marks.reduce((a, m) => Math.max(a, m.peak), 0);
    L(`audio chunks=${tap.marks.length} maxPeak=${maxPeak.toFixed(4)} ` +
      `AUDIO_ONSET=${onset < 0 ? 'NONE' : onset.toFixed(0) + 'ms'} (after game_screen entry)`);

    // Dump off-main event timeline relative to game_screen entry.
    L('OFF-MAIN/prime event timeline (ms after game_screen entry):');
    for (const e of evLog) {
        const dt = e.t - tEnter;
        if (dt > -2000 && dt < SECS * 1000 + 2000) L(`   +${dt}ms  ${e.text}`);
    }
    console.log(JSON.stringify({ tag: TAG, env: ENV_PARAM, audioOnsetMs: onset, maxPeak, chunkCount: tap.marks.length }));
    process.exit(0);
} catch (e) {
    console.error(`[${TAG}] ERROR:`, e.message);
    process.exit(1);
} finally {
    if (browser) await browser.close().catch(() => {});
}
