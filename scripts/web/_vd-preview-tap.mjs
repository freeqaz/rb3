#!/usr/bin/env node
/**
 * _vd-preview-tap.mjs — capture the AudioWorklet OUTPUT during SONG-SELECT
 * PREVIEW (not gameplay), for off-main validation.
 *
 * With RB3_WEB_OFFMAIN_MIX default-ON, the preview mogg streams through the SAME
 * StreamReceiver factory as gameplay, so its music is mixed OFF-MAIN inside the
 * worklet (the C-side rb3CaptureAudio()/sOutBuffer captures ONLY SFX in that
 * mode). To verify the preview MUSIC we must tap the AudioContext graph directly,
 * exactly like web-worklet-tap-capture.mjs, but stop at song_select and let the
 * SongPreview fire (scroll onto a couple of songs, hold, record).
 *
 *   node _vd-preview-tap.mjs --port 8563 --secs 18 --out /tmp/vd_preview.wav
 */
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const ENV_PARAM = arg('--env', '');
const SECS = parseInt(arg('--secs', '18'), 10) || 18;
const OUT = arg('--out', '/tmp/vd_preview.wav');
let URL_SUFFIX = '/?debug=true';
if (ENV_PARAM) URL_SUFFIX += '&env=' + encodeURIComponent(ENV_PARAM);

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

function installTap() {
    window.__tap = { ready: false, sampleRate: 0, frames: 0, chunks: [] };
    const tryWire = () => {
        const a = window._rb3Audio;
        if (!a || !a.ctx || !a.worklet) { setTimeout(tryWire, 200); return; }
        try {
            const ctx = a.ctx;
            const sp = ctx.createScriptProcessor(4096, 2, 2);
            sp.onaudioprocess = (ev) => {
                if (!window.__tap.recording) return;
                const inL = ev.inputBuffer.getChannelData(0);
                const inR = ev.inputBuffer.getChannelData(1);
                const n = inL.length;
                const inter = new Float32Array(n * 2);
                for (let i = 0; i < n; i++) { inter[i * 2] = inL[i]; inter[i * 2 + 1] = inR[i]; }
                window.__tap.chunks.push(inter);
                window.__tap.frames += n;
            };
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
    const dataSize = total * 2;
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
    page.on('console', m => { const t = m.text(); if (/TAP|OFF-MAIN|OFFMAIN|Preview|preview|underrun|alloc failed|SharedArrayBuffer|RangeError|out of memory/i.test(t)) console.log('  [page] ' + t.slice(0, 200)); });
    page.on('pageerror', e => console.log('  [PAGEERROR] ' + e.message));
    await page.addInitScript(installTap);

    console.log(`[preview] loading ${URL_SUFFIX}`);
    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});

    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log('[preview] booted');

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
    if (s !== 'song_select_screen') throw new Error(`never reached song_select (stuck at '${s}')`);
    console.log('[preview] song_select reached; scrolling onto songs to fire SongPreview...');
    await sleep(2500);

    // Make sure the tap is wired before we start.
    { const dl = Date.now() + 8000; while (Date.now() < dl) { const r = await page.evaluate(() => window.__tap && window.__tap.ready).catch(() => false); if (r) break; await sleep(300); } }

    // Scroll the highlight onto a couple of songs (preview fires on hover/dwell),
    // then dwell to let the preview start streaming.
    await pressKey(page, 'ArrowDown', 160);
    await sleep(1500);
    await pressKey(page, 'ArrowDown', 160);
    console.log('[preview] dwelling ~6s for preview to start + stream');
    await sleep(6000);

    // Record.
    await page.evaluate(() => { window.__tap.chunks = []; window.__tap.frames = 0; window.__tap.recording = true; });
    await sleep(SECS * 1000);
    const cap = await page.evaluate(() => {
        window.__tap.recording = false;
        const flat = [];
        for (const c of window.__tap.chunks) for (let i = 0; i < c.length; i++) flat.push(c[i]);
        return { sampleRate: window.__tap.sampleRate, frames: window.__tap.frames, data: flat };
    });
    console.log(`[preview] captured ${cap.frames} stereo frames @${cap.sampleRate}Hz (${(cap.frames / cap.sampleRate).toFixed(1)}s)`);
    if (!cap.frames) throw new Error('captured 0 frames — tap did not record');
    const wav = buildWav([Float32Array.from(cap.data)], cap.sampleRate);
    writeFileSync(OUT, wav);

    // Quick on-board RMS/peak so we can flag silence even without a reference.
    const i16 = new Int16Array(wav.buffer, wav.byteOffset + 44, (wav.length - 44) >> 1);
    let peak = 0, nz = 0, sq = 0;
    for (let i = 0; i < i16.length; i++) { const a = Math.abs(i16[i]); if (a > peak) peak = a; if (a > 64) nz++; sq += i16[i] * i16[i]; }
    const rms = Math.sqrt(sq / i16.length);
    console.log(`[preview] wrote ${OUT}  peak=${peak} nonZero=${(100*nz/i16.length).toFixed(1)}% RMS=${rms.toFixed(0)}`);
    const audible = peak > 1500 && (100*nz/i16.length) > 10;
    console.log(`[preview] AUDIBLE=${audible} (need peak>1500 & nonZero>10%)`);
    process.exit(audible ? 0 : 3);
} catch (e) {
    console.error('[preview] ERROR:', e.message);
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
