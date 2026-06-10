#!/usr/bin/env node
/**
 * t9-screen-bundle-verify.mjs — A2 per-screen bundles (PLAN.md T9) verification
 * + A/B, and Q10 prewarm A/B.
 *
 * For each ARM (a ?env= flag combo) it does a COLD-IDB boot (fresh browser
 * context = empty IndexedDB) and drives title -> main_hub -> song_select, while
 * CDP records:
 *   - every /api/bundle/screen/<name> request (proves the trigger fired) + bytes,
 *   - every /api/file/*.milo_xbox request during each transition window,
 *   - per-transition wall time (screen-name change -> next screen-name change),
 *   - the longest rAF gap during each transition (canvas-freeze proxy), sampled
 *     via a requestAnimationFrame probe installed in the page.
 *
 * Network can be throttled (E3) with --throttle (20 Mbps / 40 ms RTT).
 *
 * Usage:
 *   node scripts/web/t9-screen-bundle-verify.mjs --port 8435 \
 *        [--throttle] [--runs 3] [--arms bundles,nobundles,prewarmon,prewarmoff]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const has = (n) => argv.includes(n);
const PORT = parseInt(arg('--port', '8435'), 10);
const THROTTLE = has('--throttle');
const RUNS = parseInt(arg('--runs', '1'), 10);
const ARMS = (arg('--arms', 'bundles,nobundles')).split(',');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ?env flag combos per arm.
const ARM_ENV = {
  bundles:    'RB3_SCREEN_BUNDLES_OFF=0;RB3_PREWARM_SCREENS=0',   // A2 ON, prewarm off (isolate bundles)
  nobundles:  'RB3_SCREEN_BUNDLES_OFF=1;RB3_PREWARM_SCREENS=0',   // A2 OFF (control)
  prewarmon:  'RB3_SCREEN_BUNDLES_OFF=1;RB3_PREWARM_SCREENS=1',   // Q10 prewarm ON (bundles off, isolate prewarm)
  prewarmoff: 'RB3_SCREEN_BUNDLES_OFF=1;RB3_PREWARM_SCREENS=0',   // Q10 prewarm OFF
  default:    '',   // NO flags — proves shipping web defaults (bundles+prewarm ON)
  both:       'RB3_SCREEN_BUNDLES_OFF=0;RB3_PREWARM_SCREENS=1',   // both ON (shipping default for web)
};

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getBooted = (page) => page.evaluate(() => window.rb3AppBooted || 0);

async function press(page, key, holdMs = 240, gapMs = 350) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key); await sleep(gapMs);
}
async function waitScreenChange(page, from, timeoutMs) {
  const dl = Date.now() + timeoutMs;
  while (Date.now() < dl) { const s = await getScreen(page); if (s && s !== from) return s; await sleep(80); }
  return await getScreen(page);
}
async function waitScreen(page, target, timeoutMs) {
  const dl = Date.now() + timeoutMs;
  while (Date.now() < dl) { const s = await getScreen(page); if (s === target) return s; await sleep(120); }
  return await getScreen(page);
}

// rAF-gap probe: record the max gap between animation frames over a window.
async function startRafProbe(page) {
  await page.evaluate(() => {
    window.__rafGaps = []; window.__rafLast = performance.now(); window.__rafMax = 0;
    const tick = () => { const now = performance.now(); const g = now - window.__rafLast; window.__rafLast = now; if (g > window.__rafMax) window.__rafMax = g; window.__rafGaps.push(g); requestAnimationFrame(tick); };
    requestAnimationFrame(tick);
  });
}
async function resetRaf(page) { await page.evaluate(() => { window.__rafMax = 0; window.__rafGaps = []; window.__rafLast = performance.now(); }); }
async function readRaf(page) { return await page.evaluate(() => ({ max: window.__rafMax, over100: (window.__rafGaps || []).filter((g) => g > 100).length })); }

async function runArm(armName, runIdx) {
  const env = ARM_ENV[armName] || '';
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
  if (THROTTLE) {
    await cdp.send('Network.emulateNetworkConditions', {
      offline: false, downloadThroughput: 20 * 1024 * 1024 / 8, uploadThroughput: 5 * 1024 * 1024 / 8, latency: 40,
    });
  }
  const bundleReqs = [];    // { name, bytes }
  const fileReqs = [];      // { url, bytes, t }
  const byId = new Map();
  cdp.on('Network.requestWillBeSent', (e) => {
    const u = (e.request.url || '').replace(/^https?:\/\/[^/]+/, '');
    if (u.startsWith('/api/bundle/screen/')) { const r = { url: u, name: u.split('/').pop(), bytes: 0 }; byId.set(e.requestId, r); bundleReqs.push(r); }
    else if (u.startsWith('/api/file/') && u.endsWith('.milo_xbox')) { const r = { url: u, bytes: 0, t: Date.now() }; byId.set(e.requestId, r); fileReqs.push(r); }
  });
  cdp.on('Network.dataReceived', (e) => { const r = byId.get(e.requestId); if (r) r.bytes += e.dataLength || 0; });

  // MUST load the DEBUG build: only build.sh --debug deploys the T9 per-screen
  // bundle code (release is built later by integration). index.html defaults to
  // release without ?debug=true, so without this the bundles NEVER fire and the
  // A/B is non-discriminating (every arm reports bundles=NONE on the feature-less
  // release wasm). ?debug=true&env=... chains the env bridge after the build sel.
  const url = `http://127.0.0.1:${PORT}/?debug=true&env=${encodeURIComponent(env)}`;
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  for (let i = 0; i < 600; i++) { if (await getBooted(page)) break; await sleep(500); }
  await startRafProbe(page);
  await waitScreen(page, 'splash_screen', 180000);
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
  await sleep(400);

  // --- transition 1: splash -> main_hub
  const fileBeforeHub = fileReqs.length;
  await resetRaf(page);
  const t0hub = Date.now();
  await press(page, 'Space');
  let s = await waitScreenChange(page, 'splash_screen', 8000);
  for (let i = 0; i < 6 && (s === 'splash_screen' || !s); i++) { await press(page, 'Enter'); s = await waitScreenChange(page, 'splash_screen', 6000); }
  s = await waitScreen(page, 'main_hub_screen', 30000);
  const hubWall = Date.now() - t0hub;
  await sleep(3500);  // let main_hub reads settle
  const hubRaf = await readRaf(page);
  const hubFiles = fileReqs.length - fileBeforeHub;

  // --- transition 2: main_hub -> song_select
  const fileBeforeSS = fileReqs.length;
  await resetRaf(page);
  await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
  const t0ss = Date.now();
  for (let i = 0; i < 6; i++) { await press(page, 'Enter'); const cur = await waitScreenChange(page, 'main_hub_screen', 6000); if (cur && cur !== 'main_hub_screen') { s = cur; break; } await sleep(1000); }
  s = await waitScreen(page, 'song_select_screen', 30000);
  const ssWall = Date.now() - t0ss;
  await sleep(4500);
  const ssRaf = await readRaf(page);
  const ssFiles = fileReqs.length - fileBeforeSS;

  const reached = s;
  const errs = [];
  page.on('pageerror', (e) => errs.push(e.message));
  await browser.close();

  return {
    arm: armName, run: runIdx, reached,
    bundleReqs: bundleReqs.map((b) => ({ name: b.name, kb: Math.round(b.bytes / 1024) })),
    hub: { wallMs: hubWall, rafMaxMs: Math.round(hubRaf.max), rafOver100: hubRaf.over100, fileReqs: hubFiles },
    ss:  { wallMs: ssWall,  rafMaxMs: Math.round(ssRaf.max),  rafOver100: ssRaf.over100,  fileReqs: ssFiles },
  };
}

(async () => {
  console.log(`[t9-verify] port=${PORT} throttle=${THROTTLE} runs=${RUNS} arms=${ARMS.join(',')}`);
  const results = [];
  for (const arm of ARMS) {
    for (let r = 0; r < RUNS; r++) {
      console.log(`\n=== arm=${arm} run=${r + 1}/${RUNS} (env=${ARM_ENV[arm]}) ===`);
      try {
        const res = await runArm(arm, r + 1);
        results.push(res);
        console.log(`  reached=${res.reached}`);
        console.log(`  bundles fired: ${res.bundleReqs.length ? res.bundleReqs.map((b) => `${b.name}(${b.kb}KB)`).join(', ') : 'NONE'}`);
        console.log(`  splash->hub : wall=${res.hub.wallMs}ms rafMax=${res.hub.rafMaxMs}ms over100=${res.hub.rafOver100} fileReqs=${res.hub.fileReqs}`);
        console.log(`  hub->select : wall=${res.ss.wallMs}ms rafMax=${res.ss.rafMaxMs}ms over100=${res.ss.rafOver100} fileReqs=${res.ss.fileReqs}`);
      } catch (e) { console.log('  ARM FAILED:', e.message); results.push({ arm, run: r + 1, error: e.message }); }
    }
  }

  // Aggregate (median) per arm.
  console.log('\n===================== SUMMARY (median per arm) =====================');
  const med = (a) => { const s = a.slice().sort((x, y) => x - y); return s.length ? s[Math.floor(s.length / 2)] : 0; };
  // Which arms are EXPECTED to fire a bundle (RB3_SCREEN_BUNDLES_OFF unset/=0).
  const expectBundles = (arm) => !/RB3_SCREEN_BUNDLES_OFF=1/.test(ARM_ENV[arm] || '');
  const failures = [];
  for (const arm of ARMS) {
    const rs = results.filter((r) => r.arm === arm && !r.error);
    if (!rs.length) { console.log(`${arm}: ALL RUNS FAILED`); failures.push(`${arm}: all runs failed`); continue; }
    const firedAny = rs.some((r) => r.bundleReqs && r.bundleReqs.length);
    console.log(`${arm}: bundlesFired=${firedAny} (expected=${expectBundles(arm)})`);
    console.log(`   splash->hub  wall=${med(rs.map((r) => r.hub.wallMs))}ms  rafMax=${med(rs.map((r) => r.hub.rafMaxMs))}ms  over100=${med(rs.map((r) => r.hub.rafOver100))}  fileReqs=${med(rs.map((r) => r.hub.fileReqs))}`);
    console.log(`   hub->select  wall=${med(rs.map((r) => r.ss.wallMs))}ms  rafMax=${med(rs.map((r) => r.ss.rafMaxMs))}ms  over100=${med(rs.map((r) => r.ss.rafOver100))}  fileReqs=${med(rs.map((r) => r.ss.fileReqs))}`);
    // GATE: an arm that SHOULD fire bundles but fired NONE means we measured a
    // feature-less build (e.g. release without ?debug=true) — a non-discriminating
    // A/B must not silently pass. Conversely an OFF arm that fired a bundle is also wrong.
    if (expectBundles(arm) && !firedAny) failures.push(`${arm}: expected a /api/bundle/screen/ request but NONE fired (wrong build? need ?debug=true)`);
    if (!expectBundles(arm) && firedAny) failures.push(`${arm}: RB3_SCREEN_BUNDLES_OFF=1 but a bundle still fired`);
  }
  console.log(JSON.stringify(results));
  if (failures.length) {
    console.error('\n[t9-verify] GATE FAILED:\n  - ' + failures.join('\n  - '));
    process.exit(2);
  }
  console.log('\n[t9-verify] GATE PASSED: bundle firing matches expectation for every arm.');
})().catch((e) => { console.error(e); process.exit(1); });
