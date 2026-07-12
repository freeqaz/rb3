#!/usr/bin/env node
/**
 * _guitar-smoke.mjs — throwaway smoke test for USB guitar (Gamepad API) support.
 *
 * No physical guitar: injects a FAKE gamepad via navigator.getGamepads override
 * (page.addInitScript, so it exists before the wasm polls) and drives it. Proves
 * the JS input layer is alive + regression-free — a separate agent does the full
 * e2e gameplay pass.
 *
 * Checks:
 *   (a) classification log fires on a synthetic gamepadconnected for a guitar id
 *   (b) fret bit + hat + whammy are decoded (via _rb3GpDebug log + _rb3GpWhammy)
 *   (c) no console errors
 *   (d) a standard-mapping fake pad classifies 'standard' and leaves whammy at rest
 *
 * Assumes a server already running on --port (default 8421). Opens ONE page.
 * Usage: node scripts/web/_guitar-smoke.mjs [--port 8421]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; };
const PORT = parseInt(arg('--port', '8421'), 10) || 8421;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// Fake PS3/Wii RB guitar. 16 buttons, 10 axes (axes[9] = hat, axes[0] = whammy).
const FAKE_INIT = () => {
  const mkBtns = (n) => Array.from({ length: n }, () => ({ pressed: false, touched: false, value: 0 }));
  window.__fakeGuitar = {
    index: 0,
    id: 'Harmonix Guitar for Nintendo Wii (Vendor: 12ba Product: 0100)',
    mapping: '',
    connected: true,
    buttons: mkBtns(16),
    axes: [-1, 0, 0, 0, 0, 0, 0, 0, 0, 2], // whammy rest=-1, hat idle=2
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
};

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });
await page.addInitScript(FAKE_INIT);

const logs = [];
const errors = [];
page.on('console', (m) => { logs.push(m.text()); });
page.on('pageerror', (e) => { errors.push(String(e)); });

const grep = (re) => logs.filter(l => re.test(l));
let failures = [];
const check = (name, ok, detail) => {
  console.log(`  [${ok ? 'PASS' : 'FAIL'}] ${name}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures.push(name);
};

try {
  await page.goto(`http://127.0.0.1:${PORT}/?debug=true`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  console.log('booting...');
  // Wait until the guitar layer installs (proves the wasm ran our InitWebGuitar).
  let ready = false;
  for (let i = 0; i < 90; i++) {
    if (grep(/\[rb3-guitar\] USB guitar support ready/).length) { ready = true; break; }
    await sleep(1000);
  }
  check('(setup) InitWebGuitar ran', ready, ready ? 'support-ready log seen' : 'no support-ready log in 90s');

  // (a) Synthetic gamepadconnected for the guitar.
  await page.evaluate(() => {
    const ev = new Event('gamepadconnected');
    Object.defineProperty(ev, 'gamepad', { value: window.__fakeGuitar });
    window.dispatchEvent(ev);
  });
  await sleep(300);
  const connLog = grep(/\[rb3-guitar\] connected .*-> guitar \(ps3wii_rb\)/);
  check('(a) guitar classified on connect', connLog.length > 0, connLog[0] || 'no matching connect log');

  // (b) Enable debug, press GREEN (button 1) + hat up (axis9=-1) + full whammy (axis0=+1).
  await page.evaluate(() => {
    window._rb3GpDebug = 1;
    window.__fakeGuitar.buttons[1].pressed = true; // green fret
    window.__fakeGuitar.axes[9] = -1;              // hat up (strum up)
    window.__fakeGuitar.axes[0] = 1;               // whammy fully pressed
  });
  await sleep(600); // several engine frames
  const dbg = grep(/\[rb3-guitar\] fam=ps3wii_rb btns\[1/);
  check('(b1) green fret decoded (debug log btns[1)', dbg.length > 0, dbg[dbg.length - 1] || 'no debug fret log');
  const whammy = await page.evaluate(() => window._rb3GpWhammy);
  check('(b2) whammy axis -> ~1.0', typeof whammy === 'number' && whammy > 0.9, `_rb3GpWhammy=${whammy}`);
  const dbgUp = grep(/\[rb3-guitar\] fam=ps3wii_rb btns\[1.*whammy=1\.00/);
  check('(b3) whammy shown in debug log', dbgUp.length > 0, dbgUp[dbgUp.length - 1] || '(informational)');

  // Release -> whammy returns to rest.
  await page.evaluate(() => {
    window.__fakeGuitar.buttons[1].pressed = false;
    window.__fakeGuitar.axes[9] = 2;   // hat idle
    window.__fakeGuitar.axes[0] = -1;  // whammy rest
  });
  await sleep(400);
  const whammyRest = await page.evaluate(() => window._rb3GpWhammy);
  check('(b4) whammy returns to rest (0)', Math.abs(whammyRest) < 0.05, `_rb3GpWhammy=${whammyRest}`);

  // (d) Standard non-guitar pad: switch active pad, dispatch connected.
  await page.evaluate(() => {
    window.__activePad = () => window.__fakeStandard;
    const ev = new Event('gamepadconnected');
    Object.defineProperty(ev, 'gamepad', { value: window.__fakeStandard });
    window.dispatchEvent(ev);
  });
  await sleep(500);
  const stdLog = grep(/\[rb3-guitar\] connected .*-> standard/);
  check('(d1) standard pad classified standard', stdLog.length > 0, stdLog[0] || 'no standard connect log');
  const whammyStd = await page.evaluate(() => window._rb3GpWhammy);
  check('(d2) no-guitar -> whammy stays at rest (0)', Math.abs(whammyStd) < 0.05, `_rb3GpWhammy=${whammyStd}`);

  // (c) No page errors, no thrown JS.
  check('(c) no page/JS errors', errors.length === 0, errors.length ? errors.join(' | ') : 'clean');

  const guitarErrs = grep(/rb3-guitar.*(error|Error|undefined is not|TypeError)/);
  check('(c2) no guitar-layer error logs', guitarErrs.length === 0, guitarErrs.join(' | ') || 'clean');

} catch (e) {
  console.log('EXCEPTION:', e);
  failures.push('exception:' + e.message);
} finally {
  await browser.close();
}

console.log('\n=== guitar smoke summary ===');
console.log(failures.length ? `FAILURES: ${failures.join(', ')}` : 'ALL CHECKS PASSED');
// Dump the last few guitar logs for the record.
console.log('--- [rb3-guitar] logs ---');
grep(/rb3-guitar/).slice(-12).forEach(l => console.log('  ' + l));
process.exit(failures.length ? 1 : 0);
