// Boot-path crash probe. Unlike _fullboot_dbg.mjs (console only), this also
// listens for pageerror / crash / CDP exceptions, so a wasm abort or an OOM
// cannot pass as "no output". Reports the LAST screen reached and whether the
// page was still alive at the end -- silence here means "survived", not
// "nothing happened", because liveness is checked explicitly every tick.
import { chromium } from 'playwright';
import { appendFileSync, writeFileSync } from 'fs';

const PORT = Number(process.env.PORT || 8421);
const SECS = Number(process.env.SECS || 150);
const OUT = process.env.OUT || '/tmp/rb3-boot-crash.log';
// FLAVOR=debug (default) or release. The default page URL serves RELEASE, which
// is HTTP-cached and version-busted, so a stale release deployment is invisible
// unless you test it explicitly.
const FLAVOR = process.env.FLAVOR || 'debug';
const URL = process.env.URL ||
    (FLAVOR === 'release' ? `http://127.0.0.1:${PORT}/` : `http://127.0.0.1:${PORT}/?debug=true`);

writeFileSync(OUT, '');
const t0 = Date.now();
const el = () => ((Date.now() - t0) / 1000).toFixed(1);
const log = (s) => { appendFileSync(OUT, s + '\n'); process.stdout.write(s + '\n'); };

// GPU=swiftshader (default, deterministic CPU raster) or GPU=vulkan (the REAL
// WebGPU backend). swiftshader cannot exercise a driver/WebGPU-specific fault,
// so a clean swiftshader run is NOT evidence that a real browser survives.
const GPU = process.env.GPU || 'swiftshader';
const gpuArgs = GPU === 'vulkan'
    ? ['--use-angle=vulkan', '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration']
    : ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', '--disable-gpu',
       '--enable-features=Vulkan,WebAssemblyJSPromiseIntegration'];

const browser = await chromium.launch({
    headless: true,
    args: ['--no-sandbox', '--enable-unsafe-webgpu', ...gpuArgs, '--mute-audio',
           '--js-flags=--max-old-space-size=4096'],
});
const page = await (await browser.newContext({ viewport: { width: 1280, height: 720 } })).newPage();

let hardFail = null;
const SIG = /abort|OOM|out of memory|enlarge|RangeError|RuntimeError|unreachable|trap|exception|Maximum call stack|memory access out of bounds|table index is out of bounds|null function|MILO_FAIL|FAIL \(/i;

page.on('console', (m) => { const t = m.text(); if (SIG.test(t)) log(`[${el()}s ${m.type()}] ${t}`); });
page.on('pageerror', (e) => { hardFail ??= `pageerror: ${e.message}`; log(`[${el()}s PAGEERR] ${e.message}`); });
page.on('crash', () => { hardFail ??= 'renderer crash'; log(`[${el()}s] *** RENDERER CRASH ***`); });

const client = await page.context().newCDPSession(page);
await client.send('Log.enable').catch(() => {});
await client.send('Runtime.enable').catch(() => {});
// Known-benign: dev-only assets the shipped bundle legitimately lacks
// (dev_only/selvenue.dta, framerate/frame_rate.dta, patchcreator/*,
// rndobj/gen/sphere). They log as CDP errors but are not faults -- counting
// them as hardFail makes every run "fail" and trains you to ignore the verdict.
const BENIGN_404 = /404|Failed to load resource/i;
client.on('Log.entryAdded', (e) => {
    if (e.entry.level !== 'error') return;
    const benign = BENIGN_404.test(e.entry.text);
    if (!benign) hardFail ??= `cdp: ${e.entry.text}`;
    log(`[${el()}s CDP ${e.entry.level}${benign ? ' benign' : ''}] ${e.entry.text}`);
});
client.on('Runtime.exceptionThrown', (e) => {
    const d = e.exceptionDetails;
    hardFail ??= `uncaught: ${d.text}`;
    log(`[${el()}s UNCAUGHT] ${d.text} ${d.exception?.description || ''}`);
});

await page.goto(URL, { waitUntil: 'domcontentloaded', timeout: 30000 });

let alive = true, lastScreen = '', ticks = 0;
for (let i = 0; i < SECS && alive; i++) {
    await new Promise((r) => setTimeout(r, 1000));
    const s = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => null);
    if (s === null) { alive = false; hardFail ??= 'page unreachable (evaluate failed)'; log(`[${el()}s] PAGE UNREACHABLE`); break; }
    ticks++;
    if (s !== lastScreen) { log(`[${el()}s] screen -> '${s}'`); lastScreen = s; }
}

// Denominator is part of the verdict: a PASS with 0 liveness ticks is not a pass.
log(`VERDICT alive=${alive} ticks=${ticks}/${SECS} lastScreen='${lastScreen}' hardFail=${hardFail || 'none'}`);
await browser.close().catch(() => {});
process.exit(hardFail || !alive ? 1 : 0);
