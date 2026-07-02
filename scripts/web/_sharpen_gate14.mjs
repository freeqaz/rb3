#!/usr/bin/env node
/**
 * _sharpen_gate14.mjs — research/14 INTEGRATOR gate for the chunked, mogg-yielding
 * sharpen-sidecar fetch (Lane B).
 *
 * Protocol (baseline comparability): the underrun window mirrors
 * _sharpen_audio_throttle.mjs EXACTLY — u0 snapshot 2.5 s after game_screen entry,
 * 20 s window with 2 s polls, delta = underrunEvents(u1-u0) — so the number is
 * directly comparable to the T2 baselines (1.5 Mbps ON=635 / OFF=861; 4 Mbps ON=3).
 *
 * Additions over the T2 script:
 *   - nofail+autohit via RB3_GAME_INPUT (flow-gate pattern) so gameplay SURVIVES
 *     long enough for the chunked transfer to COMPLETE at 1.5 Mbps (strict yield
 *     means the sidecar only moves in mogg gaps — the window is long by design);
 *   - --chunkkb N  -> RB3_SHARPEN_CHUNK_KB in __rb3ExtraEnv (omit = default 256
 *     chunked; 0 = legacy single fetch);
 *   - --mbps 0     -> no CDP throttle (legacy localhost regression check);
 *   - COMPLETE-wait phase after the window (--completewait s, default 600): polls
 *     until "RB3_SHARPEN: COMPLETE" and logs the sharpen-window duration (songMs>0
 *     -> COMPLETE) + chunk pump stats + the full-phase underrun delta (informational).
 *
 * Usage:
 *   node _sharpen_gate14.mjs --port 8623 --mbps 1.5                 # chunked ON
 *   node _sharpen_gate14.mjs --port 8623 --mbps 4                   # 4 Mbps sanity
 *   node _sharpen_gate14.mjs --port 8623 --mbps 0 --chunkkb 0       # legacy check
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8623'), 10);
const MBPS = parseFloat(arg('--mbps', '4'));
const RTT = parseFloat(arg('--rtt', MBPS <= 2 ? '300' : '150'));
const SHARPEN = arg('--sharpen', '1');
const CHUNKKB = arg('--chunkkb', null);          // null = don't set (engine default 256)
const WINDOW = parseFloat(arg('--window', '20')); // T2 protocol
const COMPLETEWAIT = parseFloat(arg('--completewait', '600'));
const sleep = ms => new Promise(r => setTimeout(r, ms));
const TAG = `${MBPS}mbps ck=${CHUNKKB === null ? 'def' : CHUNKKB}`;
const L = m => console.log(`[gate14 ${TAG}] ${m}`);

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
  songMs: (window.rb3SongMs|0) || 0,
}));
const underruns = (page) => page.evaluate(() => {
  const a = window._rb3Audio; if (!a || !a.underruns) return null;
  const u = a.underruns;
  return { ev: u.underrunEvents|0, fr: u.underrunFrames|0, q: u.totalQuanta|0 };
});
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
const logs = [], sharpenLines = [], chunkLines = [];
let sawComplete = false, tComplete = 0;
page.on('console', m => { const t = m.text(); logs.push(t);
  if (/RB3_SHARPEN|\[sharpen\]|session COMPLETE/i.test(t)) sharpenLines.push(t);
  if (/chunk pump start|chunk landed|chunk assembly COMPLETE|whole body|falling back|chunkFallback|ignored Range/i.test(t)) chunkLines.push(t);
  if (/RB3_SHARPEN: COMPLETE|session COMPLETE/i.test(t) && !sawComplete) { sawComplete = true; tComplete = Date.now(); }
  if (/RB3_SHARPEN|session COMPLETE|chunk pump start|chunk assembly COMPLETE|whole body|falling back/i.test(t)) console.log('  |', t.slice(0, 180));
});

const cdp = await page.context().newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.clearBrowserCache');
if (MBPS > 0) {
  await cdp.send('Network.emulateNetworkConditions', {
    offline: false, latency: RTT,
    downloadThroughput: (MBPS * 1024 * 1024) / 8,
    uploadThroughput: (MBPS * 1024 * 1024) / 8,
  });
}

let rc = 1;
const result = { mbps: MBPS, chunkkb: CHUNKKB === null ? 'default(256)' : CHUNKKB, reached: false };
try {
  await page.addInitScript(({ sh, ck }) => {
    window.__rb3ExtraEnv = Object.assign(window.__rb3ExtraEnv || {}, {
      RB3_PROGRESSIVE_SHARPEN: sh, RB3_SHARPEN_DBG: '1',
    });
    if (ck !== null) window.__rb3ExtraEnv.RB3_SHARPEN_CHUNK_KB = String(ck);
    window.rb3NoSplashHook = 1;
    // Sustained gameplay for the long chunked window (flow-gate pattern).
    window.__rb3ExtraEnv.RB3_GAME_INPUT = '@1:nofail,@1:autohit';
    window.rb3WebUseAids = 0;
    window.rb3WebTargetSong = '20thcenturyboy';
  }, { sh: SHARPEN, ck: CHUNKKB });

  L(`goto (throttle ${MBPS > 0 ? MBPS + 'Mbps/' + RTT + 'ms' : 'NONE'}, cold cache)`);
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 60000 });
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
  if (!await waitScreen(page, s => s.screen === 'game_screen', 240000, 'game_screen')) throw new Error('stuck ready');
  result.reached = true;

  // ===== T2-protocol underrun window (comparable to 635/861/3 baselines) =====
  await sleep(2500);
  const u0 = await underruns(page);
  L(`window START underruns: ${JSON.stringify(u0)}`);
  const t0 = Date.now();
  let lastU = u0;
  while ((Date.now() - t0) / 1000 < WINDOW) {
    await sleep(2000);
    const u = await underruns(page);
    if (u && lastU && u.ev !== lastU.ev) L(`  +${((Date.now()-t0)/1000).toFixed(0)}s underruns ev=${u.ev} (+${u.ev-lastU.ev})`);
    lastU = u || lastU;
  }
  const u1 = await underruns(page);
  L(`window END underruns:   ${JSON.stringify(u1)}`);
  result.u0 = u0; result.u1 = u1;
  if (u0 && u1) {
    result.underrunDelta = u1.ev - u0.ev;
    result.quantaDelta = u1.q - u0.q;
    L(`SHARPEN-WINDOW underrun delta: ${result.underrunDelta} events / ${result.quantaDelta} quanta ` +
      `= ${(100*result.underrunDelta/Math.max(1,result.quantaDelta)).toFixed(3)}% quanta padded`);
  }

  // ===== COMPLETE-wait phase (duration is cosmetic; completion is the gate) =====
  const sMs0 = await state(page);
  const tPhase = Date.now();
  while (!sawComplete && (Date.now() - tPhase) / 1000 < COMPLETEWAIT) {
    await sleep(2000);
    const s = await state(page);
    if (s.screen !== 'game_screen') { L(`left game_screen ('${s.screen}') before COMPLETE`); break; }
  }
  const uC = await underruns(page);
  result.sawComplete = sawComplete;
  if (sawComplete) {
    result.tCompleteFromWindowStartS = +((tComplete - t0) / 1000).toFixed(1);
    L(`sharpen COMPLETE at +${result.tCompleteFromWindowStartS}s after underrun-window start (songMs at window ~${sMs0.songMs})`);
  } else {
    L(`sharpen did NOT reach COMPLETE within ${COMPLETEWAIT}s wait`);
  }
  if (u0 && uC) {
    result.fullPhaseUnderrunDelta = uC.ev - u0.ev;
    result.fullPhaseQuantaDelta = uC.q - u0.q;
    L(`FULL-PHASE underrun delta (window start -> now): ${result.fullPhaseUnderrunDelta} ev / ${result.fullPhaseQuantaDelta} quanta ` +
      `= ${(100*result.fullPhaseUnderrunDelta/Math.max(1,result.fullPhaseQuantaDelta)).toFixed(3)}%`);
  }
  result.chunkLandedCount = chunkLines.filter(l => /chunk landed/.test(l)).length;
  result.chunkLines = chunkLines.length;
  L(`chunk lines: ${chunkLines.length} (landed: ${result.chunkLandedCount})`);
  for (const l of sharpenLines.slice(-6)) console.log('   S|', l.slice(0, 180));
  rc = sawComplete ? 0 : 2;
} catch (e) {
  L('ERROR: ' + (e.message || e));
  for (const l of logs.slice(-25)) console.log('  |', l.slice(0, 200));
} finally {
  console.log('RESULT ' + JSON.stringify(result));
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
