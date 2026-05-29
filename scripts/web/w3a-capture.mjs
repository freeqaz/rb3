#!/usr/bin/env node
// Minimal W3a capture: load the App boot, wait for rb3AppBooted, sample
// frameCount/screen over a fixed window, screenshot the canvas. Never blocks on
// a frame threshold — captures whatever rendered (intro_movie / tour_welcome /
// main_hub) so we get a screenshot even if the screen flow stalls.
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const PORT = parseInt(process.argv[process.argv.indexOf('--port') + 1] || '8431', 10);
const OUT = resolve(__dirname, 'results/web-w3a/menu');
mkdirSync(OUT, { recursive: true });

const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11','--mute-audio'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
const logs = [];
page.on('console', m => logs.push(m.text()));
page.on('pageerror', e => logs.push('PAGEERR: ' + (e.message||e)));

await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

// Wait up to 90s for App construction.
let booted = 0, deadline = Date.now() + 90000;
while (Date.now() < deadline) {
    booted = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
    if (booted >= 1) break;
    await new Promise(r => setTimeout(r, 500));
}
// Screenshot ASAP after boot (the tour-welcome screen can trap shortly after,
// so grab the canvas while the page is still alive), THEN sample.
let png = null;
try {
    await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, 'canvas-early.png') });
} catch (e) { /* ignore */ }
// Sample frameCount/screen for 8s after boot (don't require a threshold).
const samples = [];
for (let i = 0; i < 8; i++) {
    const s = await page.evaluate(() => ({
        f: window.rb3FrameCount || 0, scr: window.rb3CurrentScreen || '', b: window.rb3AppBooted || 0,
    })).catch(() => ({ f: -1, scr: 'evict', b: -1 }));
    samples.push({ t: i, ...s });
    await new Promise(r => setTimeout(r, 1000));
}
try {
    await page.locator('#rb3-canvas').screenshot({ path: resolve(OUT, 'canvas.png') });
    const { PNG } = await import('pngjs'); const { readFileSync } = await import('fs');
    const img = PNG.sync.read(readFileSync(resolve(OUT, 'canvas.png')));
    let painted = 0; for (let i = 0; i < img.data.length; i += 4)
        if (img.data[i] > 12 || img.data[i+1] > 12 || img.data[i+2] > 12) painted++;
    png = { w: img.width, h: img.height, paintedPct: +(100*painted/(img.width*img.height)).toFixed(2),
        center: { r: img.data[(img.height/2*img.width+img.width/2)*4], g: img.data[(img.height/2*img.width+img.width/2)*4+1], b: img.data[(img.height/2*img.width+img.width/2)*4+2] } };
} catch (e) { png = { error: e.message }; }

const last = samples[samples.length-1];
const out = { booted, lastFrame: last.f, lastScreen: last.scr, samples, png,
    screensSeen: [...new Set(logs.filter(l=>l.includes('currentScreen')).map(l=>l.split("=").pop().trim()))],
    pageErrors: logs.filter(l=>l.startsWith('PAGEERR')), };
writeFileSync(resolve(OUT, 'capture.json'), JSON.stringify(out, null, 2));
writeFileSync(resolve(OUT, 'console.log'), logs.join('\n'));
console.log(JSON.stringify({ booted, lastFrame: last.f, lastScreen: last.scr, png }, null, 2));
await browser.close();
