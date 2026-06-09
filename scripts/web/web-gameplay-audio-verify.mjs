#!/usr/bin/env node
/**
 * web-gameplay-audio-verify.mjs — in-browser verification that GAMEPLAY song
 * audio on the web build is CONTINUOUS (no ring underrun / silence-pad "static")
 * and is the right song.
 *
 * The bug: the native/web StreamReceiver ring was the Wii's hardcoded 2-chunk
 * 0x18000 (~1.1s) DSP-staging size, far too small to keep 11-15 decoded Vorbis
 * stems buffered ahead of real-time during gameplay -> ring starves ->
 * RenderAudio zero-fills (native silence) / the AudioWorklet silence-pads on
 * underrun (web "static"). Preview = 1 stream, trivially stays ahead -> clean.
 *
 * The fix deepened the ring to mNumBuffers*0xC000 (16 chunks ~9.1s) and raised
 * mBufSecs >= 4s on the native/web path.
 *
 * This script proves the fix two ways:
 *   (1) WORKLET UNDERRUN COUNTER (the direct discriminator): the AudioWorklet
 *       now postMessages a running {underrunEvents, underrunFrames, totalQuanta,
 *       totalFrames} summary; we poll window._rb3Audio.underruns every second
 *       during sustained gameplay and report underruns-per-second. ~0/s == the
 *       ring keeps up (static fixed); many == still starving.
 *   (2) POST-RESAMPLER WAV (the content discriminator): we trigger the 30s
 *       engine capture (sOutBuffer, exactly what is pushed to the SAB ring) and
 *       download it, then compute its per-second RMS envelope — a continuous
 *       loud run with no interior silence gaps == dropout fixed.
 *
 * Output: /tmp/rb3_web_gameplay.wav + a JSON result with the underrun timeline.
 * Usage:  node web-gameplay-audio-verify.mjs [--port 8421] [--out PATH]
 *                                            [--capturedelay MS] [--diff hard]
 */
import { chromium } from 'playwright';
import { readFileSync, writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const WAV = arg('--out', '/tmp/rb3_web_gameplay.wav');
const JSONOUT = WAV.replace(/\.wav$/, '') + '_result.json';
const DIFF = arg('--diff', 'hard');
const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };
// How long to let the MOGG stream before starting the capture (s). The fix gives
// a deep ring; we want to be well into sustained playback, past the first ~10s.
const STREAM_WARMUP_S = parseInt(arg('--warmup', '14'), 10);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?', frame: window.rb3FrameCount || 0,
}));

const underruns = (page) => page.evaluate(() => {
  const a = window._rb3Audio;
  if (!a) return { present: false, reason: 'no _rb3Audio' };
  if (!a.underruns) return { present: false, reason: 'no underruns field yet' };
  return { present: true, ...a.underruns };
});

const ctxInfo = (page) => page.evaluate(() => {
  const a = window._rb3Audio;
  return {
    crossOriginIsolated: !!self.crossOriginIsolated,
    sab: !!(a && a.sab),
    sabIsSAB: !!(a && a.sab && (a.sab instanceof SharedArrayBuffer)),
    started: !!(a && a.started),
    ctxRate: (a && a.ctx) ? a.ctx.sampleRate : 0,
    ctxState: (a && a.ctx) ? a.ctx.state : 'none',
    bufFrames: (a && a.bufFrames) ? a.bufFrames : 0,
  };
});

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
  const sr = buf.readUInt32LE(24), ch = buf.readUInt16LE(22);
  const s = new Int16Array(buf.buffer, buf.byteOffset + 44, (buf.length - 44) >> 1);
  let peak = 0, nz = 0, sq = 0;
  for (let i = 0; i < s.length; i++) { const a = Math.abs(s[i]); if (a > peak) peak = a; if (a > 64) nz++; sq += s[i]*s[i]; }
  const persec = [];
  const fps = sr * ch;
  for (let k = 0; k < s.length; k += fps) {
    let q = 0, n = 0; for (let i = k; i < Math.min(k+fps, s.length); i++) { q += s[i]*s[i]; n++; }
    persec.push(n ? Math.sqrt(q / n) : 0);
  }
  return { sr, ch, dur: s.length/(sr*ch), peak, nzPct: 100*nz/s.length, rms: Math.sqrt(sq/s.length), persec };
}

