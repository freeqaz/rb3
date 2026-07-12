#!/usr/bin/env node
/**
 * guitar-webusb-e2e-test.mjs — end-to-end web run driven by a FAKE WebUSB
 * Xbox-360 X-plorer guitar, verifying native/web/guitar-webusb.js:
 *
 *   - a fake navigator.usb (X-plorer 1430:4748, one 0xFF/0x5D interface with an
 *     interrupt-IN endpoint, scripted transferIn packets) is injected via
 *     addInitScript BEFORE page load, and auto-authorized via getDevices() so no
 *     user gesture is needed;
 *   - the module claims it, decodes reports, and exposes a SYNTHETIC standard
 *     gamepad appended to navigator.getGamepads() — the existing C++ xinput_rb
 *     mapping consumes it with zero wasm changes.
 *
 * Assertions:
 *   1. the synthetic pad classifies as guitar (xinput_rb) via the [rb3-guitar]
 *      connected log (re-dispatched post-boot, as a real early connect would be
 *      missed before InitWebGuitar registered its listener);
 *   2. green fret + strum-down reach the engine chokepoint (window.__rb3Trace
 *      "in" rows, same bitmask technique as guitar-e2e-test.mjs);
 *   3. whammy half-press lands in window._rb3GpWhammy ≈ 0.5;
 *   4. USB disconnect removes the synthetic pad (getGamepads back to baseline);
 *   5. regression: navigator.usb present but NO authorized device → page loads
 *      clean, connect button visible, no synthetic pad, no JS errors.
 *
 * Usage: node scripts/web/guitar-webusb-e2e-test.mjs [--port 8421] [--release]
 *                                                    [--verbose]
 * Assumes a server already running (python3 native/web/server.py).
 * Output: scripts/web/results/guitar-webusb-e2e/
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
const OUT = resolve(__dirname, 'results/guitar-webusb-e2e');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// JoypadButton bits (engine side) for evidence checks.
const BIT = { green: 1 << 1, red: 1 << 5, down: 1 << 14, up: 1 << 12, star: 1 << 8 };

// ---------------------------------------------------------------------------
// Fake WebUSB + trace hook, installed PRE-LOAD. `authorize` toggles whether
// getDevices() returns the fake device (true = full scenario, false = regression).
// ---------------------------------------------------------------------------
const FAKE_INIT = (authorize) => {
  // Build a 20-byte X-plorer input report from a plain state object.
  const buildPacket = (st) => {
    st = st || {};
    const a = new Uint8Array(20);
    a[0] = 0x00; a[1] = 0x14;                 // input report header
    let b2 = 0, b3 = 0;
    if (st.strumUp) b2 |= 0x01;
    if (st.strumDown) b2 |= 0x02;
    if (st.dpadLeft) b2 |= 0x04;
    if (st.dpadRight) b2 |= 0x08;
    if (st.start) b2 |= 0x10;
    if (st.back) b2 |= 0x20;
    if (st.orange) b3 |= 0x01;
    if (st.guide) b3 |= 0x04;
    if (st.green) b3 |= 0x10;
    if (st.red) b3 |= 0x20;
    if (st.blue) b3 |= 0x40;
    if (st.yellow) b3 |= 0x80;
    a[2] = b2; a[3] = b3;
    // whammy: inverse of (raw+32768)/65535, so whammy01 round-trips through decode
    const w01 = (st.whammy01 == null) ? 0 : st.whammy01;
    const wRaw = (Math.round(w01 * 65535 - 32768)) & 0xffff;
    a[10] = wRaw & 0xff; a[11] = (wRaw >> 8) & 0xff;
    // tilt int16 LE @12 (active > 8192)
    const tRaw = st.tilt ? 20000 : 0;
    a[12] = tRaw & 0xff; a[13] = (tRaw >> 8) & 0xff;
    return a;
  };
  window.__xplorerState = {};                 // idle at boot
  window.__setXplorer = (st) => { window.__xplorerState = st || {}; };

  const fakeDevice = {
    vendorId: 0x1430,
    productId: 0x4748,
    opened: false,
    configuration: null,
    open: async function () { this.opened = true; },
    close: async function () { this.opened = false; },
    selectConfiguration: async function (n) {
      this.configuration = {
        configurationValue: n,
        interfaces: [{
          interfaceNumber: 0,
          alternates: [{
            alternateSetting: 0,
            interfaceClass: 0xff, interfaceSubclass: 0x5d, interfaceProtocol: 0x01,
            endpoints: [{ endpointNumber: 1, direction: 'in', type: 'interrupt', packetSize: 32 }]
          }]
        }]
      };
    },
    claimInterface: async function () { /* ok on macOS; would throw NetworkError on Linux */ },
    selectAlternateInterface: async function () { },
    clearHalt: async function () { },
    transferIn: function (ep, len) {
      return new Promise((res) => setTimeout(() => {
        if (!this.opened) { res({ status: 'babble', data: null }); return; }
        const a = buildPacket(window.__xplorerState);
        const buf = new Uint8Array(len);
        for (let i = 0; i < Math.min(len, a.length); i++) buf[i] = a[i];
        res({ status: 'ok', data: new DataView(buf.buffer) });
      }, 16));
    }
  };
  window.__fakeUsbDevice = fakeDevice;

  const listeners = {};
  // navigator.usb is a read-only accessor on the Navigator prototype in Chromium,
  // so a plain assignment silently no-ops. Override the property descriptor.
  Object.defineProperty(navigator, 'usb', {
    configurable: true,
    value: {
      getDevices: async function () { return authorize ? [fakeDevice] : []; },
      requestDevice: async function () { return fakeDevice; },
      addEventListener: function (t, cb) { (listeners[t] = listeners[t] || []).push(cb); },
      removeEventListener: function () { },
      __fire: function (t, device) { (listeners[t] || []).forEach((cb) => cb({ device })); }
    }
  });

  // Engine-chokepoint evidence collector (same as guitar-e2e-test.mjs): wrap
  // window.__rb3Trace.push to capture NDJSON "in" edge rows before the flusher drains.
  window.__testInEdges = [];
  const hookTrace = () => {
    const t = window.__rb3Trace;
    if (!Array.isArray(t) || t.__testHooked) return;
    t.__testHooked = true;
    const orig = t.push.bind(t);
    t.push = (...chunks) => {
      for (const c of chunks) for (const line of String(c).split('\n'))
        if (line.includes('"k":"in"')) { try { window.__testInEdges.push(JSON.parse(line)); } catch { } }
      return orig(...chunks);
    };
  };
  hookTrace();
  setInterval(hookTrace, 500);
};

