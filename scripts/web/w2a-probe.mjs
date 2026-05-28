#!/usr/bin/env node
// Minimal diagnostic probe: load ?milo=<path>, stream ALL console live to
// stdout + a file (survives a page crash), exit after a fixed window.
import { chromium } from 'playwright';
import { writeFileSync, mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const milo = argv[argv.indexOf('--milo') + 1] || 'ui/track/gen/gem_smasher_guitar.milo_xbox';
const pi = argv.indexOf('--port');
const port = pi >= 0 ? parseInt(argv[pi + 1], 10) : 8421;
const wi = argv.indexOf('--wait');
const waitMs = wi >= 0 ? parseInt(argv[wi + 1], 10) : 40000;

mkdirSync(resolve(__dirname, 'results/w2a'), { recursive: true });
const logFile = resolve(__dirname, 'results/w2a/probe.log');
const logs = [];
const browser = await chromium.launch({
    headless: !process.env.DISPLAY,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11', '--mute-audio'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();
const t0 = Date.now();
const el = () => ((Date.now() - t0) / 1000).toFixed(2);
const rec = (tag, text) => { const line = `[${el()}s ${tag}] ${text}`; logs.push(line); console.log(line); try { writeFileSync(logFile, logs.join('\n') + '\n'); } catch {} };
page.on('console', m => rec(m.type(), m.text()));
page.on('pageerror', e => rec('PAGEERR', e.message || String(e)));
page.on('crash', () => rec('CRASH', 'page crashed'));

const url = `http://127.0.0.1:${port}/?milo=${encodeURIComponent(milo)}`;
rec('nav', url);
try { await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 }); } catch (e) { rec('navfail', e.message); }
await new Promise(r => setTimeout(r, waitMs));
try {
    const st = await page.evaluate(() => ({ milos: window.rb3MilosLoaded || 0, frames: window.rb3FrameCount || 0 }));
    rec('state', JSON.stringify(st));
} catch (e) { rec('statefail', e.message); }
await browser.close();
console.log(`\nlog: ${logFile}`);
