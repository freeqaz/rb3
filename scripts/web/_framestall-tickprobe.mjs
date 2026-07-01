#!/usr/bin/env node
/**
 * _framestall-tickprobe.mjs — decisively split the gameplay frame period into:
 *   (a) tickMs   = wall time of `await Module._rb3MainLoopTick()` (CPU + any JSPI suspend)
 *   (b) rafWaitMs= time from tick-end to the NEXT tick-start (requestAnimationFrame pacing idle)
 * by MONKEYPATCHING Module._rb3MainLoopTick from the page before the rAF loop
 * runs. This tells us whether gameplay jank is ENGINE COST (tickMs high) or
 * rAF PACING (rafWaitMs high) — the two have completely different fixes.
 */
import { chromium } from 'playwright';
import { writeFileSync, mkdirSync } from 'fs';
import http from 'http';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const PORT = parseInt((process.argv[process.argv.indexOf('--port') + 1]) || '8421', 10);
const PLAY = parseInt((process.argv[process.argv.indexOf('--play') + 1]) || '25', 10);
const OUT = '/home/free/code/milohax/rb3/docs/native/frame-stall-2026-06-20/cap/tickprobe';
mkdirSync(OUT, { recursive: true });
function waitForServer(p, t = 20000) { return new Promise((res, rej) => { const dl = Date.now() + t; const c = () => http.get(`http://127.0.0.1:${p}/api/health`, r => r.statusCode === 200 ? res() : rt()).on('error', rt); const rt = () => Date.now() > dl ? rej(new Error('no server')) : setTimeout(c, 300); c(); }); }
const screenOf = (p) => p.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 } = {}) { const dl = Date.now() + timeoutMs; let s = ''; while (Date.now() < dl) { s = await screenOf(page); if (targets && targets.includes(s)) return s; if (from && s && s !== from) return s; await sleep(250); } return s; }
async function pressKey(page, key, hold = 250) { try { await page.keyboard.down(key); await sleep(hold); await page.keyboard.up(key); await sleep(200); } catch {} }
function pctile(a, p) { if (!a.length) return 0; const s = a.slice().sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.max(0, Math.ceil(p / 100 * s.length) - 1))]; }

// Installed BEFORE the page's own EM_ASM rAF loop. We wrap Module._rb3MainLoopTick
// as soon as it exists, recording tickMs (await duration) and the rAF wait gap.
function installTickInstrument() {
    window.__tk = { samples: [], lastTickEnd: -1, longtasks: [], rafGaps: [] };
    try { new PerformanceObserver(l => { for (const e of l.getEntries()) window.__tk.longtasks.push({ start: +e.startTime.toFixed(1), dur: +e.duration.toFixed(1) }); }).observe({ entryTypes: ['longtask'] }); } catch {}
    { let last = -1; const r = (t) => { if (last >= 0) window.__tk.rafGaps.push(+(t - last).toFixed(2)); last = t; requestAnimationFrame(r); }; requestAnimationFrame(r); }
    const tryWrap = () => {
        const M = window.Module;
        if (!M || typeof M._rb3MainLoopTick !== 'function' || M.__tkWrapped) { return setTimeout(tryWrap, 30); }
        const orig = M._rb3MainLoopTick.bind(M);
        M.__tkWrapped = true;
        M._rb3MainLoopTick = function () {
            const st = window.__tk;
            const tickStart = performance.now();
            const rafWait = st.lastTickEnd >= 0 ? (tickStart - st.lastTickEnd) : 0;
            const ret = orig();
            // rb3MainLoopTick is a JSPI export -> returns a Promise (awaited by the loop).
            const after = (r) => { const end = performance.now(); st.lastTickEnd = end; if (st.samples.length < 100000) st.samples.push({ tick: +(end - tickStart).toFixed(2), raf: +rafWait.toFixed(2), t: +tickStart.toFixed(1) }); return r; };
            if (ret && typeof ret.then === 'function') return ret.then(after);
            return after(ret);
        };
    };
    tryWrap();
}

