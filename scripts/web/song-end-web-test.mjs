#!/usr/bin/env node
/**
 * song-end-web-test.mjs — WEB song-end regression test (web sibling of
 * scripts/native/song-end-test.py).
 *
 * Boots the web build to gameplay by pure keyboard (same flow as
 * keyboard-to-gameplay.mjs), optionally scrolls to a specific song
 * (--song-token), then drives the song toward its end via the
 * window._rb3VerbQueue JS->verb bridge (main_web.cpp WebVerbQueueHook):
 *   msg:game:jump:<ms>   — same verb the native song-end-test injects over HTTP.
 * With --jump-ms short of the song length the tail plays through the REAL
 * audio-stream EOF (natural song end); past it, the by-time end path. PASS =
 * endgame screens load, frames keep advancing, and Confirm presses exit all
 * the way back to song_select.
 *
 * Freeze detection: the web main loop is JSPI on the BROWSER MAIN THREAD, so a
 * wedged game loop also wedges page.evaluate. We poll with a hard timeout —
 * evaluate not returning (or rb3FrameCount frozen) for >8s = FROZEN. On freeze
 * we attach CDP Debugger.pause (works via interrupt even in a spin) and dump
 * the wasm call stack (debug -g2 has full names; release keeps the wasm name
 * section via --profiling-funcs).
 *
 * Guards the song-end freeze fixed 2026-07-02: Stats::GetHopoPercent 0/0 ->
 * NaN -> int (UB) — at emcc -O2 the poison corrupted TriggerSongCompletion's
 * BandStatsInfo::mSoloStats vector and the destructor wedged the tab at the
 * endgame_waiting transition (release-only; -O0/debug and x86 stayed benign).
 * Diagnosis recipe for the next one: repro here -> pin suspect TU -O0
 * (RB3_WEB_O0_GLOB) -> UBSan it at -O2 (RB3_WEB_UBSAN_GLOB) -> read console.
 *
 * Usage: node scripts/web/song-end-web-test.mjs [--port 8437] [--build release]
 *        [--song-token beast] [--jump-ms 250000] [--post-secs 300]
 *        [--song-downs 3] [--natural] [--mute]
 */
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8437'), 10);
const BUILD = arg('--build', 'debug');
const JUMP_MS = parseInt(arg('--jump-ms', '600000'), 10);
const NATURAL = argv.includes('--natural');
const SONG_DOWNS = parseInt(arg('--song-downs', '3'), 10);
const SONG_TOKEN = arg('--song-token', '');  // scroll until rb3HighlightedSong matches
const POST_SECS = parseInt(arg('--post-secs', '90'), 10);
const OUT = resolve(__dirname, 'results/songend-freeze');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const L = (m) => console.log(`[songend] ${m}`);

// evaluate with a hard timeout — a wedged main thread never returns.
async function evalT(page, fn, ms = 4000) {
  return Promise.race([
    page.evaluate(fn).catch(() => null),
    sleep(ms).then(() => Symbol.for('timeout')),
  ]);
}
const state = (page, ms) => evalT(page, () => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  frame: window.rb3FrameCount || 0,
}), ms);

async function press(page, key, holdMs = 220, gapMs = 400) {
  // Race every CDP input call: against a wedged/closed page these block forever.
  const t = (p) => Promise.race([p.catch(() => {}), sleep(4000)]);
  await t(page.keyboard.down(key)); await sleep(holdMs);
  await t(page.keyboard.up(key)); await sleep(gapMs);
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page, 5000);
    if (s === Symbol.for('timeout') || !s) { await sleep(300); continue; }
    if (s.screen !== last) { L(`  ...${label}: screen='${s.screen}' view='${s.view}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}

async function dumpPausedStack(cdp, label) {
  L(`--- ${label}: pausing main thread for stack ---`);
  const paused = new Promise((res) => cdp.once('Debugger.paused', res));
  await cdp.send('Debugger.pause').catch(e => L(`pause failed: ${e.message}`));
  const ev = await Promise.race([paused, sleep(8000).then(() => null)]);
  if (!ev) { L('Debugger.paused never fired'); return false; }
  const frames = ev.callFrames || [];
  L(`paused, ${frames.length} frames:`);
  for (let i = 0; i < Math.min(frames.length, 60); i++) {
    const f = frames[i];
    L(`  #${i} ${f.functionName || '(anon)'}  ${f.url ? f.url.split('/').pop() : ''}:${f.location ? f.location.lineNumber : '?'}`);
  }
  await cdp.send('Debugger.resume').catch(() => {});
  return true;
}

const MUTE = argv.includes('--mute');
const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--autoplay-policy=no-user-gesture-required',
    ...(MUTE ? ['--mute-audio'] : [])],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });
