#!/usr/bin/env node
/**
 * _audio_early_probe.mjs — reproduce the "audio starts early during the loading
 * animation" bug on the WEB build. Injects GAME_DBG=1 (via window.__rb3ExtraEnv,
 * which the C++ boot drain reads directly) so the browser console emits the same
 *   "Game::Poll songMs=.. audioTime=.. streamPlaying=.." / "Game::Go()" trace
 * we captured on native, giving an apples-to-apples onset comparison.
 *
 * Also taps the AudioWorklet output for the first audible chunk (real audio
 * onset) relative to game_screen entry.
 *
 * Usage: node _audio_early_probe.mjs --port 8455 --song antibodies --secs 22
 */
import { chromium } from 'playwright';
import http from 'http';
import { writeFileSync, appendFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8455'), 10) || 8455;
const SONG = arg('--song', 'antibodies');
const SECS = parseInt(arg('--secs', '22'), 10) || 22;
const BUILD = arg('--build', 'debug');
const WAVOUT = arg('--wav', '');          // if set, write worklet-output PCM to this WAV
const THROTTLE = parseFloat(arg('--throttle', '1')) || 1;  // CDP CPU throttle rate
const EXTRA_ENV = arg('--env', '');   // e.g. "RB3_WEB_OFFMAIN_MIX=0"
const URL_SUFFIX = BUILD === 'release' ? '/' : '/?debug=true';

const LIVE = argv.includes('--live');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const L = m => console.log(`[early] ${m}`);

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
async function curScreen(page) { return await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => ''); }
async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) {
    const deadline = Date.now() + timeoutMs; let s = '';
    while (Date.now() < deadline) {
        s = await curScreen(page);
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await sleep(150);
    }
    return s;
}

