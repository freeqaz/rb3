#!/usr/bin/env node
/**
 * warm-boot-check.mjs — prove the R3 W4b IndexedDB write-back fix.
 *
 * R3 routes the boot-critical .milo_xbox set through the async /api/bundle/boot
 * bundle (see docs/native/web-perf-roadmap/R3-boot-bundle-expansion.md). The
 * engine's onBundleSuccess now also writes each unpacked file BACK to the W4b
 * IndexedDB warm cache (window.__rb3CachePut), so a SECOND boot serves the boot
 * set from IDB and does NOT re-download the ~60 MB boot bundle.
 *
 * The netperf cold-only matrix uses a fresh context per run (cold IDB), so it
 * CANNOT surface this regression. This script drives TWO boots in ONE persistent
 * browser context (launchPersistentContext + a userDataDir) so IndexedDB
 * survives between loads, and asserts:
 *
 *   - boot 1 (cold IDB): downloads the full /api/bundle/boot (~60 MB).
 *   - boot 2 (warm IDB): downloads MUCH less for /api/bundle/boot — ideally
 *     0 bytes (the boot opens are served from IDB before they reach any bundle
 *     fetch is even needed... in R3 the bundle still FIRES, but every file is an
 *     IDB hit so the sync path never runs; the real win is the per-file sync
 *     re-download being gone). We assert the 2nd boot's NETWORK for the boot
 *     set collapses vs the 1st.
 *
 * NOTE on the R3 shape: R3 still issues /api/bundle/boot on every boot (it does
 * not gate the bundle off when IDB is warm — that is the design's alternative
 * (b)). So the headline warm-boot win this script proves is that the PER-FILE
 * boot milos are served from IDB (window.__rb3CacheStats.hits jumps, bytesFromCache
 * ~= the boot set) and the sync XHR path issues ZERO milo re-downloads on boot 2.
 * It also reports the boot-bundle bytes both boots so the reviewer sees whether
 * the bundle itself was re-fetched (a follow-up could add the IDB-resident gate
 * to skip it; the write-back fix is the must-have correctness piece).
 *
 * USAGE:
 *   node scripts/web/warm-boot-check.mjs [--port 8421] [--profile low|normal|local]
 *                                        [--out <dir>] [--keep]
 */
import { chromium } from 'playwright';
import { waitForServer, outputDir, saveJson } from './lib/core.mjs';
import { resolve } from 'path';
import { mkdtempSync, rmSync, mkdirSync } from 'fs';
import { tmpdir } from 'os';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8421'), 10) || 8421;
const PROFILE = arg('profile', 'low');
const KEEP = argv.includes('--keep');
const OUT = outputDir('warmboot', argv.includes('--out') ? arg('out') : null);

const mbit = (m) => Math.round((m * 1_000_000) / 8);
const PROFILES = {
    low:    { label: '50 Mbit/s',  downBps: mbit(50),  upBps: mbit(10), latencyMs: 30 },
    normal: { label: '200 Mbit/s', downBps: mbit(200), upBps: mbit(50), latencyMs: 15 },
    local:  { label: 'unbounded',  downBps: -1,         upBps: -1,        latencyMs: 0  },
};
const profile = PROFILES[PROFILE] || PROFILES.low;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// Drive one boot in the given (already-open) persistent context's page, tracking
// network per-URL via CDP, and waiting for the first interactive screen.
async function bootOnce(context, label) {
    const page = await context.newPage();
    const cdp = await context.newCDPSession(page);
    await cdp.send('Network.enable');
    await cdp.send('Network.emulateNetworkConditions', {
        offline: false, latency: profile.latencyMs,
        downloadThroughput: profile.downBps, uploadThroughput: profile.upBps,
    });

    // Per-request byte tally keyed on URL path.
    const reqs = new Map(); // requestId -> {url, bytes}
    cdp.on('Network.requestWillBeSent', (e) => {
        reqs.set(e.requestId, { url: (e.request.url || '').replace(/^https?:\/\/[^/]+/, ''), bytes: 0 });
    });
    cdp.on('Network.dataReceived', (e) => { const r = reqs.get(e.requestId); if (r) r.bytes += e.dataLength || 0; });
    cdp.on('Network.loadingFinished', (e) => { const r = reqs.get(e.requestId); if (r && e.encodedDataLength) r.bytes = e.encodedDataLength; });

    const url = `http://127.0.0.1:${PORT}/`;
    console.log(`\n[${label}] navigating ${url} (net=${profile.label})`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Wait for the first interactive screen (intro/splash/main_hub) or app-booted.
    const deadline = Date.now() + 120000;
    let screen = '';
    while (Date.now() < deadline) {
        screen = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(screen)) break;
        await sleep(300);
    }
    await sleep(1500); // let tail fetches + IDB writes settle

    // Read the W4b cache stats the engine publishes.
    const stats = await page.evaluate(() => {
        const s = window.__rb3CacheStats || {};
        return {
            hits: s.hits || 0, misses: s.misses || 0,
            bytesFromCache: s.bytesFromCache || 0, bytesFetched: s.bytesFetched || 0,
            puts: s.puts || 0, writeErrors: s.writeErrors || 0,
            idbReady: window.__rb3IdbReady || 0,
            idbMapSize: (window.__rb3IdbCache && window.__rb3IdbCache.size) || 0,
            songs: window.rb3SongCount || 0,
            booted: window.rb3AppBooted || 0,
        };
    }).catch(() => ({}));

    // Roll up the network for this boot.
    const all = [...reqs.values()];
    const sumBy = (pred) => all.filter(pred).reduce((s, r) => s + (r.bytes || 0), 0);
    const cntBy = (pred) => all.filter(pred).length;
    const net = {
        totalBytes: sumBy(() => true),
        bootBundleBytes: sumBy(r => r.url === '/api/bundle/boot'),
        bootBundleReqs: cntBy(r => r.url === '/api/bundle/boot'),
        configBundleBytes: sumBy(r => r.url === '/api/bundle'),
        miloFileReqs: cntBy(r => r.url.startsWith('/api/file/') && r.url.endsWith('.milo_xbox')),
        miloFileBytes: sumBy(r => r.url.startsWith('/api/file/') && r.url.endsWith('.milo_xbox')),
    };

    console.log(`[${label}] screen='${screen}' booted=${stats.booted} songs=${stats.songs} idbReady=${stats.idbReady} idbMapSize=${stats.idbMapSize}`);
    console.log(`[${label}] cacheStats: hits=${stats.hits} misses=${stats.misses} puts=${stats.puts} bytesFromCache=${(stats.bytesFromCache/1e6).toFixed(1)}MB bytesFetched=${(stats.bytesFetched/1e6).toFixed(1)}MB writeErrors=${stats.writeErrors}`);
    console.log(`[${label}] network: total=${(net.totalBytes/1e6).toFixed(1)}MB  bootBundle=${(net.bootBundleBytes/1e6).toFixed(1)}MB(${net.bootBundleReqs}req)  configBundle=${(net.configBundleBytes/1e6).toFixed(1)}MB  /api/file milos=${net.miloFileReqs}req/${(net.miloFileBytes/1e6).toFixed(1)}MB`);

    await page.close();
    return { screen, stats, net };
}