const logs = [];
let lastSongMs = -1, lastLogT = Date.now();
page.on('console', (m) => {
  const t = m.text();
  logs.push(t); lastLogT = Date.now();
  const mm = t.match(/songMs=([\d.]+)/);
  if (mm) lastSongMs = parseFloat(mm[1]);
  if (/game_over|GameEnded|end_game|EndGame|endgame|clear_draw_glitch|Restart|runtime error|ubsan|sanitizer|STATS_/i.test(t)) L(`| ${t}`);
});
page.on('pageerror', (e) => { logs.push(`PAGEERROR: ${e.message}`); L(`PAGEERROR: ${e.message}`); });

// GAME_DBG prints Game::Poll songMs lines; UISCREEN_DBG prints screen/panel
// state transitions — both read via getenv from window.__rb3ExtraEnv.
// RB3_STATS_DBG (--stats-dbg) additionally arms the MetaPerformer stomp-watch;
// leave it OFF for regression runs — its MILO_LOG dumps recycle the MakeString
// ring and can poison an in-flight file path (diagnosis-only).
const STATS_DBG = argv.includes('--stats-dbg');
await page.addInitScript((statsDbg) => {
  window.__rb3ExtraEnv = Object.assign(window.__rb3ExtraEnv || {}, {
    GAME_DBG: '1', UISCREEN_DBG: '1',
  });
  if (statsDbg) window.__rb3ExtraEnv.RB3_STATS_DBG = '1';
  window._rb3VerbQueue = [];
}, STATS_DBG);

const cdp = await page.context().newCDPSession(page);
await cdp.send('Debugger.enable').catch(() => {});

