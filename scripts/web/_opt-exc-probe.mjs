// O1-diagnosis: pause on ALL exceptions during App-ctor boot, dump wasm stacks.
// Usage: node /tmp/rb3-opt-exc-probe.mjs [port]
import { chromium } from 'playwright';
const port = process.argv[2] || '8433';
const t0 = Date.now(); const el = () => ((Date.now() - t0) / 1000).toFixed(2);
const log = (s) => process.stdout.write(`[${el()}s] ${s}\n`);

const browser = await chromium.launch({ headless: true, args: [
  '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
  '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
  '--ozone-platform=x11', '--mute-audio',
]});
const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();
page.on('console', (m) => {
  const t = m.text();
  if (/boot error|rb3-stub|exception|Aborted|FAIL|constructing App|App constructed/i.test(t)) log(`console[${m.type()}] ${t}`);
});
page.on('pageerror', (e) => log(`PAGEERR ${e.message}\n${e.stack || ''}`));

const client = await page.context().newCDPSession(page);
const scripts = new Map(); // scriptId -> url
client.on('Debugger.scriptParsed', (e) => scripts.set(e.scriptId, e.url));
await client.send('Debugger.enable');
await client.send('Debugger.setPauseOnExceptions', { state: 'all' });

let excCount = 0;
client.on('Debugger.paused', async (e) => {
  if (e.reason !== 'exception' && e.reason !== 'promiseRejection') {
    await client.send('Debugger.resume').catch(() => {});
    return;
  }
  excCount++;
  const data = e.data ? JSON.stringify(e.data).slice(0, 600) : '';
  log(`--- EXCEPTION #${excCount} (${e.reason}) data=${data}`);
  for (const f of e.callFrames.slice(0, 40)) {
    const url = scripts.get(f.location.scriptId) || f.url || '?';
    log(`    at ${f.functionName || '<anon>'} (${url.split('/').pop()}:${f.location.lineNumber}:${f.location.columnNumber ?? ''})`);
  }
  await client.send('Debugger.resume').catch(() => {});
});

await page.goto(`http://127.0.0.1:${port}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });
// Wait until boot error or success, max 120s
for (let i = 0; i < 120; i++) {
  await new Promise((r) => setTimeout(r, 1000));
  const st = await page.evaluate(() => ({ booted: window.rb3AppBooted | 0, frames: window.rb3FrameCount | 0 })).catch(() => null);
  if (!st) { log('page unreachable'); break; }
  if (st.booted) { log(`App booted, frames=${st.frames}`); break; }
}
log(`done; exceptions seen: ${excCount}`);
await browser.close().catch(() => {});
