#!/usr/bin/env node
/**
 * web-audio-tone-verify.mjs — DETERMINISTIC single-run proof of the web audio
 * rate fix. Forces the AudioContext to --force Hz (default 48000), injects a pure
 * --tone Hz sine generated at the ENGINE/mix rate (44100) into the mix, lets the
 * resampler convert it to the ctx rate, captures the resampled output (tagged at
 * the ctx rate) and downloads it. A correct resampler keeps the captured tone at
 * EXACTLY --tone Hz. The chipmunk bug would shift it to tone*ctxRate/44100.
 *
 * Usage: node web-audio-tone-verify.mjs --force 48000 --tone 440 --out /tmp/x.wav
 */
import { launchBrowser, createCapture } from './lib/core.mjs';
const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const FORCE = parseInt(arg('--force', '48000'), 10) || 48000;
const TONE = parseInt(arg('--tone', '440'), 10) || 440;
const OUT = arg('--out', `/tmp/rb3_tone_${FORCE}.wav`);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
// PumpAudio only runs while a stream is active, so we must reach a song preview
// first (which starts the audio pump); the debug tone then OVERRIDES the preview
// mix content, giving a deterministic known-frequency capture.
const state = (page) => page.evaluate(() => ({ screen: window.rb3CurrentScreen || '' }));
async function press(page, key, hold = 220, gap = 350) {
  await page.keyboard.down(key); await sleep(hold); await page.keyboard.up(key); await sleep(gap);
}
async function navToPreview(page) {
  // boot -> main_hub
  for (let i = 0; i < 50; i++) { const s = await state(page); if (/main_hub|main_menu/.test(s.screen)) break; await sleep(800); }
  await press(page, 'Enter'); await sleep(800);
  for (let i = 0; i < 7; i++) { const s = await state(page); if (/song_select/.test(s.screen)) break; await press(page, 'Enter'); await sleep(900); }
  for (let i = 0; i < 20; i++) { const s = await state(page); if (/song_select/.test(s.screen)) break; await sleep(500); }
  // scroll once to trigger a preview (starts the audio pump)
  await press(page, 'ArrowDown'); await sleep(3000);
}

(async () => {
  const { browser, page } = await launchBrowser(PORT, { noGoto: true });
  await page.addInitScript((forceHz) => {
    const Native = window.AudioContext || window.webkitAudioContext;
    function Wrapped(opts) { return new Native(Object.assign({}, opts || {}, { sampleRate: forceHz })); }
    Wrapped.prototype = Native.prototype;
    window.AudioContext = Wrapped; window.webkitAudioContext = Wrapped;
  }, FORCE);
  createCapture(page, { filter: /AudioDevice|RESAMPLING|debug tone|capture/ });
  let downloaded = null;
  page.on('download', async (d) => { await d.saveAs(OUT); downloaded = OUT; console.log(`    [download] ${OUT}`); });

  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

  // reach a song preview so the audio pump is running
  await navToPreview(page);
  const ready = await page.evaluate(() => !!(window._rb3Audio && window._rb3Audio.started)).catch(() => false);
  console.log(`    audio started=${ready}, at preview`);

  // enable the tone (overrides the live preview mix), then capture
  await page.evaluate((hz) => {
    if (window.Module && window.Module.ccall) window.Module.ccall('rb3_debug_tone', null, ['number'], [hz]);
  }, TONE).catch(e => console.log('tone enable err', e));
  await sleep(400);

  await page.evaluate(() => window.rb3CaptureAudio && window.rb3CaptureAudio());
  console.log('    [capture] tone capture started...');
  // keep the preview/pump alive while the capture fills (~3s). Don't press keys
  // that would change the song; just wait with periodic harmless re-press of the
  // current selection is risky, so rely on the running preview pump.
  await sleep(5000);
  const ctxRate = await page.evaluate(() => window._rb3Audio && window._rb3Audio.ctx ? window._rb3Audio.ctx.sampleRate : null);
  await page.evaluate(() => window.rb3DownloadAudio && window.rb3DownloadAudio());
  await sleep(2500);
  // turn tone off
  await page.evaluate(() => { if (window.Module && window.Module.ccall) window.Module.ccall('rb3_debug_tone', null, ['number'], [0]); });

  console.log(`\n==== TONE VERIFY (force ${FORCE}, tone ${TONE}Hz) ====`);
  console.log(`ctxRate=${ctxRate}  expectedCapturedTone=${TONE}Hz  chipmunkWouldBe=${(TONE*ctxRate/44100).toFixed(1)}Hz`);
  console.log(`downloaded=${downloaded || 'FAILED'}`);
  console.log('=================================================\n');
  await browser.close();
  if (!downloaded) process.exit(2);
})().catch(e => { console.error(e); process.exit(1); });