await waitForServer(PORT);
const browser = await chromium.launch({ headless: !process.env.DISPLAY, args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan', '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration', '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required'] });
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
page.on('console', m => { const t = m.text(); if (/boot error|CRASH/i.test(t)) console.log('  [page]', t.slice(0, 120)); });
await page.addInitScript(installTickInstrument);
await page.goto(`http://127.0.0.1:${PORT}/?debug=true&env=${encodeURIComponent('RB3_FRAME_TRACE=/trace.jsonl')}`, { waitUntil: 'domcontentloaded' });
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
    if ((await screenOf(page)) === 'part_difficulty_screen') { await sleep(1500); for (let i = 0; i < 5; i++) { await pressKey(page, 'Enter', 150); await sleep(1200); if ((await screenOf(page)) === 'game_screen') break; } if ((await waitScreen(page, { targets: ['game_screen'], timeoutMs: 60000 })) === 'game_screen') phase = 'gameplay'; }
    if (phase !== 'gameplay') phase = `stuck:${await screenOf(page)}`;
} else phase = `nosongselect:${s}`;
console.log(`[tk] phase=${phase}`);
// warmup, clear samples, then measure
await sleep(8000);
await page.evaluate(() => { if (window.__tk) window.__tk.samples = []; });
const wrapped = await page.evaluate(() => !!(window.Module && window.Module.__tkWrapped));
console.log(`[tk] tick wrapped=${wrapped}; measuring ${PLAY}s...`);
await sleep(PLAY * 1000);
const tkAll = await page.evaluate(() => (window.__tk ? { samples: window.__tk.samples, longtasks: window.__tk.longtasks, rafGaps: window.__tk.rafGaps } : { samples: [], longtasks: [], rafGaps: [] }));
const samp = tkAll.samples;
const ticks = samp.map(s => s.tick), rafs = samp.map(s => s.raf).filter(r => r > 0);
const ltDurs = (tkAll.longtasks || []).map(l => l.dur);
const ltStat = { count: ltDurs.length, sumMs: +ltDurs.reduce((a, b) => a + b, 0).toFixed(0), p50: +pctile(ltDurs, 50).toFixed(0), p99: +pctile(ltDurs, 99).toFixed(0), max: ltDurs.length ? +Math.max(...ltDurs).toFixed(0) : 0, over100: ltDurs.filter(d => d > 100).length };
const period = samp.map(s => s.tick + s.raf);
const stat = (a) => ({ n: a.length, p50: +pctile(a, 50).toFixed(2), p90: +pctile(a, 90).toFixed(2), p99: +pctile(a, 99).toFixed(2), max: a.length ? +Math.max(...a).toFixed(2) : 0, mean: a.length ? +(a.reduce((x, y) => x + y, 0) / a.length).toFixed(2) : 0 });
const out = { phase, wrapped, samples: samp.length, tickMs: stat(ticks), rafWaitMs: stat(rafs), periodMs: stat(period), fps: +(1000 / (stat(period).mean || 1)).toFixed(1) };
// FRAME_TRACE: wasm-side per-frame dt (no profiler overhead). Find worst frames.
let frTxt = await page.evaluate(() => { try { return window.FS.readFile('/trace.jsonl', { encoding: 'utf8' }); } catch (e) { return '__ERR__' + e.message; } });
let frFrames = [];
if (!frTxt.startsWith('__ERR__')) for (const l of frTxt.split('\n')) { const t = l.trim(); if (!t || t[0] === '#') continue; try { frFrames.push(JSON.parse(t)); } catch {} }
const gameFrames = frFrames.filter(f => f.scr === 'game_screen');
const frDts = gameFrames.map(f => f.dt || 0);
const frStat = { n: gameFrames.length, p50: +pctile(frDts, 50).toFixed(2), p99: +pctile(frDts, 99).toFixed(2), max: frDts.length ? +Math.max(...frDts).toFixed(2) : 0, over33: frDts.filter(d => d > 33).length, over50: frDts.filter(d => d > 50).length, over100: frDts.filter(d => d > 100).length };
const bk = ['lp', 'lpu', 'fetchMs', 'dtaMs', 'objMs', 'primeMs', 'texMs', 'meshMs', 'unpackMs', 'pipeMs', 'inflMs'];
const worst = gameFrames.slice().sort((a, b) => (b.dt || 0) - (a.dt || 0)).slice(0, 20).map(f => { const sum = bk.reduce((a, k) => a + (f[k] || 0), 0); return { f: f.f, dt: +(f.dt || 0).toFixed(1), residue: +((f.dt || 0) - sum).toFixed(1), lp: f.lp, lpu: f.lpu, objMs: f.objMs, objWNm: f.objWNm, primeMs: f.primeMs, fetchMs: f.fetchMs, texMs: f.texMs, meshMs: f.meshMs, st: f.st, ld: f.ld, pend: f.pend }; });
out.frameTrace = frStat; out.worstTraceFrames = worst;
writeFileSync(`${OUT}/tickprobe.json`, JSON.stringify({ ...out, raw: samp.slice(0, 4000), longtasks: tkAll.longtasks }, null, 2));
console.log(`FRAME-TRACE (game frames, wasm-side, no profiler): n=${frStat.n} p50=${frStat.p50} p99=${frStat.p99} max=${frStat.max} >33=${frStat.over33} >50=${frStat.over50} >100=${frStat.over100}`);
console.log('WORST game frames (FRAME_TRACE):');
for (const w of worst.slice(0, 10)) console.log(`  f${w.f} dt=${w.dt}ms residue=${w.residue} lp=${w.lp} lpu=${w.lpu} obj=${w.objMs}(${w.objWNm}) prime=${w.primeMs} fetch=${w.fetchMs} tex=${w.texMs} mesh=${w.meshMs} st=${w.st} ld=${w.ld} pend=${w.pend}`);
console.log('\n==== TICK PROBE (gameplay) ====');
console.log(`samples=${out.samples} fps≈${out.fps}`);
console.log(`tickMs  (await rb3MainLoopTick): p50=${out.tickMs.p50} p90=${out.tickMs.p90} p99=${out.tickMs.p99} mean=${out.tickMs.mean} max=${out.tickMs.max}`);
console.log(`rafWait (pacing idle to next tick): p50=${out.rafWaitMs.p50} p90=${out.rafWaitMs.p90} p99=${out.rafWaitMs.p99} mean=${out.rafWaitMs.mean} max=${out.rafWaitMs.max}`);
console.log(`period  (tick+raf): p50=${out.periodMs.p50} mean=${out.periodMs.mean}`);
console.log(`longtasks (>50ms, passive PO): count=${ltStat.count} sum=${ltStat.sumMs}ms p50=${ltStat.p50} p99=${ltStat.p99} max=${ltStat.max} over100=${ltStat.over100}`);
console.log(`-> ${OUT}/tickprobe.json`);
await browser.close(); process.exit(0);