// Page-side worklet output tap (first audible chunk = real audio onset).
function installTap() {
    // Force GAME_DBG on before the wasm boot drain reads window.__rb3ExtraEnv.
    window.__rb3ExtraEnv = Object.assign(window.__rb3ExtraEnv || {}, { GAME_DBG: '1' });
    window.__tap = { ready: false, t0: 0, recording: false, marks: [], sr: 0, pcmL: [], pcmR: [], maxFrames: 0, gotFrames: 0 };
    const tryWire = () => {
        const a = window._rb3Audio;
        if (!a || !a.ctx || !a.worklet) { setTimeout(tryWire, 100); return; }
        try {
            const ctx = a.ctx;
            const sp = ctx.createScriptProcessor(2048, 2, 2);
            window.__tap.sr = ctx.sampleRate;
            window.__tap.maxFrames = ctx.sampleRate * 45; // cap ~45s
            sp.onaudioprocess = (ev) => {
                if (!window.__tap.recording) return;
                const inL = ev.inputBuffer.getChannelData(0), inR = ev.inputBuffer.getChannelData(1);
                let peak = 0;
                for (let i = 0; i < inL.length; i++) { const x = Math.abs(inL[i]), y = Math.abs(inR[i]); if (x > peak) peak = x; if (y > peak) peak = y; }
                window.__tap.marks.push({ t: performance.now() - window.__tap.t0, peak });
                if (window.__tap.gotFrames < window.__tap.maxFrames) {
                    window.__tap.pcmL.push(new Float32Array(inL)); window.__tap.pcmR.push(new Float32Array(inR));
                    window.__tap.gotFrames += inL.length;
                }
            };
            a.worklet.connect(sp); sp.connect(ctx.destination);
            window.__tap.ready = true;
            console.log('TAP: wired @' + ctx.sampleRate + 'Hz');
        } catch (e) { setTimeout(tryWire, 300); }
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
    const cdp = await ctx.newCDPSession(page);

    // Capture the GAME_DBG onset trace + audio/stream markers with wall-clock.
    const dbg = [];       // {t, text}
    let tGameScreen = 0;
    const CONSOLE_FILE = arg('--console', '');
    page.on('console', m => {
        const t = m.text();
        if (CONSOLE_FILE) { try { appendFileSync(CONSOLE_FILE, t.slice(0, 300) + '\n'); } catch {} }
        if (/abort|assert|Assert|RuntimeError|MILO_|bad state|unreachable|exception|Aborted/i.test(t)) {
            console.log('  [CRASH?] ' + t.slice(0, 300));
        }
        if (/PollStream state/i.test(t)) return;  // drop reader-pump spam
        if (/GAME_DBG|BEATMASTER_DBG|MasterAudio|Go\(\)|SetIntroRealTime|StartGame|streamPlaying|OFF-MAIN|prime|underrun|Play\(\)|RB3STREAM|PlayImpl|RegisterMusicStem|SeedStem/i.test(t)) {
            dbg.push({ t: Date.now(), text: t.slice(0, 260) });
            if (LIVE && /Play\(\) ENTER|RB3STREAM|Game::Go/i.test(t)) console.log('  [live] ' + t.slice(0, 200));
        }
    });
    page.on('pageerror', e => console.log('  [PAGEERROR] ' + e.message.slice(0, 200)));
    page.on('crash', () => console.log('  [PAGE CRASHED]'));
    await page.addInitScript(installTap);
    if (EXTRA_ENV) await page.addInitScript((e) => {
        const o = {}; e.split(';').forEach(p => { const i = p.indexOf('='); if (i > 0) o[p.slice(0, i)] = p.slice(i + 1); });
        window.__rb3ExtraEnv = Object.assign(window.__rb3ExtraEnv || {}, o);
    }, EXTRA_ENV);

    L(`loading ${URL_SUFFIX}  song=${SONG} secs=${SECS} env='${EXTRA_ENV}'`);
    await page.goto(`http://127.0.0.1:${PORT}${URL_SUFFIX}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await sleep(1000);
    await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
    { const dl = Date.now() + 300000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
    L('booted');

    let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
    for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) { await pressKey(page, 'Space', 300); s = await curScreen(page); }
    await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 });
    s = await curScreen(page); await sleep(2000);
    if (s === 'splash_screen') {
        await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        await pressKey(page, 'Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 8 && s === 'splash_screen'; i++) { await pressKey(page, 'Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); }
        if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
    }
    await sleep(3000); s = await curScreen(page);
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
    for (let i = 0; i < 6; i++) { await pressKey(page, 'Enter', 150); await sleep(800); if (await curScreen(page) === 'game_screen') break; }
    s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 120000 });
    if (s !== 'game_screen') throw new Error(`never reached game_screen (stuck at '${s}')`);

    tGameScreen = Date.now();
    // Start an rAF-gap recorder + reset the audio tap, then throttle the CPU to
    // force the documented song-start main-thread stall (venue reveal / texture
    // drains) so the off-main audio can run ahead of rendering.
    await page.evaluate(() => {
        window.__raf = { t0: performance.now(), stamps: [] };
        const loop = () => { window.__raf.stamps.push(performance.now() - window.__raf.t0); requestAnimationFrame(loop); };
        requestAnimationFrame(loop);
        if (window.__tap && window.__tap.ready) { window.__tap.marks = []; window.__tap.t0 = performance.now(); window.__tap.recording = true; }
    });
    if (THROTTLE > 1) { await cdp.send('Emulation.setCPUThrottlingRate', { rate: THROTTLE }); L(`CPU throttle x${THROTTLE} applied at game_screen entry`); }
    L(`game_screen reached — capturing onset trace for ${SECS}s`);
    await sleep(SECS * 1000);
    if (THROTTLE > 1) await cdp.send('Emulation.setCPUThrottlingRate', { rate: 1 }).catch(() => {});

    const tap = await page.evaluate(() => { if (window.__tap) window.__tap.recording = false; return window.__tap ? { marks: window.__tap.marks, ready: window.__tap.ready } : { marks: [], ready: false }; });
    const raf = await page.evaluate(() => window.__raf ? window.__raf.stamps : []);
    // rAF gaps > 250ms = main-thread render stalls.
    const gaps = [];
    for (let i = 1; i < raf.length; i++) { const g = raf[i] - raf[i - 1]; if (g > 250) gaps.push({ at: Math.round(raf[i - 1]), gap: Math.round(g) }); }
    const audiblePeakInWindow = (a, b) => { let pk = 0; for (const m of tap.marks) if (m.t >= a && m.t <= b && m.peak > pk) pk = m.peak; return pk; };
    const THRESH = 0.01;
    let audioOnset = -1;
    for (const m of tap.marks) if (m.peak >= THRESH) { audioOnset = m.t; break; }
    const maxPeak = tap.marks.reduce((a, m) => Math.max(a, m.peak), 0);

    console.log('\n===== GAME_DBG onset trace (t = ms after game_screen entry) =====');
    for (const e of dbg) {
        const dt = e.t - tGameScreen;
        if (dt > -12000) console.log(`  ${dt >= 0 ? '+' : ''}${dt}ms  ${e.text}`);
    }
    // Audible envelope in the first 8s (peak per ~250ms bucket) so we can see if
    // loud audio is present DURING the count-in (before Go()).
    console.log('\n===== audible envelope, first 8s (t = ms after game_screen; peak per 250ms) =====');
    const buckets = {};
    for (const m of tap.marks) { if (m.t > 8000) break; const b = Math.floor(m.t / 250) * 250; buckets[b] = Math.max(buckets[b] || 0, m.peak); }
    for (const b of Object.keys(buckets).map(Number).sort((a, x) => a - x)) {
        const pk = buckets[b]; const bar = '#'.repeat(Math.round(pk * 40));
        console.log(`  +${b}ms  ${pk.toFixed(3)} ${bar}`);
    }
    console.log('\n===== rAF render stalls (gap>250ms) after game_screen; + audio peak DURING the stall =====');
    for (const g of gaps) console.log(`  +${g.at}ms  STALL ${g.gap}ms  audioPeakDuringStall=${audiblePeakInWindow(g.at, g.at + g.gap).toFixed(3)}`);
    const biggest = gaps.reduce((a, g) => g.gap > (a ? a.gap : 0) ? g : a, null);

    // Find the Go() (clock=0) offset relative to game_screen entry (== tap t0).
    let goOffsetMs = -1;
    for (const e of dbg) if (/Game::Go\(\) ENTERED/.test(e.text)) { goOffsetMs = e.t - tGameScreen; break; }

    // Export worklet-output PCM to a 16-bit stereo WAV for reference correlation.
    if (WAVOUT) {
        const wav = await page.evaluate(() => {
            const t = window.__tap; if (!t || !t.pcmL.length) return null;
            let n = 0; for (const c of t.pcmL) n += c.length;
            const sr = t.sr | 0;
            const bytes = 44 + n * 4; // 16-bit stereo
            const buf = new Uint8Array(bytes); const dv = new DataView(buf.buffer);
            const ws = (o, s) => { for (let i = 0; i < s.length; i++) dv.setUint8(o + i, s.charCodeAt(i)); };
            ws(0, 'RIFF'); dv.setUint32(4, bytes - 8, true); ws(8, 'WAVE'); ws(12, 'fmt ');
            dv.setUint32(16, 16, true); dv.setUint16(20, 1, true); dv.setUint16(22, 2, true);
            dv.setUint32(24, sr, true); dv.setUint32(28, sr * 4, true); dv.setUint16(32, 4, true); dv.setUint16(34, 16, true);
            ws(36, 'data'); dv.setUint32(40, n * 4, true);
            let off = 44;
            for (let c = 0; c < t.pcmL.length; c++) {
                const L = t.pcmL[c], R = t.pcmR[c];
                for (let i = 0; i < L.length; i++) {
                    let l = Math.max(-1, Math.min(1, L[i])), r = Math.max(-1, Math.min(1, R[i]));
                    dv.setInt16(off, l * 32767, true); dv.setInt16(off + 2, r * 32767, true); off += 4;
                }
            }
            // base64
            let bin = ''; for (let i = 0; i < buf.length; i++) bin += String.fromCharCode(buf[i]);
            return { b64: btoa(bin), sr, frames: n };
        });
        if (wav) { writeFileSync(WAVOUT, Buffer.from(wav.b64, 'base64')); L(`WROTE ${WAVOUT}  sr=${wav.sr} frames=${wav.frames} (${(wav.frames / wav.sr).toFixed(1)}s)`); L(`  Go()/clock=0 is at +${goOffsetMs}ms into this WAV (== tap t0 + ${goOffsetMs}ms)`); }
    }

    console.log('\n===== SUMMARY =====');
    L(`throttle=x${THROTTLE} tap ready=${tap.ready} chunks=${tap.marks.length} maxPeak=${maxPeak.toFixed(4)} AUDIO_ONSET(audible)=${audioOnset < 0 ? 'NONE' : audioOnset.toFixed(0) + 'ms'}`);
    L(`rAF frames=${raf.length} stalls>250ms=${gaps.length} biggestStall=${biggest ? biggest.gap + 'ms @+' + biggest.at + 'ms' : 'none'}`);
    process.exit(0);
} catch (e) {
    console.error(`[early] ERROR:`, e.message);
    process.exit(1);
} finally {
    if (browser) await browser.close().catch(() => {});
}
