#!/usr/bin/env node
// Quick probe: what FS/frame-timing surface does the running web build expose?
// Also measures rAF cadence under three conditions to explain the 50ms floor.
import { chromium } from 'playwright';
import http from 'http';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const PORT = 8421;
function waitForServer(port, t = 20000) { return new Promise((res, rej) => { const dl = Date.now() + t; const c = () => http.get(`http://127.0.0.1:${port}/api/health`, r => r.statusCode === 200 ? res() : rt()).on('error', rt); const rt = () => Date.now() > dl ? rej(new Error('no server')) : setTimeout(c, 300); c(); }); }
await waitForServer(PORT);
const browser = await chromium.launch({ headless: !process.env.DISPLAY, args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan', '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration', '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required'] });
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
page.on('console', m => { const t = m.text(); if (/env RB3|frame|FRAME/i.test(t)) console.log('  [page]', t.slice(0, 120)); });
await page.goto(`http://127.0.0.1:${PORT}/?debug=true&env=RB3_FRAME_TRACE=${encodeURIComponent('/trace.jsonl')}`, { waitUntil: 'domcontentloaded' });
await sleep(1500);
await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
{ const dl = Date.now() + 120000; while (Date.now() < dl) { const b = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0); if (b >= 1) break; await sleep(500); } }
console.log('booted');
await sleep(4000);
// Probe FS exposure + rAF cadence + whether RunOneFrame is even being driven each rAF.
const probe = await page.evaluate(async () => {
    const out = {};
    out.hasModule = typeof window.Module !== 'undefined';
    // Do NOT enumerate Module keys (trap-on-access stubs abort()). Access named
    // props only inside try/catch.
    const safe = (label, fn) => { try { out[label] = fn(); } catch (e) { out[label] = 'ERR:' + e.message; } };
    safe('FS_window', () => typeof window.FS);
    safe('FS_module', () => (window.Module && window.Module.FS) ? 'object' : 'absent');
    safe('FS_writeFile', () => (window.FS && window.FS.writeFile) ? 'fn' : 'absent');
    const tryRead = (label, fn) => { try { const v = fn(); out['read_' + label] = (typeof v === 'string') ? ('len=' + v.length) : ('?' + typeof v); } catch (e) { out['read_' + label] = 'ERR:' + e.message; } };
    tryRead('windowFS', () => window.FS.readFile('/trace.jsonl', { encoding: 'utf8' }));
    safe('tracePathExists', () => window.FS.analyzePath('/trace.jsonl').exists);
    // rAF cadence over ~1.5s
    const gaps = [];
    await new Promise(res => { let last = -1, n = 0; const r = (t) => { if (last >= 0) gaps.push(+(t - last).toFixed(1)); last = t; if (++n < 60) requestAnimationFrame(r); else res(); }; requestAnimationFrame(r); });
    out.rafGaps_sample = gaps.slice(0, 20);
    out.rafGap_median = gaps.sort((a, b) => a - b)[Math.floor(gaps.length / 2)];
    // is the wasm frame counter advancing? sample twice 1s apart
    const f1 = window.rb3FrameCount || 0; await new Promise(r => setTimeout(r, 1000)); const f2 = window.rb3FrameCount || 0;
    out.wasmFps = f2 - f1;
    out.screen = window.rb3CurrentScreen;
    out.docHidden = document.hidden;
    out.visibilityState = document.visibilityState;
    return out;
});
console.log(JSON.stringify(probe, null, 2));
await browser.close();
process.exit(0);
