#!/usr/bin/env node
/**
 * W6-V2 probe — minimal navigation to main_hub_screen, then dump engine logs
 * to investigate why the menu list is invisible.
 *
 * Captures console output, then takes a screenshot.
 *
 * Usage: node scripts/web/w6-v2-probe.mjs [--port 8529]
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
function arg(name, def) {
    const i = argv.indexOf(name);
    if (i < 0 || i+1 >= argv.length) return def;
    return argv[i+1];
}
const opts = {
    port: parseInt(arg('--port', '8529'), 10) || 8529,
    out: arg('--out', 'docs/sessions/web/screenshots/w6-v2-probe'),
};

const REPO_ROOT = resolve(__dirname, '../..');
const OUT_DIR = resolve(REPO_ROOT, opts.out);
mkdirSync(OUT_DIR, { recursive: true });

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
                if (r.statusCode === 200) return res();
                if (Date.now() > deadline) return rej(new Error('Timeout'));
                setTimeout(check, 250);
            }).on('error', () => {
                if (Date.now() > deadline) return rej(new Error('Timeout'));
                setTimeout(check, 250);
            });
        };
        check();
    });
}

async function getScreen(page) {
    return await page.evaluate(() => {
        // Read from window.__rb3CurrentScreen which is set from console-log
        // tap in this script's init.
        return window.__rb3CurrentScreen || null;
    });
}

(async () => {
    const start = Date.now();
    const elapsed = () => ((Date.now() - start) / 1000).toFixed(1);

    await waitForServer(opts.port);

    const browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions', '--disable-background-networking',
            '--disable-default-apps', '--disable-sync', '--mute-audio',
        ],
    });
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();

    const consoleLines = [];
    page.on('console', m => {
        const t = m.text();
        consoleLines.push(`[${m.type()}] ${t}`);
    });
    page.on('pageerror', e => consoleLines.push(`[error] ${e.message}`));

    await page.addInitScript(() => {
        window.__rb3CurrentScreen = null;
        const origLog = console.log;
        const origErr = console.error;
        function tap(args) {
            try {
                const s = String(args[0] || '');
                const m = s.match(/currentScreen\s*=\s*'([^']+)'/);
                if (m) window.__rb3CurrentScreen = m[1];
            } catch (e) {}
        }
        console.log = function(...a) { tap(a); origLog.apply(this, a); };
        console.error = function(...a) { tap(a); origErr.apply(this, a); };
    });

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`→ ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    // Wait for App boot
    let appBooted = 0;
    const deadline = Date.now() + 180000;
    while (Date.now() < deadline) {
        appBooted = await page.evaluate(() => window.rb3AppBooted || 0);
        if (appBooted >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`App booted=${appBooted} (${elapsed()}s)`);

    // Wait for splash
    let s = '';
    const splashDeadline = Date.now() + 60000;
    while (Date.now() < splashDeadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`Splash settled: '${s}' (${elapsed()}s)`);

    // Press start
    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));
    await page.keyboard.press('Space');
    await new Promise(r => setTimeout(r, 1000));
    await page.keyboard.press('Enter');

    // Wait for main_hub
    const hubDeadline = Date.now() + 30000;
    while (Date.now() < hubDeadline) {
        s = await getScreen(page);
        if (s === 'main_hub_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`Main hub: '${s}' (${elapsed()}s)`);
    if (s !== 'main_hub_screen') {
        console.log('NOT in main_hub. Dumping logs anyway.');
        const logPath = resolve(OUT_DIR, 'console.log');
        writeFileSync(logPath, consoleLines.join('\n'));
        console.log(`Saved logs: ${consoleLines.length} lines → ${logPath}`);
        const shotPath = resolve(OUT_DIR, 'noboot.png');
        try { await page.locator('#rb3-canvas').screenshot({ path: shotPath }); } catch (e) {}
        await browser.close();
        process.exit(1);
    }

    // Wait 3s for it to settle
    await new Promise(r => setTimeout(r, 3000));

    const shotPath = resolve(OUT_DIR, '02_main_hub.png');
    await page.locator('#rb3-canvas').screenshot({ path: shotPath });
    console.log(`Saved ${shotPath} (${elapsed()}s)`);

    // Dump filtered console: probe + asserts + errors
    const filter = (l) => /V2_DBG|MILO_ASSERT|main_hub|none_to_main|playnow|career|trainers|customize|musicstore|UITrigger|SetShowing|update_state_view|mb_|WARN|FAIL|error|Error|fault/i.test(l);
    const filtered = consoleLines.filter(filter);
    const logPath = resolve(OUT_DIR, 'console.log');
    writeFileSync(logPath, consoleLines.join('\n'));
    const filteredPath = resolve(OUT_DIR, 'console-filtered.log');
    writeFileSync(filteredPath, filtered.join('\n'));
    console.log(`Saved logs: ${filtered.length} filtered / ${consoleLines.length} total → ${filteredPath}`);

    await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
