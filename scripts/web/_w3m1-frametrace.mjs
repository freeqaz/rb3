#!/usr/bin/env node
/**
 * _w3m1-frametrace.mjs — Wave 3 / M1 measurement harness.
 *
 * Goal: fresh per-frame counter-attributed frame-trace on the WEB build for
 *   (a) SONG START (last song_select frames + first ~5s of game_screen), and
 *   (b) SPLASH -> MAIN_HUB transition.
 *
 * Unlike _netmatrix.mjs this runs UNTHROTTLED (localhost, full speed): waves
 * 0-2 already eliminated the network/loader stalls, so M1 wants to expose the
 * residual CREATION-work spikes (pipeMs/meshMs/texMs/primeMs), not network.
 *
 * Activates the engine frame-trace via the ?env bridge
 * (RB3_FRAME_TRACE=/trace.jsonl), drives boot -> intro -> splash -> main_hub ->
 * song_select -> hover -> START -> ~5s gameplay by pure keyboard, then pulls the
 * MEMFS trace.jsonl out and prints/saves it. The trace itself carries the screen
 * name (`scr`) per frame so phase windowing is done on the trace, not on wall
 * clock — exact and robust.
 *
 * Usage:
 *   node _w3m1-frametrace.mjs --port 8434 --out /tmp/rb3perf-w3/web1 [--diff hard]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8434'), 10);
const OUT = arg('out', '/tmp/rb3perf-w3/web');
const DIFF = arg('diff', 'hard');
const SONG_DOWNS = parseInt(arg('song-downs', '3'), 10);
const GAMEPLAY_SECS = parseFloat(arg('gameplay-secs', '6'));
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };

const stateOf = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?',
  frame: window.rb3FrameCount || 0,
  booted: window.rb3AppBooted || 0,
  songs: window.rb3SongCount || 0,
})).catch(() => ({ screen: '', view: '?', diff: '?', frame: 0, booted: 0, songs: 0 }));

async function press(page, key, holdMs = 200, gapMs = 280) {
  try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(gapMs); } catch {}
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await stateOf(page);
    if (s.screen !== last) { console.log(`    [${((Date.now()-T0)/1000).toFixed(1)}s] ${label}: '${s.screen}' view='${s.view}' frame=${s.frame}`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}
async function waitView(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await stateOf(page);
    if (s.view !== last) { console.log(`    [${((Date.now()-T0)/1000).toFixed(1)}s] ${label}: view='${s.view}' screen='${s.screen}'`); last = s.view; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}

const T0 = Date.now();
const ENVQ = `RB3_FRAME_TRACE=/trace.jsonl`;
const params = ['debug=true', `env=${encodeURIComponent(ENVQ)}`].join('&');

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
const cons = [];
page.on('console', m => cons.push({ epoch: Date.now() - T0, text: m.text() }));
page.on('pageerror', e => cons.push({ epoch: Date.now() - T0, text: 'PAGEERROR: ' + e.message }));

let rc = 1;
try {
  console.log(`[w3m1] boot ?${params}  port=${PORT}`);
  await page.goto(`http://127.0.0.1:${PORT}/?${params}`, { waitUntil: 'domcontentloaded', timeout: 120000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });

  // ---- boot -> splash ----
  const sp = await waitScreen(page, s => s.screen === 'splash_screen' || s.screen === 'main_hub_screen', 120000, 'splash');
  if (!sp) { console.log('FAIL: never reached splash'); throw 0; }
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true }).catch(()=>{});
  await sleep(300);

  // ---- splash -> main_hub (THE A5 spike window) ----
  for (let i = 0; i < 16 && (await stateOf(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 500);
  const hub = await waitScreen(page, s => s.screen === 'main_hub_screen', 60000, 'main_hub');
  if (!hub) { console.log('FAIL: never reached main_hub'); throw 0; }
  await sleep(2500); // dwell so the post-transition frames settle + are recorded

  // ---- main_hub -> song_select ----
  for (let i = 0; i < 12 && (await stateOf(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 450);
  const ss = await waitScreen(page, s => s.screen === 'song_select_screen', 60000, 'song_select');
  if (!ss) { console.log('FAIL: never reached song_select'); throw 0; }
  await sleep(3000); // let highlighted-song preview fire + settle

  // ---- hover a deep song (cold), dwell for preview ----
  for (let i = 0; i < SONG_DOWNS; i++) await press(page, 'ArrowDown', 150, 250);
  await sleep(2500);

  // ---- START the song (THE T10 prime window) ----
  await press(page, 'Enter', 220, 450); // confirm song -> part_difficulty
  const pd = await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty');
  if (pd) {
    const cp = await waitView(page, s => s.view.startsWith('choose_part') || s.view === 'choose_diff', 30000, 'choose_part');
    if (cp && cp.view.startsWith('choose_part')) await press(page, 'Enter', 220, 450);
    const cd = await waitView(page, s => s.view === 'choose_diff', 30000, 'choose_diff');
    if (cd) {
      const di = DIFF_IDX[DIFF] || 0;
      for (let i = 0; i < di; i++) await press(page, 'ArrowDown', 150, 280);
      await press(page, 'Enter', 220, 450); // select diff -> ready -> launch
    }
  }
  const gs = await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen');
  if (!gs) { console.log('WARN: never reached game_screen (song-start window may be empty)'); }
  else { console.log(`    [${((Date.now()-T0)/1000).toFixed(1)}s] game_screen reached — dwelling ${GAMEPLAY_SECS}s`); }
  await sleep(GAMEPLAY_SECS * 1000);

  rc = gs ? 0 : 2;
} catch (e) {
  if (e) console.log('ERROR:', e);
} finally {
  // ---- pull MEMFS frame trace ----
  let traceText = '';
  try {
    traceText = await page.evaluate(() => { try { return FS.readFile('/trace.jsonl', { encoding: 'utf8' }); } catch (e) { return 'ERR:' + e; } });
  } catch (e) { traceText = 'EVAL_ERR:' + e; }
  writeFileSync(`${OUT}/trace.jsonl`, traceText);
  writeFileSync(`${OUT}/console.ndjson`, cons.map(e => JSON.stringify(e)).join('\n') + '\n');
  console.log(`[w3m1] trace bytes=${traceText.length}  out=${OUT}/trace.jsonl`);
  console.log('=== last 25 console lines ===');
  for (const l of cons.slice(-25)) console.log('  |', l.text);
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
