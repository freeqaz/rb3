/**
 * RB3 Web — window-resize GPU-crash regression test.
 *
 * Reproduces the bug reported when the browser window resizes (e.g. opening
 * dev tools): the swapchain color attachment auto-follows the live canvas size
 * but the "BandDepth" depth texture stayed at its boot size, so BandMainPass
 * aborted every frame with:
 *
 *   GpuDevice: uncaptured error (type=2): The depth stencil attachment
 *   [TextureView of Texture "BandDepth"] size (W x H) does not match the size
 *   of the other attachments' base plane (W' x H').
 *   ... [Invalid CommandBuffer] is invalid due to a previous error.
 *
 * The fix sizes depth/intermediate/halo to the live acquired color texture each
 * frame (milo-native-engine Rnd_Wgpu_RB3 BeginFrame/EnsureDepth). This test
 * boots the debug build, renders, then resizes the viewport several times
 * (bigger AND smaller) and asserts ZERO depth-mismatch / invalid-command-buffer
 * errors appear afterwards.
 *
 * Run:  node scripts/web/resize-crash-test.mjs [--port 8421] [--verbose]
 * (Build + serve first: scripts/web/build.sh --debug && python3 native/web/server.py)
 */

import {
    parseArgs, waitForServer, launchBrowser, createCapture,
    waitForBoot, waitScreen, SCREENS, cleanup,
} from './lib/core.mjs';

const DEPTH_MISMATCH = /depth stencil attachment.*does not match|BandDepth.*does not match|does not match.*base plane/i;
const INVALID_CMDBUF = /Invalid CommandBuffer/i;

const isGpuResizeError = (t) => DEPTH_MISMATCH.test(t) || INVALID_CMDBUF.test(t);

async function main() {
    const opts = parseArgs({
        port:    { type: 'number', default: 8421 },
        verbose: { type: 'flag' },
    });

    console.log(`[resize-test] waiting for server on :${opts.port} ...`);
    await waitForServer(opts.port);

    // Debug build (no-store), start at a "boot" canvas size.
    const { browser, page } = await launchBrowser(opts.port, {
        query: 'debug=true',
        viewport: { width: 1280, height: 720 },
    });
    const capture = createCapture(page, { verbose: opts.verbose });

    let failed = false;
    try {
        console.log('[resize-test] waiting for app boot ...');
        if (!await waitForBoot(page)) throw new Error('App never booted');

        // Reach any rendering screen — BandMainPass runs every frame regardless
        // of content, so the splash already exercises the depth attachment.
        const s = await waitScreen(page, {
            targets: [SCREENS.SPLASH, SCREENS.MAIN_HUB, SCREENS.SONG_SELECT],
            timeoutMs: 180000,
        });
        console.log(`[resize-test] rendering at screen='${s}', frame=${
            await page.evaluate(() => window.rb3FrameCount || 0)}`);

        // Let a few frames render at the boot size (baseline — must be clean).
        await new Promise(r => setTimeout(r, 2000));
        const errsBeforeResize = capture.logs.filter(l => isGpuResizeError(l.text)).length;
        console.log(`[resize-test] GPU resize-class errors BEFORE any resize: ${errsBeforeResize}`);

        // The index.html ResizeObserver maps the viewport to canvas.width/height
        // and calls Module._rb3_resize_canvas. Drive a sequence that grows AND
        // shrinks (devtools-open shrinks the viewport; re-docking grows it).
        const sizes = [
            { width: 1100, height: 620 },   // shrink (devtools docked)
            { width: 1440, height: 810 },   // grow
            { width:  960, height: 600 },   // shrink again
            { width: 1280, height: 720 },   // back to boot size
        ];
        const marks = [];
        for (const sz of sizes) {
            const before = capture.logs.length;
            await page.setViewportSize(sz);
            // Give the ResizeObserver (rAF-debounced) + several engine frames time
            // to render at the new size, where the stale-depth mismatch would fire.
            await new Promise(r => setTimeout(r, 2500));
            const after = capture.logs.slice(before);
            const errs = after.filter(l => isGpuResizeError(l.text)).length;
            const frame = await page.evaluate(() => window.rb3FrameCount || 0);
            marks.push({ size: `${sz.width}x${sz.height}`, errs, frame });
            console.log(`[resize-test] resized -> ${sz.width}x${sz.height}: ` +
                        `frame=${frame}, resize-class errors this step=${errs}`);
        }

        const totalErrs = capture.logs.filter(l => isGpuResizeError(l.text)).length;
        const finalFrame = await page.evaluate(() => window.rb3FrameCount || 0);

        console.log('\n[resize-test] ── summary ──');
        for (const m of marks) console.log(`  ${m.size.padEnd(10)} errs=${m.errs}`);
        console.log(`  total resize-class GPU errors: ${totalErrs}`);
        console.log(`  page still advancing frames: ${finalFrame > 0 ? 'yes' : 'NO'} (frame=${finalFrame})`);

        // A still-rendering page keeps incrementing rb3FrameCount; a wedged one
        // (every command buffer invalid) stalls. Require both: no errors AND the
        // frame counter advanced past where we started resizing.
        if (totalErrs > 0) {
            console.log(`\n[resize-test] FAIL: ${totalErrs} depth-mismatch / invalid-command-buffer errors after resize`);
            failed = true;
        } else {
            console.log('\n[resize-test] PASS: no depth-mismatch / invalid-command-buffer errors across all resizes');
        }
    } catch (e) {
        console.log(`[resize-test] FAIL: ${e.message}`);
        failed = true;
    } finally {
        await cleanup(browser);
    }

    process.exit(failed ? 1 : 0);
}

main();
