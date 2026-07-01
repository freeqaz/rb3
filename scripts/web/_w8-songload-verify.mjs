#!/usr/bin/env node
/**
 * _w8-songload-verify.mjs — verify the FULL song set loads+plays in the BROWSER
 * web build (the render-polish wave-8 wrap-up "web-songload" item).
 *
 * Native was proven (wave-7 chart-wiring: 80 wired .mid symlinks + the SongParser
 * SIGSEGV fix). This drives the freshly-built DEBUG web build (?debug=true,
 * no-store, no stale wasm) through the browser delivery path: server.py
 * /api/file follows the .mid symlinks, the engine lazily fetches each chart over
 * the wire (br-compressed, fetch() decodes transparently), and the committed
 * #ifdef HX_NATIVE SongParser fix compiles into rb3-web identically.
 *
 * Unlike keyboard-to-gameplay.mjs (which picks an arbitrary song via N ArrowDowns),
 * this selects SPECIFIC previously-dead songs BY NAME using the
 * window.rb3HighlightedSong lever (main_web.cpp PublishHighlightedSong → the
 * highlighted SortNode's token == song shortname).
 *
 * ISOLATION: each song runs in a FRESH browser session (own boot), so accumulated
 * page state can't cascade a failure across songs. For each target: boot →
 * splash → main_hub → song_select, scroll until highlightedSong matches, Confirm,
 * pick guitar/<diff>, launch, assert game_screen + advancing frame + a painted
 * highway, then SAMPLE PROMPTLY (~4s) and close before the no-input player is
 * booed off (~13s). A console scan flags WebAssets FAILED / 404 / abort / SIGSEGV
 * on a .mid/.mogg specifically.
 *
 * Usage:
 *   node scripts/web/_w8-songload-verify.mjs --port 9805 \
 *        --songs bohemianrhapsody,freebird,rehab,crazytrain,20thcenturyboy \
 *        --out /tmp/rp8-web-songload/run
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const SONGS = arg('--songs', 'bohemianrhapsody,freebird,rehab,crazytrain,20thcenturyboy')
  .split(',').map(s => s.trim()).filter(Boolean);
const OUT = arg('--out', '/tmp/rp8-web-songload/run');
const DIFF = arg('--diff', 'expert');
mkdirSync(OUT, { recursive: true });

const DIFF_IDX = { easy: 0, medium: 1, hard: 2, expert: 3 };
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?',
  frame: window.rb3FrameCount || 0,
  songs: window.rb3SongCount || 0,
  hl: window.rb3HighlightedSong || '',
})).catch(() => ({ screen: '', view: '?', diff: '?', frame: 0, songs: 0, hl: '' }));

async function press(page, key, holdMs = 200, gapMs = 250) {
  try {
    await page.keyboard.down(key); await sleep(holdMs);
    await page.keyboard.up(key); await sleep(gapMs);
  } catch { /* page gone */ }
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

async function paintedPct(page, pngPath, threshold = 12) {
  try {
    await page.locator('#rb3-canvas').screenshot({ path: pngPath });
    const { PNG } = await import('pngjs');
    const { readFileSync } = await import('fs');
    const png = PNG.sync.read(readFileSync(pngPath));
    let painted = 0; const total = png.width * png.height;
    for (let p = 0; p < png.data.length; p += 4) {
      if (png.data[p] > threshold || png.data[p + 1] > threshold || png.data[p + 2] > threshold) painted++;
    }
    return Number((100 * painted / total).toFixed(2));
  } catch { return -1; }
}

// Scroll the song list until rb3HighlightedSong == target. Caps presses; flips
// direction once so we sweep the whole (wrapping) list without spinning forever.
async function scrollToSong(page, target, maxPresses = 260) {
  const want = target.toLowerCase();
  const match = (hl) => { const h = (hl || '').toLowerCase(); return h === want; };
  let s = await state(page);
  if (match(s.hl)) return s.hl;
  let dir = 'ArrowDown', pressesThisDir = 0;
  for (let i = 0; i < maxPresses; i++) {
    await press(page, dir, 80, 120);
    s = await state(page);
    if (match(s.hl)) return s.hl;
    if (++pressesThisDir > 150 && dir === 'ArrowDown') { dir = 'ArrowUp'; pressesThisDir = 0; }
  }
  return null;
}