// Arms a download listener, triggers the engine capture (CAPTURE_SECONDS=30s),
// while polling the worklet underrun counter once/sec; then downloads + saves.
async function captureWithUnderrunPoll(page, captureSecs = 30) {
  const timeline = [];
  const dlP = page.waitForEvent('download', { timeout: 60000 }).catch(() => null);
  const base = await underruns(page);
  console.log(`  underrun baseline: ${JSON.stringify(base)}`);
  await page.evaluate(() => window.rb3CaptureAudio());
  const t0 = Date.now();
  // Poll for captureSecs + margin.
  const pollUntil = t0 + (captureSecs + 4) * 1000;
  let prev = base.present ? base : { underrunEvents: 0, underrunFrames: 0, totalQuanta: 0, totalFrames: 0 };
  while (Date.now() < pollUntil) {
    await sleep(1000);
    const u = await underruns(page);
    if (u.present) {
      const dEv = u.underrunEvents - (prev.underrunEvents || 0);
      const dFr = u.underrunFrames - (prev.underrunFrames || 0);
      const dQ  = u.totalQuanta - (prev.totalQuanta || 0);
      timeline.push({ t: ((Date.now()-t0)/1000).toFixed(1), dEvents: dEv, dPaddedFrames: dFr, dQuanta: dQ,
                      cumEvents: u.underrunEvents, cumPaddedFrames: u.underrunFrames });
      prev = u;
    } else {
      timeline.push({ t: ((Date.now()-t0)/1000).toFixed(1), present: false, reason: u.reason });
    }
  }
  const endScreen = await page.evaluate(() => window.rb3CurrentScreen || '');
  console.log(`  screen at capture end: '${endScreen}'`);
  await page.evaluate(() => { window.rb3AudioStats(); window.rb3DumpSAB(8); });
  await sleep(400);
  await page.evaluate(() => window.rb3DownloadAudio());
  const dl = await dlP;
  if (!dl) throw new Error('no download event from rb3DownloadAudio()');
  await dl.saveAs(WAV);
  return { timeline, baseline: base, final: await underruns(page) };
}

const browser = await chromium.launch({
  headless: true,
  // NOTE: do NOT --mute-audio; the capture + underrun counting are engine/worklet
  // side and run regardless, but keep the audio graph fully live.
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });
const logs = [];
page.on('console', m => { const t = m.text(); logs.push(t);
  if (/AudioDevice|capture|SAB|source count|underrun|stream ch|RB3STREAM|worklet|Worklet/i.test(t)) console.log('  |', t); });

