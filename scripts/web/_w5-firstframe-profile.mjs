#!/usr/bin/env node
/**
 * _w5-firstframe-profile.mjs — Wave-5 probe: CPU-profile the game_screen
 * FIRST-FRAME hitch. Drives boot -> hub -> song_select -> start song, arms the
 * CDP V8 sampling profiler right before confirming at part_difficulty, stops it
 * once game_screen is reached + a short dwell, writes firstframe.cpuprofile +
 * the MEMFS frame trace. Run against the DEBUG build (wasm names need -g2).
 *
 * Usage: node scripts/web/_w5-firstframe-profile.mjs --port 8446 --out /tmp/w5-prof [--release]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const has = (k) => argv.includes(`--${k}`);
const PORT = parseInt(arg('port', '8446'), 10);
const OUT = arg('out', '/tmp/w5-prof');
const RELEASE = has('release');
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const ENVQ = [`RB3_FRAME_TRACE=/trace.jsonl`, process.env.RB3_EXTRA_ENVQ].filter(Boolean).join(';');
const params = [RELEASE ? '' : 'debug=true', `env=${encodeURIComponent(ENVQ)}`].filter(Boolean).join('&');

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
const cdp = await ctx.newCDPSession(page);
await cdp.send('Storage.clearDataForOrigin', { origin: `http://127.0.0.1:${PORT}`, storageTypes: 'all' }).catch(() => {});

const stateOf = () => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
})).catch(() => ({ screen: '', frame: 0 }));
const press = async (key, holdMs = 200, gapMs = 450) => {
  try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(gapMs); } catch {}
};
const waitScreen = async (pred, timeoutMs, label) => {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await stateOf();
    if (s.screen !== last) { console.log(`  ${label}: '${s.screen}' f=${s.frame}`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
};

console.log(`goto :${PORT} build=${RELEASE ? 'release' : 'debug'}`);
await page.goto(`http://127.0.0.1:${PORT}/?${params}`, { waitUntil: 'domcontentloaded', timeout: 120000 });
await waitScreen(s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s.screen), 420000, 'boot');
await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
await sleep(500);
for (let i = 0; i < 30; i++) { const sc = (await stateOf()).screen; if (sc === 'splash_screen' || sc === 'main_hub_screen') break; await press('Space', 200, 600); }
await waitScreen(s => ['splash_screen', 'main_hub_screen'].includes(s.screen), 180000, 'splash');
await sleep(1200);
for (let i = 0; i < 18 && (await stateOf()).screen !== 'main_hub_screen'; i++) await press('Space', 250, 600);
await waitScreen(s => s.screen === 'main_hub_screen', 180000, 'main_hub');
await sleep(1500);
for (let i = 0; i < 14 && (await stateOf()).screen !== 'song_select_screen'; i++) await press('Enter', 220, 500);
await waitScreen(s => s.screen === 'song_select_screen', 90000, 'song_select');
await sleep(3000);
for (let i = 0; i < 3; i++) await press('ArrowDown', 130, 250);
await sleep(1000);

// to part_difficulty
for (let i = 0; i < 6 && !['part_difficulty_screen', 'game_screen'].includes((await stateOf()).screen); i++) await press('Enter', 220, 600);
console.log(`at: ${(await stateOf()).screen} — arming profiler`);

await cdp.send('Profiler.enable');
await cdp.send('Profiler.setSamplingInterval', { interval: 200 }); // 200us
await cdp.send('Profiler.start');
const profT0 = Date.now();

for (let i = 0; i < 6 && (await stateOf()).screen !== 'game_screen'; i++) await press('Enter', 220, 700);
const reached = await waitScreen(s => s.screen === 'game_screen', 180000, 'start_song');
await sleep(3000); // a few post-reveal frames

const { profile } = await cdp.send('Profiler.stop');
console.log(`profiled ${Date.now() - profT0} ms; reached_game=${!!reached}`);
writeFileSync(`${OUT}/firstframe.cpuprofile`, JSON.stringify(profile));

let traceText = '';
try {
  traceText = await page.evaluate(() => { try { return FS.readFile('/trace.jsonl', { encoding: 'utf8' }); } catch (e) { return 'ERR:' + e; } });
} catch (e) { traceText = 'EVAL_ERR:' + e; }
writeFileSync(`${OUT}/trace.jsonl`, traceText);
console.log(`wrote ${OUT}/firstframe.cpuprofile + trace.jsonl`);
await browser.close();
