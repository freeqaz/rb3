// Capture the browser PROCESS stderr (GPU/Dawn/renderer crash signatures) at the
// song_select crash. Launches chromium directly with verbose logging so a GPU
// process crash or wasm trap shows its real signal.
import { chromium } from 'playwright';

const PORT = parseInt(process.env.PORT || '8421', 10);
const QUERY = process.env.QUERY || '';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await chromium.launch({
    headless: true,
    args: [
      '--enable-logging=stderr', '--v=1',
      '--enable-unsafe-webgpu', '--enable-features=Vulkan',
      '--use-angle=vulkan',
      '--no-sandbox',
    ],
  });
  const ctx = await browser.newContext();
  const page = await ctx.newPage();
  const interesting = [];
  page.on('console', m => { const t = m.text(); if (/abort|memory|enlarge|RangeError|trap|unreachable|out of bounds|webgpu|device.*lost|gpu/i.test(t)) interesting.push(`[console:${m.type()}] ${t}`.slice(0,200)); });
  page.on('pageerror', e => interesting.push('PAGEERROR: ' + e.message.slice(0,200)));
  let crashed = false; page.on('crash', () => { crashed = true; interesting.push('*** page.on(crash) FIRED ***'); });

  const url = `http://localhost:${PORT}/` + (QUERY ? `?${QUERY}` : '');
  await page.goto(url, { waitUntil: 'domcontentloaded' }).catch(e => interesting.push('goto: '+e.message));
  const screenOf = () => page.evaluate(() => window.rb3CurrentScreen || '').catch(() => 'ERR');
  const press = async (k) => { try { await page.keyboard.down(k); await sleep(220); await page.keyboard.up(k); await sleep(180); } catch {} };

  for (let i = 0; i < 200; i++) { const s = await screenOf(); if (['intro_movie_screen','splash_screen','main_hub_screen'].includes(s)) break; await sleep(500); }
  try { await page.locator('#rb3-canvas').click({ force: true }); } catch {}
  for (let i = 0; i < 18 && (await screenOf()) !== 'main_hub_screen'; i++) await press('Space');
  console.log('reached:', await screenOf());
  for (let i = 0; i < 14 && !crashed && (await screenOf()) !== 'song_select_screen'; i++) await press('Enter');
  for (let i = 0; i < 12 && !crashed; i++) await sleep(500);
  console.log('FINAL:', crashed ? '(CRASHED)' : await screenOf());
  console.log('--- interesting page events ---');
  for (const l of [...new Set(interesting)]) console.log(' ', l);
  await browser.close().catch(()=>{});
  process.exit(0);
})().catch(e => { console.error('ERR', e.message); process.exit(2); });