let rc = 1;
const result = { ok: false };
try {
  // Use LOCALHOST so the context is a secure context -> SharedArrayBuffer works.
  await page.goto(`http://localhost:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  // rb3NoSplashHook=1: pure-keyboard menu nav (no splash verb aid).
  // rb3WebUseAids=1: arm the part-select crossing sequence that also fires
  //   `nofail` + `autohit` once gameplay is live, so the song PLAYS ITSELF and
  //   does not get booed off to lose_screen ~13s in (gives a full continuous run).
  // rb3WebTargetSong: pin the song selection so we can correlate against a known
  //   ground-truth reference. (default 20thcenturyboy; --song overrides)
  const TARGET_SONG = arg('--song', '20thcenturyboy');
  await page.evaluate((song) => {
    window.rb3NoSplashHook = 1;
    window.rb3WebUseAids = 1;
    window.rb3WebTargetSong = song;
  }, TARGET_SONG);
  result.targetSong = TARGET_SONG;
  console.log('booting...');
  if (!await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 180000, 'boot')) throw new Error('no boot screen');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);

  // Verify the secure-context preconditions (the whole web audio path depends on these).
  const ci = await ctxInfo(page);
  console.log(`  context: crossOriginIsolated=${ci.crossOriginIsolated} SAB=${ci.sab} isSAB=${ci.sabIsSAB} started=${ci.started} ctxRate=${ci.ctxRate} ctxState=${ci.ctxState} ring=${ci.bufFrames}f`);
  result.crossOriginIsolated = ci.crossOriginIsolated;
  result.sab = ci.sab; result.ctxRate = ci.ctxRate;
  if (!ci.crossOriginIsolated) console.log('  WARNING: not crossOriginIsolated — SAB/audio ring may be broken!');

  // Skip intro_movie if present.
  for (let i = 0; i < 20; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 600);
  }
  if (!await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 60000, 'splash')) throw new Error('no splash');
  await sleep(2000);

  // splash -> main_hub
  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('stuck splash');
  // main_hub -> song_select
  for (let i = 0; i < 12 && (await state(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('stuck main_hub');
  console.log('reached song_select');
  await sleep(2500);

  // song_select -> game_screen
  await press(page, 'ArrowDown', 150, 250);
  await press(page, 'ArrowDown', 150, 250);
  await press(page, 'ArrowDown', 150, 250);
  await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) throw new Error('stuck song_select');
  await sleep(1500); await press(page, 'Enter', 220, 450);          // part (guitar) confirm
  await sleep(1500);
  for (let i = 0; i < (DIFF_IDX[DIFF]||0); i++) await press(page, 'ArrowDown', 150, 280);  // difficulty
  await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen')) throw new Error('stuck ready');
  result.gameplayReached = true;
  console.log(`reached game_screen; letting MOGG stream ${STREAM_WARMUP_S}s before capture...`);
  await sleep(STREAM_WARMUP_S * 1000);

  console.log('capturing 30s of gameplay audio + polling worklet underruns...');
  const cap = await captureWithUnderrunPoll(page, 30);
  result.underrunTimeline = cap.timeline;
  result.underrunBaseline = cap.baseline;
  result.underrunFinal = cap.final;

  const st = analyzeWav(WAV);
  result.wav = { path: WAV, ...st };
  console.log(`\n=== WAV (${WAV}) ===`);
  console.log(`dur=${st.dur.toFixed(2)}s ${st.sr}Hz x${st.ch}  peak=${st.peak}  nonZero=${st.nzPct.toFixed(1)}%  RMS=${st.rms.toFixed(0)}`);
  console.log('per-second RMS: ' + st.persec.map(r => r.toFixed(0)).join(' '));

  // ---- underrun verdict ----
  const tl = cap.timeline.filter(e => e.present !== false);
  const totalPadded = cap.final && cap.final.present ? (cap.final.underrunFrames - (cap.baseline.present ? cap.baseline.underrunFrames : 0)) : -1;
  const totalQuanta = cap.final && cap.final.present ? (cap.final.totalQuanta - (cap.baseline.present ? cap.baseline.totalQuanta : 0)) : -1;
  const padRatio = (totalQuanta > 0) ? totalPadded / (totalQuanta * 128) : -1; // 128 frames/quantum
  console.log('\n=== underrun timeline (per-second deltas during capture) ===');
  for (const e of cap.timeline) console.log('  ' + JSON.stringify(e));
  console.log(`\nTotal during capture: paddedFrames=${totalPadded} quanta=${totalQuanta} (padRatio=${(padRatio*100).toFixed(3)}% of all audio frames)`);
  result.totalPaddedFrames = totalPadded;
  result.totalQuanta = totalQuanta;
  result.padRatio = padRatio;

  // ---- combined verdict ----
  // Interior dropout: a 1-second RMS bucket that goes near-silent in the middle
  // of an otherwise loud run.
  const ps = st.persec;
  const loud = ps.filter(v => v > 200);
  const interiorSilent = (() => {
    // find first & last loud index; any near-zero between them = dropout
    let first = ps.findIndex(v => v > 200), last = -1;
    for (let i = ps.length - 1; i >= 0; i--) if (ps[i] > 200) { last = i; break; }
    if (first < 0 || last < 0 || last <= first) return false;
    for (let i = first; i <= last; i++) if (ps[i] < 50) return true;
    return false;
  })();
  result.loudSeconds = loud.length;
  result.interiorSilent = interiorSilent;

  const audible = st.peak > 1500 && st.nzPct > 10 && loud.length >= 8;
  const continuous = !interiorSilent;
  const ringHealthy = (padRatio >= 0) ? (padRatio < 0.01) : null; // <1% silence-pad

  result.audible = audible;
  result.continuous = continuous;
  result.ringHealthy = ringHealthy;
  result.ok = audible && continuous && (ringHealthy !== false);

  console.log(`\nRESULT:`);
  console.log(`  audible (peak>1500, nz>10%, >=8 loud secs): ${audible}`);
  console.log(`  continuous (no interior silence buckets):    ${continuous}`);
  console.log(`  ring healthy (worklet silence-pad <1%):      ${ringHealthy}`);
  console.log(`  => ${result.ok ? 'PASS — gameplay audio continuous + audible' : 'FAIL'}`);
  rc = result.ok ? 0 : 1;
} catch (e) {
  result.error = e.message || String(e);
  console.log('ERROR:', result.error);
  console.log('last 40 console lines:'); for (const l of logs.slice(-40)) console.log('  |', l);
} finally {
  writeFileSync(JSONOUT, JSON.stringify(result, null, 2));
  console.log(`\n[json] ${JSONOUT}`);
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
