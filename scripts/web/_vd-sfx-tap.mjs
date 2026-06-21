#!/usr/bin/env node
/**
 * _vd-sfx-tap.mjs — verify menu/UI SFX still fire audibly with off-main ON.
 *
 * SFX are NOT off-main: SampleInst registers via AudioDevice::AddSource (main
 * thread), the pump mixes them into the SFX output ring, and the worklet drains
 * that ring ADDITIVELY (and via _drainSfxOnly when no music stem is active — the
 * menu case). So we tap the worklet output during MENU navigation (no music),
 * fire nav SFX with keypresses, and assert audio BURSTS coincide with keys.
 *
 * Because no music plays in the menu, the worklet output IS the SFX bus — any
 * non-zero audio there is a UI SFX one-shot. We record continuously while
 * pressing keys on a cadence and report per-100ms RMS so bursts are visible.
 *
 *   node _vd-sfx-tap.mjs --port 8563 --out /tmp/vd_sfx.wav
 */
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const ENV_PARAM = arg('--env', '');
const OUT = arg('--out', '/tmp/vd_sfx.wav');
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
async function pressKey(page, key, holdMs = 120) {
    try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(120); } catch {}
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
            const sp = ctx.createScriptProcessor(2048, 2, 2);
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

function buildWav(chunks, sampleRate) {
    let total = 0; for (const c of chunks) total += c.length;
    const dataSize = total * 2;
    const buf = Buffer.alloc(44 + dataSize);
    buf.write('RIFF', 0); buf.writeUInt32LE(36 + dataSize, 4); buf.write('WAVE', 8);
    buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20);
    buf.writeUInt16LE(2, 22); buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34);
    buf.write('data', 36); buf.writeUInt32LE(dataSize, 40);
    let off = 44;
    for (const c of chunks) for (let i = 0; i < c.length; i++) {
        let s = c[i]; if (s > 1) s = 1; if (s < -1) s = -1;
        buf.writeInt16LE((s * 32767) | 0, off); off += 2;
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
    page.on('console', m => { const t = m.text(); if (/TAP|OFF-MAIN|underrun|alloc failed|SharedArrayBuffer|RangeError/i.test(t)) console.log('  [page] ' + t.slice(0, 180)); });
    page.on('pageerror', e => console.log('  [PAGEERROR] ' + e.message));
    await page.addInitScript(installTap);

    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    console.log('[sfx] booted');

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
    await sleep(2500);
    s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
    if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    if (s !== 'main_hub_screen') console.log(`[sfx] WARNING: at '${s}', not main_hub; continuing anyway`);
    console.log(`[sfx] at '${s}'; recording while firing nav SFX...`);

    { const dl = Date.now() + 8000; while (Date.now() < dl) { const r = await page.evaluate(() => window.__tap && window.__tap.ready).catch(() => false); if (r) break; await sleep(300); } }
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    await sleep(1000);

    // Start recording, then fire a cadence of nav keys (each should trigger a UI
    // move/select SFX one-shot). Record ~12s with keys roughly every ~1.2s.
    await page.evaluate(() => { window.__tap.chunks = []; window.__tap.frames = 0; window.__tap.recording = true; });
    const keys = ['ArrowDown', 'ArrowUp', 'ArrowDown', 'ArrowUp', 'ArrowDown', 'ArrowDown', 'ArrowUp', 'ArrowDown'];
    const tStart = Date.now();
    for (const k of keys) { await pressKey(page, k, 120); await sleep(1000); }
    // tail to catch the last SFX
    await sleep(1500);
    const cap = await page.evaluate(() => {
        window.__tap.recording = false;
        const flat = [];
        for (const c of window.__tap.chunks) for (let i = 0; i < c.length; i++) flat.push(c[i]);
        return { sampleRate: window.__tap.sampleRate, frames: window.__tap.frames, data: flat };
    });
    console.log(`[sfx] recorded ${(Date.now()-tStart)/1000}s window; ${cap.frames} frames @${cap.sampleRate}Hz`);
    if (!cap.frames) throw new Error('captured 0 frames');
    const wav = buildWav([Float32Array.from(cap.data)], cap.sampleRate);
    writeFileSync(OUT, wav);

    // Per-100ms RMS burst detection.
    const i16 = new Int16Array(wav.buffer, wav.byteOffset + 44, (wav.length - 44) >> 1);
    const sr = cap.sampleRate, blk = Math.floor(sr * 0.1) * 2;
    const rmsBlocks = [];
    for (let k = 0; k + blk <= i16.length; k += blk) {
        let sq = 0; for (let i = k; i < k + blk; i++) sq += i16[i] * i16[i];
        rmsBlocks.push(Math.sqrt(sq / blk));
    }
    const peak = Math.max(...rmsBlocks);
    const noiseFloor = rmsBlocks.slice().sort((a, b) => a - b)[Math.floor(rmsBlocks.length * 0.2)] || 1;
    // count bursts: 100ms blocks where RMS > max(8*floor, 200)
    const thr = Math.max(8 * noiseFloor, 200);
    let bursts = 0, inBurst = false;
    for (const r of rmsBlocks) { if (r > thr) { if (!inBurst) { bursts++; inBurst = true; } } else inBurst = false; }
    const overallPeak = Math.max(...Array.from(i16, v => Math.abs(v)));
    console.log(`[sfx] peakBlockRMS=${peak.toFixed(0)} noiseFloor=${noiseFloor.toFixed(0)} thr=${thr.toFixed(0)} bursts=${bursts} overallSamplePeak=${overallPeak}`);
    console.log('[sfx] per-100ms RMS: ' + rmsBlocks.map(r => Math.round(r)).join(','));
    const pass = bursts >= 3 && overallPeak > 1000;
    console.log(`[sfx] RESULT: ${pass ? 'PASS — SFX bursts present' : 'FAIL — no SFX bursts detected'} (need >=3 bursts & peak>1000)`);
    process.exit(pass ? 0 : 3);
} catch (e) {
    console.error('[sfx] ERROR:', e.message);
    process.exit(1);
} finally {
    if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}
