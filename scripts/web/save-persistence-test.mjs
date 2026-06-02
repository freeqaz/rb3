#!/usr/bin/env node
/**
 * RB3 Web — B3 IndexedDB save-persistence verification.
 *
 * Proves the IndexedDB-backed IPersistBackend round-trips ProfileMgr global
 * options across a page reload, plus two negative controls.
 *
 *   ROUND-TRIP (core): boot -> set A/V calibration (ExcessVideoLag) to a unique
 *     value via window.rb3SetExcessVideoLag -> verify it landed
 *     (window.rb3ExcessVideoLag) -> trigger the save (window.__rb3SaveRequested=1,
 *     advance a frame) -> assert window.__rb3SaveCache has the rev-locked
 *     globaloptions.bin + gameplayopts_0.bin -> page.reload() -> assert the value
 *     SURVIVED (prewarm -> Read -> MetaPanel post-clobber re-apply round-tripped
 *     through IDB).
 *
 *   FALLBACK: a fresh context with window.indexedDB forced undefined at boot.
 *     Assert no crash, __rb3SaveReady===1, __rb3SaveCache.size===0, app still
 *     reaches a real screen (first-run defaults path).
 *
 *   EXACT-SIZE GATE: pre-seed IDB 'rb3-web-saves' with a WRONG-length
 *     globaloptions.bin, boot, set a DIFFERENT calibration in this session, save,
 *     reload, and assert the value persisted (the wrong-length seed was rejected
 *     by the exact-size Read gate, so the session's own correct-length save won;
 *     i.e. the bad blob never trapped the rev-asserting deserializer).
 *
 * Forked from scripts/web/web-audio-capture.mjs (chromium launch args + boot
 * wait). Persistent on-disk context so IndexedDB survives reload AND a context
 * relaunch (the gate test seeds IDB before the wasm boots).
 *
 * Usage: node scripts/web/save-persistence-test.mjs [--port 8629]
 */

import { chromium } from 'playwright';
import { mkdtempSync, rmSync } from 'fs';
import { tmpdir } from 'os';
import { join } from 'path';
import http from 'http';

const argv = process.argv.slice(2);
const PORT = parseInt(argv[argv.indexOf('--port') + 1] || '8629', 10) || 8629;
const URL  = `http://127.0.0.1:${PORT}/`;

const BOOT_TIMEOUT_MS   = 300000;
const SCREEN_TIMEOUT_MS = 180000;

const LAUNCH_ARGS = [
    '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--disable-extensions',
    '--disable-background-networking', '--disable-default-apps',
    '--disable-sync', '--mute-audio',
];

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const tick = () => {
            const req = http.get({ host: '127.0.0.1', port, path: '/' }, (r) => {
                r.destroy(); res();
            });
            req.on('error', () => {
                if (Date.now() > deadline) rej(new Error('server never came up'));
                else setTimeout(tick, 300);
            });
        };
        tick();
    });
}

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

async function waitBooted(page, label) {
    const deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        const v = await page.evaluate(() => window.rb3AppBooted || 0).catch(() => 0);
        if (v >= 1) return true;
        await sleep(500);
    }
    throw new Error(`[${label}] app never booted`);
}

async function getScreen(page) {
    return page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
}

// Advance >=N rendered frames so the BOOT_RUNNING per-frame branches (save poll,
// SetExcessVideoLag poke, value publish) actually execute.
async function advanceFrames(page, n) {
    const start = await page.evaluate(() => window.rb3FrameCount || 0);
    const deadline = Date.now() + 30000;
    while (Date.now() < deadline) {
        const f = await page.evaluate(() => window.rb3FrameCount || 0);
        if (f >= start + n) return f;
        await sleep(100);
    }
    return await page.evaluate(() => window.rb3FrameCount || 0);
}

// Set the A/V calibration via the polled command sentinel, then wait for the
// frame loop to apply it and publish the readback.
async function setCalibration(page, value) {
    await page.evaluate((v) => { window.rb3SetExcessVideoLag = v; }, value);
    // The poke is applied + cleared on the next frame; wait until the published
    // readback reflects it (or the sentinel is cleared) and a couple frames pass.
    await advanceFrames(page, 3);
    return page.evaluate(() => window.rb3ExcessVideoLag);
}

async function getCalibration(page) {
    await advanceFrames(page, 2);
    return page.evaluate(() => window.rb3ExcessVideoLag);
}

async function triggerSaveAndWait(page) {
    await page.evaluate(() => { window.__rb3SaveRequested = 1; });
    await advanceFrames(page, 3);  // poll branch clears flag + runs save
    // Read back the JS save-cache state.
    return page.evaluate(() => {
        const c = window.__rb3SaveCache;
        const out = { ready: window.__rb3SaveReady, size: c ? c.size : -1, keys: {} };
        if (c) {
            for (const k of ['globaloptions.bin', 'gameplayopts_0.bin']) {
                const b = c.get(k);
                out.keys[k] = b ? b.byteLength : null;
            }
        }
        out.stats = window.__rb3SaveStats || null;
        return out;
    });
}

