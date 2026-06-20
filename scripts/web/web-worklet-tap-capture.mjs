#!/usr/bin/env node
/**
 * web-worklet-tap-capture.mjs — capture the ACTUAL AudioWorklet OUTPUT (post-mix,
 * post-limiter, what hits the speaker) to a WAV, for off-main verification.
 *
 * The C-side rb3CaptureAudio() records MixSources/sOutBuffer — which, in OFF-MAIN
 * mode (RB3_WEB_OFFMAIN_MIX=1), only carries SFX (music is mixed on the audio
 * thread inside the worklet). To verify the worklet's MUSIC output we must tap
 * the AudioContext graph directly: worklet node -> ScriptProcessorNode -> raw
 * float PCM accumulator -> WAV. ScriptProcessorNode is deprecated but works
 * headless and yields exact float samples (no opus/webm lossy re-encode).
 *
 * Usage:
 *   node web-worklet-tap-capture.mjs --port 8663 --env RB3_WEB_OFFMAIN_MIX=1 \
 *        --song 20thcenturyboy --secs 20 --out /tmp/rb3_worklet_offmain.wav
 */
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const ENV_PARAM = arg('--env', '');
const SONG = arg('--song', '20thcenturyboy');
const SECS = parseInt(arg('--secs', '20'), 10) || 20;
const OUT = arg('--out', '/tmp/rb3_worklet_capture.wav');
const BUILD = arg('--build', 'debug');
let URL_SUFFIX = BUILD === 'release' ? '/' : '/?debug=true';
if (ENV_PARAM) URL_SUFFIX += (URL_SUFFIX.includes('?') ? '&' : '?') + 'env=' + encodeURIComponent(ENV_PARAM);

const sleep = (ms) => new Promise(r => setTimeout(r, ms));
function waitForServer(port, timeoutMs = 20000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => http.get(`http://127.0.0.1:${port}/api/health`, r => {
            if (r.statusCode === 200) return res(); retry();
        }).on('error', retry);
        const retry = () => Date.now() > deadline ? rej(new Error('no server')) : setTimeout(check, 300);
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
        await sleep(250);
    }
    return s;
}

// Page-side: install the worklet output tap once the AudioContext + worklet node
// exist. We poll for window._rb3Audio.worklet and wire a ScriptProcessor tap.
function installTap() {
    window.__tap = { ready: false, sampleRate: 0, frames: 0, chunks: [] /* [Float32Array L,R interleaved] */ };
    const tryWire = () => {
        const a = window._rb3Audio;
        if (!a || !a.ctx || !a.worklet) { setTimeout(tryWire, 200); return; }
        try {
            const ctx = a.ctx;
            // ScriptProcessor tap: 2-in (from worklet), 2-out (to nowhere / dummy).
            const sp = ctx.createScriptProcessor(4096, 2, 2);
            sp.onaudioprocess = (ev) => {
                if (!window.__tap.recording) {
                    // pass-through silence to keep the graph alive without recording.
                    return;
                }
                const inL = ev.inputBuffer.getChannelData(0);
                const inR = ev.inputBuffer.getChannelData(1);
                const n = inL.length;
                const inter = new Float32Array(n * 2);
                for (let i = 0; i < n; i++) { inter[i * 2] = inL[i]; inter[i * 2 + 1] = inR[i]; }
                window.__tap.chunks.push(inter);
                window.__tap.frames += n;
            };
            // worklet -> tap -> destination (tap must reach a sink to pump).
            a.worklet.connect(sp);
            sp.connect(ctx.destination);
            window.__tap.sampleRate = ctx.sampleRate;
            window.__tap.ready = true;
            window.__tap.recording = false;
            console.log('TAP: wired worklet -> ScriptProcessor @' + ctx.sampleRate + 'Hz');
        } catch (e) { console.error('TAP wire failed: ' + e); setTimeout(tryWire, 500); }
    };
    tryWire();
}

function buildWav(interleavedChunks, sampleRate) {
    let total = 0;
    for (const c of interleavedChunks) total += c.length;
    const numSamples = total; // interleaved stereo float count
    const dataSize = numSamples * 2; // int16
    const buf = Buffer.alloc(44 + dataSize);
    buf.write('RIFF', 0); buf.writeUInt32LE(36 + dataSize, 4); buf.write('WAVE', 8);
    buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20);
    buf.writeUInt16LE(2, 22); buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34);
    buf.write('data', 36); buf.writeUInt32LE(dataSize, 40);
    let off = 44;
    for (const c of interleavedChunks) {
        for (let i = 0; i < c.length; i++) {
            let s = c[i]; if (s > 1) s = 1; if (s < -1) s = -1;
            buf.writeInt16LE((s * 32767) | 0, off); off += 2;
        }
    }
    return buf;
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
    page.on('console', m => { const t = m.text(); if (/TAP|OFF-MAIN|OFFMAIN|AudioWorklet|underrun|alloc failed|SharedArrayBuffer|RangeError|enlarge|out of memory/i.test(t)) console.log('  [page] ' + t.slice(0, 220)); });
    page.on('pageerror', e => console.log('  [PAGEERROR] ' + e.message));
    await page.addInitScript(installTap);

    console.log(`[tap] loading ${URL_SUFFIX}`);
    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log('[tap] booted');

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
    console.log('[tap] launching song -> game_screen...');
    await page.evaluate((song) => { window.rb3WebUseAids = 1; window.rb3WebTargetSong = song; }, SONG).catch(() => {});
    await pressKey(page, 'Enter', 220);
    s = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 30000 });
    await sleep(2500);
    for (let i = 0; i < 6; i++) { await pressKey(page, 'Enter', 150); await sleep(1200); const cur = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => ''); if (cur === 'game_screen') { s = cur; break; } }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    if (s !== 'game_screen') throw new Error(`never reached game_screen (stuck at '${s}')`);
    console.log('[tap] game_screen — stabilizing 6s then recording ' + SECS + 's');
    await sleep(6000);

    // Start recording the worklet output.
    const wired = await page.evaluate(() => window.__tap && window.__tap.ready);
    if (!wired) { await sleep(3000); }
    await page.evaluate(() => { window.__tap.chunks = []; window.__tap.frames = 0; window.__tap.recording = true; });
    await sleep(SECS * 1000);
    const cap = await page.evaluate(() => {
        window.__tap.recording = false;
        // serialize chunks as a flat array of numbers (small enough for SECS<=25).
        const flat = [];
        for (const c of window.__tap.chunks) for (let i = 0; i < c.length; i++) flat.push(c[i]);
        return { sampleRate: window.__tap.sampleRate, frames: window.__tap.frames, data: flat };
    });
    console.log(`[tap] captured ${cap.frames} stereo frames @${cap.sampleRate}Hz (${(cap.frames / cap.sampleRate).toFixed(1)}s)`);
    if (!cap.frames) throw new Error('captured 0 frames — tap did not record (worklet silent or tap not wired)');
    const wav = buildWav([Float32Array.from(cap.data)], cap.sampleRate);
    writeFileSync(OUT, wav);
    console.log(`[tap] wrote ${OUT} (${(wav.length / 1e6).toFixed(1)} MB)`);
    process.exit(0);
} catch (e) {
    console.error('[tap] ERROR:', e.message);
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
