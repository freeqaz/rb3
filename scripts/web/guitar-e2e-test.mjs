#!/usr/bin/env node
/**
 * guitar-e2e-test.mjs — FULL end-to-end web run driven by a simulated USB
 * guitar controller (Gamepad API), verifying the guitar input layer landed in
 * native/src/rb3_joypad_native.cpp (InitWebGuitar / ReadWebGamepadButtons).
 *
 * Injects a fake ps3wii_rb guitar (12ba:0100) via a navigator.getGamepads
 * override (installed pre-load, so the wasm's per-frame poll sees it from
 * frame 0) and drives the WHOLE flow with guitar buttons only:
 *
 *   splash -> main_hub -> song_select -> choose_part -> choose_diff ->
 *   game_screen (is_playing), then in-gameplay input evidence:
 *     - fret+strum edges observed at the ENGINE chokepoint via the
 *       session-telemetry trace (window.__rb3Trace "in" rows carry the exact
 *       bitmask SendButtonMessages broadcast — we wrap .push to collect them)
 *     - whammy axis tracking (window._rb3GpWhammy)
 *     - tilt button -> star-power bit 8 in the chokepoint mask
 *     - guitar Start -> pause overlay appears (engine UI reacts in-song)
 *   plus: live remap via window.rb3GuitarMap (green<->blue swap), and a
 *   standard-mapping pad regression (face buttons still decode, whammy rest).
 *
 * Splash gate: the splash screen advances via WebSplashAdvanceHook
 * (main_web.cpp), which edge-detects Start/Confirm on the merged
 * _rb3Keys|_rb3GpMask bitmask. Older builds read only window._rb3Keys
 * (keyboard) — this harness auto-detects that: if guitar Start doesn't cross
 * splash it records the finding and falls back to one keyboard Space press so
 * the rest of the matrix still runs.
 *
 * Guitar button indices (ps3wii_rb family, browser Gamepad API):
 *   green=1 red=2 yellow=0 blue=3 orange=4 tilt/pedal=5 select=8 start=9
 *   hat (strum+dpad) = axes[9] 8-step fractional; whammy = axes[0] (-1 rest)
 *
 * Usage: node scripts/web/guitar-e2e-test.mjs [--port 8421] [--release]
 *                                             [--play-seconds 15] [--verbose]
 * Assumes a server already running (python3 native/web/server.py).
 * Output: scripts/web/results/guitar-e2e/ (screenshots + summary.json)
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const RELEASE = argv.includes('--release');       // default: ?debug=true build
const VERBOSE = argv.includes('--verbose');
const PLAY_SECONDS = parseInt(arg('--play-seconds', '15'), 10) || 15;
const OUT = resolve(__dirname, 'results/guitar-e2e');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// JoypadButton bits (engine side) for evidence checks.
const BIT = {
  green: 1 << 1, red: 1 << 5, yellow: 1 << 4, blue: 1 << 6, orange: 1 << 7,
  star: 1 << 8, start: 1 << 11, up: 1 << 12, right: 1 << 13, down: 1 << 14, left: 1 << 15,
};
// Hat axis values (Chrome 8-step fractional: step = round((v+1)*3.5)).
const HAT = { up: -1, down: 4 / 3.5 - 1, idle: 2 };

// ---------------------------------------------------------------------------
// Fake gamepads — installed PRE-LOAD so the wasm's rAF poll sees them always.
// ---------------------------------------------------------------------------
const FAKE_INIT = () => {
  const mkBtns = (n) => Array.from({ length: n }, () => ({ pressed: false, touched: false, value: 0 }));
  window.__fakeGuitar = {
    index: 0,
    id: 'Licensed by Nintendo of America Harmonix Guitar Controller for Nintendo Wii (Vendor: 12ba Product: 0100)',
    mapping: '',
    connected: true,
    buttons: mkBtns(16),
    axes: [-1, 0, 0, 0, 0, 0, 0, 0, 0, 2], // axes[0] whammy rest=-1, axes[9] hat idle=2
  };
  window.__fakeStandard = {
    index: 0,
    id: 'Xbox Wireless Controller (STANDARD GAMEPAD Vendor: 045e Product: 02fd)',
    mapping: 'standard',
    connected: true,
    buttons: mkBtns(17),
    axes: [0, 0, 0, 0],
  };
  window.__activePad = () => window.__fakeGuitar;
  navigator.getGamepads = () => [window.__activePad()];

  // Engine-chokepoint evidence collector: the session-telemetry recorder
  // (default-ON on web) pushes NDJSON chunks to window.__rb3Trace; input edges
  // are {"k":"in","b":<bitmask>,"dn":...}. Wrap push to also collect them
  // BEFORE the pre-js flusher drains the array.
  window.__testInEdges = [];
  const hookTrace = () => {
    const t = window.__rb3Trace;
    if (!Array.isArray(t) || t.__testHooked) return;
    t.__testHooked = true;
    const orig = t.push.bind(t);
    t.push = (...chunks) => {
      for (const c of chunks) {
        for (const line of String(c).split('\n')) {
          if (line.includes('"k":"in"')) {
            try { window.__testInEdges.push(JSON.parse(line)); } catch { /* partial */ }
          }
        }
      }
      return orig(...chunks);
    };
  };
  hookTrace();
  setInterval(hookTrace, 500); // __rb3Trace may be (re)created after us
};