const results = [];
function check(name, cond, detail) {
    results.push({ name, pass: !!cond, detail });
    console.log(`  ${cond ? 'PASS' : 'FAIL'}  ${name}${detail ? '  — ' + detail : ''}`);
}

let browser, userDataDir;
const APPROX = (a, b) => Math.abs(a - b) < 1e-3;

try {
    await waitForServer(PORT);
    console.log(`[save-persistence-test] port=${PORT}`);

    userDataDir = mkdtempSync(join(tmpdir(), 'rb3-savetest-'));

    // ===================================================================
    // PHASE 1 — ROUND-TRIP. Persistent context so IDB survives reload.
    // ===================================================================
    console.log('\n=== PHASE 1: round-trip (set -> save -> reload -> persisted) ===');
    let ctx = await chromium.launchPersistentContext(userDataDir, {
        headless: true, args: LAUNCH_ARGS,
        viewport: { width: 1280, height: 720 },
    });
    let page = ctx.pages()[0] || await ctx.newPage();
    page.on('pageerror', (e) => console.log(`  [PAGE_ERROR] ${e.message || e}`));
    page.on('console', (m) => {
        const t = m.text();
        if (/rb3-idb-save|persist|poll|SaveReady|SaveCache/.test(t) || m.type() === 'error')
            console.log(`    [console.${m.type()}] ${t}`);
    });

    await page.goto(URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitBooted(page, 'phase1');
    console.log('  app booted');

    // Unique value (rounded to a clean float so the round-trip compare is exact).
    const TARGET = 0.042;
    const landed = await setCalibration(page, TARGET);
    check('calibration set landed in-session', APPROX(landed, TARGET),
          `set=${TARGET} read=${landed}`);

    const saved = await triggerSaveAndWait(page);
    console.log(`  save-cache after trigger: ${JSON.stringify(saved)}`);
    const goLen = saved.keys['globaloptions.bin'];
    check('globaloptions.bin written to JS save-cache', goLen != null && goLen > 0,
          `byteLength=${goLen}`);
    check('gameplayopts_0.bin written to JS save-cache',
          saved.keys['gameplayopts_0.bin'] != null,
          `byteLength=${saved.keys['gameplayopts_0.bin']}`);
    check('save put-counter advanced', saved.stats && saved.stats.puts >= 2,
          `puts=${saved.stats ? saved.stats.puts : 'n/a'}`);

    // RELOAD — same origin, same on-disk IDB. Prewarm should read the row back.
    console.log('  reloading page...');
    await page.reload({ waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitBooted(page, 'phase1-reload');
    const afterReady = await page.evaluate(() => ({
        ready: window.__rb3SaveReady,
        rows:  window.__rb3SaveStats ? window.__rb3SaveStats.rows : -1,
        has:   window.__rb3SaveCache ? window.__rb3SaveCache.has('globaloptions.bin') : false,
    }));
    console.log(`  post-reload prewarm: ${JSON.stringify(afterReady)}`);
    check('prewarm reloaded >=1 save row from IDB', afterReady.rows >= 1,
          `rows=${afterReady.rows}`);

    const persisted = await getCalibration(page);
    check('CALIBRATION PERSISTED across reload (CORE)', APPROX(persisted, TARGET),
          `expected=${TARGET} got=${persisted}`);

    const expectedLen = goLen;
    await ctx.close();

    // ===================================================================
    // PHASE 2 — FALLBACK: indexedDB undefined -> graceful no-persistence.
    // Fresh context (no shared state).
    // ===================================================================
    console.log('\n=== PHASE 2: fallback (indexedDB undefined) ===');
    const fbDir = mkdtempSync(join(tmpdir(), 'rb3-savetest-fb-'));
    let fbCtx = await chromium.launchPersistentContext(fbDir, {
        headless: true, args: LAUNCH_ARGS,
        viewport: { width: 1280, height: 720 },
    });
    let fbPage = fbCtx.pages()[0] || await fbCtx.newPage();
    let fbCrash = false;
    fbPage.on('pageerror', (e) => { fbCrash = true; console.log(`  [PAGE_ERROR] ${e.message || e}`); });
    await fbPage.addInitScript(() => {
        try { Object.defineProperty(window, 'indexedDB', { get: () => undefined }); } catch (e) {}
    });
    await fbPage.goto(URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitBooted(fbPage, 'phase2');
    const fbState = await fbPage.evaluate(() => ({
        ready: window.__rb3SaveReady,
        size:  window.__rb3SaveCache ? window.__rb3SaveCache.size : -1,
    }));
    let fbScreen = '';
    {
        const deadline = Date.now() + SCREEN_TIMEOUT_MS;
        while (Date.now() < deadline) {
            fbScreen = await getScreen(fbPage);
            if (fbScreen && fbScreen.length) break;
            await sleep(500);
        }
    }
    console.log(`  fallback state: ${JSON.stringify(fbState)} screen='${fbScreen}'`);
    check('no crash with indexedDB undefined', !fbCrash);
    check('__rb3SaveReady===1 in fallback (never blocks boot)', fbState.ready === 1);
    check('__rb3SaveCache empty in fallback', fbState.size === 0, `size=${fbState.size}`);
    check('app reaches a real screen in fallback', !!fbScreen && fbScreen.length > 0,
          `screen='${fbScreen}'`);
    await fbCtx.close();
    rmSync(fbDir, { recursive: true, force: true });

    // ===================================================================
    // PHASE 3 — EXACT-SIZE GATE: pre-seed a WRONG-length globaloptions.bin into
    // the save DB before boot; assert no rev-assert trap, and the session's own
    // correct-length save persists (the bad seed was rejected).
    // ===================================================================
    console.log('\n=== PHASE 3: exact-size gate (wrong-length seed rejected) ===');
    const gateDir = mkdtempSync(join(tmpdir(), 'rb3-savetest-gate-'));
    let gCtx = await chromium.launchPersistentContext(gateDir, {
        headless: true, args: LAUNCH_ARGS,
        viewport: { width: 1280, height: 720 },
    });
    let gPage = gCtx.pages()[0] || await gCtx.newPage();
    let gCrash = false, gAbort = false;
    gPage.on('pageerror', (e) => { gCrash = true; console.log(`  [PAGE_ERROR] ${e.message || e}`); });
    gPage.on('console', (m) => {
        const t = m.text();
        // Exclude the benign boot log "[rb3-pre] replaced N missing-function
        // abort stubs" — it contains "abort" but is not a crash. A real
        // rev-assert trap is a MILO_FAIL/ASSERT or an emscripten abort().
        if (/missing-function abort stubs/.test(t)) return;
        if (/\babort\(|ASSERT|MILO_FAIL|RuntimeError|memory access out of bounds/i.test(t)) {
            gAbort = true; console.log(`    [${m.type()}] ${t}`);
        }
    });

    // Seed the wrong-length blob directly into 'rb3-web-saves' before navigating
    // to the app. Use a blank page on the same origin so IDB is the app's origin.
    await gPage.goto(URL.replace(/\/$/, '') + '/favicon.ico', { waitUntil: 'domcontentloaded', timeout: 30000 }).catch(() => {});
    const seedRes = await gPage.evaluate(async () => {
        return await new Promise((resolve) => {
            const req = indexedDB.open('rb3-web-saves', 1);
            req.onupgradeneeded = (e) => {
                const db = e.target.result;
                if (!db.objectStoreNames.contains('saves')) db.createObjectStore('saves');
            };
            req.onsuccess = (e) => {
                const db = e.target.result;
                const tx = db.transaction('saves', 'readwrite');
                // Deliberately WRONG length (3 bytes) — never matches the rev-locked size.
                tx.objectStore('saves').put(new Uint8Array([1, 2, 3]), 'globaloptions.bin');
                tx.oncomplete = () => resolve('seeded');
                tx.onerror = () => resolve('seed-error');
            };
            req.onerror = () => resolve('open-error');
        });
    });
    console.log(`  seed result: ${seedRes}`);

    await gPage.goto(URL, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitBooted(gPage, 'phase3');
    check('no crash/abort with wrong-length seed (exact-size gate held)', !gCrash && !gAbort);

    // Now set our own calibration (correct length), save, reload, and assert it
    // persists — proving the bad seed was overwritten by a valid write and the
    // gate let the session run with defaults rather than trapping.
    const GATE_TARGET = -0.037;
    const gLanded = await setCalibration(gPage, GATE_TARGET);
    check('calibration set in gate session', APPROX(gLanded, GATE_TARGET),
          `set=${GATE_TARGET} read=${gLanded}`);
    await triggerSaveAndWait(gPage);
    await gPage.reload({ waitUntil: 'domcontentloaded', timeout: 30000 });
    await waitBooted(gPage, 'phase3-reload');
    const gPersisted = await getCalibration(gPage);
    check('gate-session calibration persisted after correct-length re-save',
          APPROX(gPersisted, GATE_TARGET), `expected=${GATE_TARGET} got=${gPersisted}`);
    await gCtx.close();
    rmSync(gateDir, { recursive: true, force: true });

    console.log(`\n  (round-trip globaloptions.bin rev-locked size = ${expectedLen} bytes)`);

} catch (e) {
    console.error('\n[save-persistence-test] ERROR:', e.message || e);
    results.push({ name: 'harness-exception', pass: false, detail: String(e.message || e) });
} finally {
    if (browser) await browser.close().catch(() => {});
    if (userDataDir) rmSync(userDataDir, { recursive: true, force: true });
}

const failed = results.filter(r => !r.pass);
console.log(`\n==== SUMMARY: ${results.length - failed.length}/${results.length} passed ====`);
if (failed.length) { console.log('FAILED:'); failed.forEach(r => console.log(`  - ${r.name}: ${r.detail || ''}`)); }
process.exit(failed.length ? 1 : 0);
