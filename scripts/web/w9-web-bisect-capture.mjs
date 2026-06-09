#!/usr/bin/env node
/**
 * w9-web-bisect-capture.mjs — WAVE 09 web runtime + stage bisect.
 *
 * Goals:
 *  (1) Read the RUNTIME AudioContext sampleRate (window._rb3Audio.ctx.sampleRate)
 *      + the engine's "RESAMPLING" log line (mix 44100 -> ctx) to confirm the
 *      resample path is taken.
 *  (2) Navigate to game_screen (real MOGG), arm the engine 30s capture
 *      (rb3CaptureAudio), wait the FULL CAPTURE_SECONDS (engine is now 30s, the
 *      stale preview harness only waited 4.2s and never got a ready capture),
 *      then rb3DownloadAudio() the post-resampler SAB-bound output WAV.
 *
 * Output: /tmp/rb3_w9_web_<phase>.wav
 * Usage: node w9-web-bisect-capture.mjs [--port 8421] [--phase song|preview]
 *        [--song beastandtheharlot] [--capsecs 32]
 */
import { chromium } from 'playwright';
import { readFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const PHASE = arg('--phase', 'song');
const SONG = arg('--song', 'beastandtheharlot');
const CAPSECS = parseInt(arg('--capsecs', '33'), 10) || 33;  // > engine CAPTURE_SECONDS(30)+margin
const WAV = `/tmp/rb3_w9_web_${PHASE}.wav`;
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
  return { sr, ch, dur: s.length/(sr*ch), peak, nzPct: 100*nz/s.length, rms: Math.sqrt(sq/s.length), n: s.length };
}

// read runtime AudioContext info from the page
async function readCtx(page) {
  return page.evaluate(() => {
    const a = window._rb3Audio;
    if (!a) return { ok: false, reason: 'no _rb3Audio' };
    return {
      ok: true,
      ctxRate: a.ctx ? a.ctx.sampleRate : null,
      ctxState: a.ctx ? a.ctx.state : null,
      started: !!a.started,
      bufFrames: a.bufFrames,
      underruns: a.underruns || null,
    };
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
page.on('console', m => { const t = m.text(); logs.push(t);
  if (/AudioDevice|RESAMPLING|capture|SAB|source count|ctx |Hz|underrun|nonZero/.test(t)) console.log('  |', t); });

let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate((song) => { window.rb3NoSplashHook = 1; window.rb3WebTargetSong = song; }, SONG);
  console.log(`booting (song=${SONG} phase=${PHASE} capsecs=${CAPSECS})...`);
  if (!await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 180000, 'boot')) throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  if (!await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 60000, 'splash')) throw new Error('no splash');
  await sleep(2000);
  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('stuck splash');
  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('stuck main_hub');
  console.log('reached song_select');
  await sleep(2500);

  // --- report the runtime ctx rate as soon as audio is initialized ---
  let ctxInfo = await readCtx(page);
  console.log('\n=== RUNTIME AUDIO CTX (at song_select) ===');
  console.log(JSON.stringify(ctxInfo));
  await page.evaluate(() => window.rb3AudioStats());
  await sleep(300);

  if (PHASE === 'preview') {
    await press(page, 'ArrowDown', 150, 350);
    await press(page, 'ArrowDown', 150, 350);
    console.log('on a song; waiting ~6s for SongPreview...');
    await sleep(6000);
  } else {
    // song_select -> game_screen, with the target song pinned
    await page.evaluate((song) => { window.rb3WebTargetSong = song; window.rb3WebUseAids = 1; }, SONG);
    await sleep(1000);
    await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) {
      // fallback: scroll then enter
      await press(page, 'ArrowDown', 150, 250);
      await press(page, 'Enter', 220, 450);
      if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 40000, 'part_difficulty2')) throw new Error('stuck song_select');
    }
    await sleep(1500); await press(page, 'Enter', 220, 450);
    await sleep(1500);
    for (let i = 0; i < 2; i++) await press(page, 'ArrowDown', 150, 280);  // hard-ish
    await press(page, 'Enter', 220, 450);
    if (!await waitScreen(page, s => s.screen === 'game_screen', 150000, 'game_screen')) throw new Error('stuck ready');
    console.log('reached game_screen; letting MOGG stream 8s...');
    await sleep(8000);
  }

  // re-read ctx now that audio is definitely running
  ctxInfo = await readCtx(page);
  console.log('\n=== RUNTIME AUDIO CTX (capture point) ===');
  console.log(JSON.stringify(ctxInfo));

  // === arm engine 30s capture, wait FULL window, download ===
  console.log(`\narming rb3CaptureAudio() — waiting ${CAPSECS}s for engine CAPTURE_SECONDS...`);
  const dlP = page.waitForEvent('download', { timeout: (CAPSECS + 20) * 1000 }).catch(() => null);
  await page.evaluate(() => window.rb3CaptureAudio());
  // poll capture progress via rb3AudioStats every 5s
  const t0 = Date.now();
  let ready = false;
  while ((Date.now() - t0) / 1000 < CAPSECS + 10) {
    await sleep(5000);
    await page.evaluate(() => window.rb3AudioStats());
    await sleep(200);
    const last = logs.slice(-12).join('\n');
    const m = last.match(/capture: active=(\d+), ready=(\d+), pos=(\d+)\/(\d+)/);
    if (m) {
      console.log(`  capture progress: active=${m[1]} ready=${m[2]} pos=${m[3]}/${m[4]} (${((Date.now()-t0)/1000).toFixed(0)}s)`);
      if (m[2] === '1') { ready = true; break; }
    }
  }
  await sleep(500);
  await page.evaluate(() => { window.rb3AudioStats(); window.rb3DumpSAB(8); window.rb3DownloadAudio(); });
  const dl = await dlP;
  if (!dl) throw new Error(`no download event (ready=${ready}) — capture may not have completed`);
  await dl.saveAs(WAV);
  console.log(`WAV saved to ${WAV}`);

  const st = analyzeWav(WAV);
  console.log(`\n=== WAV (${WAV}) ===`);
  console.log(`dur=${st.dur.toFixed(2)}s ${st.sr}Hz x${st.ch}  peak=${st.peak}  nonZero=${st.nzPct.toFixed(1)}%  RMS=${st.rms.toFixed(0)}`);
  ctxInfo = await readCtx(page);
  console.log('final ctx info: ' + JSON.stringify(ctxInfo));
  rc = (st.peak > 1000 && st.nzPct > 5) ? 0 : 1;
  console.log(`\nRESULT: ${rc === 0 ? 'CAPTURED audible' : 'capture silent/quiet'}`);
} catch (e) {
  console.log('ERROR:', e.message || e);
  console.log('last 30 console lines:'); for (const l of logs.slice(-30)) console.log('  |', l);
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
