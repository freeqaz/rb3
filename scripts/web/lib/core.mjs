/**
 * RB3 Web Test — shared core module.
 *
 * Every command script (`smoke-test.mjs`, `splash-diag.mjs`, …) imports from
 * here instead of duplicating browser launch, console capture, navigation, etc.
 *
 * Adapted from dc3-decomp/scripts/web/lib/core.mjs to RB3 specifics:
 *   - canvas id is "rb3-canvas" (native/web/index.html)
 *   - screen state is published to window.rb3CurrentScreen / rb3FocusButton /
 *     rb3OvershellView (native/src/main_web.cpp PublishCurrentScreen) and the
 *     song DB count to window.rb3SongCount (PublishSongCount).
 *   - boot flag is window.rb3AppBooted; frame counter is window.rb3FrameCount.
 *   - screen flow is: splash_screen → main_hub_screen → song_select_screen →
 *     part_difficulty_screen → game_screen.
 *   - input keymap (rb3_game_input.cpp): Space=Start, Enter=Confirm,
 *     ArrowUp/Down=nav. The splash advances via Start then Confirm
 *     (WebSplashAdvanceHook in main_web.cpp routes those through the
 *     direct-injection verb path).
 *
 * No xvfb: launchBrowser runs HEADLESS via Playwright's bundled Chromium with
 * WebGPU+ANGLE/Vulkan flags. headless is true unless DISPLAY is set.
 */

import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import http from 'http';

// ---------------------------------------------------------------------------
// CLI arg parsing
// ---------------------------------------------------------------------------

/**
 * Parse process.argv against a spec.
 *
 *   const opts = parseArgs({
 *     port:    { type: 'number', default: 8421 },
 *     verbose: { type: 'flag' },
 *     out:     { type: 'string' },
 *   });
 */
export function parseArgs(spec) {
    const argv = process.argv.slice(2);
    const result = {};

    for (const [name, def] of Object.entries(spec)) {
        const flag = `--${name}`;
        const idx = argv.indexOf(flag);

        if (def.type === 'flag') {
            result[name] = idx !== -1;
        } else if (idx !== -1 && idx + 1 < argv.length) {
            const raw = argv[idx + 1];
            result[name] = def.type === 'number' ? parseInt(raw, 10) : raw;
        } else {
            result[name] = def.default;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------

/** Poll /api/health until the server is ready (server.py serves this). */
export function waitForServer(port, timeoutMs = 15000) {
    return new Promise((resolve, reject) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (res) => {
                if (res.statusCode === 200) return resolve();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline)
                return reject(new Error(`Server not ready after ${timeoutMs}ms`));
            setTimeout(check, 300);
        };
        check();
    });
}

// ---------------------------------------------------------------------------
// Browser
// ---------------------------------------------------------------------------

const CANVAS_ID = 'rb3-canvas';

/**
 * Launch Chromium HEADLESS with WebGPU flags (no xvfb / no display server).
 * Returns { browser, context, page }.
 *
 * The no-xvfb magic: Playwright's bundled Chromium + `--enable-unsafe-webgpu`
 * + `--use-angle=vulkan` runs WebGPU under SwiftShader/Vulkan with NO X server.
 * `headless` is `!process.env.DISPLAY` so a developer with a display still gets
 * a visible window for debugging, while CI/agents run truly headless.
 *
 * @param {number} port server port
 * @param {object} [o]
 * @param {string} [o.query] extra URL query (e.g. 'milo=...') appended with `?`
 * @param {{width:number,height:number}} [o.viewport]
 */
export async function launchBrowser(port, { query = '', viewport = { width: 1280, height: 720 }, noGoto = false } = {}) {
    const browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox',
            '--enable-unsafe-webgpu',
            '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11',
            '--disable-extensions',
            '--disable-background-networking',
            '--disable-default-apps',
            '--disable-sync',
            '--mute-audio',
            '--autoplay-policy=no-user-gesture-required',
        ],
    });

    const context = await browser.newContext({ viewport });
    const page = await context.newPage();

    const url = `http://127.0.0.1:${port}/${query ? `?${query}` : ''}`;
    // noGoto lets a caller install init scripts / start a CDP trace BEFORE the
    // page navigates (so boot-time instrumentation captures the whole boot).
    if (!noGoto) {
        await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
    }

    return { browser, context, page, url };
}

// ---------------------------------------------------------------------------
// Console capture
// ---------------------------------------------------------------------------

