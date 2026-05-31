// Draw-time MENU_DBG probe capture for the main-hub button-stacking bug.
// Boots splash -> main_hub, sets MENU_DBG=1 so the engine's BandRnd::DrawMesh
// logs each hub button/label mesh's world position + projected screen-Y, then
// dumps the unique [MENU_DBG] lines and saves a screenshot.
import { chromium } from 'playwright';

const port = parseInt(process.argv[2] || '8710', 10);
const out = process.argv[3] || '/tmp/menuhub_probe.png';

const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11', '--mute-audio'],
});

const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();

const menuLines = [];
page.on('console', m => {
    const t = m.text();
    if (t.includes('[MENU_DBG]')) menuLines.push(t);
});
page.on('pageerror', e => console.log('PAGEERR:', e.message));

await page.addInitScript(() => {
    window.Module = window.Module || {};
    window.Module.preRun = window.Module.preRun || [];
    window.Module.preRun.push(function() {
        if (typeof ENV !== 'undefined') ENV.MENU_DBG = '1';
    });
});

await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
console.log('page loaded, waiting for boot...');

for (let i = 0; i < 600; i++) {
    const booted = await page.evaluate(() => window.rb3AppBooted || 0);
    if (booted >= 1) { console.log(`boot after ${i*0.5}s`); break; }
    await new Promise(r => setTimeout(r, 500));
}

console.log('waiting for splash...');
for (let i = 0; i < 120; i++) {
    const s = await page.evaluate(() => window.rb3CurrentScreen || '');
    if (s === 'splash_screen') { console.log(`splash after ${i*0.5}s`); break; }
    await new Promise(r => setTimeout(r, 500));
}

await page.locator('#rb3-canvas').click({ force: true });
await new Promise(r => setTimeout(r, 800));
await page.keyboard.press('Space');
await new Promise(r => setTimeout(r, 500));
await page.keyboard.press('Enter');
await new Promise(r => setTimeout(r, 500));
await page.keyboard.press('Enter');

console.log('waiting for main_hub...');
for (let i = 0; i < 60; i++) {
    const s = await page.evaluate(() => window.rb3CurrentScreen || '');
    if (s === 'main_hub_screen') { console.log(`main_hub after ${i*0.5}s`); break; }
    await new Promise(r => setTimeout(r, 500));
}

await new Promise(r => setTimeout(r, 5000));

const screen = await page.evaluate(() => window.rb3CurrentScreen || '');
const frame = await page.evaluate(() => window.rb3FrameCount || 0);
console.log(`screen='${screen}' frame=${frame}`);

await page.locator('#rb3-canvas').screenshot({ path: out });
console.log(`saved ${out}`);

console.log('\n===== MENU_DBG (engine dedups by world x,z) =====');
const seen = new Set();
for (const l of menuLines) {
    const clean = l.replace(/^.*\[MENU_DBG\]/, '[MENU_DBG]');
    if (seen.has(clean)) continue;
    seen.add(clean);
    console.log(clean);
}
console.log(`===== ${seen.size} unique lines, ${menuLines.length} total =====`);

await browser.close();