// ---------------------------------------------------------------------------
let browser;
const logs = [];
const errors = [];
const results = [];   // { name, ok, detail }
const findings = [];  // usability/bug findings (non-fatal)
const check = (name, ok, detail = '') => {
  results.push({ name, ok: !!ok, detail });
  console.log(`  [${ok ? 'PASS' : 'FAIL'}] ${name}${detail ? ' — ' + detail : ''}`);
  return !!ok;
};
const finding = (text) => { findings.push(text); console.log(`  [FINDING] ${text}`); };

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  view: window.rb3OvershellView || '?',
  diff: window.rb3OvershellDiff || '?',
  focus: window.rb3FocusButton || '',
  frame: window.rb3FrameCount || 0,
  songs: window.rb3SongCount || 0,
}));

async function snap(page, label) {
  const path = resolve(OUT, `${label}.png`);
  try { await page.locator('#rb3-canvas').screenshot({ path }); } catch { /* ignore */ }
  const s = await state(page);
  console.log(`  SNAP [${label}] screen='${s.screen}' view='${s.view}' frame=${s.frame} -> ${path}`);
  return s;
}

// One clean fake-guitar button press: pressed=true -> hold -> false -> gap.
// Hold >= ~220ms so the once-per-rAF poll sees it across several frames.
async function gpPress(page, idx, holdMs = 240, gapMs = 420) {
  await page.evaluate((i) => { window.__fakeGuitar.buttons[i].pressed = true; }, idx);
  await sleep(holdMs);
  await page.evaluate((i) => { window.__fakeGuitar.buttons[i].pressed = false; }, idx);
  await sleep(gapMs);
}

// One clean strum / d-pad hat flick on axes[9].
async function gpStrum(page, dirVal, holdMs = 200, gapMs = 380) {
  await page.evaluate((v) => { window.__fakeGuitar.axes[9] = v; }, dirVal);
  await sleep(holdMs);
  await page.evaluate((v) => { window.__fakeGuitar.axes[9] = v; }, HAT.idle);
  await sleep(gapMs);
}

