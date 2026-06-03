#!/usr/bin/env node
/**
 * intro-cinematic-test.mjs — verify the intro cinematic plays on the web build.
 *
 * The intro is a hardware-decoded <video> overlay (rb3_movie_native.cpp, web
 * arm) over the WebGPU canvas: MoviePanel on intro_movie_screen ($first_screen)
 * calls Movie::Begin -> RB3MovieNativeBegin -> creates a <video> element pointing
 * at the pre-transcoded /api/file/videos/rb3_intro_cinematic.webm, and Movie::Poll
 * keeps the screen up until the video 'ended' event, then advances to splash.
 *
 * This boots headless (no xvfb), watches for the <video> element + its playback
 * progress, screenshots during playback, then confirms the screen advances.
 *
 * Usage: node scripts/web/intro-cinematic-test.mjs [--port 8421] [--out <dir>]
 */
import { waitForServer, launchBrowser, createCapture, outputDir, screenshot, saveJson, cleanup }
    from './lib/core.mjs';

const argv = process.argv.slice(2);
const PORT = parseInt(argv[argv.indexOf('--port') + 1] || '8421', 10) || 8421;
const OUT = outputDir('intro', argv.includes('--out') ? argv[argv.indexOf('--out') + 1] : null);
const SEEK_END = argv.includes('--seek-end');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// Read the <video> overlay state the shim created (if any).
const videoState = (page) => page.evaluate(() => {
    const v = document.querySelector('#canvas-container video');
    if (!v) return { present: false };
    return {
        present: true,
        src: v.src,
        readyState: v.readyState,          // 0..4 (4 = HAVE_ENOUGH_DATA)
        currentTime: +v.currentTime.toFixed(2),
        duration: isFinite(v.duration) ? +v.duration.toFixed(2) : 0,
        paused: v.paused,
        ended: v.ended,
        muted: v.muted,
        videoWidth: v.videoWidth,
        videoHeight: v.videoHeight,
        error: v.error ? v.error.code : 0,
    };
});

(async () => {
    console.log(`[intro] waiting for server on :${PORT}`);
    await waitForServer(PORT, 20000);

    const { browser, page, url } = await launchBrowser(PORT, { noGoto: true });
    const capture = createCapture(page, { filter: /rb3-intro|WebMovie|movie|intro/i });

    console.log(`[intro] navigating ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });

    const result = { videoSeen: false, samples: [], maxCurrentTime: 0, screens: [] };
    let lastScreen = '';

    // Watch for ~45s: detect the <video>, sample its progress, screenshot a few
    // times, and record screen transitions.
    const t0 = Date.now();
    let shotPlaying = false, shotEarly = false;
    while ((Date.now() - t0) / 1000 < 45) {
        const vs = await videoState(page).catch(() => ({ present: false }));
        const screen = await page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
        if (screen && screen !== lastScreen) {
            result.screens.push({ t: +((Date.now() - t0) / 1000).toFixed(2), screen });
            console.log(`  t=${((Date.now() - t0) / 1000).toFixed(1)}s  screen=${screen}`);
            lastScreen = screen;
        }
        if (vs.present) {
            if (!result.videoSeen) {
                result.videoSeen = true;
                console.log(`  [video] appeared: ${vs.src.split('/').pop()} ${vs.videoWidth}x${vs.videoHeight} muted=${vs.muted}`);
            }
            result.maxCurrentTime = Math.max(result.maxCurrentTime, vs.currentTime);
            result.samples.push({ t: +((Date.now() - t0) / 1000).toFixed(2), ...vs });
            // Screenshot once shortly after it starts, and once mid-playback.
            if (!shotEarly && vs.currentTime > 0.2) {
                shotEarly = true; await screenshot(page, OUT, '01_intro_early');
            }
            if (!shotPlaying && vs.currentTime > 2.0) {
                shotPlaying = true; await screenshot(page, OUT, '02_intro_playing');
                console.log(`  [video] playing, currentTime=${vs.currentTime}s readyState=${vs.readyState} err=${vs.error}`);
                if (SEEK_END && vs.duration > 5) {
                    // Jump near the end to trigger the 'ended' event fast and verify
                    // the intro -> splash advance without waiting the full 68.5s.
                    const target = vs.duration - 1.5;
                    console.log(`  [video] seeking to ${target.toFixed(1)}s to verify end->advance`);
                    await page.evaluate((t) => {
                        const v = document.querySelector('#canvas-container video');
                        if (v) v.currentTime = t;
                    }, target);
                }
            }
        } else if (result.videoSeen && lastScreen && lastScreen !== 'intro_movie_screen') {
            // Video gone + advanced past intro: done.
            console.log(`  [video] removed after advancing to ${lastScreen}`);
            break;
        }
        await sleep(500);
    }
    await screenshot(page, OUT, '03_after_intro');
    result.finalScreen = lastScreen;
    saveJson(result, OUT, 'intro-result.json');
    await cleanup(browser);

    // Verdict.
    console.log('='.repeat(60));
    const peak = result.maxCurrentTime;
    console.log(`[intro] video seen:      ${result.videoSeen}`);
    console.log(`[intro] peak currentTime:${peak.toFixed(2)}s (advanced playback => real decode)`);
    console.log(`[intro] screen timeline: ${result.screens.map(s => `${s.t}s:${s.screen}`).join(' -> ')}`);
    const advanced = result.screens.some(s => s.screen === 'splash_screen' || s.screen === 'main_hub_screen');
    if (!result.videoSeen) {
        console.log('[intro] FAIL: <video> overlay never appeared (mMovies empty? config not loaded?)');
        process.exit(1);
    }
    if (peak < 0.3) {
        console.log('[intro] FAIL: video element present but never advanced playback (decode/fetch error?)');
        process.exit(1);
    }
    if (!advanced) {
        console.log('[intro] WARN: video played but never observed advancing to splash/main_hub in window');
        process.exit(2);
    }
    console.log('[intro] PASS: intro video decoded + played, then advanced past intro_movie_screen');
    process.exit(0);
})().catch(e => { console.error('[intro] ERROR', e); process.exit(3); });
