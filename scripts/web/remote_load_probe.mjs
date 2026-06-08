#!/usr/bin/env node
// Load an arbitrary RB3-web URL (e.g. the remote tailnet server) in headless
// chromium and report whether it boots + the audio context rate + console health.
// Usage: node remote_load_probe.mjs [url] [waitSecs]
import { chromium } from 'playwright';

const URL = process.argv[2] || 'http://home.freeqaz.com:8421/';
const WAIT = parseInt(process.argv[3] || '180', 10);
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();

let randAsserts = 0, lastConsole = '';
const errors = [];
page.on('console', (m) => {
  const t = m.text();
  if (/Rand\.cpp.*MainThread/.test(t)) randAsserts++;
  else lastConsole = t.slice(0, 160);
});
page.on('pageerror', (e) => errors.push('pageerror: ' + e.message.slice(0, 160)));
page.on('crash', () => errors.push('PAGE CRASHED'));

console.log(`loading ${URL} (wait ${WAIT}s) ...`);
try {
  await page.goto(URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
} catch (e) {
  console.log('goto FAILED: ' + e.message); await browser.close(); process.exit(2);
}

// One-shot environment report: cross-origin isolation + SAB + the real GPU adapter
const env = await page.evaluate(async () => {
  let gpu = 'none';
  try {
    const a = await navigator.gpu.requestAdapter();
    const info = a && (a.info || (a.requestAdapterInfo && await a.requestAdapterInfo()));
    gpu = info ? `${info.vendor}/${info.architecture}${info.isFallback ? ' (FALLBACK)' : ''}` : 'adapter-no-info';
  } catch (e) { gpu = 'err:' + e.message; }
  return { coi: window.crossOriginIsolated, sab: typeof SharedArrayBuffer, gpu };
}).catch((e) => ({ err: String(e) }));
console.log(`ENV: crossOriginIsolated=${env.coi} SharedArrayBuffer=${env.sab} gpu=${env.gpu}`);

const t0 = Date.now();
let last = '', booted = false;
while ((Date.now() - t0) / 1000 < WAIT) {
  let s;
  try {
    s = await page.evaluate(() => ({
      screen: window.rb3CurrentScreen || '',
      frame: window.rb3FrameCount || 0,
      booted: !!window.rb3AppBooted,
      ctxRate: (window.rb3AudioContext && window.rb3AudioContext.sampleRate) ||
               window.rb3AudioCtxRate || 0,
    }));
  } catch (e) { errors.push('eval: ' + String(e).slice(0, 120)); break; }
  const el = ((Date.now() - t0) / 1000).toFixed(1);
  if (s.screen !== last) { console.log(`[${el}s] screen='${s.screen}' frame=${s.frame} booted=${s.booted} ctxRate=${s.ctxRate}`); last = s.screen; }
  if (s.frame > 30 && s.screen) { booted = true; console.log(`[${el}s] BOOTED OK -> screen='${s.screen}' frame=${s.frame} ctxRate=${s.ctxRate}`); break; }
  await sleep(1500);
}
console.log(`result: booted=${booted} randAsserts=${randAsserts} errors=${JSON.stringify(errors.slice(0, 4))} lastConsole='${lastConsole}'`);
await browser.close();
process.exit(booted ? 0 : 1);
