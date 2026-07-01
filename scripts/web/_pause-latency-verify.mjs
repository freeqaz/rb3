#!/usr/bin/env node
/**
 * _pause-latency-verify.mjs (Bug-4) — reuse keyboard-to-gameplay's proven nav flow
 * to reach "25 or 6 to 4" gameplay, then open the PAUSE menu (Escape), hold it, and
 * confirm the adaptive latency law does NOT ramp the target to the 500ms ceiling
 * while paused; then unpause and confirm playback resumes.
 *
 * Captures every "AudioDevice: latency GROW/HIGH" console line tagged by phase.
 *
 * Usage: node scripts/web/_pause-latency-verify.mjs --port 8830 [--pause-secs 12]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8830'), 10) || 8830;
const SONG_DOWNS = parseInt(arg('--song-downs', '3'), 10);
const DIFF = arg('--diff', 'hard');
const PAUSE_SECS = parseInt(arg('--pause-secs', '14'), 10) || 14;
const QUERY = arg('--query', 'env=RB3_WEB_OFFMAIN_MIX%3D1');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };

let phase = 'nav';
const latLogs = [];

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?',
  frame: window.rb3FrameCount || 0,
}));
async function press(page, key, holdMs = 220, gapMs = 450) {
  await page.keyboard.down(key); await sleep(holdMs);
  await page.keyboard.up(key); await sleep(gapMs);
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
    if (s.view !== last) { console.log(`    ...${label}: view='${s.view}'`); last = s.view; }
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
page.on('console', (m) => {
  const t = m.text();
  if (t.includes('AudioDevice: latency') || t.includes('OFF-MAIN mix ENABLED')) {
    latLogs.push({ t: Date.now(), phase, line: t });
    console.log(`[${phase}] ${t}`);
  }
});

let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/${QUERY ? '?' + QUERY : ''}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting...');
  if (!await waitScreen(page, s => s.screen === 'splash_screen', 120000, 'splash')) throw new Error('no splash');
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(300);

  for (let i = 0; i < 14; i++) { if ((await state(page)).screen === 'main_hub_screen') break; await press(page, 'Space', 250, 500); }
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('no main_hub');
  for (let i = 0; i < 12; i++) { if ((await state(page)).screen === 'song_select_screen') break; await press(page, 'Enter', 220, 450); }
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('no song_select');
  await sleep(1500);

  for (let i = 0; i < SONG_DOWNS; i++) await press(page, 'ArrowDown', 150, 250);
  await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_diff')) throw new Error('no part_diff');
  // choose part -> choose diff: retry the confirm until the view advances (a
  // single Enter sometimes misses the rising edge).
  await waitView(page, s => s.view.startsWith('choose_part') || s.view === 'choose_diff', 30000, 'choose_part');
  for (let i = 0; i < 6; i++) {
    if ((await state(page)).view === 'choose_diff') break;
    if ((await state(page)).screen !== 'part_difficulty_screen') break;
    await press(page, 'Enter', 220, 500);
  }
  const cd = await waitView(page, s => s.view === 'choose_diff', 20000, 'choose_diff');
  if (cd) {
    const di = DIFF_IDX[DIFF] || 0;
    for (let i = 0; i < di; i++) await press(page, 'ArrowDown', 150, 280);
    for (let i = 0; i < 6; i++) {
      if ((await state(page)).screen !== 'part_difficulty_screen') break;
      await press(page, 'Enter', 220, 500);
    }
  }

  const g = await waitScreen(page, s => s.screen === 'game_screen', 60000, 'game_screen');
  if (!g) throw new Error('no game_screen');
  console.log('GAMEPLAY reached');

  // let the song play + the latency law settle at the floor
  phase = 'play_pre'; await sleep(8000);
  const preFrame = (await state(page)).frame;

  console.log('--- PRESS ESCAPE (pause) ---');
  await press(page, 'Escape', 150, 400);
  phase = 'paused';
  await sleep(PAUSE_SECS * 1000);
  const growsDuringPause = latLogs.filter(l => l.phase === 'paused' && l.line.includes('GROW')).length;
  const highDuringPause  = latLogs.filter(l => l.phase === 'paused' && l.line.includes('HIGH')).length;
  console.log(`--- paused ${PAUSE_SECS}s: GROW=${growsDuringPause} HIGH=${highDuringPause} ---`);

  console.log('--- PRESS ESCAPE (resume) ---');
  phase = 'play_post';
  await press(page, 'Escape', 150, 400);
  await sleep(5000);
  const postFrame = (await state(page)).frame;

  const allGrows = latLogs.filter(l => l.line.includes('GROW'));
  console.log('\n=== VERDICT ===');
  console.log(`frames advanced post-resume: ${postFrame - preFrame}`);
  console.log(`GROW during pause: ${growsDuringPause} (expect 0)`);
  console.log(`HIGH during pause: ${highDuringPause} (expect 0)`);
  console.log(`total GROW (whole run): ${allGrows.length}`);
  const pass = growsDuringPause === 0 && highDuringPause === 0 && (postFrame - preFrame) > 0;
  console.log(pass ? 'PASS' : 'FAIL');
  rc = pass ? 0 : 1;
} catch (e) {
  console.error('ERR', e.message);
  rc = 3;
}
await browser.close();
process.exit(rc);