// One isolated song run in its OWN browser.
async function runSong(song) {
  const r = { song, reached: false, paintedPct: -1, screen: '', diff: '', frameStart: 0, frameEnd: 0, hl: '', songCount: 0, error: null, midFetched: false, moggFetched: false, chart404: false, audio404: false };
  const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
      '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
      '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
  });
  const page = await browser.newPage();
  await page.setViewportSize({ width: 1280, height: 720 });
  const logs = [];
  page.on('console', (m) => logs.push(`[${m.type()}] ${m.text()}`));
  page.on('pageerror', (e) => logs.push(`[PAGEERROR] ${e.message || e}`));
  page.on('crash', () => logs.push('[CRASH] page crashed'));

  console.log(`\n=== TARGET: ${song} ===`);
  try {
    await page.goto(`http://127.0.0.1:${PORT}/?debug=true`, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await page.evaluate(() => { window.rb3NoSplashHook = 1; });

    const sp = await waitScreen(page, s => s.screen === 'splash_screen', 180000, 'splash');
    if (!sp) throw new Error('never reached splash_screen');
    await sleep(2500);
    await page.locator('#rb3-canvas').click({ force: true });
    await sleep(300);

    for (let i = 0; i < 14; i++) { if ((await state(page)).screen === 'main_hub_screen') break; await press(page, 'Space', 250, 500); }
    if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw new Error('never reached main_hub_screen');

    for (let i = 0; i < 12; i++) { if ((await state(page)).screen === 'song_select_screen') break; await press(page, 'Enter', 220, 450); }
    if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw new Error('never reached song_select_screen');
    await sleep(2500);
    r.songCount = (await state(page)).songs;
    await page.locator('#rb3-canvas').click({ force: true });

    const hl = await scrollToSong(page, song);
    r.hl = hl || (await state(page)).hl;
    if (!hl) throw new Error(`could not scroll to '${song}' (last hl='${r.hl}')`);
    console.log(`  highlighted '${hl}' — confirming`);

    await press(page, 'Enter', 220, 450);
    const pd = await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty');
    if (!pd) { r.screen = (await state(page)).screen; throw new Error('never reached part_difficulty_screen'); }

    const cp = await waitView(page, s => s.view.startsWith('choose_part') || s.view === 'choose_diff', 30000, 'choose_part');
    if (cp && cp.view.startsWith('choose_part')) await press(page, 'Enter', 220, 450);

    const cd = await waitView(page, s => s.view === 'choose_diff', 30000, 'choose_diff');
    if (cd) {
      const di = DIFF_IDX[DIFF] || 0;
      for (let i = 0; i < di; i++) await press(page, 'ArrowDown', 130, 230);
      await press(page, 'Enter', 220, 450);
    }

    const gs = await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game_screen');
    if (!gs) { r.screen = (await state(page)).screen; throw new Error('never reached game_screen'); }
    r.reached = true;
    const fs0 = await state(page); r.frameStart = fs0.frame; r.diff = fs0.diff;
    await sleep(4000);  // SAMPLE PROMPTLY: highway renders + clock advances, before the ~13s boo-off
    const fs1 = await state(page); r.frameEnd = fs1.frame; r.screen = fs1.screen;
    r.paintedPct = await paintedPct(page, resolve(OUT, `game_${song}.png`));
    console.log(`  game_screen: frame ${r.frameStart}→${r.frameEnd} (Δ${r.frameEnd - r.frameStart}), painted=${r.paintedPct}%, diff='${r.diff}'`);
  } catch (e) {
    r.error = String(e.message || e);
    console.log('  ERROR: ' + r.error);
  } finally {
    // Per-song console analysis (chart/audio delivery specifically).
    const midRe = new RegExp(`songs/${song}/${song}\\.mid`, 'i');
    const moggRe = new RegExp(`songs/${song}/${song}\\.mogg`, 'i');
    for (const l of logs) {
      if (midRe.test(l) && /bytes\)/.test(l)) r.midFetched = true;
      if (moggRe.test(l) && /(bytes\)|Range|range)/.test(l)) r.moggFetched = true;
      if (midRe.test(l) && /(FAILED|404)/i.test(l)) r.chart404 = true;
      if (moggRe.test(l) && /(FAILED|404)/i.test(l)) r.audio404 = true;
    }
    // Hard-crash scan (engine-level, not the benign dev-asset 404s).
    const crashRe = /SIGSEGV|SIGABRT|abort\(|RuntimeError|MILO_FAIL/i;
    r.hardCrashLogs = logs.filter(l => crashRe.test(l)).slice(0, 10);
    writeFileSync(resolve(OUT, `console_${song}.log`), logs.join('\n') + '\n');
    await Promise.race([browser.close(), sleep(3000)]).catch(() => {});
  }
  return r;
}

const results = [];
for (const song of SONGS) {
  // eslint-disable-next-line no-await-in-loop
  results.push(await runSong(song));
}

const pass = (r) => r.reached && r.screen === 'game_screen' && r.frameEnd > r.frameStart && r.paintedPct > 3 && !r.chart404 && !r.audio404 && (r.hardCrashLogs || []).length === 0;
const summary = {
  port: PORT, targets: SONGS, diff: DIFF,
  songCount: results.find(r => r.songCount)?.songCount || 0,
  results,
  pass: results.length === SONGS.length && results.every(pass),
};
writeFileSync(resolve(OUT, 'summary.json'), JSON.stringify(summary, null, 2));

console.log('\n=== SUMMARY ===');
for (const r of results) {
  console.log(`  ${pass(r) ? 'PASS' : 'FAIL'}  ${r.song.padEnd(20)} screen='${r.screen}' Δframe=${r.frameEnd - r.frameStart} painted=${r.paintedPct}% midFetched=${r.midFetched} hl='${r.hl}'${r.error ? ' err=' + r.error : ''}${(r.hardCrashLogs || []).length ? ' HARDCRASH=' + r.hardCrashLogs.length : ''}`);
}
console.log(`  songCount=${summary.songCount}`);
console.log(`  OVERALL: ${summary.pass ? 'PASS' : 'FAIL'}`);
console.log(`  evidence: ${OUT}`);
process.exit(summary.pass ? 0 : 2);