async function waitFor(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    const sig = `${s.screen}|${s.view}`;
    if (sig !== last) { if (VERBOSE) console.log(`    ...${label}: screen='${s.screen}' view='${s.view}'`); last = sig; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}

// Engine-chokepoint edges collected since the given index.
const inEdges = (page, since = 0) => page.evaluate((n) => window.__testInEdges.slice(n), since);
const inEdgeCount = (page) => page.evaluate(() => window.__testInEdges.length);

// ===========================================================================
try {
  browser = await chromium.launch({
    headless: true,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
      '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
      '--ozone-platform=x11', '--mute-audio'],
  });
  const page = await browser.newPage();
  await page.setViewportSize({ width: 1280, height: 720 });
  await page.addInitScript(FAKE_INIT);
  page.on('console', (m) => {
    const text = m.text();
    logs.push(text);
    if (VERBOSE || /rb3-guitar|splash|screen:|FIRE|game_screen/.test(text)) console.log(`  [con] ${text}`);
  });
  page.on('pageerror', (e) => { errors.push(String(e)); console.log(`  [PAGE_ERROR] ${e}`); });
  page.on('crash', () => { errors.push('page crashed'); console.log('  [CRASH]'); });

  const url = `http://127.0.0.1:${PORT}/${RELEASE ? '' : '?debug=true'}`;
  console.log(`\n=== guitar e2e: ${url} ===`);
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  // Raw guitar run: aids OFF (no rb3WebUseAids), splash hook left ENABLED
  // (production behavior — that hook is exactly what a real guitar hits).

  // --- boot -> splash ------------------------------------------------------
  console.log('\n[boot] waiting for splash_screen...');
  const sp = await waitFor(page, s => s.screen === 'splash_screen', 240000, 'splash');
  if (!check('boot reaches splash_screen', !!sp)) throw new Error('no splash');
  await sleep(2500);
  await snap(page, '00_splash');

  // Dispatch gamepadconnected once (classification log; poll works regardless).
  await page.evaluate(() => {
    const ev = new Event('gamepadconnected');
    Object.defineProperty(ev, 'gamepad', { value: window.__fakeGuitar });
    window.dispatchEvent(ev);
  });
  await sleep(500);
  const connLog = logs.filter(l => /\[rb3-guitar\] connected .*-> guitar \(ps3wii_rb\)/.test(l));
  check('guitar id classified ps3wii_rb', connLog.length > 0, connLog[0] || 'no connect log');

  // --- 1a. splash -> main_hub via GUITAR START -----------------------------
  console.log('\n[splash] guitar Start (button 9) presses...');
  let splashViaGuitar = false;
  for (let i = 0; i < 6; i++) {
    await gpPress(page, 9, 260, 520);           // Start
    const s = await state(page);
    if (s.screen !== 'splash_screen') { splashViaGuitar = true; break; }
  }
  if (!splashViaGuitar) {
    // Some builds want Confirm on a sub-prompt; try blue (Confirm) too.
    for (let i = 0; i < 3; i++) {
      await gpPress(page, 3, 260, 520);         // blue fret -> kPad_X Confirm
      const s = await state(page);
      if (s.screen !== 'splash_screen') { splashViaGuitar = true; break; }
    }
  }
  check('splash advances via GUITAR Start', splashViaGuitar,
    splashViaGuitar ? '' : 'WebSplashAdvanceHook likely reads only window._rb3Keys (keyboard)');
  if (!splashViaGuitar) {
    finding('SPLASH GATE: guitar Start does not advance splash_screen — ' +
      'WebSplashAdvanceHook (main_web.cpp) edge-detects window._rb3Keys only; ' +
      'the Gamepad API mask never reaches it. Falling back to keyboard Space.');
    await page.locator('#rb3-canvas').click({ force: true });
    for (let i = 0; i < 10; i++) {
      if ((await state(page)).screen !== 'splash_screen') break;
      await page.keyboard.down(' '); await sleep(250); await page.keyboard.up(' '); await sleep(500);
    }
  }
  const hub = await waitFor(page, s => s.screen === 'main_hub_screen', 40000, 'main_hub');
  if (!check('main_hub_screen reached', !!hub)) throw new Error('stuck before main_hub');
  await sleep(2500);
  await snap(page, '01_main_hub');

  // --- 1b. main_hub -> song_select via guitar confirm ----------------------
  // Empirically determine which fret confirms menus. The main_hub -> song_select
  // crossing needs 2+ confirms (PLAY NOW -> QUICKPLAY -> song_select) and the
  // screen NAME only changes at the end, so "does one press change the screen"
  // is not a valid probe — instead run the whole proven confirm chain (the same
  // repeated-press loop the keyboard tests use) with GREEN (kPad_R2; the HX_WII
  // button_meanings map R2 -> Confirm — Enter uses this very bit), and fall
  // back to the whole chain with BLUE (kPad_X, the UIScreen Confirm bit).
  console.log('\n[main_hub] confirm chain (green fret first, blue fallback)...');
  let confirmBtn = null; // gamepad button index that confirms menus
  const confirmChain = async (btnIdx, presses) => {
    for (let i = 0; i < presses; i++) {
      const s = await state(page);
      if (s.screen === 'song_select_screen') return true;
      await gpPress(page, btnIdx, 240, 550);
      if (VERBOSE) console.log(`    press[${btnIdx}] #${i + 1}: screen='${(await state(page)).screen}'`);
    }
    return !!(await waitFor(page, s => s.screen === 'song_select_screen', 25000, `confirm-chain btn${btnIdx}`));
  };
  if (await confirmChain(1, 8)) confirmBtn = 1;         // green
  else if (await confirmChain(3, 8)) confirmBtn = 3;    // blue
  check('a fret confirms menus (main_hub -> song_select)', confirmBtn !== null,
    confirmBtn === 1 ? 'GREEN (kPad_R2 via button_meanings)' :
    confirmBtn === 3 ? 'BLUE (kPad_X) — green did NOT confirm' : 'neither green nor blue confirmed');
  if (confirmBtn === null) throw new Error('no menu confirm from guitar frets');
  if (confirmBtn === 3)
    finding('MENU CONFIRM: green fret does not confirm menus; blue (kPad_X) does. ' +
      'Real RB3 Wii guitars confirm with green — check joypad.dta button_meanings for kPad_R2.');

  const ss = await waitFor(page, s => s.screen === 'song_select_screen', 40000, 'song_select');
  if (!check('song_select_screen reached via guitar', !!ss)) throw new Error('stuck before song_select');
  await sleep(2500);
  await snap(page, '02_song_select');
  console.log(`  songCount=${(await state(page)).songs}`);

  // --- 1c. song_select: strum-nav + confirm --------------------------------
  console.log('\n[song_select] strum-down x3 then confirm...');
  for (let i = 0; i < 3; i++) await gpStrum(page, HAT.down, 200, 350);
  await snap(page, '02b_song_nav');
  await gpPress(page, confirmBtn, 240, 600);
  let pd = await waitFor(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty');
  if (!pd) {
    // one retry — a header node may need a second confirm
    await gpPress(page, confirmBtn, 240, 600);
    pd = await waitFor(page, s => s.screen === 'part_difficulty_screen', 60000, 'part_difficulty#2');
  }
  if (!check('part_difficulty_screen reached via guitar strum+confirm', !!pd)) throw new Error('stuck at song_select');
  await snap(page, '03_part_difficulty');

  // --- 1d. choose_part -> choose_diff -> launch ----------------------------
  console.log('\n[part_difficulty] choose_part confirm -> choose_diff strum-down x2 (hard) -> confirm...');
  const cp = await waitFor(page, s => s.view.startsWith('choose_part') || s.view === 'choose_diff', 40000, 'choose_part');
  check('overshell enters choose_part', !!cp, cp ? `view='${cp.view}'` : 'never entered');
  if (cp && cp.view.startsWith('choose_part')) await gpPress(page, confirmBtn, 240, 650);
  const cd = await waitFor(page, s => s.view === 'choose_diff', 40000, 'choose_diff');
  check('overshell enters choose_diff', !!cd, cd ? '' : 'never reached choose_diff');
  if (cd) {
    for (let i = 0; i < 2; i++) await gpStrum(page, HAT.down, 200, 350); // Easy->Medium->Hard
    await snap(page, '04_choose_diff');
    await gpPress(page, confirmBtn, 240, 650);
  }
  console.log('  waiting for game_screen (song load)...');
  const gs = await waitFor(page, s => s.screen === 'game_screen', 270000, 'game_screen');
  if (!check('game_screen reached — full guitar-driven boot-to-gameplay', !!gs))
    throw new Error('never reached game_screen');
  const sAfter = await state(page);
  check('difficulty selected via strum nav', sAfter.diff === 'hard', `diff='${sAfter.diff}' (wanted hard)`);
  await sleep(3000);
  await snap(page, '05_game_screen');

  // --- 2. gameplay input evidence ------------------------------------------
  console.log('\n[gameplay] fret+strum edges at the engine chokepoint...');
  await page.evaluate(() => { window._rb3GpDebug = 1; });
  const mark0 = await inEdgeCount(page);
  // Several green/red fret + strum-down combos (gem hits need not be accurate).
  for (let i = 0; i < 4; i++) {
    await page.evaluate(() => { window.__fakeGuitar.buttons[1].pressed = true; });   // green
    await sleep(120);
    await gpStrum(page, HAT.down, 180, 200);                                          // strum while held
    await page.evaluate(() => { window.__fakeGuitar.buttons[1].pressed = false; });
    await sleep(300);
  }
  await gpPress(page, 2, 240, 400);   // red fret alone
  await sleep(500);
  const edges1 = await inEdges(page, mark0);
  const sawGreen = edges1.some(e => (e.dn & BIT.green) !== 0);
  const sawStrum = edges1.some(e => (e.dn & BIT.down) !== 0);
  const sawGreenAndStrum = edges1.some(e => (e.b & BIT.green) && (e.b & BIT.down));
  const sawRed = edges1.some(e => (e.dn & BIT.red) !== 0);
  check('green fret edge reaches SendButtonMessages (trace "in" dn bit1)', sawGreen, `${edges1.length} edges`);
  check('strum-down edge reaches chokepoint (dn bit14)', sawStrum);
  check('fret+strum simultaneously in one mask (b has bit1+bit14)', sawGreenAndStrum);
  check('red fret edge reaches chokepoint (dn bit5)', sawRed);
  await snap(page, '06_gameplay_input');

  // Whammy: sweep the axis, verify _rb3GpWhammy tracks (0 -> ~1 -> 0).
  console.log('\n[gameplay] whammy sweep...');
  const wh0 = await page.evaluate(() => window._rb3GpWhammy);
  await page.evaluate(() => { window.__fakeGuitar.axes[0] = 1; });   // full
  await sleep(400);
  const wh1 = await page.evaluate(() => window._rb3GpWhammy);
  await page.evaluate(() => { window.__fakeGuitar.axes[0] = 0; });   // half
  await sleep(400);
  const whHalf = await page.evaluate(() => window._rb3GpWhammy);
  await page.evaluate(() => { window.__fakeGuitar.axes[0] = -1; });  // rest
  await sleep(400);
  const wh2 = await page.evaluate(() => window._rb3GpWhammy);
  check('whammy tracks axis: rest->full->half->rest',
    Math.abs(wh0) < 0.05 && wh1 > 0.9 && whHalf > 0.4 && whHalf < 0.6 && Math.abs(wh2) < 0.05,
    `0=${wh0} full=${wh1} half=${whHalf} rest=${wh2}`);
  // (Engine effect: whammy01 -> mSticks[1][0] RX -> GetWhammyBar; the trace's
  // wh field samples LY, so RX isn't headless-observable — code-path verified.)

  // Tilt button -> star power bit 8 at the chokepoint.
  console.log('\n[gameplay] tilt -> star power bit...');
  const mark1 = await inEdgeCount(page);
  await gpPress(page, 5, 300, 450);   // tilt/pedal switch
  const tiltEdges = await inEdges(page, mark1);
  const sawStar = tiltEdges.some(e => (e.dn & BIT.star) !== 0);
  check('tilt button sets star-power bit 8 (kPad_Select) at chokepoint', sawStar,
    `${tiltEdges.length} edges`);
  const tiltFlag = logs.some(l => /\[rb3-guitar\].*tilt=1/.test(l));
  check('tilt visible in _rb3GpDebug log', tiltFlag, tiltFlag ? '' : '(informational)');

  // Guitar Start in-song -> pause overlay (engine UI reacts to guitar in play).
  console.log('\n[gameplay] guitar Start -> pause...');
  const preView = (await state(page)).view;
  await gpPress(page, 9, 260, 800);
  const paused = await waitFor(page, s => s.view !== preView || s.screen !== 'game_screen', 8000, 'pause');
  check('guitar Start pauses gameplay (view/screen change)', !!paused,
    paused ? `view '${preView}' -> '${paused.view}' screen='${paused.screen}'` : 'no change');
  await snap(page, '07_paused');
  if (paused) {
    await gpPress(page, confirmBtn, 240, 650);  // Continue (focused default)
    const resumed = await waitFor(page, s => s.screen === 'game_screen' && s.view === preView, 10000, 'resume');
    check('guitar confirm resumes from pause', !!resumed, resumed ? '' : `view='${(await state(page)).view}'`);
  }

  // --- 3. live remap override ----------------------------------------------
  console.log('\n[remap] window.rb3GuitarMap = {green:3, blue:1} swap...');
  await page.evaluate(() => { window.rb3GuitarMap = { green: 3, blue: 1 }; });
  await sleep(300);
  const mark2 = await inEdgeCount(page);
  await gpPress(page, 3, 260, 450);   // physical button 3: was blue, now GREEN
  await gpPress(page, 1, 260, 450);   // physical button 1: was green, now BLUE
  const remapEdges = await inEdges(page, mark2);
  const b3isGreen = remapEdges.some(e => (e.dn & BIT.green) !== 0 && !(e.dn & BIT.blue));
  const b1isBlue = remapEdges.some(e => (e.dn & BIT.blue) !== 0 && !(e.dn & BIT.green));
  check('remap: physical btn3 now GREEN (bit1)', b3isGreen, `${remapEdges.length} edges`);
  check('remap: physical btn1 now BLUE (bit6)', b1isBlue);
  await page.evaluate(() => { delete window.rb3GuitarMap; });
  await sleep(300);

  // --- 4b. standard-mapping pad regression ---------------------------------
  console.log('\n[regression] standard pad fallback path...');
  await page.evaluate(() => {
    window.__activePad = () => window.__fakeStandard;
    const ev = new Event('gamepadconnected');
    Object.defineProperty(ev, 'gamepad', { value: window.__fakeStandard });
    window.dispatchEvent(ev);
  });
  await sleep(400);
  const stdLog = logs.filter(l => /\[rb3-guitar\] connected .*-> standard/.test(l));
  check('standard pad classifies standard', stdLog.length > 0, stdLog[0] || 'no log');
  const mark3 = await inEdgeCount(page);
  await page.evaluate(() => { window.__fakeStandard.buttons[0].pressed = true; }); // A
  await sleep(300);
  await page.evaluate(() => { window.__fakeStandard.buttons[0].pressed = false; });
  await sleep(450);
  const stdEdges = await inEdges(page, mark3);
  const stdA = stdEdges.some(e => (e.dn & BIT.blue) !== 0);  // A -> kPad_X (bit 6)
  check('standard pad A -> kPad_X (bit6) still decodes', stdA, `${stdEdges.length} edges`);
  const whStd = await page.evaluate(() => window._rb3GpWhammy);
  check('standard pad leaves whammy at rest', Math.abs(whStd) < 0.05, `_rb3GpWhammy=${whStd}`);
  await page.evaluate(() => { window.__activePad = () => window.__fakeGuitar; });

  // let gameplay run a bit more, final screenshot
  await sleep(Math.max(0, PLAY_SECONDS - 10) * 1000);
  await snap(page, '08_final');

  // --- 4c. console error scan ----------------------------------------------
  const errLogs = logs.filter(l => /^\s*(Uncaught|TypeError|ReferenceError)/.test(l) ||
    /rb3-guitar.*(TypeError|undefined is not|ReferenceError)/.test(l));
  check('no page/JS errors during run', errors.length === 0, errors.slice(0, 3).join(' | ') || 'clean');
  check('no guitar-layer console errors', errLogs.length === 0, errLogs.slice(0, 3).join(' | ') || 'clean');

} catch (e) {
  console.log('\nEXCEPTION:', e && e.message ? e.message : e);
  results.push({ name: 'exception', ok: false, detail: String(e && e.message || e) });
} finally {
  if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch { /* ignore */ } }
}

// --- summary ----------------------------------------------------------------
const failCount = results.filter(r => !r.ok).length;
console.log('\n=== guitar e2e summary ===');
for (const r of results) console.log(`  ${r.ok ? 'PASS' : 'FAIL'}  ${r.name}${r.detail ? ' — ' + r.detail : ''}`);
if (findings.length) { console.log('--- findings ---'); findings.forEach(f => console.log('  * ' + f)); }
writeFileSync(resolve(OUT, 'summary.json'), JSON.stringify({ results, findings, errors: errors.slice(0, 30) }, null, 2));
console.log(`\n${failCount ? failCount + ' FAILURE(S)' : 'ALL CHECKS PASSED'} — details in ${resolve(OUT, 'summary.json')}`);
process.exit(failCount ? 1 : 0);
