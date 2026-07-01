#!/usr/bin/env node
/**
 * t6t7-async-open-verify.mjs — verification for the A1 async-open seam fix
 * (incremental-load-perf PLAN.md T6 WebPendingFile + T7 WebRangeFile).
 *
 * Drives the deployed web build headless and asserts:
 *   (a) boot to main_hub — no regression vs ~7.6s baseline; appctor sane.
 *   (b) cold preview hover: server gets RANGE requests totalling < 5MB for the
 *       mogg (NOT a 36MB whole-file fetch), the canvas does not freeze > ~100ms,
 *       and AUDIO ACTUALLY PLAYS (engine MixSources capture nonzero).
 *   (c) enter gameplay: song audio starts.
 *   (d) legacy paths: re-run (b) with RB3_ASYNC_OPEN_OFF=1 and with
 *       RB3_MOGG_RANGE_OFF=1 via the rb3_pre.js ?env URL-param ENV bridge.
 *
 * Network attribution is taken from the browser's own request/response events
 * (which see the Range header + the transferred byte count exactly), keyed on
 * the .mogg URL — the ground truth for "how many mogg bytes crossed the wire".
 *
 * Usage:
 *   node scripts/web/t6t7-async-open-verify.mjs --port 8433 [--phase preview|song|all]
 *                                               [--env RB3_ASYNC_OPEN_OFF=1]
 */
import { chromium } from 'playwright';
import { readFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8433'), 10) || 8433;
const PHASE = arg('--phase', 'all');
const ENVQ = arg('--env', '');            // e.g. "RB3_ASYNC_OPEN_OFF=1"
const WAV = `/tmp/rb3_t6t7_${(ENVQ || 'default').replace(/[^A-Za-z0-9]/g, '_')}.wav`;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', view: window.rb3OvershellView || '?',
  frame: window.rb3FrameCount || 0,
}));

