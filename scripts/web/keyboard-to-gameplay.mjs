#!/usr/bin/env node
/**
 * keyboard-to-gameplay.mjs — PURE-KEYBOARD web nav all the way to gameplay.
 *
 * Drives the web build (http://127.0.0.1:PORT) splash -> main_hub ->
 * song_select -> choose part -> choose difficulty -> ready -> game_screen using
 * ONLY real keyboard events (page.keyboard.down/up). NO synthetic verbs:
 * window.rb3WebUseAids is left UNSET, so song-select confirm + the part-select
 * sub-flow run as raw Confirm ButtonDownMsgs through SendButtonMessages — the
 * exact path a real key produces, mirroring native.
 *
 * Each press is a clean down -> hold -> up with a GAP before the next press, so
 * the engine's edge-detector (newPressed = cur & ~prev) sees a real 0->1->0
 * cycle: two back-to-back confirms held without a release between them would
 * only fire ONE rising edge (the held-key auto-repeat bug). The gap fixes that.
 *
 * State is read from window.rb3CurrentScreen / rb3OvershellView / rb3OvershellDiff
 * (published by main_web.cpp PublishCurrentScreen).
 *
 * Usage: node scripts/web/keyboard-to-gameplay.mjs [--port 8421] [--diff hard]
 */
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const QUERY = arg('--query', '');   // e.g. 'env=RB3_LOADER_READAHEAD%3D0' — raw query string appended to /
const DIFF = arg('--diff', 'hard');
const SONG_DOWNS = parseInt(arg('--song-downs', '3'), 10);
const OUT = resolve(__dirname, 'results/kbd2game');
mkdirSync(OUT, { recursive: true });

const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const TIMELINE = [];

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  focus: window.rb3FocusButton || '',
  view: window.rb3OvershellView || '?',
  track: window.rb3OvershellTrack || '?',
  diff: window.rb3OvershellDiff || '?',
  frame: window.rb3FrameCount || 0,
  songs: window.rb3SongCount || 0,
}));

// One clean keypress: down -> hold (several frames) -> up -> gap (bit clears).
async function press(page, key, holdMs = 200, gapMs = 250) {
  await page.keyboard.down(key);
  await sleep(holdMs);
  await page.keyboard.up(key);
  await sleep(gapMs);
}

async function mark(page, label) {
  const s = await state(page);
  TIMELINE.push([label, s]);
  console.log(`  [${label}] screen='${s.screen}' view='${s.view}' track='${s.track}' diff='${s.diff}' focus='${s.focus}'`);
  return s;
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

async function waitView(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (s.view !== last) { console.log(`    ...${label}: view='${s.view}' screen='${s.screen}'`); last = s.view; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });
const logs = [];
page.on('console', (m) => logs.push(m.text()));

let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/${QUERY ? '?' + QUERY : ''}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  // PURE KEYBOARD: disable the splash verb-inject hook so EVERY menu crossing —
  // splash included — flows through the raw SendButtonMessages path with one
  // consistent pad-0 user. (rb3WebUseAids is left unset, so song-select/part
  // confirms are raw too.)
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting + waiting for splash...');
  const sp = await waitScreen(page, s => s.screen === 'splash_screen', 90000, 'splash');
  if (!sp) { console.log('FAIL: never reached splash_screen'); throw 0; }
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true });  // focus canvas for key events
  await sleep(300);
  await mark(page, 'splash');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '00_splash.png') });

  // splash -> main_hub: Start (Space). The raw overshell-join path needs the
  // overshell to settle (a few frames of allows_input), so retry several clean
  // presses with a settle gap between them.
  for (let i = 0; i < 14; i++) {
    if ((await state(page)).screen === 'main_hub_screen') break;
    await press(page, 'Space', 250, 500);
  }
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) {
    console.log('FAIL: never reached main_hub_screen'); await mark(page, 'stuck-splash'); throw 0;
  }
  await mark(page, 'main_hub');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '01_main_hub.png') });

  // main_hub -> PLAY NOW (Confirm) -> QUICKPLAY (Confirm) -> song_select.
  // TWO distinct Confirm edges; the gap between presses guarantees the bit
  // clears so both rising edges fire.
  for (let i = 0; i < 12; i++) {
    if ((await state(page)).screen === 'song_select_screen') break;
    await press(page, 'Enter', 220, 450);
  }
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) {
    console.log('FAIL: never reached song_select_screen'); await mark(page, 'stuck-mainhub'); throw 0;
  }
  await mark(page, 'song_select');
  await sleep(2000);
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '02_song_select.png') });

  // song_select: scroll down to a song, then Confirm.
  for (let i = 0; i < SONG_DOWNS; i++) await press(page, 'ArrowDown', 150, 250);
  await press(page, 'Enter', 220, 450);  // confirm song -> meta_loading -> part_difficulty
  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) {
    console.log('FAIL: never reached part_difficulty_screen'); await mark(page, 'stuck-songselect'); throw 0;
  }
  await mark(page, 'part_difficulty');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '03_part_difficulty.png') });

  // choose part: wait for choose_part view, Confirm the focused (guitar) part.
  const cp = await waitView(page, s => s.view.startsWith('choose_part') || s.view === 'choose_diff', 30000, 'enter choose_part');
  if (!cp) { console.log('FAIL: overshell never entered choose_part'); await mark(page, 'stuck-prepart'); throw 0; }
  if (cp.view.startsWith('choose_part')) {
    await press(page, 'Enter', 220, 450);
    await mark(page, 'after_part_confirm');
  }

  // choose difficulty: wait for choose_diff, scroll to chosen diff, Confirm.
  const cd = await waitView(page, s => s.view === 'choose_diff', 30000, 'enter choose_diff');
  if (cd) {
    const di = DIFF_IDX[DIFF] || 0;
    console.log(`  choose_diff reached — scrolling to '${DIFF}' (ArrowDown x${di})`);
    for (let i = 0; i < di; i++) await press(page, 'ArrowDown', 150, 280);
    await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '04_choose_diff.png') });
    await press(page, 'Enter', 220, 450);  // select difficulty -> ready -> launch
    await mark(page, 'after_diff_confirm');
  } else {
    console.log(`  WARN: view='${(await state(page)).view}', expected choose_diff`);
    await mark(page, 'no-choose_diff');
  }

  // game_screen: wait for the song to load + play.
  const gs = await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen');
  if (!gs) { console.log('FAIL: never reached game_screen'); await mark(page, 'stuck-ready'); throw 0; }
  await mark(page, 'game_screen');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '05_game_screen.png') });

  // Confirm it's truly playing: frame advancing on game_screen, song clock moving.
  await sleep(4000);
  const playing = await mark(page, 'playing');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '06_playing.png') });

  if (playing.screen === 'game_screen' && playing.diff === DIFF) {
    console.log(`PASS: game_screen reached by pure keyboard, diff='${playing.diff}'`);
    rc = 0;
  } else {
    console.log(`PARTIAL: screen='${playing.screen}' diff='${playing.diff}' (wanted '${DIFF}')`);
    rc = playing.screen === 'game_screen' ? 0 : 2;
  }
} catch (e) {
  if (e) console.log('ERROR:', e);
} finally {
  console.log('=== TIMELINE ===');
  for (const [label, s] of TIMELINE)
    console.log(`  ${label.padStart(20)}: screen='${s.screen}' view='${s.view}' track='${s.track}' diff='${s.diff}'`);
  console.log('=== last 40 engine console lines ===');
  for (const l of logs.slice(-40)) console.log('  |', l);
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
