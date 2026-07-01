#!/usr/bin/env node
// _w3-flagcheck.mjs — Wave-3 gate 5f: boot + title->hub->song_select with an
// arbitrary ?env flag combo on the RELEASE build (no ?debug). Pass = reaches
// song_select, no pageerror. Cold IDB. Usage:
//   node _w3-flagcheck.mjs --port 8437 --env "RB3_PIPELINE_PREWARM_OFF=1"
import { chromium } from 'playwright';
const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8437'), 10);
const ENVQ = arg('--env', '');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const getScreen = (p) => p.evaluate(() => window.rb3CurrentScreen || '');
const getBooted = (p) => p.evaluate(() => window.rb3AppBooted || 0);
async function press(page, key, h = 240, g = 360) { await page.keyboard.down(key); await sleep(h); await page.keyboard.up(key); await sleep(g); }
async function waitScreen(page, t, ms) { const dl = Date.now() + ms; while (Date.now() < dl) { const s = await getScreen(page); if (s === t) return s; await sleep(120); } return await getScreen(page); }
async function waitChange(page, from, ms) { const dl = Date.now() + ms; while (Date.now() < dl) { const s = await getScreen(page); if (s && s !== from) return s; await sleep(80); } return await getScreen(page); }

const browser = await chromium.launch({ headless: !process.env.DISPLAY, args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan', '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration', '--ozone-platform=x11', '--disable-extensions', '--mute-audio', '--autoplay-policy=no-user-gesture-required'] });
const context = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await context.newPage();
const errs = [];
page.on('pageerror', (e) => errs.push(e.message));
const url = `http://127.0.0.1:${PORT}/` + (ENVQ ? `?env=${encodeURIComponent(ENVQ)}` : '');
console.log('>>>', url, '(release, cold IDB)');
let rc = 1;
try {
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  for (let i = 0; i < 600 && !(await getBooted(page)); i++) await sleep(500);
  await waitScreen(page, 'splash_screen', 180000);
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
  await sleep(400);
  await press(page, 'Space');
  let s = await waitChange(page, 'splash_screen', 8000);
  for (let i = 0; i < 6 && (s === 'splash_screen' || !s); i++) { await press(page, 'Enter'); s = await waitChange(page, 'splash_screen', 6000); }
  s = await waitScreen(page, 'main_hub_screen', 40000);
  const hub = (s === 'main_hub_screen');
  await sleep(2000);
  for (let i = 0; i < 8 && (await getScreen(page)) !== 'song_select_screen'; i++) { await press(page, 'Enter'); await waitChange(page, 'main_hub_screen', 5000); }
  s = await waitScreen(page, 'song_select_screen', 40000);
  const ss = (s === 'song_select_screen');
  const pass = hub && ss && errs.length === 0;
  console.log(`env='${ENVQ || 'default'}' hub=${hub} song_select=${ss} pageerrors=${errs.length} -> ${pass ? 'PASS' : 'FAIL'}`);
  if (errs.length) console.log('  pageerrors:', errs.slice(0, 3));
  rc = pass ? 0 : 1;
} catch (e) { console.log('ERROR', e.message); }
await browser.close();
process.exit(rc);
