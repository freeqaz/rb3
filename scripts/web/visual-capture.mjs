/**
 * visual-capture.mjs — deterministic canonical-screen capture for the WEB build.
 *
 * Drives the Playwright harness (reusing scripts/web/lib/core.mjs's navigateTo)
 * to boot → main_hub → song_select (→ game, optional) and writes one canvas PNG
 * per canonical screen into --out. This is the WEB leg of the visual-diff driver
 * (scripts/analysis/visual_diff_capture.py): it produces the same canonical
 * screen set the native leg captures over /api/screenshot, so the two are
 * directly diffable.
 *
 * Determinism: same nav verbs, fixed settle waits (core.navigateTo already
 * settles per screen), fixed 1280x720 viewport, capture the #rb3-canvas only
 * (not chrome). The web build's frame rate is variable so we settle on the
 * published window.rb3CurrentScreen, never on a frame number.
 *
 *   node visual-capture.mjs --port 8421 --out DIR
 *        [--screens main_hub,song_select] [--query debug=true] [--verbose]
 *
 * Prints a JSON line `CAPTURE_RESULT {...}` (screen -> png path | null) and
 * exits 0 if every requested screen was captured, 1 otherwise.
 */
import {
    parseArgs, waitForServer, launchBrowser, createCapture,
    navigateTo, screenshot, cleanup, SCREENS,
} from './lib/core.mjs';

// Map the driver's short canonical names to core.mjs SCREENS + nav targets.
const SCREEN_MAP = {
    main_hub:    SCREENS.MAIN_HUB,
    song_select: SCREENS.SONG_SELECT,
    game:        SCREENS.GAME,
};
// Capture order = boot order, so navigateTo passes through each on its way down.
const ORDER = ['main_hub', 'song_select', 'game'];

async function main() {
    const opts = parseArgs({
        port:    { type: 'number', default: 8421 },
        out:     { type: 'string', default: '/tmp/rb3-vcap-web' },
        screens: { type: 'string', default: 'main_hub,song_select' },
        query:   { type: 'string', default: '' },
        // --dup-screen NAME: navigate to NAME, then capture it TWICE back-to-back
        // in the SAME session with no input/animation advance between. Writes
        // <out>/a/NAME.png and <out>/b/NAME.png — the strict-mode determinism
        // proof (isolates capture/encode determinism from scene animation phase,
        // which differs across two independent boots of an animated screen).
        'dup-screen': { type: 'string', default: '' },
        verbose: { type: 'flag' },
    });

    if (opts['dup-screen']) return dupCapture(opts);

    const want = opts.screens.split(',').map(s => s.trim()).filter(Boolean);
    for (const s of want) {
        if (!SCREEN_MAP[s]) {
            console.error(`unknown screen '${s}' (valid: ${Object.keys(SCREEN_MAP).join(', ')})`);
            process.exit(2);
        }
    }
    // deepest screen we must navigate to = last requested in boot order.
    const deepest = ORDER.filter(s => want.includes(s)).pop();

    const { mkdirSync } = await import('fs');
    mkdirSync(opts.out, { recursive: true });

    await waitForServer(opts.port, 20000);
    const { browser, page } = await launchBrowser(opts.port, { query: opts.query });
    const capture = createCapture(page, { verbose: opts.verbose });

    const result = {};
    for (const s of want) result[s] = null;

    let crashed = false;
    page.on('crash', () => { crashed = true; });

    try {
        // navigateTo fires onScreen once per canonical screen it reaches; capture
        // there. It settles per-screen internally (fixed waits) for determinism.
        await navigateTo(page, capture, SCREEN_MAP[deepest], {
            onScreen: async (reached) => {
                const short = Object.keys(SCREEN_MAP).find(k => SCREEN_MAP[k] === reached);
                if (short && want.includes(short)) {
                    const p = await screenshot(page, opts.out, short);
                    result[short] = p;
                    if (opts.verbose) console.log(`  captured ${short} -> ${p}`);
                }
            },
        });
    } catch (e) {
        console.error(`[visual-capture] nav failed: ${e.message}`);
    } finally {
        await cleanup(browser);
    }

    const ok = want.every(s => result[s]) && !crashed;
    console.log('CAPTURE_RESULT ' + JSON.stringify({ ok, crashed, screens: result }));
    process.exit(ok ? 0 : 1);
}

/**
 * Same-session back-to-back duplicate capture: navigate to one screen, then
 * screenshot it twice with no input between. The two PNGs should be byte-near
 * identical (~0% strict diff) — proving the capture path itself is deterministic
 * even though re-booting an animated screen lands on a different animation phase.
 */
async function dupCapture(opts) {
    const short = opts['dup-screen'];
    if (!SCREEN_MAP[short]) {
        console.error(`unknown dup-screen '${short}' (valid: ${Object.keys(SCREEN_MAP).join(', ')})`);
        process.exit(2);
    }
    const { mkdirSync } = await import('fs');
    const dirA = `${opts.out}/a`, dirB = `${opts.out}/b`;
    mkdirSync(dirA, { recursive: true });
    mkdirSync(dirB, { recursive: true });

    await waitForServer(opts.port, 20000);
    const { browser, page } = await launchBrowser(opts.port, { query: opts.query });
    const capture = createCapture(page, { verbose: opts.verbose });
    const result = { a: null, b: null };
    try {
        await navigateTo(page, capture, SCREEN_MAP[short]);
        // back-to-back: no input, no settle delay — capture the same painted frame.
        result.a = await screenshot(page, dirA, short);
        result.b = await screenshot(page, dirB, short);
    } catch (e) {
        console.error(`[visual-capture] dup nav failed: ${e.message}`);
    } finally {
        await cleanup(browser);
    }
    const ok = !!(result.a && result.b);
    console.log('CAPTURE_RESULT ' + JSON.stringify({
        ok, dup: short, screens: { [short]: result.a }, dupB: result.b,
    }));
    process.exit(ok ? 0 : 1);
}

main().catch(e => { console.error(e); process.exit(1); });