/**
 * Wire page.on('console') + pageerror + crash.
 * Returns { logs, errors, waitForLog(text, ms), elapsed(), silenceMs() }.
 *
 * @param {import('playwright').Page} page
 * @param {object} [o]
 * @param {boolean} [o.verbose] echo every console line
 * @param {RegExp}  [o.filter]  echo only lines matching this (ignored if verbose)
 */
export function createCapture(page, { verbose = false, filter = null } = {}) {
    const startTime = Date.now();
    const logs = [];      // { elapsed, type, text }
    const errors = [];    // string[]
    let lastLogTime = Date.now();

    const elapsed = () => ((Date.now() - startTime) / 1000).toFixed(2);

    page.on('console', (msg) => {
        const text = msg.text();
        const entry = { elapsed: elapsed(), type: msg.type(), text };
        logs.push(entry);
        if (text.trim().length > 0) lastLogTime = Date.now();

        if (verbose || msg.type() === 'error' || (filter && filter.test(text))) {
            console.log(`  [${entry.elapsed}s ${msg.type()}] ${text}`);
        }
    });

    page.on('pageerror', (err) => {
        const text = err.message || String(err);
        errors.push(text);
        console.log(`  [PAGE_ERROR] ${text}`);
    });

    page.on('crash', () => {
        errors.push('Page crashed');
        console.log('  [CRASH] Page crashed!');
    });

    /** Poll logs for a substring. Resolves true/false. */
    function waitForLog(text, timeoutMs = 30000) {
        return new Promise((resolve) => {
            const deadline = Date.now() + timeoutMs;
            const check = () => {
                if (logs.some(l => l.text.includes(text))) return resolve(true);
                if (Date.now() > deadline) return resolve(false);
                setTimeout(check, 200);
            };
            check();
        });
    }

    /** Milliseconds since last non-empty log line. */
    function silenceMs() {
        return Date.now() - lastLogTime;
    }

    return { logs, errors, waitForLog, elapsed, silenceMs };
}

// ---------------------------------------------------------------------------
// Engine state polling (window.rb3* globals from main_web.cpp)
// ---------------------------------------------------------------------------

/**
 * Read the engine state snapshot the web build publishes each frame.
 * Returns { screen, focus, overshell, track, diff, songs, frame, booted,
 *           highlightedSong, highlightedType }.
 */
export function engineState(page) {
    return page.evaluate(() => ({
        screen:          window.rb3CurrentScreen   || '',
        focus:           window.rb3FocusButton     || '',
        overshell:       window.rb3OvershellView   || '',
        track:           window.rb3OvershellTrack  || '',
        diff:            window.rb3OvershellDiff    || '',
        songs:           window.rb3SongCount        || 0,
        frame:           window.rb3FrameCount       || 0,
        booted:          window.rb3AppBooted        || 0,
        highlightedSong: window.rb3HighlightedSong  || '',
        highlightedType: window.rb3HighlightedType,
        keys:            window._rb3Keys            || 0,
    }));
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getSongCount = (page) => page.evaluate(() => window.rb3SongCount || 0);
const getBooted = (page) => page.evaluate(() => window.rb3AppBooted || 0);

/** Poll window.rb3AppBooted >= 1. Returns true on boot, false on timeout. */
export async function waitForBoot(page, timeoutMs = 300000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        if (await getBooted(page) >= 1) return true;
        await new Promise(r => setTimeout(r, 500));
    }
    return false;
}

/**
 * Wait until window.rb3CurrentScreen becomes one of `targets`, or (if `from` is
 * given) changes away from `from`. Returns the final screen name (may be the
 * timeout value if neither condition met).
 */
export async function waitScreen(page, { targets = null, from = null, timeoutMs = 30000 }) {
    const deadline = Date.now() + timeoutMs;
    let s = await getScreen(page);
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from) return s;
        await new Promise(r => setTimeout(r, 250));
    }
    return s;
}

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

/**
 * Press a key with a hold time and inter-key delay. The web build edge-detects
 * the _rb3Keys bitmask once per frame, so HOLDING for several frames guarantees
 * the rising edge is observed even at a low engine frame rate. 3s timeout guard
 * keeps us alive if the page freezes.
 *
 * @param {import('playwright').Page} page
 * @param {string} key  Playwright key name ('Space','Enter','ArrowDown',…)
 * @param {number} [holdMs]
 */