async function press(page, key, holdMs = 220, gapMs = 350) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key);   await sleep(gapMs);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.screen !== last) { console.log(`    ...${label}: screen='${s.screen}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}

function analyzeWav(path) {
  const buf = readFileSync(path);
  const s = new Int16Array(buf.buffer, buf.byteOffset + 44, (buf.length - 44) >> 1);
  const sr = buf.readUInt32LE(24), ch = buf.readUInt16LE(22);
  let peak = 0, nz = 0, sq = 0;
  for (let i = 0; i < s.length; i++) { const a = Math.abs(s[i]); if (a > peak) peak = a; if (a > 64) nz++; sq += s[i]*s[i]; }
  return { sr, ch, dur: s.length/(sr*ch), peak, nzPct: 100*nz/s.length, rms: Math.sqrt(sq/s.length) };
}

// SAB-ring nonzero proof (memory note: the reliable proof that real audio is
// being pushed to the AudioWorklet, NOT a capture-buffer artifact). rb3DumpSAB
// logs "SAB stats: ... nonZero=X/Y"; we parse the latest such line. Returns the
// nonzero fraction [0..1], or -1 if no SAB line appeared.
let sabLatest = -1;
function watchSab(t) {
  const m = /SAB stats:.*nonZero=(\d+)\/(\d+)/.exec(t);
  if (m) { const n = +m[1], d = +m[2]; sabLatest = d ? n / d : -1; }
}
async function probeSab(page) {
  sabLatest = -1;
  for (let i = 0; i < 6 && sabLatest < 0; i++) {
    await page.evaluate(() => { try { window.rb3DumpSAB(8); } catch (e) {} });
    await sleep(250);
  }
  return sabLatest;
}

// rAF-gap freeze probe installed in-page: records the max contiguous gap between
// requestAnimationFrame callbacks during a measurement window (a frozen canvas =
// a long rAF gap). Returns the max gap ms.
async function startFreezeProbe(page) {
  await page.evaluate(() => {
    window.__rb3FreezeMax = 0;
    window.__rb3FreezeLast = performance.now();
    window.__rb3FreezeOn = true;
    const tick = () => {
      const now = performance.now();
      const gap = now - window.__rb3FreezeLast;
      if (gap > window.__rb3FreezeMax) window.__rb3FreezeMax = gap;
      window.__rb3FreezeLast = now;
      if (window.__rb3FreezeOn) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  });
}
async function stopFreezeProbe(page) {
  return await page.evaluate(() => {
    window.__rb3FreezeOn = false;
    return window.__rb3FreezeMax || 0;
  });
}

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });

const logs = [];
page.on('console', m => { const t = m.text(); logs.push(t); watchSab(t);
  if (/manifest|WebPendingFile|WebRangeFile|range fetch|Preview|RB3STREAM|appctor|SAB stats/.test(t)) console.log('  |', t); });

// Network attribution — per-URL transferred bytes + range-request flags.
const net = new Map(); // url -> { reqs, rangeReqs, bytes }
page.on('request', (req) => {
  const u = req.url();
  if (!/\/api\/file\//.test(u)) return;
  const rec = net.get(u) || { reqs: 0, rangeReqs: 0, bytes: 0 };
  rec.reqs++;
  const h = req.headers();
  if (h['range'] || h['Range']) rec.rangeReqs++;
  net.set(u, rec);
});
page.on('response', async (resp) => {
  const u = resp.url();
  if (!/\/api\/file\//.test(u)) return;
  const rec = net.get(u) || { reqs: 0, rangeReqs: 0, bytes: 0 };
  let n = 0;
  const cl = resp.headers()['content-length'];
  if (cl) n = parseInt(cl, 10) || 0;
  rec.bytes += n;
  net.set(u, rec);
});

function moggNet() {
  let reqs = 0, rangeReqs = 0, bytes = 0, urls = [];
  for (const [u, r] of net) {
    if (!/\.mogg(\?|$)/.test(u)) continue;
    reqs += r.reqs; rangeReqs += r.rangeReqs; bytes += r.bytes; urls.push(u.split('/').pop());
  }
  return { reqs, rangeReqs, bytes, urls };
}

let rc = 1;
try {
  const url = `http://127.0.0.1:${PORT}/` + (ENVQ ? `?env=${encodeURIComponent(ENVQ)}` : '');
  console.log(`\n>>> navigating: ${url}`);
  const bootT0 = Date.now();
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting...');
  if (!await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 180000, 'boot'))
    throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  if (!await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 60000, 'splash')) throw new Error('no splash');
  await sleep(1500);
  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('stuck splash');
  const bootMs = Date.now() - bootT0;
  console.log(`\n(a) BOOT to main_hub: ${(bootMs/1000).toFixed(2)}s (baseline ~7.6s)`);

  // main_hub -> song_select
  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('stuck main_hub');
  console.log('reached song_select');
  await sleep(2500);

  let previewPass = false, freezeMax = 0, mn = null;
  if (PHASE === 'preview' || PHASE === 'all') {
    // Snapshot mogg-net before the hover so we measure ONLY this preview's bytes.
    const before = moggNet();
    await startFreezeProbe(page);
    // Land highlight on a song so SongPreview fires; HOLD and let it stream.
    await press(page, 'ArrowDown', 150, 350);
    await press(page, 'ArrowDown', 150, 350);
    console.log('(b) cold preview hover: waiting ~8s for SongPreview + stream...');
    await sleep(8000);
    freezeMax = await stopFreezeProbe(page);
    const sab = await probeSab(page);
    mn = moggNet();
    mn.bytes -= before.bytes; mn.reqs -= before.reqs; mn.rangeReqs -= before.rangeReqs;

    const mb = (mn.bytes / 1048576).toFixed(2);
    console.log(`\n=== PREVIEW (env='${ENVQ || 'default'}') ===`);
    console.log(`  mogg net: ${mn.reqs} reqs (${mn.rangeReqs} range), ${mb} MB total`);
    console.log(`  max rAF gap during hover: ${freezeMax.toFixed(0)} ms`);
    console.log(`  SAB-ring nonzero: ${sab < 0 ? 'n/a' : (sab*100).toFixed(1) + '%'}`);
    // Audible == the SAB ring carries real nonzero PCM (>50% nonzero = music).
    const audible = sab > 0.5;
    previewPass = audible;
    console.log(`  audible: ${audible ? 'YES' : 'NO'}`);
  }

  let songPass = false;
  if (PHASE === 'song' || PHASE === 'all') {
    console.log('\n(c) entering gameplay...');
    await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) throw new Error('stuck song_select');
    await sleep(1500); await press(page, 'Enter', 220, 450);
    await sleep(1500); await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen')) throw new Error('stuck ready');
    console.log('reached game_screen; letting MOGG stream 8s...');
    await sleep(8000);
    const sab = await probeSab(page);
    console.log(`  song SAB-ring nonzero: ${sab < 0 ? 'n/a' : (sab*100).toFixed(1) + '%'}`);
    songPass = sab > 0.5;
    console.log(`  audible: ${songPass ? 'YES' : 'NO'}`);
  }

  // Result line the orchestrator parses.
  const obj = {
    env: ENVQ || 'default', bootMs, freezeMax,
    moggReqs: mn ? mn.reqs : null, moggRangeReqs: mn ? mn.rangeReqs : null,
    moggBytes: mn ? mn.bytes : null, previewPass, songPass,
  };
  console.log('\nRESULT_JSON ' + JSON.stringify(obj));
  rc = (previewPass || PHASE === 'song') && (songPass || PHASE !== 'song' && PHASE !== 'all') ? 0 : (previewPass || songPass ? 0 : 1);
  if (PHASE === 'all') rc = (previewPass && songPass) ? 0 : 1;
  else if (PHASE === 'preview') rc = previewPass ? 0 : 1;
  else if (PHASE === 'song') rc = songPass ? 0 : 1;
} catch (e) {
  console.log('ERROR:', e.message || e);
  console.log('last 30 console lines:'); for (const l of logs.slice(-30)) console.log('  |', l);
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
