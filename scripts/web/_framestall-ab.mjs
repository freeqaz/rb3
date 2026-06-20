#!/usr/bin/env node
/**
 * _framestall-ab.mjs — A/B a set of RB3_* env flags against gameplay frame time,
 * using the engine-side RB3_FRAME_TRACE dt (the authoritative RunOneFrame wall
 * time) sliced to a steady window. Each run is a fresh browser session.
 *
 * Usage:
 *   node _framestall-ab.mjs --label baseline --env ""
 *   node _framestall-ab.mjs --label noRebind --env "RB3_NO_SKEL_REBIND=1;RB3_NO_HEAD_REBIND=1"
 * Multiple --env/--label pairs may be given to run several arms in one process.
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import http from 'http';

const argv = process.argv.slice(2);
const PORT = parseInt((argv[argv.indexOf('--port') + 1]) || '8421', 10);
const PLAY = parseInt((argv[argv.indexOf('--play') + 1]) || '30', 10);
const WARMUP = parseInt((argv[argv.indexOf('--warmup') + 1]) || '8', 10);
const OUT = (argv[argv.indexOf('--out') + 1]) || '/home/free/code/milohax/rb3/docs/native/frame-stall-2026-06-20/cap/ab';
mkdirSync(OUT, { recursive: true });
// collect (label,env) arms
const arms = [];
for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--arm') { const [label, env] = argv[i + 1].split('::'); arms.push({ label, env: env || '' }); }
}
if (!arms.length) arms.push({ label: 'baseline', env: '' });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));
function waitForServer(p, t = 20000) { return new Promise((res, rej) => { const dl = Date.now() + t; const c = () => http.get(`http://127.0.0.1:${p}/api/health`, r => r.statusCode === 200 ? res() : rt()).on('error', rt); const rt = () => Date.now() > dl ? rej(new Error('no server')) : setTimeout(c, 300); c(); }); }
const screenOf = (p) => p.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) { const dl = Date.now() + timeoutMs; let s = ''; while (Date.now() < dl) { s = await screenOf(page); if (targets && targets.includes(s)) return s; if (from && s && s !== from) return s; await sleep(250); } return s; }
async function pressKey(page, key, hold = 250) { try { await page.keyboard.down(key); await sleep(hold); await page.keyboard.up(key); await sleep(200); } catch {} }
function pctile(a, p) { if (!a.length) return 0; const s = a.slice().sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.max(0, Math.ceil(p / 100 * s.length) - 1))]; }

async function runArm(arm) {
    const env = `RB3_FRAME_TRACE=/trace.jsonl${arm.env ? ';' + arm.env : ''}`;
    const url = `http://127.0.0.1:${PORT}/?debug=true&env=${encodeURIComponent(env)}`;
    const browser = await chromium.launch({ headless: !process.env.DISPLAY, args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan', '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration', '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required'] });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0 = Date.now(); const el = () => ((Date.now() - t0) / 1000).toFixed(1);
    try {
        await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
        await sleep(1200); await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
        { const dl = Date.now() + 240000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
        let s = await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen', 'intro_movie_screen'], timeoutMs: 180000 });
        for (let i = 0; i < 15 && s === 'intro_movie_screen'; i++) { await pressKey(page, 'Space', 300); s = await screenOf(page); }
        await waitScreen(page, { targets: ['splash_screen', 'main_hub_screen'], timeoutMs: 30000 }); s = await screenOf(page); await sleep(2000);
        if (s === 'splash_screen') { await pressKey(page, 'Space'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 }); for (let i = 0; i < 8 && s === 'splash_screen'; i++) { await pressKey(page, 'Enter'); s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 }); } if (s !== 'main_hub_screen') s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 }); }
        await sleep(2500); s = await screenOf(page);
        if (s === 'main_hub_screen') { for (let i = 0; i < 5; i++) { await pressKey(page, 'Enter'); const cur = await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 }); if (cur && cur !== 'main_hub_screen') { s = cur; break; } await sleep(1500); } s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 }); if (s === 'song_select_enter_screen') s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 }); }
        await sleep(2500); s = await screenOf(page);
        let phase = 'unknown';
        if (s === 'song_select_screen') {
            await pressKey(page, 'ArrowDown', 200); await sleep(1500);
            await page.evaluate(() => { try { window.rb3WebUseAids = 1; window.rb3WebTargetSong = '20thcenturyboy'; } catch {} }).catch(() => {});
            try { await pressKey(page, 'Enter', 220); await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 }); } catch {}
            let c = await screenOf(page);
            if (c === 'part_difficulty_screen') { await sleep(1500); for (let i = 0; i < 5; i++) { await pressKey(page, 'Enter', 150); await sleep(1200); if ((await screenOf(page)) === 'game_screen') break; } const g = await waitScreen(page, { targets: ['game_screen'], timeoutMs: 60000 }); if (g === 'game_screen') phase = 'gameplay'; }
            if (phase !== 'gameplay') phase = `stuck:${await screenOf(page)}`;
        } else phase = `nosongselect:${s}`;
        // warmup then window
        await sleep(WARMUP * 1000);
        const startFrame = await page.evaluate(() => window.rb3FrameCount || 0);
        const tWin = Date.now();
        await sleep(PLAY * 1000);
        const endFrame = await page.evaluate(() => window.rb3FrameCount || 0);
        const winSecs = (Date.now() - tWin) / 1000;
        const txt = await page.evaluate(() => { try { return window.FS.readFile('/trace.jsonl', { encoding: 'utf8' }); } catch (e) { return '__ERR__' + e.message; } });
        let frames = [];
        if (!txt.startsWith('__ERR__')) for (const l of txt.split('\n')) { const t = l.trim(); if (!t || t[0] === '#') continue; try { frames.push(JSON.parse(t)); } catch {} }
        const win = frames.filter(f => f.f >= startFrame && f.f <= endFrame);
        const dts = win.map(f => f.dt || 0);
        const res = {
            label: arm.label, env: arm.env, phase,
            window: { startFrame, endFrame, wasmFrames: endFrame - startFrame, secs: +winSecs.toFixed(1), wasmFps: +((endFrame - startFrame) / winSecs).toFixed(1) },
            dt: { n: dts.length, p50: +pctile(dts, 50).toFixed(2), p90: +pctile(dts, 90).toFixed(2), p99: +pctile(dts, 99).toFixed(2), max: dts.length ? +Math.max(...dts).toFixed(2) : 0, mean: dts.length ? +(dts.reduce((a, b) => a + b, 0) / dts.length).toFixed(2) : 0, over33: dts.filter(d => d > 33).length, over100: dts.filter(d => d > 100).length },
        };
        console.log(`[${arm.label}] phase=${phase} win=${res.window.wasmFrames}f/${res.window.secs}s fps=${res.window.wasmFps} dt p50=${res.dt.p50} p90=${res.dt.p90} p99=${res.dt.p99} max=${res.dt.max} mean=${res.dt.mean} (${el()}s)`);
        return res;
    } catch (e) {
        console.error(`[${arm.label}] ERROR ${e.message}`); return { label: arm.label, env: arm.env, error: e.message };
    } finally { try { await Promise.race([browser.close(), sleep(3000)]); } catch {} }
}

await waitForServer(PORT);
const results = [];
for (const arm of arms) { console.log(`\n=== ARM: ${arm.label} (env="${arm.env}") ===`); results.push(await runArm(arm)); }
writeFileSync(`${OUT}/ab-results.json`, JSON.stringify({ port: PORT, play: PLAY, warmup: WARMUP, results }, null, 2));
console.log('\n==== A/B SUMMARY (RunOneFrame dt, ms) ====');
for (const r of results) { if (r.error) { console.log(`${r.label.padEnd(20)} ERROR ${r.error}`); continue; } console.log(`${r.label.padEnd(20)} fps=${String(r.window.wasmFps).padStart(5)} p50=${String(r.dt.p50).padStart(7)} p90=${String(r.dt.p90).padStart(7)} p99=${String(r.dt.p99).padStart(7)} mean=${String(r.dt.mean).padStart(7)} max=${String(r.dt.max).padStart(7)}  (${r.phase})`); }
console.log(`-> ${OUT}/ab-results.json`);
process.exit(0);