export async function pressKey(page, key, holdMs = 250) {
    try {
        await Promise.race([
            (async () => {
                await page.keyboard.down(key);
                await new Promise(r => setTimeout(r, holdMs));
                await page.keyboard.up(key);
            })(),
            new Promise(r => setTimeout(r, 3000)),
        ]);
        await new Promise(r => setTimeout(r, 200));
    } catch { /* page frozen — swallow */ }
}

/** Focus the rb3 canvas so keyboard events land on the game, not an overlay. */
export async function focusCanvas(page) {
    try {
        await page.locator(`#${CANVAS_ID}`).click({ force: true });
    } catch { /* canvas not ready — swallow */ }
}

// rb3 screen names, in boot→gameplay order.
export const SCREENS = {
    SPLASH:          'splash_screen',
    MAIN_HUB:        'main_hub_screen',
    SONG_SELECT:     'song_select_screen',
    PART_DIFFICULTY: 'part_difficulty_screen',
    GAME:            'game_screen',
};

/**
 * Full RB3 navigation: splash_screen → main_hub_screen → song_select_screen →
 * part_difficulty_screen → game_screen. `target` is the final screen to reach
 * (default: main_hub_screen).
 *
 * Per main_web.cpp WebSplashAdvanceHook, the splash advances via Start (Space)
 * then Confirm (Enter); the overshell continue can need several Confirms to
 * ride the settle. main_hub → song_select is a Confirm chain (playnow →
 * quickplay → quickplay). Mirrors the proven w3cnav-test sequence.
 *
 * @param {import('playwright').Page} page
 * @param {ReturnType<typeof createCapture>} capture
 * @param {string} [target]
 * @param {object} [o]
 * @param {(label:string)=>Promise<void>} [o.onScreen] called once per reached screen
 */
export async function navigateTo(page, capture, target = SCREENS.MAIN_HUB, { onScreen = null } = {}) {
    // 1. App boot.
    if (!await waitForBoot(page)) throw new Error('App never booted (no rb3AppBooted)');

    // 2. Reach splash_screen.
    let s = await waitScreen(page, { targets: [SCREENS.SPLASH], timeoutMs: 180000 });
    await new Promise(r => setTimeout(r, 2000));
    s = await getScreen(page);
    if (s !== SCREENS.SPLASH) throw new Error(`Never reached splash_screen (stuck at '${s}')`);
    if (onScreen) await onScreen(SCREENS.SPLASH);
    if (target === SCREENS.SPLASH) return SCREENS.SPLASH;

    await focusCanvas(page);
    await new Promise(r => setTimeout(r, 500));

    // 3. splash → main_hub: Start (Space) then up to 6× Confirm (Enter).
    await pressKey(page, 'Space');
    s = await waitScreen(page, { from: SCREENS.SPLASH, timeoutMs: 8000 });
    for (let i = 0; i < 6 && s === SCREENS.SPLASH; i++) {
        await pressKey(page, 'Enter');
        s = await waitScreen(page, { from: SCREENS.SPLASH, timeoutMs: 6000 });
    }
    if (s !== SCREENS.MAIN_HUB) {
        s = await waitScreen(page, { targets: [SCREENS.MAIN_HUB], timeoutMs: 30000 });
    }
    if (s !== SCREENS.MAIN_HUB) throw new Error(`Never reached main_hub_screen (stuck at '${s}')`);
    await new Promise(r => setTimeout(r, 3000));
    if (onScreen) await onScreen(SCREENS.MAIN_HUB);
    if (target === SCREENS.MAIN_HUB) return SCREENS.MAIN_HUB;

    // 4. main_hub → song_select: Confirm chain (playnow → quickplay → quickplay).
    await focusCanvas(page);
    for (let i = 0; i < 5; i++) {
        await pressKey(page, 'Enter');
        const cur = await waitScreen(page, { from: SCREENS.MAIN_HUB, timeoutMs: 6000 });
        if (cur && cur !== SCREENS.MAIN_HUB) { s = cur; break; }
        await new Promise(r => setTimeout(r, 1500));
    }
    s = await waitScreen(page, {
        targets: [SCREENS.SONG_SELECT, 'song_select_enter_screen'], timeoutMs: 30000,
    });
    if (s === 'song_select_enter_screen') {
        s = await waitScreen(page, { targets: [SCREENS.SONG_SELECT], timeoutMs: 30000 });
    }
    if (s !== SCREENS.SONG_SELECT) throw new Error(`Never reached song_select_screen (stuck at '${s}')`);
    await new Promise(r => setTimeout(r, 4000));
    if (onScreen) await onScreen(SCREENS.SONG_SELECT);
    if (target === SCREENS.SONG_SELECT) return SCREENS.SONG_SELECT;

    // 5. song_select → part_difficulty.
    await focusCanvas(page);
    await pressKey(page, 'ArrowDown');
    await new Promise(r => setTimeout(r, 1000));
    await pressKey(page, 'Enter');
    s = await waitScreen(page, { targets: [SCREENS.PART_DIFFICULTY], from: SCREENS.SONG_SELECT, timeoutMs: 30000 });
    if (s !== SCREENS.PART_DIFFICULTY) throw new Error(`Never reached part_difficulty_screen (stuck at '${s}')`);
    await new Promise(r => setTimeout(r, 3000));
    if (onScreen) await onScreen(SCREENS.PART_DIFFICULTY);
    if (target === SCREENS.PART_DIFFICULTY) return SCREENS.PART_DIFFICULTY;

    // 6. part_difficulty → game.
    for (let i = 0; i < 4; i++) {
        await pressKey(page, 'Enter');
        const cur = await waitScreen(page, { targets: [SCREENS.GAME], from: SCREENS.PART_DIFFICULTY, timeoutMs: 10000 });
        if (cur === SCREENS.GAME) { s = cur; break; }
        await new Promise(r => setTimeout(r, 1500));
    }
    s = await waitScreen(page, { targets: [SCREENS.GAME], timeoutMs: 30000 });
    if (s !== SCREENS.GAME) throw new Error(`Never reached game_screen (stuck at '${s}')`);
    await new Promise(r => setTimeout(r, 4000));
    if (onScreen) await onScreen(SCREENS.GAME);
    return SCREENS.GAME;
}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

