#!/usr/bin/env node
/**
 * _sharpen_chunk_smoke.mjs — Lane B smoke (research/14): chunked sharpen-sidecar
 * fetch on web. UNTHROTTLED localhost, DEBUG build. Confirms:
 *   (1) the chunk pump runs (RB3_SHARPEN_DBG "chunk landed" lines, 256KB paced),
 *   (2) the assembly completes and is written to MEMFS ("chunk assembly COMPLETE"),
 *   (3) the existing load path runs unchanged -> sharpen reaches COMPLETE 15/15.
 * The throttled A/B (underruns vs T2 baselines) is the INTEGRATOR's gate, not this.
 *
 * Usage: node scripts/web/_sharpen_chunk_smoke.mjs --port 8797 [--chunk-kb 256]
 * (server must already run with RB3_WEB_DOWNSCALE=1 serving the debug build)
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8797'), 10);
const CHUNK_KB = arg('--chunk-kb', '256');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const L = m => console.log(`[chunk-smoke] ${m}`);

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
  songMs: (window.rb3SongMs|0) || 0,
}));
async function press(page, key, holdMs = 220, gapMs = 350) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key); await sleep(gapMs);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.screen !== last) { L(`  ...${label}: '${s.screen}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(400);
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

const chunkLines = [];   // "chunk pump start" / "chunk landed" / "chunk assembly"
const sharpenLines = []; // everything RB3_SHARPEN / [sharpen]
page.on('console', m => { const t = m.text();
  if (/RB3_SHARPEN|\[sharpen\]/i.test(t)) {
    sharpenLines.push(t);
    if (/chunk/i.test(t)) chunkLines.push(t);
    console.log('  |', t.slice(0, 180));
  }
});

let rc = 1;
const result = { chunkKB: CHUNK_KB, reached: false };
try {
  await page.addInitScript((kb) => {
    window.__rb3ExtraEnv = Object.assign(window.__rb3ExtraEnv || {}, {
      RB3_PROGRESSIVE_SHARPEN: '1', RB3_SHARPEN_DBG: '1',
      RB3_SHARPEN_CHUNK_KB: kb,
      RB3_GAME_INPUT: '@1:nofail,@1:autohit',
    });
    window.rb3NoSplashHook = 1;
    window.rb3WebUseAids = 0;
    window.rb3WebTargetSong = '20thcenturyboy';
  }, CHUNK_KB);

  L(`goto (debug build, unthrottled) chunkKB=${CHUNK_KB}`);
  await page.goto(`http://127.0.0.1:${PORT}/?debug=true`, { waitUntil: 'domcontentloaded', timeout: 60000 });
  if (!await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 400000, 'boot')) throw new Error('no boot');
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(500);
  for (let i = 0; i < 24; i++) {
    const sc = (await state(page)).screen;
    if (sc === 'splash_screen' || sc === 'main_hub_screen') break;
    await press(page, 'Space', 200, 700);
  }
  if (!await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 180000, 'splash')) throw new Error('no splash');
  await sleep(2000);
  for (let i = 0; i < 14 && (await state(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 60000, 'main_hub')) throw new Error('stuck splash');
  { const dl2 = Date.now() + 150000;
    while (Date.now() < dl2 && (await state(page)).screen !== 'song_select_screen') {
      await press(page, 'Enter', 220, 450); await sleep(1800);
    } }
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 20000, 'song_select')) throw new Error('stuck main_hub');
  L('reached song_select'); await sleep(2500);
  await press(page, 'ArrowDown', 150, 250);
  await press(page, 'ArrowDown', 150, 250);
  await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 120000, 'part_difficulty')) throw new Error('stuck song_select');
  await sleep(1500); await press(page, 'Enter', 220, 450);
  await sleep(1500);
  for (let i = 0; i < 2; i++) await press(page, 'ArrowDown', 150, 280);
  await press(page, 'Enter', 220, 450);
  const g = await waitScreen(page, s => s.screen === 'game_screen', 240000, 'game_screen');
  if (!g) throw new Error('stuck ready');
  result.reached = true;
  L('REACHED game_screen');

  await waitScreen(page, s => s.songMs > 0, 60000, 'songMs>0');
  L('gameplay running — waiting for chunk pump + sharpen COMPLETE (up to 120s)');
  const dl = Date.now() + 120000;
  while (Date.now() < dl) {
    if (sharpenLines.some(l => /COMPLETE —|session COMPLETE/.test(l))) break;
    await sleep(1000);
  }

  result.chunkLandedLines = chunkLines.filter(l => /chunk landed/.test(l)).length;
  result.assemblyComplete = chunkLines.some(l => /chunk assembly COMPLETE/.test(l));
  result.assemblyLine = chunkLines.find(l => /chunk assembly/.test(l)) || null;
  result.pumpStartLine = chunkLines.find(l => /chunk pump start/.test(l)) || null;
  result.sawSharpenComplete = sharpenLines.some(l => /COMPLETE —|session COMPLETE/.test(l));
  result.completeLine = sharpenLines.find(l => /COMPLETE —|session COMPLETE/.test(l)) || null;
  L(`chunk-landed lines: ${result.chunkLandedLines}`);
  L(`assembly: ${result.assemblyLine}`);
  L(`sharpen COMPLETE: ${result.sawSharpenComplete} (${result.completeLine})`);
  rc = (result.assemblyComplete && result.sawSharpenComplete && result.chunkLandedLines >= 2) ? 0 : 1;
} catch (e) {
  L('ERROR: ' + (e.message || e));
} finally {
  console.log('RESULT ' + JSON.stringify(result));
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
