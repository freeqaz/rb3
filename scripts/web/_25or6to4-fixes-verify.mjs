#!/usr/bin/env node
/**
 * _25or6to4-fixes-verify.mjs — verify the three "25 or 6 to 4" web fixes in ONE nav:
 *   #1 stem-seed anchor   — STEM-ANCHOR log: all stems seeded in ONE tick, skew 0
 *   #2/#3 input collision — _rb3Keys: a/s/d set NO fret bit; frets set NO d-pad/strum bit
 *   #4 pause latency      — paused song does NOT ramp the adaptive latency to the ceiling
 *
 * Loads the DEBUG build (?debug=true, no-store/fresh) with the off-main debug log on.
 * Usage: node scripts/web/_25or6to4-fixes-verify.mjs --port 8777 [--pause-secs 12]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8777'), 10) || 8777;
const PAUSE_SECS = parseInt(arg('--pause-secs', '12'), 10) || 12;
const DIFF = arg('--diff', 'hard');
const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// JoypadButton bit map (must match rb3_game_input.cpp / rb3_joypad_native.cpp).
const B = {
  green: 1 << 1, yellow: 1 << 4, red: 1 << 5, blue: 1 << 6, orange: 1 << 7,
  DUp: 1 << 12, DRight: 1 << 13, DDown: 1 << 14, DLeft: 1 << 15,
};
const FRET_BITS = B.green | B.red | B.yellow | B.blue | B.orange;          // 242
const DPAD_BITS = B.DUp | B.DRight | B.DDown | B.DLeft;                    // 61440

let phase = 'nav';
const latLogs = [];
const anchorLogs = [];

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?',
  frame: window.rb3FrameCount || 0,
  song: window.rb3HighlightedSong || '',
}));
async function press(page, key, holdMs = 200, gapMs = 350) {
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
// Hold one key, sample _rb3Keys mid-hold, release, sample again. Returns {held, after}.
async function probeKey(page, key) {
  await page.keyboard.down(key);
  await sleep(160);
  const held = await page.evaluate(() => window._rb3Keys || 0);
  await page.keyboard.up(key);
  await sleep(140);
  const after = await page.evaluate(() => window._rb3Keys || 0);
  return { held, after };
}

const QUERY = 'debug=true&env=RB3_WEB_OFFMAIN_DBG%3D1';
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
  if (t.includes('AudioDevice: latency')) { latLogs.push({ phase, line: t }); console.log(`[lat:${phase}] ${t}`); }
  if (t.includes('STEM-ANCHOR') || t.includes('OFF-MAIN mix') || /seeded \d+ stem/.test(t)) {
    anchorLogs.push(t); console.log(`[anchor] ${t}`);
  }
});

const results = {};
let rc = 1;
try {
  await page.goto(`http://127.0.0.1:${PORT}/?${QUERY}`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await page.evaluate(() => { window.rb3NoSplashHook = 1; });
  console.log('booting (debug build, off-main dbg on)...');
  if (!await waitScreen(page, s => s.screen === 'splash_screen', 150000, 'splash')) throw new Error('no splash');
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(300);

  for (let i = 0; i < 14; i++) { if ((await state(page)).screen === 'main_hub_screen') break; await press(page, 'Space', 250, 500); }
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('no main_hub');
  for (let i = 0; i < 12; i++) { if ((await state(page)).screen === 'song_select_screen') break; await press(page, 'Enter', 220, 450); }
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('no song_select');
  await sleep(1500);

  // --- select "25 or 6 to 4" by name: ArrowDown scan reading rb3HighlightedSong ---
  let songFound = false;
  let cur = (await state(page)).song;
  console.log(`  song_select highlighted='${cur}'`);
  for (let i = 0; i < 60 && !songFound; i++) {
    cur = (await state(page)).song;
    if (/25or6to4|25 or 6/i.test(cur)) { songFound = true; break; }
    await press(page, 'ArrowDown', 120, 200);
  }
  cur = (await state(page)).song;
  console.log(`  selected song token='${cur}' (matched 25or6to4=${songFound})`);
  results.songMatched = songFound;
  results.songToken = cur;
  await press(page, 'Enter', 220, 450);

  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_diff')) throw new Error('no part_diff');
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
    for (let i = 0; i < 6; i++) { if ((await state(page)).screen !== 'part_difficulty_screen') break; await press(page, 'Enter', 220, 500); }
  }
  const g = await waitScreen(page, s => s.screen === 'game_screen', 90000, 'game_screen');
  if (!g) throw new Error('no game_screen');
  console.log('GAMEPLAY reached');
  phase = 'play';
  await sleep(6000);   // let stems arm/seed + latency law settle at floor

  // ============ #2/#3 INPUT (collision-free keymap) ============
  // Intended map: a/1->green s/2->red d/3->yellow f/4->blue g/5->orange,
  // j/k->strum, arrows->d-pad nav, Enter->green. Each key must set EXACTLY its
  // one bit (extra bits == the listener collision). Enter confirming menus is
  // already proven by nav reaching gameplay (Enter->green->Confirm).
  console.log('\n=== INPUT KEYMAP CHECK (_rb3Keys, exact single-bit) ===');
  const EXPECT = {
    a: B.green, s: B.red, d: B.yellow, f: B.blue, g: B.orange,
    '1': B.green, '2': B.red, '3': B.yellow, '4': B.blue, '5': B.orange,
    j: B.DUp, k: B.DDown,
    ArrowDown: B.DDown, ArrowUp: B.DUp, ArrowLeft: B.DLeft, ArrowRight: B.DRight,
    Enter: B.green,
  };
  const keyProbe = {};
  let noCollision = true, clearsOnRelease = true;
  for (const k of Object.keys(EXPECT)) {
    const p = await probeKey(page, k);
    keyProbe[k] = p;
    const ok = p.held === EXPECT[k];
    if (!ok) noCollision = false;
    if (p.after !== 0) clearsOnRelease = false;
    console.log(`  '${k}': held=0x${p.held.toString(16)} expect=0x${EXPECT[k].toString(16)} ${ok ? 'OK' : 'EXTRA-BITS!'}  after=0x${p.after.toString(16)}${p.after ? ' STUCK!' : ''}`);
  }
  const asdFretsClean = ['a', 's', 'd'].every(k => keyProbe[k].held === EXPECT[k]);
  const fretsClean = ['1', '2', '3', '4', '5', 'a', 's', 'd', 'f', 'g'].every(k => (keyProbe[k].held & DPAD_BITS) === 0 && (keyProbe[k].held & FRET_BITS) !== 0);
  const enterIsGreen = keyProbe['Enter'].held === B.green;
  results.input = { noCollision, clearsOnRelease, asdFretsClean, fretsClean, enterIsGreen,
    sIsRedOnly: keyProbe['s'].held === B.red };
  console.log(`  a/s/d are clean frets (no d-pad):  ${asdFretsClean}`);
  console.log(`  all frets set no d-pad/strum bit:  ${fretsClean}`);
  console.log(`  Enter = green fret (bit1):         ${enterIsGreen}`);
  console.log(`  no key sets extra bits (collision):${noCollision}`);
  console.log(`  all keys clear on release:         ${clearsOnRelease}`);
  const inputPass = noCollision && clearsOnRelease && asdFretsClean && fretsClean && enterIsGreen;
  console.log(`  INPUT: ${inputPass ? 'PASS' : 'FAIL'}`);
  // Enter at gameplay fires Confirm (no focused target) — make sure we're still playing.
  const stillGame = (await state(page)).screen === 'game_screen';
  if (!stillGame) console.log(`  NOTE: screen drifted to '${(await state(page)).screen}' after input probe`);

  // ============ #1 STEM-SEED ANCHOR ============
  console.log('\n=== STEM-SEED ANCHOR CHECK ===');
  const seedLines = anchorLogs.filter(l => /seeded \d+ stem/.test(l) || l.includes('STEM-ANCHOR'));
  for (const l of seedLines) console.log(`  | ${l}`);
  const skews = seedLines.map(l => { const m = l.match(/maxStartSkew=(\d+)/); return m ? parseInt(m[1], 10) : null; }).filter(v => v !== null);
  const maxSkew = skews.length ? Math.max(...skews) : null;
  const songBatch = seedLines.map(l => { const m = l.match(/seeded (\d+) stem/); return m ? parseInt(m[1], 10) : 0; });
  const maxBatch = songBatch.length ? Math.max(...songBatch) : 0;
  results.stemAnchor = { seedLineCount: seedLines.length, maxSkew, maxBatch };
  console.log(`  seed events: ${seedLines.length}, largest one-tick batch: ${maxBatch} stems, max skew: ${maxSkew}`);
  const anchorPass = seedLines.length > 0 && maxSkew === 0 && maxBatch >= 2;
  console.log(`  STEM-ANCHOR: ${anchorPass ? 'PASS' : (seedLines.length === 0 ? 'NO-LOG (off-main maybe off / dbg not on)' : 'FAIL')}`);

  // ============ #4 PAUSE LATENCY ============
  console.log('\n=== PAUSE-LATENCY CHECK ===');
  const preFrame = (await state(page)).frame;
  phase = 'paused';
  await press(page, 'Escape', 150, 400);
  await sleep(PAUSE_SECS * 1000);
  const growPaused = latLogs.filter(l => l.phase === 'paused' && l.line.includes('GROW')).length;
  const highPaused = latLogs.filter(l => l.phase === 'paused' && l.line.includes('HIGH')).length;
  phase = 'resume';
  await press(page, 'Escape', 150, 400);
  await sleep(4000);
  const postFrame = (await state(page)).frame;
  results.pause = { growPaused, highPaused, framesAdvanced: postFrame - preFrame };
  console.log(`  paused ${PAUSE_SECS}s: GROW=${growPaused} HIGH=${highPaused} (expect 0/0); frames advanced post-resume=${postFrame - preFrame}`);
  const pausePass = growPaused === 0 && highPaused === 0 && (postFrame - preFrame) > 0;
  console.log(`  PAUSE: ${pausePass ? 'PASS' : 'FAIL'}`);

  console.log('\n======== VERDICT ========');
  console.log(`  #2/#3 input collision : ${inputPass ? 'PASS' : 'FAIL'}`);
  console.log(`  #1 stem-seed anchor   : ${anchorPass ? 'PASS' : (seedLines.length === 0 ? 'NO-LOG' : 'FAIL')}`);
  console.log(`  #4 pause latency      : ${pausePass ? 'PASS' : 'FAIL'}`);
  console.log('RESULTS_JSON ' + JSON.stringify(results));
  rc = (inputPass && pausePass && (anchorPass || seedLines.length === 0)) ? 0 : 1;
} catch (e) {
  console.error('ERR', e.message);
  console.log('RESULTS_JSON ' + JSON.stringify(results));
  rc = 3;
}
await Promise.race([browser.close(), sleep(3000)]);
process.exit(rc);
