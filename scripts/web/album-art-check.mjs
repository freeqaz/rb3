// Verify the album-art smear fix on the REAL web swapchain: boot to song_select,
// scroll to a song with a known cover (The Beautiful People → Marilyn Manson), and
// screenshot. Check: center smear gone AND top-right cover still present (the fix
// hides album.mesh under HX_NATIVE, which also applies to web — must not remove the
// working web cover). Held keys avoid the keydown/keyup-vs-poll press race.
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import http from 'http';
const port = parseInt(process.argv[process.argv.indexOf('--port') + 1] || '8920', 10);
const OUT = '/tmp/rb3-art-web'; mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const getScreen = (p) => p.evaluate(() => window.rb3CurrentScreen || '');
const getHL = (p) => p.evaluate(() => ({ tok: window.rb3HighlightedSong || '', ty: window.rb3HighlightedType }));
function waitForServer(p, t = 15000) { return new Promise((res, rej) => { const d = Date.now() + t;
  const c = () => http.get(`http://127.0.0.1:${p}/api/health`, r => r.statusCode === 200 ? res() : rt()).on('error', rt);
  const rt = () => Date.now() > d ? rej(new Error('no server')) : setTimeout(c, 300); c(); }); }
async function waitScreen(p, targets, t = 30000) { const d = Date.now() + t; let s = await getScreen(p);
  while (Date.now() < d) { s = await getScreen(p); if (targets.includes(s)) return s; await sleep(250); } return s; }
async function hold(page, key, h = 150, g = 120) { await page.keyboard.down(key); await sleep(h); await page.keyboard.up(key); await sleep(g); }

await waitForServer(port);
const browser = await chromium.launch({ headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration', '--ozone-platform=x11', '--mute-audio'] });
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
const page = await ctx.newPage();
page.on('pageerror', e => console.log('PAGEERR:', e.message));
await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
for (let i = 0; i < 600; i++) { if (await page.evaluate(() => window.rb3AppBooted || 0) >= 1) break; await sleep(500); }
await waitScreen(page, ['splash_screen'], 120000);
await page.locator('#rb3-canvas').click({ force: true }); await sleep(800);
await hold(page, 'Space'); await hold(page, 'Enter'); await hold(page, 'Enter');
let s = await waitScreen(page, ['main_hub_screen'], 30000);
for (let i = 0; i < 8 && s === 'main_hub_screen'; i++) { await hold(page, 'Enter');
  const cur = await getScreen(page); if (cur && cur !== 'main_hub_screen') { s = cur; break; } }
s = await waitScreen(page, ['song_select_screen', 'song_select_enter_screen'], 30000);
if (s === 'song_select_enter_screen') s = await waitScreen(page, ['song_select_screen'], 30000);
console.log('song_select:', s); await sleep(2500);
// Scroll to a mounted-cover song and screenshot a few depths.
const shots = [];
for (let i = 1; i <= 12; i++) {
  await hold(page, 'ArrowDown', 150, 130);
  const hl = await getHL(page);
  if ([4, 8, 12].includes(i)) {
    const path = `${OUT}/web_depth_${String(i).padStart(2,'0')}_${hl.tok || 'x'}.png`;
    await page.locator('#rb3-canvas').screenshot({ path });
    console.log(`depth ${i}: token='${hl.tok}' type=${hl.ty} -> ${path}`);
  }
}
await browser.close();
console.log('done; frames in', OUT);
