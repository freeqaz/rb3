#!/usr/bin/env node
/**
 * screen-netlog.mjs — A2 (incremental-load-perf PLAN.md T9) per-screen
 * dependency DISCOVERY harness.
 *
 * Drives a clean cold-IDB boot through splash -> main_hub -> song_select and
 * records EVERY /api/file request via CDP (Network.requestWillBeSent), tagging
 * each with the UI screen that was current when it fired (polled from
 * window.rb3CurrentScreen). Writes one netlog JSON whose `requests` array carries
 * a synthetic marker request at each transition boundary
 * (url = "MARKER:enter:<screen>") so gen-boot-manifest.mjs --screen can slice the
 * per-screen read window with --enter-marker/--exit-marker.
 *
 * The output is a superset netlog in the {requests:[{url,status,bytes,screen}]}
 * shape the manifest generator consumes. Run with the screen bundles DISABLED
 * (RB3_SCREEN_BUNDLES_OFF=1) so the per-file requests aren't masked by the bundle
 * prefetch we're trying to populate — discovery must see the raw per-file reads.
 *
 * Usage:
 *   node scripts/web/screen-netlog.mjs --port 8435 [--out /tmp/screen-netlog.json]
 */
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8435'), 10);
const OUT = arg('--out', '/tmp/screen-netlog.json');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getBooted = (page) => page.evaluate(() => window.rb3AppBooted || 0);

async function press(page, key, holdMs = 240, gapMs = 350) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key); await sleep(gapMs);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await getScreen(page);
    if (s !== last) { console.log(`    ...${label}: '${s}'`); last = s; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}

(async () => {
  const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
      '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
      '--ozone-platform=x11', '--disable-extensions', '--mute-audio',
      '--autoplay-policy=no-user-gesture-required'],
  });
  const context = await browser.newContext({ viewport: { width: 1280, height: 720 } });
  // Cold IDB: a fresh context starts with no IndexedDB, so the asset cache is cold.
  const page = await context.newPage();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Network.enable');

  const requests = [];        // { url, status, bytes, screen, t }
  const byId = new Map();
  cdp.on('Network.requestWillBeSent', (e) => {
    const url = (e.request.url || '').replace(/^https?:\/\/[^/]+/, '');
    if (!url.startsWith('/api/file/')) return;
    const r = { url, status: null, bytes: 0, screen: '', t: Date.now() };
    byId.set(e.requestId, r);
    requests.push(r);
  });
  cdp.on('Network.responseReceived', (e) => { const r = byId.get(e.requestId); if (r) r.status = e.response.status; });
  cdp.on('Network.dataReceived', (e) => { const r = byId.get(e.requestId); if (r) r.bytes += e.dataLength || 0; });

  // Load the DEBUG build (?debug=true): only build.sh --debug deploys the T9
  // per-screen code; index.html defaults to the feature-less release build
  // otherwise. Screen-bundles OFF for discovery so we see the raw per-file reads,
  // not the bundle prefetch we're populating. (?env bridge → ::setenv in main_web.cpp.)
  const url = `http://127.0.0.1:${PORT}/?debug=true&env=RB3_SCREEN_BUNDLES_OFF=1;RB3_PREWARM_SCREENS=0`;
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  page.on('console', (m) => { if (m.type() === 'error') console.log('  [err]', m.text()); });

  // Tag every request that arrives with the current screen, polled continuously.
  let curScreen = '';
  const marker = (kind, screen) => requests.push({ url: `MARKER:${kind}:${screen}`, status: 200, bytes: 0, screen, t: Date.now() });
  const poll = setInterval(async () => {
    try {
      const s = await getScreen(page);
      if (s && s !== curScreen) { marker('enter', s); curScreen = s; }
      // tag in-flight untagged requests with the current screen
      for (const r of requests) if (r.screen === '' && !r.url.startsWith('MARKER:')) r.screen = curScreen;
    } catch { /* page busy */ }
  }, 120);

  console.log('[screen-netlog] booting (cold IDB)...');
  for (let i = 0; i < 600; i++) { if (await getBooted(page)) break; await sleep(500); }
  console.log('[screen-netlog] booted; waiting for splash...');
  await waitScreen(page, (s) => s === 'splash_screen', 180000, 'boot');
  await sleep(2500);

  // splash -> main_hub
  await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
  await sleep(500);
  await press(page, 'Space');
  let s = await waitScreen(page, (s) => s !== 'splash_screen' && s !== '', 8000, 'splash->?');
  for (let i = 0; i < 6 && (s === 'splash_screen' || !s); i++) { await press(page, 'Enter'); s = await waitScreen(page, (s) => s === 'main_hub_screen', 6000, 'splash->hub'); }
  s = await waitScreen(page, (s) => s === 'main_hub_screen', 30000, 'main_hub');
  console.log('[screen-netlog] reached', s);
  await sleep(4000);   // let main_hub finish its reads

  // main_hub -> song_select
  await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
  for (let i = 0; i < 6; i++) {
    await press(page, 'Enter');
    const cur = await waitScreen(page, (s) => s && s !== 'main_hub_screen', 6000, 'hub->?');
    if (cur && cur !== 'main_hub_screen') { s = cur; break; }
    await sleep(1200);
  }
  s = await waitScreen(page, (s) => s === 'song_select_screen', 30000, 'song_select');
  console.log('[screen-netlog] reached', s);
  await sleep(5000);   // let song_select finish its reads

  clearInterval(poll);
  const milos = requests.filter((r) => /\.milo_xbox$/.test(r.url) && r.status === 200);
  console.log(`[screen-netlog] ${requests.length} /api/file reqs (${milos.length} milo 200s)`);
  const perScreen = {};
  for (const r of milos) perScreen[r.screen] = (perScreen[r.screen] || 0) + 1;
  console.log('  milo reads per screen:', JSON.stringify(perScreen));
  writeFileSync(OUT, JSON.stringify({ requests }, null, 0));
  console.log('[screen-netlog] wrote', OUT);
  await browser.close();
})().catch((e) => { console.error(e); process.exit(1); });
