#!/usr/bin/env node
/**
 * web-stutter-probe.mjs — does the game actually STUTTER in a real browser?
 *
 * The native frame profiler (scripts/native/frame_profiler.py) found two long
 * frames: the boot-splash venue draw (~185 ms) and the song_select-ENTER
 * milo-parse (~46 ms). But the headless native render adds a per-frame
 * Submit+WaitAny GPU stall that windowed/web vsync does NOT have, so the splash
 * cost may be a measurement artifact. This probe measures the REAL web main-thread
 * stall by sampling requestAnimationFrame inter-frame gaps (a long gap == a frame
 * where wasm RunOneFrame blocked the main thread = a visible hitch), tagged by the
 * live screen, while navigating boot -> splash -> main_hub -> song_select(scroll).
 *
 * Uses a REAL GPU (ANGLE-Vulkan), NOT SwiftShader, so the splash venue raster cost
 * is realistic. Reports per-screen frame-gap p50/p95/p99/max + the worst gaps, and
 * the isolated song_select-ENTER transition gap.
 *
 * Usage: node scripts/web/web-stutter-probe.mjs [--port 8421]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const state = (page) => page.evaluate(() => ({ screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0 }));

async function press(page, key, hold = 220, gap = 350) {
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

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });

// Install the rAF inter-frame-gap recorder BEFORE the wasm runs. Each entry is the
// ms the main thread was busy/blocked since the previous animation frame, plus the
// live screen so we can attribute hitches to splash vs song_select-enter.
await page.addInitScript(() => {
  window.__gaps = [];
  let prev = performance.now();
  function tick(now) {
    const gap = now - prev; prev = now;
    // skip the first few and absurd tab-throttle gaps (>2s = backgrounded)
    if (gap > 0 && gap < 2000) window.__gaps.push([Math.round(gap * 10) / 10, window.rb3CurrentScreen || '?']);
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
});

const logs = [];
page.on('console', m => logs.push(m.text()));

function summarize(tag) {
  return page.evaluate((t) => {
    const g = window.__gaps;
    const start = window.__mark_idx || 0;
    const slice = g.slice(start);
    window.__mark_idx = g.length;
    return { tag: t, slice };
  }, tag);
}

function stats(rows) {
  // rows: [[gap, screen], ...] -> per-screen percentiles + worst
  const byScreen = {};
  for (const [gap, scr] of rows) { (byScreen[scr] ||= []).push(gap); }
  const out = {};
  for (const [scr, arr] of Object.entries(byScreen)) {
    arr.sort((a, b) => a - b);
    const q = (p) => arr[Math.min(arr.length - 1, Math.floor(p * arr.length))];
    out[scr] = { n: arr.length, p50: q(0.5), p95: q(0.95), p99: q(0.99), max: arr[arr.length - 1] };
  }
  return out;
}

let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting (real GPU, ANGLE-Vulkan)...');
  if (!await waitScreen(page, s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s.screen), 180000, 'boot'))
    throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  await waitScreen(page, s => ['splash_screen', 'main_hub_screen'].includes(s.screen), 60000, 'splash');
  console.log('on splash — recording 6s of SPLASH frame gaps (venue-behind-logo)...');
  await page.evaluate(() => { window.__mark_idx = window.__gaps.length; }); // mark
  await sleep(6000);
  const splash = await summarize('splash');

  // splash -> main_hub
  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub');
  await sleep(1500);
  // mark, then main_hub -> song_select (the ENTER transition is the target hitch)
  await page.evaluate(() => { window.__mark_idx = window.__gaps.length; });
  console.log('main_hub -> song_select (capturing the ENTER transition)...');
  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select');
  await sleep(1500);
  const enter = await summarize('enter');

  // scroll a few songs (steady-state)
  await page.evaluate(() => { window.__mark_idx = window.__gaps.length; });
  for (let i = 0; i < 8; i++) await press(page, 'ArrowDown', 120, 260);
  await sleep(1000);
  const scroll = await summarize('scroll');

  const fmt = (o) => Object.entries(o).map(([s, v]) => `      ${s.padEnd(26)} n=${String(v.n).padStart(4)}  p50=${v.p50.toFixed(1)}  p95=${v.p95.toFixed(1)}  p99=${v.p99.toFixed(1)}  max=${v.max.toFixed(1)} ms`).join('\n');
  const worst = (rows, k = 6) => rows.slice().sort((a, b) => b[0] - a[0]).slice(0, k).map(([g, s]) => `${g}ms(${s})`).join(', ');

  console.log('\n================ WEB FRAME-GAP STUTTER REPORT (real GPU) ================');
  console.log('\n[SPLASH phase] per-screen rAF inter-frame gap:');
  console.log(fmt(stats(splash.slice)));
  console.log('    worst: ' + worst(splash.slice));
  console.log('\n[main_hub -> song_select ENTER transition] per-screen gap:');
  console.log(fmt(stats(enter.slice)));
  console.log('    worst: ' + worst(enter.slice));
  console.log('\n[song_select SCROLL steady-state] per-screen gap:');
  console.log(fmt(stats(scroll.slice)));
  console.log('    worst: ' + worst(scroll.slice));
  console.log('\nInterpretation: a real visible hitch = a single gap >> ~33ms. 60fps=16.7ms.');
  rc = 0;
} catch (e) {
  console.log('ERROR:', e.message || e);
  console.log('last 20 console:'); for (const l of logs.slice(-20)) console.log('  |', l);
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
