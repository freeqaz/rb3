#!/usr/bin/env node
// e3-regression.mjs — Wave-3 integration gate 5b: E3 (CDP 20Mbps/40ms) regression.
//   (1) boot -> main_hub: longest rAF gap + over100 count (wave-2 baseline: 62ms, over100=0)
//   (2) cold preview hover: longest rAF gap, frozen ms, mogg Range bytes/reqs
//        (wave-2 baseline: longest 20ms, frozen 0ms, ~4MB Range)
// Drives the RELEASE build (no ?debug). Cold IDB (fresh context). Output JSON.
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8437'), 10);
const ENVQ = arg('--env', '');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const getScreen = (p) => p.evaluate(() => window.rb3CurrentScreen || '');
const getBooted = (p) => p.evaluate(() => window.rb3AppBooted || 0);

async function press(page, key, holdMs = 240, gapMs = 380) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key); await sleep(gapMs);
}
async function waitScreen(page, target, timeoutMs) {
  const dl = Date.now() + timeoutMs;
  while (Date.now() < dl) { const s = await getScreen(page); if (s === target) return s; await sleep(120); }
  return await getScreen(page);
}
async function waitScreenChange(page, from, timeoutMs) {
  const dl = Date.now() + timeoutMs;
  while (Date.now() < dl) { const s = await getScreen(page); if (s && s !== from) return s; await sleep(80); }
  return await getScreen(page);
}
async function startRaf(page) {
  await page.evaluate(() => {
    window.__g = []; window.__last = performance.now(); window.__max = 0;
    const t = () => { const n = performance.now(); const g = n - window.__last; window.__last = n; if (g > window.__max) window.__max = g; window.__g.push(g); requestAnimationFrame(t); };
    requestAnimationFrame(t);
  });
}
async function resetRaf(page) { await page.evaluate(() => { window.__max = 0; window.__g = []; window.__last = performance.now(); }); }
async function readRaf(page) { return await page.evaluate(() => ({ max: window.__max, over100: (window.__g || []).filter(g => g > 100).length, frozen: (window.__g || []).filter(g => g > 33).reduce((a, g) => a + (g - 16.7), 0) })); }

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--disable-extensions', '--mute-audio',
    '--autoplay-policy=no-user-gesture-required'],
});
const context = await browser.newContext({ viewport: { width: 1280, height: 720 } }); // cold IDB
const page = await context.newPage();
const cdp = await context.newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.emulateNetworkConditions', {
  offline: false, downloadThroughput: 20 * 1024 * 1024 / 8, uploadThroughput: 5 * 1024 * 1024 / 8, latency: 40,
});

const mogg = { reqs: 0, range: 0, bytes: 0 };
const byId = new Map();
cdp.on('Network.requestWillBeSent', (e) => {
  const u = (e.request.url || '');
  if (/\.mogg(\?|$)/.test(u)) {
    const isRange = !!(e.request.headers && (e.request.headers.Range || e.request.headers.range));
    byId.set(e.requestId, { mogg: true }); mogg.reqs++; if (isRange) mogg.range++;
  }
});
cdp.on('Network.dataReceived', (e) => { const r = byId.get(e.requestId); if (r && r.mogg) mogg.bytes += e.dataLength || 0; });

const errs = [];
page.on('pageerror', (e) => errs.push(e.message));

const out = { env: ENVQ || 'default' };
const url = `http://127.0.0.1:${PORT}/` + (ENVQ ? `?env=${encodeURIComponent(ENVQ)}` : '');
console.log('>>>', url, '(throttled 20Mbps/40ms, cold IDB)');
await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
for (let i = 0; i < 600 && !(await getBooted(page)); i++) await sleep(500);
await startRaf(page);
await waitScreen(page, 'splash_screen', 180000);
await sleep(2500);
await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
await sleep(400);

// (1) boot/splash -> main_hub
await resetRaf(page);
const tHub = Date.now();
await press(page, 'Space');
let s = await waitScreenChange(page, 'splash_screen', 8000);
for (let i = 0; i < 6 && (s === 'splash_screen' || !s); i++) { await press(page, 'Enter'); s = await waitScreenChange(page, 'splash_screen', 6000); }
s = await waitScreen(page, 'main_hub_screen', 40000);
out.hubWallMs = Date.now() - tHub;
await sleep(3000);
let raf = await readRaf(page);
out.bootToHub = { longestMs: Math.round(raf.max), over100: raf.over100, frozenMs: Math.round(raf.frozen) };
console.log(`(1) boot->hub: reached=${s} wall=${out.hubWallMs}ms longest=${out.bootToHub.longestMs}ms over100=${out.bootToHub.over100}`);

// nav to song_select for the preview hover
for (let i = 0; i < 8 && (await getScreen(page)) !== 'song_select_screen'; i++) { await press(page, 'Enter'); await waitScreenChange(page, 'main_hub_screen', 5000); }
s = await waitScreen(page, 'song_select_screen', 40000);
console.log(`    reached song_select: ${s}`);
await sleep(2500);

// (2) cold preview hover — land highlight on a fresh (un-prefetched) song, hold to stream
const moggBefore = { ...mogg };
await resetRaf(page);
await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
await press(page, 'ArrowDown', 150, 350);
await press(page, 'ArrowDown', 150, 350);
await sleep(8000);
raf = await readRaf(page);
out.coldHover = {
  longestMs: Math.round(raf.max), over100: raf.over100, frozenMs: Math.round(raf.frozen),
  moggReqs: mogg.reqs - moggBefore.reqs, moggRange: mogg.range - moggBefore.range,
  moggMB: ((mogg.bytes - moggBefore.bytes) / 1048576).toFixed(2),
};
console.log(`(2) cold hover: longest=${out.coldHover.longestMs}ms over100=${out.coldHover.over100} frozen=${out.coldHover.frozenMs}ms mogg=${out.coldHover.moggReqs}req/${out.coldHover.moggRange}range/${out.coldHover.moggMB}MB`);

out.pageerrors = errs;
console.log('RESULT_JSON ' + JSON.stringify(out));
await browser.close();
process.exit(0);
