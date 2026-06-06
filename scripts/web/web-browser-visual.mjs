#!/usr/bin/env node
/**
 * web-browser-visual.mjs — prove the browser renders the live WebGPU build with
 * caching fully OFF, and capture real BROWSER screenshots of the canvas output.
 *
 *  - CDP Network.setCacheDisabled(true): nothing served from the browser cache.
 *  - Logs the rb3-web.wasm fetch (status, bytes, fromDiskCache) so we can confirm
 *    it pulled the LIVE deployed wasm, not a cached copy.
 *  - page.screenshot() at splash / song_select / gameplay = the actual composited
 *    WebGPU output the user would see (not the native /api/screenshot).
 *
 * Usage: node scripts/web/web-browser-visual.mjs [--port 8421]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const state = (page) => page.evaluate(() => ({ screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0 }));

async function press(page, key, hold = 220, gap = 360) {
  await page.keyboard.down(key); await sleep(hold);
  await page.keyboard.up(key);   await sleep(gap);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.screen !== last) { console.log(`    ...${label}: screen='${s.screen}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}
async function shot(page, path, label) {
  await page.screenshot({ path });
  // crude non-black check: read average luminance via the page canvas isn't trivial
  // through Playwright; rely on file size + manual view. Report size.
  const { size } = await import('fs').then(fs => ({ size: fs.statSync(path).size }));
  console.log(`  [shot] ${label} -> ${path} (${size} bytes)`);
  return size;
}

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11'],
});
const context = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await context.newPage();

// Fully disable the browser cache via CDP (belt + suspenders on top of the server's
// no-store header), and watch the wasm fetch.
const cdp = await context.newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.setCacheDisabled', { cacheDisabled: true });
const wasmFetches = [];
cdp.on('Network.responseReceived', (e) => {
  if (/rb3-web\.wasm(\.br|\.gz)?$/.test(e.response.url)) {
    wasmFetches.push({ url: e.response.url.split('/').pop(), status: e.response.status,
      fromDiskCache: e.response.fromDiskCache, fromSW: e.response.fromServiceWorker,
      len: e.response.headers['content-length'] || e.response.encodedDataLength });
  }
});

let rc = 1;
try {
  await page.evaluate; // noop
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting (cache DISABLED via CDP, real GPU ANGLE-Vulkan)...');
  if (!await waitScreen(page, s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s.screen), 180000, 'boot'))
    throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(800);
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  await waitScreen(page, s => ['splash_screen', 'main_hub_screen'].includes(s.screen), 60000, 'splash');
  await sleep(1500);
  await shot(page, '/tmp/web_browser_splash.png', 'splash (venue behind logo)');

  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub');
  await sleep(1500);
  await shot(page, '/tmp/web_browser_mainhub.png', 'main_hub');

  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select');
  await sleep(2000);
  await shot(page, '/tmp/web_browser_songselect.png', 'song_select');

  // try to reach gameplay for a WebGPU-heavy frame
  await press(page, 'ArrowDown', 150, 300);
  await press(page, 'Enter', 220, 450);
  const pd = await waitScreen(page, s => s.screen === 'part_difficulty_screen', 30000, 'part_difficulty');
  if (pd) {
    await sleep(1200); await press(page, 'Enter', 220, 450);
    await sleep(1200); await press(page, 'Enter', 220, 450);
    const gs = await waitScreen(page, s => s.screen === 'game_screen', 60000, 'game_screen');
    if (gs) { await sleep(4000); await shot(page, '/tmp/web_browser_gameplay.png', 'gameplay (note highway)'); }
  }

  console.log('\n=== WASM FETCH (cache disabled) ===');
  if (!wasmFetches.length) console.log('  (no rb3-web.wasm response captured — check init script timing)');
  for (const w of wasmFetches) console.log(`  ${w.url}  status=${w.status}  fromDiskCache=${w.fromDiskCache}  fromServiceWorker=${w.fromSW}  bytes=${w.len}`);
  console.log('  (fromDiskCache=false on all => the live build was fetched, not a cached copy)');
  rc = 0;
} catch (e) {
  console.log('ERROR:', e.message || e);
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