// ---------------------------------------------------------------------------
let browser;
const logs = [];
const errors = [];
const results = [];
const check = (name, ok, detail = '') => {
  results.push({ name, ok: !!ok, detail });
  console.log(`  [${ok ? 'PASS' : 'FAIL'}] ${name}${detail ? ' — ' + detail : ''}`);
  return !!ok;
};

const state = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '',
  frame: window.rb3FrameCount || 0,
}));

async function waitFor(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await state(page);
    if (VERBOSE && s.screen !== last) { console.log(`    ...${label}: screen='${s.screen}' frame=${s.frame}`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(300);
  }
  return null;
}
async function snap(page, label) {
  try { await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, `${label}.png`) }); } catch { }
}
const inEdgeCount = (page) => page.evaluate(() => window.__testInEdges.length);
const inEdges = (page, since) => page.evaluate((n) => window.__testInEdges.slice(n), since);

const LAUNCH = {
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio'],
};

// ===========================================================================
try {
  browser = await chromium.launch(LAUNCH);

  // ---- SCENARIO A: authorized fake X-plorer -------------------------------
  const ctxA = await browser.newContext();
  const page = await ctxA.newPage();
  await page.setViewportSize({ width: 1280, height: 720 });
  await page.addInitScript(FAKE_INIT, true);
  page.on('console', (m) => {
    const t = m.text(); logs.push(t);
    if (VERBOSE || /rb3-guitar|xplorer/i.test(t)) console.log(`  [con] ${t}`);
  });
  page.on('pageerror', (e) => { errors.push(String(e)); console.log(`  [PAGE_ERROR] ${e}`); });

  const url = `http://127.0.0.1:${PORT}/${RELEASE ? '' : '?debug=true'}`;
  console.log(`\n=== guitar WebUSB e2e: ${url} ===`);
  await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

  // Module should have auto-claimed the fake device (no gesture). Poll — the
  // WebUSB open/claim/first-transfer chain is async.
  let claimed = false;
  for (let i = 0; i < 40 && !claimed; i++) {
    await sleep(200);
    claimed = await page.evaluate(() => !!(window._rb3Xplorer && window._rb3Xplorer.connected));
  }
  check('module auto-claims authorized X-plorer (no gesture)', claimed);
  const readyLog = logs.some(l => /\[rb3-guitar\] WebUSB X-plorer support ready/.test(l));
  check('module init log present', readyLog);

  // Synthetic pad is appended to getGamepads.
  const synth = await page.evaluate(() => {
    const gps = navigator.getGamepads();
    for (const g of gps) if (g && /X-plorer/.test(g.id)) return { id: g.id, mapping: g.mapping, btns: g.buttons.length, axes: g.axes.length };
    return null;
  });
  check('synthetic pad appended to getGamepads (standard, ≥17 btns, ≥4 axes)',
    synth && synth.mapping === 'standard' && synth.btns >= 17 && synth.axes >= 4,
    synth ? `id="${synth.id}" btns=${synth.btns} axes=${synth.axes}` : 'no synthetic pad');

  // Boot to splash so InitWebGuitar has registered its classifier listener.
  console.log('\n[boot] waiting for splash_screen...');
  const sp = await waitFor(page, s => s.screen === 'splash_screen', 240000, 'splash');
  if (!check('boot reaches splash_screen', !!sp)) throw new Error('no splash');
  await sleep(1500);
  await snap(page, '00_splash');

  // 1. Classification: re-dispatch gamepadconnected with the synthetic pad (a real
  //    early connect is missed before InitWebGuitar's listener exists).
  await page.evaluate(() => {
    const ev = new Event('gamepadconnected');
    Object.defineProperty(ev, 'gamepad', { value: window._rb3Xplorer.state });
    window.dispatchEvent(ev);
  });
  await sleep(400);
  const classLog = logs.filter(l => /\[rb3-guitar\] connected .*-> guitar \(xinput_rb\)/.test(l));
  check('synthetic pad classifies as guitar (xinput_rb)', classLog.length > 0, classLog[0] || 'no classify log');

  // 2. green fret + strum-down reach the engine chokepoint.
  console.log('\n[input] green + strum-down -> chokepoint...');
  const mark0 = await inEdgeCount(page);
  for (let i = 0; i < 4; i++) {
    await page.evaluate(() => window.__setXplorer({ green: true, strumDown: true }));
    await sleep(280);
    await page.evaluate(() => window.__setXplorer({}));    // release
    await sleep(320);
  }
  await page.evaluate(() => window.__setXplorer({ red: true }));
  await sleep(280);
  await page.evaluate(() => window.__setXplorer({}));
  await sleep(320);
  const edges = await inEdges(page, mark0);
  const sawGreen = edges.some(e => (e.dn & BIT.green) !== 0);
  const sawStrum = edges.some(e => (e.dn & BIT.down) !== 0);
  const sawBoth = edges.some(e => (e.b & BIT.green) && (e.b & BIT.down));
  const sawRed = edges.some(e => (e.dn & BIT.red) !== 0);
  check('green fret edge reaches chokepoint (dn bit1)', sawGreen, `${edges.length} edges`);
  check('strum-down edge reaches chokepoint (dn bit14)', sawStrum);
  check('green+strum simultaneously in one mask (b bit1+bit14)', sawBoth);
  check('red fret edge reaches chokepoint (dn bit5)', sawRed);

  // 3. whammy half-press -> window._rb3GpWhammy ≈ 0.5.
  console.log('\n[input] whammy sweep...');
  await page.evaluate(() => window.__setXplorer({ whammy01: 1.0 }));
  await sleep(400);
  const whFull = await page.evaluate(() => window._rb3GpWhammy);
  await page.evaluate(() => window.__setXplorer({ whammy01: 0.5 }));
  await sleep(400);
  const whHalf = await page.evaluate(() => window._rb3GpWhammy);
  await page.evaluate(() => window.__setXplorer({}));
  await sleep(400);
  const whRest = await page.evaluate(() => window._rb3GpWhammy);
  check('whammy tracks: full≈1, half≈0.5, rest≈0',
    whFull > 0.9 && whHalf > 0.4 && whHalf < 0.6 && Math.abs(whRest) < 0.05,
    `full=${whFull} half=${whHalf} rest=${whRest}`);

  // 4. USB disconnect removes the synthetic pad.
  console.log('\n[disconnect] fire navigator.usb disconnect...');
  const baseline = await page.evaluate(() => (navigator.getGamepads() || []).filter(Boolean).length);
  await page.evaluate(() => navigator.usb.__fire('disconnect', window.__fakeUsbDevice));
  await sleep(600);
  const afterDisc = await page.evaluate(() => ({
    connected: !!(window._rb3Xplorer && window._rb3Xplorer.connected),
    hasSynth: (navigator.getGamepads() || []).some(g => g && /X-plorer/.test(g.id)),
    count: (navigator.getGamepads() || []).filter(Boolean).length,
  }));
  check('disconnect clears synthetic pad', !afterDisc.connected && !afterDisc.hasSynth,
    `connected=${afterDisc.connected} hasSynth=${afterDisc.hasSynth}`);
  // baseline was measured WHILE the synthetic pad was active, so removing it drops
  // the count by exactly one (back to the real-pad-only baseline).
  check('getGamepads returns to baseline (synthetic removed)', afterDisc.count === baseline - 1,
    `while-active=${baseline} after=${afterDisc.count}`);

  await ctxA.close();

  // ---- SCENARIO B: regression — navigator.usb present, NO authorized device ----
  console.log('\n=== regression: navigator.usb present, no device ===');
  const ctxB = await browser.newContext();
  const pageB = await ctxB.newPage();
  const errorsB = [];
  await pageB.addInitScript(FAKE_INIT, false);   // getDevices() -> []
  pageB.on('console', (m) => { if (VERBOSE) console.log(`  [conB] ${m.text()}`); });
  pageB.on('pageerror', (e) => { errorsB.push(String(e)); console.log(`  [PAGE_ERROR_B] ${e}`); });
  await pageB.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
  await sleep(2000);
  const reg = await pageB.evaluate(() => {
    const btn = Array.from(document.querySelectorAll('button')).find(b => /Connect USB guitar/.test(b.textContent));
    return {
      btnVisible: !!(btn && btn.style.display !== 'none'),
      connected: !!(window._rb3Xplorer && window._rb3Xplorer.connected),
      hasSynth: (navigator.getGamepads() || []).some(g => g && /X-plorer/.test(g.id)),
    };
  });
  check('regression: connect button visible (no device authorized)', reg.btnVisible);
  check('regression: no synthetic pad without a device', !reg.connected && !reg.hasSynth);
  check('regression: no page/JS errors', errorsB.length === 0, errorsB.slice(0, 2).join(' | ') || 'clean');
  await ctxB.close();

  // Console-error scan for scenario A.
  const errLogs = logs.filter(l => /rb3-guitar.*(TypeError|undefined is not|ReferenceError)/.test(l));
  check('no page errors during scenario A', errors.length === 0, errors.slice(0, 3).join(' | ') || 'clean');
  check('no guitar-layer console errors', errLogs.length === 0, errLogs.slice(0, 3).join(' | ') || 'clean');

} catch (e) {
  console.log('\nEXCEPTION:', e && e.message ? e.message : e);
  results.push({ name: 'exception', ok: false, detail: String(e && e.message || e) });
} finally {
  if (browser) { try { await Promise.race([browser.close(), sleep(3000)]); } catch { } }
}

const failCount = results.filter(r => !r.ok).length;
console.log('\n=== guitar WebUSB e2e summary ===');
for (const r of results) console.log(`  ${r.ok ? 'PASS' : 'FAIL'}  ${r.name}${r.detail ? ' — ' + r.detail : ''}`);
writeFileSync(resolve(OUT, 'summary.json'), JSON.stringify({ results, errors: errors.slice(0, 30) }, null, 2));
console.log(`\n${failCount ? failCount + ' FAILURE(S)' : 'ALL CHECKS PASSED'} — details in ${resolve(OUT, 'summary.json')}`);
process.exit(failCount ? 1 : 0);
