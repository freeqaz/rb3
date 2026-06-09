#!/usr/bin/env node
/**
 * web-music-force-capture.mjs — Wave 08 investigation A.
 * Capture REAL in-browser MUSIC (preview or song) through the user's actual
 * pipeline, with the AudioContext FORCED to a chosen rate so the 44100->ctx
 * linear resampler in AudioDevice_Web.cpp PumpAudio is exercised (the default
 * headless ctx is 44100 = the 1:1 FAST path that HIDES the resampler).
 *
 * The capture (rb3CaptureAudio/rb3DownloadAudio) records the POST-resample
 * device-rate output, i.e. exactly what the user hears.
 *
 *   --phase preview|song   which audio to capture
 *   --force <hz>           force ctx.sampleRate (0 = no override, native rate)
 *   --out <wav>            output path
 *   --port <n>             server port (default 8421)
 */
import { chromium } from 'playwright';
import { readFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PHASE = arg('--phase', 'song');
const FORCE = parseInt(arg('--force', '48000'), 10);
const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };
const WAV = arg('--out', `/tmp/rb3_web_force_${PHASE}_${FORCE}.wav`);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?', frame: window.rb3FrameCount || 0,
}));

async function press(page, key, holdMs = 220, gapMs = 350) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key);   await sleep(gapMs);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.screen !== last) { console.log(`    ...${label}: screen='${s.screen}' view='${s.view}'`); last = s.screen; }
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
  const persec = [];
  const fps = sr * ch;
  for (let k = 0; k < s.length; k += fps) {
    let q = 0, n = 0; for (let i = k; i < Math.min(k+fps, s.length); i++) { q += s[i]*s[i]; n++; }
    persec.push(Math.sqrt(q / n));
  }
  return { sr, ch, dur: s.length/(sr*ch), peak, nzPct: 100*nz/s.length, rms: Math.sqrt(sq/s.length), persec };
}

async function captureAudio(page) {
  const dlP = page.waitForEvent('download', { timeout: 15000 }).catch(() => null);
  await page.evaluate(() => window.rb3CaptureAudio());
  await sleep(4200);
  await page.evaluate(() => { window.rb3AudioStats(); window.rb3DumpSAB(8); });
  await sleep(400);
  await page.evaluate(() => window.rb3DownloadAudio());
  const dl = await dlP;
  if (!dl) throw new Error('no download event from rb3DownloadAudio()');
  await dl.saveAs(WAV);
}

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });

// FORCE every AudioContext to FORCE Hz (unless FORCE<=0). Mirrors a browser that
// clamps to its hardware rate (the common real-world case the headless box does
// NOT reproduce — it returns 44100 = the 1:1 fast path).
if (FORCE > 0) {
  await page.addInitScript((forceHz) => {
    const Native = window.AudioContext || window.webkitAudioContext;
    window.__forcedHz = forceHz;
    function Wrapped(opts) {
      const o = Object.assign({}, opts || {}, { sampleRate: forceHz });
      const ctx = new Native(o);
      window.__lastCtxRate = ctx.sampleRate;
      return ctx;
    }
    Wrapped.prototype = Native.prototype;
    window.AudioContext = Wrapped;
    window.webkitAudioContext = Wrapped;
  }, FORCE);
}

const logs = [];
page.on('console', m => { const t = m.text(); logs.push(t);
  if (/AudioDevice|capture|SAB|source count|Preview|stream ch|RB3STREAM|RESAMPL|ctx|rate/i.test(t)) console.log('  |', t); });

let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log(`booting... (FORCE=${FORCE} Hz, phase=${PHASE})`);
  if (!await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 180000, 'boot')) throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);
  // Read back the forced ctx rate as soon as audio exists.
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  if (!await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 60000, 'splash')) throw new Error('no splash');
  await sleep(2000);

  const ctxInfo = await page.evaluate(() => {
    const a = window._rb3Audio;
    return { forced: window.__forcedHz || null, lastCtxRate: window.__lastCtxRate || null,
      present: !!a, ctxRate: (a && a.ctx) ? a.ctx.sampleRate : null,
      state: (a && a.ctx) ? a.ctx.state : null };
  }).catch(() => null);
  console.log('CTX INFO:', JSON.stringify(ctxInfo));

  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('stuck splash');
  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('stuck main_hub');
  console.log(`reached song_select`);
  await sleep(2500);

  if (PHASE === 'preview') {
    await press(page, 'ArrowDown', 150, 350);
    await press(page, 'ArrowDown', 150, 350);
    console.log('on a song; waiting ~6s for SongPreview...');
    await sleep(6000);
    console.log('capturing preview audio...');
    await captureAudio(page);
  } else {
    await press(page, 'ArrowDown', 150, 250);
    await press(page, 'ArrowDown', 150, 250);
    await press(page, 'ArrowDown', 150, 250);
    await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) throw new Error('stuck song_select');
    await sleep(1500); await press(page, 'Enter', 220, 450);
    await sleep(1500);
    for (let i = 0; i < (DIFF_IDX['hard']||0); i++) await press(page, 'ArrowDown', 150, 280);
    await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen')) throw new Error('stuck ready');
    console.log('reached game_screen; letting MOGG stream 8s...');
    await sleep(8000);
    console.log('capturing song audio...');
    await captureAudio(page);
  }

  const st = analyzeWav(WAV);
  console.log(`\n=== WAV (${WAV}) ===`);
  console.log(`dur=${st.dur.toFixed(2)}s ${st.sr}Hz x${st.ch}  peak=${st.peak}  nonZero=${st.nzPct.toFixed(1)}%  RMS=${st.rms.toFixed(0)}`);
  console.log('per-second RMS: ' + st.persec.map(r => r.toFixed(0)).join(', '));
  const pass = st.peak > 1500 && st.nzPct > 10;
  console.log(`\nRESULT: ${pass ? 'PASS — audible' : 'FAIL — silent/quiet'} (capture sr=${st.sr})`);
  rc = pass ? 0 : 1;
} catch (e) {
  console.log('ERROR:', e.message || e);
  console.log('last 40 console lines:'); for (const l of logs.slice(-40)) console.log('  |', l);
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
