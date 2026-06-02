#!/usr/bin/env node
/**
 * splash-diag.mjs — focused diagnostic for the splash → main_hub stall.
 *
 * Boots the web App, reaches splash_screen, then drives Start/Confirm with
 * KEY HOLDS (keyboard.down → wait several frames → keyboard.up) instead of
 * instantaneous press(), polling window.rb3CurrentScreen + window.rb3FocusButton
 * + window._rb3Keys after each step. Captures the full console (incl. the
 * engine RB3 web-input / RB3 screen / SendButtonMessages trace) and screenshots.
 *
 * Usage: node scripts/web/splash-diag.mjs [--port 8421]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const PORT = parseInt(argv[argv.indexOf('--port') + 1] || '8421', 10) || 8421;
const OUT = resolve(__dirname, 'results/splash-diag');
mkdirSync(OUT, { recursive: true });

const logs = [];
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

async function state(page) {
  return page.evaluate(() => ({
    screen: window.rb3CurrentScreen || '',
    focus: window.rb3FocusButton || '',
    keys: window._rb3Keys || 0,
    frame: window.rb3FrameCount || 0,
    songs: window.rb3SongCount || 0,
  }));
}

// Hold a key down for `holdMs`, then release. The engine edge-detects the
// _rb3Keys bitmask once per frame; holding for several frames guarantees the
// rising edge is observed even if the engine frame rate is low.
async function holdKey(page, key, holdMs = 700) {
  await page.keyboard.down(key);
  await sleep(holdMs);
  await page.keyboard.up(key);
}

const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio'],
});
const page = await browser.newPage();
await page.setViewportSize({ width: 1280, height: 720 });
const t0 = Date.now();
const el = () => ((Date.now() - t0) / 1000).toFixed(2);
page.on('console', (m) => {
  const t = m.text();
  logs.push({ el: el(), type: m.type(), text: t });
});

try {
  await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
  console.log('waiting for app boot + splash...');
  let s = {};
  for (let i = 0; i < 360; i++) {
    s = await state(page);
    if (s.screen === 'splash_screen') break;
    await sleep(500);
  }
  await sleep(2500);
  s = await state(page);
  console.log(`SPLASH reached: ${JSON.stringify(s)}  (${el()}s)`);
  await page.locator('#rb3-canvas').click({ force: true });
  await sleep(300);
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '00-splash.png') });

  // Step 1: hold Start (Space).
  console.log('\n--- HOLD Start (Space) ---');
  await holdKey(page, 'Space', 800);
  for (let i = 0; i < 8; i++) { await sleep(500); console.log(`  +${i}: ${JSON.stringify(await state(page))}`); }
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '01-after-start.png') });

  // Step 2: hold Confirm (Enter) up to 8 times, watching for screen change.
  for (let k = 0; k < 8; k++) {
    console.log(`\n--- HOLD Confirm (Enter) #${k + 1} ---`);
    await holdKey(page, 'Enter', 700);
    for (let i = 0; i < 4; i++) { await sleep(500); }
    s = await state(page);
    console.log(`  after confirm #${k + 1}: ${JSON.stringify(s)}`);
    if (s.screen && s.screen !== 'splash_screen') { console.log('  >>> SCREEN CHANGED'); break; }
  }
  await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, '02-after-confirms.png') });

  const final = await state(page);
  console.log(`\n=== FINAL: ${JSON.stringify(final)} ===`);
  writeFileSync(resolve(OUT, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
  writeFileSync(resolve(OUT, 'result.json'), JSON.stringify({ final }, null, 2));
} catch (e) {
  console.error('ERR', e.message);
  writeFileSync(resolve(OUT, 'console.jsonl'), logs.map(e => JSON.stringify(e)).join('\n') + '\n');
} finally {
  await Promise.race([browser.close(), sleep(3000)]);
}
