// Wave-4 UAF stress: at a throttled condition, hover-switch songs rapidly BEFORE
// each cold load completes, N times. Exercises abandon-in-flight (N1 kicked
// fetches, N2 Range-chunk drops, engine UAF fix). Asserts no pageerror/crash.
// Usage: node _w4-uaf-stress.mjs --port 8448 --mbps 4 --rtt 150 --iters 10
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i >= 0 ? argv[i+1] : d; };
const PORT = parseInt(arg('port', '8448'), 10);
const MBPS = parseFloat(arg('mbps', '4'));
const RTT = parseFloat(arg('rtt', '150'));
const ITERS = parseInt(arg('iters', '10'), 10);
const sleep = ms => new Promise(r => setTimeout(r, ms));

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
const cdp = await ctx.newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.emulateNetworkConditions', {
  offline: false, latency: RTT,
  downloadThroughput: (MBPS*1024*1024)/8, uploadThroughput: (MBPS*1024*1024)/8 });
await cdp.send('Storage.clearDataForOrigin', { origin: `http://127.0.0.1:${PORT}`, storageTypes: 'all' }).catch(()=>{});

const errors = [];
page.on('pageerror', e => errors.push('PAGEERROR: ' + e.message));
page.on('crash', () => errors.push('CRASH'));
page.on('console', m => { const t = m.text(); if (/function signature mismatch|call_indirect|RuntimeError|abort\(|UAF|use-after-free/.test(t)) errors.push('TRAP: ' + t); });

const state = () => page.evaluate(() => ({ screen: window.rb3CurrentScreen || '?', song: window.rb3HighlightedSong || '' })).catch(() => ({ screen: '?' }));
// Held keypress (game input layer needs a hold duration; instant press is ignored).
const press = async (k, hold = 200, gap = 420) => { try { await page.keyboard.down(k); await sleep(hold); await page.keyboard.up(k); await sleep(gap); } catch {} };

await page.goto(`http://127.0.0.1:${PORT}/?env=${encodeURIComponent('RB3_FRAME_TRACE=/trace.jsonl')}`, { waitUntil: 'domcontentloaded', timeout: 120000 });

// boot -> song_select
const deadline = Date.now() + 300000;
let s = '?';
while (Date.now() < deadline) { s = (await state()).screen; if (['splash_screen','main_hub_screen'].includes(s)) break; await sleep(800); }
await page.locator('#rb3-canvas').click({ force: true }).catch(()=>{});
for (let i = 0; i < 30 && !['splash_screen','main_hub_screen'].includes((await state()).screen); i++) await press('Space');
for (let i = 0; i < 30 && (await state()).screen !== 'main_hub_screen'; i++) { await press('Space'); await sleep(700); }
// hub -> song_select: Enter chain (playnow -> quickplay -> library); under heavy
// throttle each transition fetch takes seconds, so retry slowly with long waits.
for (let i = 0; i < 40 && (await state()).screen !== 'song_select_screen'; i++) {
  await press('Enter'); await sleep(2500);
  if (errors.length) break;
}
const reachedSel = (await state()).screen === 'song_select_screen';
await sleep(3000);

// RAPID hover-switch: jump rows, wait only ~700ms (< the ~1s preview debounce +
// cold-chunk latency), then jump again — abandoning each in-flight load.
let switches = 0;
for (let i = 0; i < ITERS; i++) {
  for (let j = 0; j < 4; j++) { await press('ArrowDown', 90, 60); }
  switches++;
  await sleep(700); // deliberately too short for the cold load to finish
  if (errors.length) break;
}
// settle: let one load finish to prove we're still alive
await sleep(8000);
const finalScreen = (await state()).screen;
const alive = finalScreen !== '?' && errors.length === 0;
console.log(JSON.stringify({ reachedSel, switches, finalScreen, errors: errors.slice(0,8), errorCount: errors.length, alive }));
await browser.close();
process.exit(alive && reachedSel ? 0 : 1);