(async () => {
    console.log(`[warmboot] waiting for server :${PORT}`);
    await waitForServer(PORT, 20000);
    try { mkdirSync(OUT, { recursive: true }); } catch {}

    const userDataDir = mkdtempSync(resolve(tmpdir(), 'rb3-warmboot-'));
    console.log(`[warmboot] persistent userDataDir=${userDataDir} (IDB survives between boots)`);

    const context = await chromium.launchPersistentContext(userDataDir, {
        headless: !process.env.DISPLAY,
        viewport: { width: 1280, height: 720 },
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps',
            '--disable-sync', '--mute-audio', '--autoplay-policy=no-user-gesture-required',
        ],
    });

    let boot1, boot2, verdict;
    try {
        boot1 = await bootOnce(context, 'boot1-COLD');
        // Close all pages but keep the SAME persistent context (and its IDB) for boot 2.
        boot2 = await bootOnce(context, 'boot2-WARM');
    } finally {
        await context.close().catch(() => {});
        if (!KEEP) { try { rmSync(userDataDir, { recursive: true, force: true }); } catch {} }
    }

    // ---- verdicts -----------------------------------------------------------
    const b1bb = boot1.net.bootBundleBytes, b2bb = boot2.net.bootBundleBytes;
    const dropPct = b1bb > 0 ? (100 * (b1bb - b2bb) / b1bb) : 0;
    // The headline W4b correctness signal: boot 2 served the boot set from IDB.
    const warmHitsBytes = boot2.stats.bytesFromCache;
    const wroteThroughOnBoot1 = boot1.stats.puts > 0 && boot1.stats.writeErrors === 0;
    const boot2ServedFromCache = boot2.stats.hits > 0 && warmHitsBytes > 30e6; // most of the ~60MB boot set
    const boot2NoMiloResync = boot2.net.miloFileBytes < 1e6; // no per-file milo re-download (404s aside)
    const bothBooted = !!boot1.stats.booted && !!boot2.stats.booted;

    const pass = wroteThroughOnBoot1 && boot2ServedFromCache && boot2NoMiloResync && bothBooted;
    verdict = {
        pass,
        wroteThroughOnBoot1, boot2ServedFromCache, boot2NoMiloResync, bothBooted,
        boot1_bootBundleBytes: b1bb, boot2_bootBundleBytes: b2bb, bootBundleDropPct: +dropPct.toFixed(1),
        boot2_bytesFromCacheMB: +(warmHitsBytes / 1e6).toFixed(1),
        boot1_putsCount: boot1.stats.puts, boot1_writeErrors: boot1.stats.writeErrors,
        boot2_idbMapSize: boot2.stats.idbMapSize,
    };

    console.log('\n' + '='.repeat(70));
    console.log('WARM-BOOT (W4b IDB write-back) VERDICT');
    console.log('='.repeat(70));
    console.log(`  boot1 wrote ${boot1.stats.puts} files to IDB (writeErrors=${boot1.stats.writeErrors})`);
    console.log(`  boot2 served ${(warmHitsBytes/1e6).toFixed(1)} MB from IDB cache (${boot2.stats.hits} hits)`);
    console.log(`  boot2 IDB map holds ${boot2.stats.idbMapSize} files`);
    console.log(`  /api/bundle/boot bytes: boot1=${(b1bb/1e6).toFixed(1)}MB  boot2=${(b2bb/1e6).toFixed(1)}MB  (drop ${dropPct.toFixed(1)}%)`);
    console.log(`  boot2 per-file /api/file milo re-download: ${(boot2.net.miloFileBytes/1e6).toFixed(2)}MB`);
    console.log(`  RESULT: ${pass ? 'PASS — boot set persisted to IDB and served warm on boot 2' : 'FAIL — see flags above'}`);
    console.log('='.repeat(70));

    saveJson({ profile: PROFILE, boot1, boot2, verdict }, OUT, 'warm-boot.json');
    console.log(`[warmboot] data -> ${resolve(OUT, 'warm-boot.json')}`);
    process.exit(pass ? 0 : 1);
})().catch(e => { console.error('[warmboot] FATAL', e); process.exit(2); });
