#!/usr/bin/env node
/**
 * web-audio-pitch-verify.mjs — END-TO-END proof the web resampler fixes the
 * chipmunk. Boots the web build with the AudioContext forced to a rate (default
 * 48000, the common hardware-locked case), navigates to a song preview, captures
 * the post-resample audio that is pushed to the SAB/worklet (rb3CaptureAudio() ->
 * rb3DownloadAudio()), and writes it to a WAV. The WAV is tagged with the DEVICE
 * rate; when played at that rate it should have the SAME pitch as the 44100 mix.
 *
 * Run twice: --force 44100 (control, no resample) and --force 48000 (fix path).
 * Then measure the dominant pitch of each WAV; they must match (~equal) — proving
 * the 48000 path is NO LONGER 1.0884x fast.
 *
 * Usage: node web-audio-pitch-verify.mjs --force 48000 --out /tmp/rb3_web_48k.wav
 */
import { launchBrowser, createCapture } from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const FORCE = parseInt(arg('--force', '48000'), 10) || 48000;
const OUT = arg('--out', `/tmp/rb3_web_force${FORCE}.wav`);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

async function press(page, key, hold = 220, gap = 350) {
  await page.keyboard.down(key); await sleep(hold);
  await page.keyboard.up(key); await sleep(gap);
}
const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
}));
async function waitScreen(page, pred, ms, label) {
  const dl = Date.now() + ms; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.screen !== last) { console.log(`    ...${label}: '${s.screen}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}

(async () => {
  const { browser, context, page } = await launchBrowser(PORT, { noGoto: true });
  await page.addInitScript((forceHz) => {
    const Native = window.AudioContext || window.webkitAudioContext;
    function Wrapped(opts) {
      const o = Object.assign({}, opts || {}, { sampleRate: forceHz });
      return new Native(o);
    }
    Wrapped.prototype = Native.prototype;
    window.AudioContext = Wrapped; window.webkitAudioContext = Wrapped;
    window.rb3NoSplashHook = 1;
  }, FORCE);

  const cap = createCapture(page, { filter: /AudioDevice|RESAMPLING|capture/ });

  // capture the WAV download
  let downloaded = null;
  context.on('page', () => {});
  page.on('download', async (d) => {
    const path = OUT;
    await d.saveAs(path);
    downloaded = path;
    console.log(`    [download] saved ${path}`);
  });

  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

  // clean keyboard nav to song_select (mirrors web-song-preview-audio.mjs)
  await waitScreen(page, s => /main_hub|main_menu/.test(s.screen), 60000, 'boot');
  await press(page, 'Enter'); await sleep(800);
  // drill into quickplay -> song select
  for (let i = 0; i < 6; i++) {
    const s = await state(page);
    if (/song_select/.test(s.screen)) break;
    await press(page, 'Enter'); await sleep(900);
  }
  await waitScreen(page, s => /song_select/.test(s.screen), 40000, 'songsel');
  // scroll to trigger a preview
  await press(page, 'ArrowDown'); await sleep(2500);
  await press(page, 'ArrowDown'); await sleep(3500);

  // start capture, wait, download
  await page.evaluate(() => { if (window.rb3CaptureAudio) window.rb3CaptureAudio(); });
  console.log('    [capture] started, waiting 3.5s...');
  await sleep(3800);
  const stats = await page.evaluate(() => {
    const a = window._rb3Audio;
    return { ctxRate: a && a.ctx ? a.ctx.sampleRate : null };
  });
  await page.evaluate(() => { if (window.rb3DownloadAudio) window.rb3DownloadAudio(); });
  await sleep(2500);

  console.log(`\n==== PITCH-VERIFY (force ${FORCE}) ====`);
  console.log(`ctxRate=${stats.ctxRate}  downloaded=${downloaded || 'FAILED'}`);
  console.log('=======================================\n');
  await browser.close();
  if (!downloaded) process.exit(2);
})().catch(e => { console.error(e); process.exit(1); });