let rc = 1;
try {
  const url = `http://127.0.0.1:${PORT}/${BUILD === 'debug' ? '?debug=true' : ''}`;
  L(`loading ${url}`);
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

  if (!await waitScreen(page, s => s.screen === 'splash_screen', 120000, 'splash')) throw 'no splash';
  await sleep(2500);
  await page.locator('#rb3-canvas').click({ force: true });
  for (let i = 0; i < 14; i++) {
    const s = await state(page, 4000);
    if (s !== Symbol.for('timeout') && s && s.screen === 'main_hub_screen') break;
    await press(page, 'Space', 250, 500);
  }
  if (!await waitScreen(page, s => s.screen === 'main_hub_screen', 30000, 'main_hub')) throw 'no main_hub';
  for (let i = 0; i < 12; i++) {
    const s = await state(page, 4000);
    if (s !== Symbol.for('timeout') && s && s.screen === 'song_select_screen') break;
    await press(page, 'Enter', 220, 450);
  }
  if (!await waitScreen(page, s => s.screen === 'song_select_screen', 40000, 'song_select')) throw 'no song_select';
  await sleep(2000);
  if (SONG_TOKEN) {
    // scroll until the highlighted song matches the requested token
    let found = false, first = '';
    for (let i = 0; i < 140; i++) {
      const cur = await evalT(page, () => window.rb3HighlightedSong || '', 4000);
      if (typeof cur === 'string' && cur) {
        if (i === 0) first = cur;
        else if (cur === first && i > 3) break;  // wrapped around
        if (cur.toLowerCase().includes(SONG_TOKEN.toLowerCase())) { found = true; L(`song '${cur}' highlighted after ${i} downs`); break; }
      }
      await press(page, 'ArrowDown', 130, 200);
    }
    if (!found) throw `song token '${SONG_TOKEN}' not found in library`;
  } else {
    for (let i = 0; i < SONG_DOWNS; i++) await press(page, 'ArrowDown', 150, 250);
  }
  await press(page, 'Enter', 220, 450);
  if (!await waitScreen(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty')) throw 'no part_difficulty';

  // part + difficulty: real key nav across the choose_part -> choose_diff
  // overshell sub-views (window.rb3OvershellView, already in `state()`). The
  // old synthetic track:guitar -> overshell:end_override_flow autopilot
  // (armed by ANY Confirm on part_difficulty_screen) no longer exists — the
  // screen must actually be crossed via choose_part then choose_diff.
  await sleep(1500);
  if (!await waitScreen(page, s => s.view && s.view.startsWith('choose_part'), 40000, 'choose_part')) throw 'no choose_part view';
  await press(page, 'Enter', 220, 600);   // confirm part (guitar focused)
  if (!await waitScreen(page, s => s.view === 'choose_diff', 40000, 'choose_diff')) throw 'no choose_diff view';
  // Easy(0)/Medium(1)/Hard(2)/Expert(3) top-down, default focus Easy — scroll
  // down to Expert (same difficulty the old hard-coded shortcut always used).
  for (let i = 0; i < 3; i++) await press(page, 'ArrowDown', 150, 250);
  await press(page, 'Enter', 220, 600);   // confirm difficulty (expert)
  // nofail/autohit used to fire automatically as part of the removed
  // autopilot; push them onto the same generic verb bridge (ExecVerb,
  // readiness-gated + self-timing-out) so the song-end jump below reaches the
  // end deterministically instead of failing out mid-song.
  await evalT(page, () => { window._rb3VerbQueue.push('nofail'); window._rb3VerbQueue.push('autohit'); }, 3000);
  const gs = await waitScreen(page, s => s.screen === 'game_screen', 210000, 'game_screen');
  if (!gs) throw 'no game_screen';
  L('gameplay reached; letting the song run...');
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '01_playing.png') }).catch(() => {});

  // wait until the clock is really moving
  const t0 = Date.now();
  while (lastSongMs < 4000 && Date.now() - t0 < 90000) await sleep(500);
  L(`songMs=${lastSongMs} — ${NATURAL ? 'waiting for natural end' : `jumping to ${JUMP_MS}`}`);
  if (!NATURAL) {
    const r = await evalT(page, `window._rb3VerbQueue.push('msg:game:jump:${JUMP_MS}')`, 5000);
    if (r === Symbol.for('timeout')) L('WARN: jump push timed out (already wedged?)');
  }

  // Post-end watch: PASS if we leave game_screen for an endgame screen and
  // frames keep advancing; FROZEN if evaluate times out / frame counter stalls.
  let lastFrame = -1, frozenSince = 0, sawScreen = '';
  const dl = Date.now() + POST_SECS * 1000;
  while (Date.now() < dl) {
    const s = await state(page, 4000);
    const now = Date.now();
    if (s === Symbol.for('timeout') || !s) {
      if (!frozenSince) frozenSince = now;
      L(`  evaluate TIMEOUT (frozen ${(now - frozenSince) / 1000 | 0}s) lastScreen='${sawScreen}' lastSongMs=${lastSongMs}`);
      if (now - frozenSince > 8000) {
        L('FROZEN: main thread wedged — capturing stack');
        await dumpPausedStack(cdp, 'freeze stack');
        await dumpPausedStack(cdp, 'freeze stack (2nd sample)');
        rc = 3;
        break;
      }
    } else {
      if (s.frame === lastFrame) {
        if (!frozenSince) frozenSince = now;
        if (now - frozenSince > 8000) {
          L(`FROZEN: frame counter stuck at ${s.frame} on '${s.screen}' — capturing stack`);
          await dumpPausedStack(cdp, 'freeze stack');
          rc = 3;
          break;
        }
      } else frozenSince = 0;
      lastFrame = s.frame;
      if (s.screen !== sawScreen) { L(`  screen -> '${s.screen}' frame=${s.frame} songMs=${lastSongMs}`); sawScreen = s.screen; }
      if (s.screen && s.screen !== 'game_screen' && /endgame|stats|awards|complete|win|results/i.test(s.screen)) {
        L(`PASS-ish: reached post-song screen '${s.screen}' with frames advancing`);
        await sleep(10000);
        const s2 = await state(page, 4000);
        if (s2 === Symbol.for('timeout') || !s2 || s2.frame === s.frame) {
          L('...but froze right after — capturing stack');
          await dumpPausedStack(cdp, 'post-endgame freeze');
          rc = 3;
        } else {
          L(`STABLE: screen='${s2.screen}' frame advanced ${s.frame} -> ${s2.frame}`);
          rc = 0;
          // Continue THROUGH the endgame screens back to the menu — the
          // score-screen exit runs {game clear_draw_glitch} and is its own
          // freeze candidate. Confirm each press we keep advancing frames.
          L('advancing through endgame screens (Confirm presses)...');
          let prevScreen = s2.screen;
          // 32 presses: award/goal screens swallow Confirms during their
          // animations, so 16 sometimes ends the run one screen short of
          // song_select even though nothing is wrong.
          for (let p = 0; p < 32; p++) {
            await press(page, 'Enter', 220, 1500);
            const sp = await state(page, 5000);
            if (sp === Symbol.for('timeout') || !sp) {
              L(`FROZEN after endgame Confirm #${p + 1} (last screen '${prevScreen}')`);
              await dumpPausedStack(cdp, 'endgame-exit freeze');
              rc = 3;
              break;
            }
            if (sp.screen !== prevScreen) { L(`  exit-nav: screen -> '${sp.screen}' frame=${sp.frame}`); prevScreen = sp.screen; }
            if (/song_select|main_hub|music_library/i.test(sp.screen)) {
              L(`EXIT OK: back on '${sp.screen}' — full song-end round trip survived`);
              break;
            }
          }
        }
        break;
      }
    }
    await sleep(700);
  }
  if (rc === 1) L(`TIMEOUT: never reached endgame within ${POST_SECS}s (lastScreen='${sawScreen}' songMs=${lastSongMs})`);
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '02_end.png'), timeout: 5000 }).catch(() => {});
} catch (e) {
  L(`ERROR: ${e && e.message || e}`);
  await dumpPausedStack(cdp, 'error-state stack').catch(() => {});
} finally {
  console.log('=== last 80 engine console lines ===');
  for (const l of logs.slice(-80)) console.log('  |', l);
  await Promise.race([browser.close(), sleep(3000)]);
}
process.exit(rc);
