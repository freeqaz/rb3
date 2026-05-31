import { chromium } from 'playwright';
import { writeFileSync } from 'fs';

const port = parseInt(process.argv[2] || '8903', 10);
const out = process.argv[3] || '/tmp/main_hub_capture.png';

const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11', '--mute-audio'],
});

const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();

page.on('console', m => {
    const t = m.text();
    if (/error|fail|ERR|main_hub|btn|reveal|anim|PROPANIM|PK target|BTNPROBE|CAMPROBE|W8|MESHCAM|frame drawn|meshes=|CAM_DBG/i.test(t)) console.log(`[${m.type()}] ${t}`);
});
page.on('pageerror', e => console.log('PAGEERR:', e.message));

// Set MILO_PROPANIM_DBG=1 in WASM env BEFORE main() runs
await page.addInitScript(() => {
    window.Module = window.Module || {};
    window.Module.preRun = window.Module.preRun || [];
    window.Module.preRun.push(function() {
        if (typeof ENV !== 'undefined') ENV.MILO_W8_CAMPROBE = '1';
    });
});

await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
console.log('page loaded, waiting for boot...');

// Wait for app booted
for (let i = 0; i < 600; i++) {
    const booted = await page.evaluate(() => window.rb3AppBooted || 0);
    if (booted >= 1) { console.log(`boot after ${i*0.5}s`); break; }
    await new Promise(r => setTimeout(r, 500));
}

// Wait for splash + send keys to advance to main_hub
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

// Wait an additional 5s for anims to settle
await new Promise(r => setTimeout(r, 5000));

const screen = await page.evaluate(() => window.rb3CurrentScreen || '');
const frame = await page.evaluate(() => window.rb3FrameCount || 0);
console.log(`screen='${screen}' frame=${frame}`);

await page.locator('#rb3-canvas').screenshot({ path: out });
console.log(`saved ${out}`);
await browser.close();
