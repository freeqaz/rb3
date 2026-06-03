#!/usr/bin/env node
/**
 * gpu-boot-probe.mjs — report the ACTUAL WebGPU adapter rb3-web gets + the
 * App-ctor boot time, per launch backend. Used to settle whether the web
 * App-ctor cost (web-loadperf-findings) is software-shader-compile (SwiftShader)
 * or a genuine real-GPU cost.
 *
 * Backends:
 *   bundled (default) — Playwright's bundled chromium with the project's standard
 *                       WebGPU flags (the config every other web script uses).
 *                       Per ../dc3-decomp/docs/debugging/web.md this is expected
 *                       to get a REAL GPU context with no DISPLAY (Vulkan + the
 *                       /dev/dri render node). This probe verifies that.
 *   real    — system chromium (/usr/bin/chromium) + explicit real-GPU Vulkan flags.
 *   swift   — bundled chromium forced to SwiftShader software (the slow baseline).
 *
 * Run the Bash tool with dangerouslyDisableSandbox (Chromium needs the GPU device).
 * Usage: node scripts/web/gpu-boot-probe.mjs [--port 8421] [--backend bundled|real|swift] [--runs 3]
 */
import { chromium } from 'playwright';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8421'), 10) || 8421;
const BACKEND = arg('backend', 'bundled');
const RUNS = parseInt(arg('runs', '3'), 10) || 3;
const EXEC = arg('exec', '/usr/bin/chromium');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// The project-standard flags (mirror scripts/web/lib/core.mjs).
const STD = [
    '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--disable-extensions', '--disable-background-networking',
    '--disable-default-apps', '--disable-sync', '--mute-audio',
    '--autoplay-policy=no-user-gesture-required',
];

function launchOptsFor(backend) {
    if (backend === 'swift') {
        return { headless: true, args: STD.filter(a => !a.startsWith('--use-angle'))
            .concat(['--use-angle=swiftshader', '--use-gl=angle']) };
    }
    if (backend === 'real') {
        return { headless: true, executablePath: EXEC,
            args: STD.concat(['--use-gl=angle', '--ignore-gpu-blocklist', '--enable-gpu', '--ozone-platform=headless']) };
    }
    // bundled (default): exactly the project-standard config.
    return { headless: !process.env.DISPLAY, args: STD.concat(['--ozone-platform=x11']) };
}

async function oneRun() {
    const browser = await chromium.launch(launchOptsFor(BACKEND));
    try {
        const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
        const page = await ctx.newPage();
        let pageErr = '';
        page.on('pageerror', e => { pageErr = e.message; });
        await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil: 'domcontentloaded', timeout: 30000 });

        const adapter = await page.evaluate(async () => {
            if (!navigator.gpu) return { error: 'no navigator.gpu' };
            const a = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
            if (!a) return { error: 'no adapter' };
            let info = a.info || {};
            try { if (a.requestAdapterInfo) info = await a.requestAdapterInfo(); } catch (e) {}
            return { vendor: info.vendor || '', architecture: info.architecture || '',
                     device: info.device || '', description: info.description || '',
                     isFallback: !!a.isFallbackAdapter };
        }).catch(e => ({ error: e.message }));

        const deadline = Date.now() + 60000;
        let phases = [];
        while (Date.now() < deadline) {
            phases = await page.evaluate(() => window.rb3BootPhaseLog || []).catch(() => []);
            const booted = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
            if (booted >= 1 || pageErr) break;
            await sleep(200);
        }
        const m = Object.fromEntries(phases);
        const span = (a, b) => (m[a] != null && m[b] != null) ? (m[b] - m[a]) / 1000 : null;
        return { adapter, prefetch: m['fetch_start'] != null ? m['fetch_start'] / 1000 : null,
                 appctor: span('appctor_start', 'appctor_done'),
                 total: m['appctor_done'] != null ? m['appctor_done'] / 1000 : null, pageErr };
    } finally {
        await browser.close().catch(() => {});
    }
}

(async () => {
    console.log(`[gpu-probe] backend=${BACKEND}${BACKEND === 'real' ? ` exec=${EXEC}` : ' (bundled chromium)'} runs=${RUNS}`);
    const appctors = [];
    for (let i = 0; i < RUNS; i++) {
        const r = await oneRun();
        if (i === 0) {
            const a = r.adapter || {};
            const sw = /swiftshader|llvmpipe|software|google/i.test((a.vendor || '') + (a.description || '') + (a.architecture || ''));
            console.log(`[gpu-probe] WebGPU adapter: vendor='${a.vendor}' arch='${a.architecture}' device='${a.device}'`);
            console.log(`[gpu-probe]   description='${a.description}' isFallback=${a.isFallback}${a.error ? ` ERROR:${a.error}` : ''}`);
            console.log(`[gpu-probe]   => ${sw ? 'SOFTWARE (SwiftShader/llvmpipe)' : 'REAL GPU (hardware)'}`);
        }
        if (r.pageErr) console.log(`  run ${i}: PAGE ERROR ${r.pageErr.slice(0, 100)}`);
        console.log(`  run ${i}: prefetch=${r.prefetch?.toFixed(2)}s  appctor=${r.appctor?.toFixed(2)}s  total=${r.total?.toFixed(2)}s`);
        if (r.appctor != null) appctors.push(r.appctor);
    }
    if (appctors.length) {
        const min = Math.min(...appctors), max = Math.max(...appctors);
        const avg = appctors.reduce((a, b) => a + b, 0) / appctors.length;
        console.log(`[gpu-probe] App-ctor: min=${min.toFixed(2)}s avg=${avg.toFixed(2)}s max=${max.toFixed(2)}s`);
    }
})().catch(e => { console.error('[gpu-probe] ERROR', e); process.exit(2); });