/** Resolve output directory. Auto-generates a timestamped path if none given. */
export function outputDir(name, explicit) {
    if (explicit) {
        mkdirSync(explicit, { recursive: true });
        return explicit;
    }
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    const dir = `/tmp/rb3-web/${name}-${ts}`;
    mkdirSync(dir, { recursive: true });
    return dir;
}

/**
 * Screenshot the rb3 canvas (not the whole page) with a 5s timeout guard.
 * Returns the path on success, null on failure.
 */
export async function screenshot(page, dir, name) {
    const path = resolve(dir, `${name}.png`);
    try {
        await Promise.race([
            page.locator(`#${CANVAS_ID}`).screenshot({ path }),
            new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), 5000)),
        ]);
        console.log(`[screenshot] ${path}`);
        return path;
    } catch {
        console.log(`[screenshot] FAILED: ${path}`);
        return null;
    }
}

/**
 * Screenshot + decode painted%: fraction of pixels brighter than `threshold`
 * on any channel. Useful to assert the canvas isn't black. Returns
 * { path, paintedPct, w, h } or { error }.
 */
export async function captureCanvasStats(page, dir, name, threshold = 12) {
    const path = resolve(dir, `${name}.png`);
    try {
        await page.locator(`#${CANVAS_ID}`).screenshot({ path });
        const { PNG } = await import('pngjs');
        const { readFileSync } = await import('fs');
        const png = PNG.sync.read(readFileSync(path));
        let painted = 0;
        const total = png.width * png.height;
        for (let p = 0; p < png.data.length; p += 4) {
            if (png.data[p] > threshold || png.data[p + 1] > threshold || png.data[p + 2] > threshold) painted++;
        }
        return { path, paintedPct: Number((100 * painted / total).toFixed(2)), w: png.width, h: png.height };
    } catch (e) {
        return { error: e.message };
    }
}

/** Write logs as JSONL. */
export function saveLogs(logs, dir, name = 'console.jsonl') {
    const path = resolve(dir, name);
    writeFileSync(path, logs.map(e => JSON.stringify(e)).join('\n') + '\n');
    console.log(`[logs] ${path}`);
    return path;
}

/** Write an arbitrary JSON result blob. */
export function saveJson(obj, dir, name = 'result.json') {
    const path = resolve(dir, name);
    writeFileSync(path, JSON.stringify(obj, null, 2));
    console.log(`[json] ${path}`);
    return path;
}

/** Close browser with a 3s timeout. */
export async function cleanup(browser) {
    if (!browser) return;
    try {
        await Promise.race([
            browser.close(),
            new Promise(r => setTimeout(r, 3000)),
        ]);
    } catch { /* swallow */ }
}
