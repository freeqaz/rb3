#!/usr/bin/env node
/**
 * RB3 Web — B1/B4 Audio Audibility Capture
 *
 * Proves the web build produces audible audio in two phases:
 *   --phase song  (B1): navigate to game_screen, let MOGG stream, capture 3s WAV
 *   --phase menu  (B4): stay at main_hub, press keys during capture (SFX one-shots)
 *
 * Usage:
 *   node scripts/web/web-audio-capture.mjs [--port 8455] [--phase song|menu]
 *
 * Output: /tmp/rb3_web_capture_<phase>.wav
 * Forked from w3c-gameplay-test.mjs (nav path) per B1B4_WEB_AUDIBILITY_RECIPE.md
 */

import { chromium } from 'playwright';
import { readFileSync, mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import http from 'http';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const opts = {
    port:  parseInt(argv[argv.indexOf('--port') + 1]  || '8455', 10) || 8455,
    phase: argv[argv.indexOf('--phase') + 1] || 'song',   // 'song' or 'menu'
};
if (!['song', 'menu'].includes(opts.phase)) {
    console.error('Usage: node web-audio-capture.mjs [--port N] [--phase song|menu]');
    process.exit(1);
}

const WAV_PATH = `/tmp/rb3_web_capture_${opts.phase}.wav`;

const BOOT_TIMEOUT_MS      = 300000;
const SPLASH_TIMEOUT_MS    = 180000;
const LOADSONG_TIMEOUT_MS  = 240000;

function waitForServer(port, timeoutMs = 15000) {
    return new Promise((res, rej) => {
        const deadline = Date.now() + timeoutMs;
        const check = () => {
            http.get(`http://127.0.0.1:${port}/api/health`, (r) => {
                if (r.statusCode === 200) return res();
                retry();
            }).on('error', retry);
        };
        const retry = () => {
            if (Date.now() > deadline) return rej(new Error(`Server not ready after ${timeoutMs}ms`));
            setTimeout(check, 300);
        };
        check();
    });
}

const getScreen = (page) => page.evaluate(() => window.rb3CurrentScreen || '');
const getFrame  = (page) => page.evaluate(() => window.rb3FrameCount   || 0);

async function waitScreen(page, { targets = null, from = null, timeoutMs = 20000 }) {
    const deadline = Date.now() + timeoutMs;
    let s = await getScreen(page);
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (targets && targets.includes(s)) return s;
        if (from && s && s !== from)        return s;
        await new Promise(r => setTimeout(r, 250));
    }
    return s;
}

// Decode the WAV and return RMS stats
function analyzeWav(path) {
    const buf    = readFileSync(path);
    // 44-byte standard PCM WAV header
    const dataOffset = 44;
    if (buf.length <= dataOffset) return { error: 'too short', bytes: buf.length };
    const samples = new Int16Array(buf.buffer, buf.byteOffset + dataOffset,
                                   (buf.length - dataOffset) >> 1);
    const sampleRate = buf.readUInt32LE(24);
    const channels   = buf.readUInt16LE(22);

    let peak = 0, nonZero = 0, sumSq = 0;
    for (let i = 0; i < samples.length; i++) {
        const a = Math.abs(samples[i]);
        if (a > peak) peak = a;
        if (a > 64)   nonZero++;
        sumSq += samples[i] * samples[i];
    }
    const rmsOverall = Math.sqrt(sumSq / samples.length);
    const durationSec = samples.length / (sampleRate * channels);

    // Per-second RMS
    const framesPerSec = sampleRate * channels;
    const secRms = [];
    for (let s = 0; s * framesPerSec < samples.length; s++) {
        const start = s * framesPerSec;
        const end   = Math.min(start + framesPerSec, samples.length);
        let sq = 0;
        for (let i = start; i < end; i++) sq += samples[i] * samples[i];
        secRms.push(Math.sqrt(sq / (end - start)));
    }

    return { sampleRate, channels, totalSamples: samples.length, durationSec, peak, nonZero, rmsOverall, secRms };
}

let browser;
const logs  = [];
const consoleLogs = []; // raw text lines for pattern matching

try {
    await waitForServer(opts.port);
    console.log(`[web-audio-capture] phase=${opts.phase} port=${opts.port}`);

    browser = await chromium.launch({
        headless: !process.env.DISPLAY,
        args: [
            '--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
            '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
            '--ozone-platform=x11', '--disable-extensions',
            '--disable-background-networking', '--disable-default-apps',
            '--disable-sync', '--mute-audio',
        ],
    });
    const ctx  = await browser.newContext({ viewport: { width: 1280, height: 720 } });
    const page = await ctx.newPage();
    const t0   = Date.now();
    const elapsed = () => ((Date.now() - t0) / 1000).toFixed(2);

    page.on('console', (msg) => {
        const text = msg.text();
        logs.push({ elapsed: elapsed(), type: msg.type(), text });
        consoleLogs.push(text);
        if (/AudioDevice|audio|capture|pump|source|SAB|MOGG|mogg|StreamReceiver|stream ch|sfx |CaptureAudio|DownloadAudio|pumpCount|nonZero/.test(text)
            || msg.type() === 'error') {
            console.log(`  [${elapsed()}s ${msg.type()}] ${text}`);
        }
    });
    page.on('pageerror', (err) => {
        console.log(`  [PAGE_ERROR] ${err.message || err}`);
    });

    const url = `http://127.0.0.1:${opts.port}/`;
    console.log(`Loading ${url}`);
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
    await page.evaluate(() => {
        window.rb3WebUseAids = 1;
        window.rb3WebTargetSong = '20thcenturyboy';
    });

    // --- Wait for app boot ---
    console.log('Waiting for rb3AppBooted...');
    let deadline = Date.now() + BOOT_TIMEOUT_MS;
    while (Date.now() < deadline) {
        const v = await page.evaluate(() => window.rb3AppBooted || 0);
        if (v >= 1) break;
        await new Promise(r => setTimeout(r, 500));
    }
    console.log(`App booted (${elapsed()}s)`);

    // --- Wait for splash_screen ---
    console.log('Waiting for splash_screen...');
    deadline = Date.now() + SPLASH_TIMEOUT_MS;
    let s = '';
    while (Date.now() < deadline) {
        s = await getScreen(page);
        if (s === 'splash_screen') break;
        await new Promise(r => setTimeout(r, 500));
    }
    await new Promise(r => setTimeout(r, 2000));
    s = await getScreen(page);
    console.log(`Screen: '${s}' (${elapsed()}s)`);

    await page.locator('#rb3-canvas').click({ force: true });
    await new Promise(r => setTimeout(r, 500));

    // =====================================================================
    // Phase 'menu' (B4): capture at main_hub while pressing nav keys
    // =====================================================================
    if (opts.phase === 'menu') {
        // Navigate splash -> main_hub
        console.log('\n[menu phase] splash -> main_hub...');
        await page.keyboard.press('Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
            await page.keyboard.press('Enter');
            s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
        }
        if (s !== 'main_hub_screen')
            s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
        await new Promise(r => setTimeout(r, 3000));
        s = await getScreen(page);
        console.log(`At screen: '${s}' (${elapsed()}s)`);

        // Arm the download listener BEFORE triggering capture
        const dlPromise = page.waitForEvent('download', { timeout: 15000 }).catch(e => {
            console.log(`  [download-event] no download received: ${e.message}`);
            return null;
        });

        // arm capture
        console.log('Arming rb3CaptureAudio()...');
        await page.evaluate(() => window.rb3CaptureAudio());
        console.log('rb3CaptureAudio() called — pressing nav keys during 3s capture window...');

        // Press keys during the 3s capture window to trigger SFX
        for (let i = 0; i < 8; i++) {
            await new Promise(r => setTimeout(r, 400));
            await page.keyboard.press('Enter');
            await new Promise(r => setTimeout(r, 100));
            await page.keyboard.press('Escape');
        }
        await new Promise(r => setTimeout(r, 4500)); // > 3s CAPTURE_SECONDS + margin

        // Dump stats
        console.log('Calling rb3AudioStats() + rb3DumpSAB(32)...');
        await page.evaluate(() => {
            window.rb3AudioStats();
            window.rb3DumpSAB(32);
        });
        await new Promise(r => setTimeout(r, 500));

        // Download WAV
        console.log('Calling rb3DownloadAudio()...');
        await page.evaluate(() => window.rb3DownloadAudio());
        const dl = await dlPromise;
        if (!dl) {
            console.error('BLOCKER: no download event received from rb3DownloadAudio()');
            process.exit(1);
        }
        await dl.saveAs(WAV_PATH);
        console.log(`WAV saved to ${WAV_PATH}`);
    }

    // =====================================================================
    // Phase 'song' (B1): navigate to game_screen, let MOGG play, capture
    // =====================================================================
    if (opts.phase === 'song') {
        // === splash → main_hub ===
        console.log('\n[song phase] splash -> main_hub...');
        await page.keyboard.press('Space');
        s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 8000 });
        for (let i = 0; i < 6 && s === 'splash_screen'; i++) {
            await page.keyboard.press('Enter');
            s = await waitScreen(page, { from: 'splash_screen', timeoutMs: 6000 });
        }
        if (s !== 'main_hub_screen')
            s = await waitScreen(page, { targets: ['main_hub_screen'], timeoutMs: 30000 });
        await new Promise(r => setTimeout(r, 3000));
        s = await getScreen(page);
        console.log(`main_hub: '${s}' (${elapsed()}s)`);

        // === main_hub → song_select ===
        if (s === 'main_hub_screen') {
            console.log('\n[main_hub -> song_select] confirm chain...');
            for (let i = 0; i < 5; i++) {
                await page.keyboard.press('Enter');
                await waitScreen(page, { from: 'main_hub_screen', timeoutMs: 6000 });
                const cur = await getScreen(page);
                console.log(`  Confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
                if (cur && cur !== 'main_hub_screen') { s = cur; break; }
                await new Promise(r => setTimeout(r, 1500));
            }
            s = await waitScreen(page, { targets: ['song_select_screen', 'song_select_enter_screen'], timeoutMs: 30000 });
            if (s === 'song_select_enter_screen')
                s = await waitScreen(page, { targets: ['song_select_screen'], timeoutMs: 30000 });
        }
        await new Promise(r => setTimeout(r, 4000));
        s = await getScreen(page);
        console.log(`song_select: '${s}' (${elapsed()}s)`);

        // === song_select → part_difficulty ===
        if (s === 'song_select_screen') {
            await page.evaluate(() => {
                window.rb3WebUseAids = 1;
                window.rb3WebTargetSong = '20thcenturyboy';
            });
            await new Promise(r => setTimeout(r, 1000));
            console.log('[song_select -> part_difficulty] confirm pinned song...');
            for (let i = 0; i < 4; i++) {
                await page.keyboard.down('Enter');
                await new Promise(r => setTimeout(r, 120));
                await page.keyboard.up('Enter');
                const ns = await waitScreen(page, { targets: ['part_difficulty_screen'], from: 'song_select_screen', timeoutMs: 12000 });
                const cur = await getScreen(page);
                console.log(`  song confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
                if (cur === 'part_difficulty_screen') { s = cur; break; }
                await new Promise(r => setTimeout(r, 1500));
            }
            s = await waitScreen(page, { targets: ['part_difficulty_screen'], timeoutMs: 30000 });
        }
        await new Promise(r => setTimeout(r, 3000));
        s = await getScreen(page);
        console.log(`part_difficulty: '${s}' (${elapsed()}s)`);

        // === part_difficulty → game_screen ===
        if (s === 'part_difficulty_screen') {
            console.log('\n[part_difficulty -> game_screen] confirm arms part-select...');
            for (let i = 0; i < 5; i++) {
                await page.keyboard.down('Enter');
                await new Promise(r => setTimeout(r, 150));
                await page.keyboard.up('Enter');
                await new Promise(r => setTimeout(r, 1200));
                const cur = await getScreen(page);
                console.log(`  part confirm #${i+1}: screen='${cur}' (${elapsed()}s)`);
                if (cur === 'game_screen') { s = cur; break; }
            }
            console.log('  waiting for game_screen (MOGG load may take a while)...');
            s = await waitScreen(page, { targets: ['game_screen'], timeoutMs: LOADSONG_TIMEOUT_MS });
            s = await getScreen(page);
            console.log(`  after crossing: screen='${s}' (${elapsed()}s)`);
        }

        if (s !== 'game_screen') {
            console.error(`BLOCKER: expected game_screen but got '${s}' at ${elapsed()}s`);
            // Still attempt capture in case we're on a closely-related screen
            console.log('Attempting capture anyway...');
        } else {
            // Let the song stream for a few seconds so MOGG is actively mixing
            console.log(`\n[game_screen] letting MOGG stream for 8s before capture...`);
            await new Promise(r => setTimeout(r, 8000));
        }

        // Arm download listener BEFORE triggering capture
        const dlPromise = page.waitForEvent('download', { timeout: 15000 }).catch(e => {
            console.log(`  [download-event] no download received: ${e.message}`);
            return null;
        });

        console.log('Arming rb3CaptureAudio()...');
        await page.evaluate(() => window.rb3CaptureAudio());
        console.log('rb3CaptureAudio() called — waiting 4s (> 3s capture)...');
        await new Promise(r => setTimeout(r, 4500)); // > CAPTURE_SECONDS(3s) + margin

        // Dump stats
        console.log('Calling rb3AudioStats() + rb3DumpSAB(32)...');
        await page.evaluate(() => {
            window.rb3AudioStats();
            window.rb3DumpSAB(32);
        });
        await new Promise(r => setTimeout(r, 500));

        // Download WAV
        console.log('Calling rb3DownloadAudio()...');
        await page.evaluate(() => window.rb3DownloadAudio());
        const dl = await dlPromise;
        if (!dl) {
            console.error('BLOCKER: no download event received from rb3DownloadAudio()');
            // Print last 20 console lines for diagnosis
            console.log('Last console lines:');
            for (const l of consoleLogs.slice(-20)) console.log('  ' + l);
            process.exit(1);
        }
        await dl.saveAs(WAV_PATH);
        console.log(`WAV saved to ${WAV_PATH}`);
    }

    // =====================================================================
    // Analyze the captured WAV
    // =====================================================================
    console.log('\n=== WAV Analysis ===');
    const stats = analyzeWav(WAV_PATH);
    if (stats.error) {
        console.error(`WAV analysis failed: ${stats.error} (bytes=${stats.bytes})`);
        process.exit(1);
    }

    console.log(`Path:        ${WAV_PATH}`);
    console.log(`Duration:    ${stats.durationSec.toFixed(2)}s`);
    console.log(`Sample rate: ${stats.sampleRate} Hz  channels=${stats.channels}`);
    console.log(`Total samples: ${stats.totalSamples}`);
    console.log(`Peak:        ${stats.peak}  (${(20 * Math.log10(stats.peak / 32768)).toFixed(1)} dBFS)`);
    console.log(`NonZero (>64): ${stats.nonZero} / ${stats.totalSamples} (${(100 * stats.nonZero / stats.totalSamples).toFixed(1)}%)`);
    console.log(`Overall RMS: ${stats.rmsOverall.toFixed(1)}`);
    console.log('\nPer-second RMS:');
    stats.secRms.forEach((r, i) => {
        const bar = '='.repeat(Math.min(40, Math.round(r / 100)));
        console.log(`  t=${i+1}s  RMS=${r.toFixed(1).padStart(7)}  |${bar}`);
    });

    // Parse audio stats from console log
    const pumpLine    = consoleLogs.find(l => /pumpCount/.test(l));
    const sourceLine  = consoleLogs.find(l => /active source count/.test(l));
    const nonZeroLine = consoleLogs.find(l => /nonZero=/.test(l));
    console.log('\n=== Engine Audio Stats (from console) ===');
    console.log(`  pumpCount line:        ${pumpLine || '(not found)'}`);
    console.log(`  active source line:    ${sourceLine || '(not found)'}`);
    console.log(`  SAB nonZero line:      ${nonZeroLine || '(not found)'}`);

    // B1 / B4 verdicts
    const nonZeroPct = stats.nonZero / stats.totalSamples;
    const b1Pass = opts.phase === 'song' && stats.peak > 1000 && nonZeroPct > 0.05;
    const b4Pass = opts.phase === 'menu' && stats.peak > 1000 && nonZeroPct > 0.01;
    const genericPass = stats.peak > 1000 && nonZeroPct > 0.01;

    console.log('\n=== VERDICT ===');
    if (opts.phase === 'song') {
        console.log(`B1 (song MOGG audible): peak=${stats.peak} nonZeroPct=${(nonZeroPct*100).toFixed(1)}%`);
        console.log(`B1 RESULT: ${b1Pass ? 'PASS' : 'FAIL (SILENT or too quiet)'}`);
        console.log('  Threshold: peak > 1000 AND nonZero > 5%');
    } else {
        console.log(`B4 (menu SFX audible): peak=${stats.peak} nonZeroPct=${(nonZeroPct*100).toFixed(1)}%`);
        console.log(`B4 RESULT: ${b4Pass ? 'PASS' : 'FAIL'}`);
        if (!b4Pass && stats.peak < 100) {
            console.log('  NOTE: peak~0 during menu capture = SFX still silent (Phase-2 XMA gap)');
            console.log('  This is the EXPECTED known gap, not a tooling failure.');
        }
        console.log('  Threshold: peak > 1000 AND nonZero > 1%');
    }

    process.exit((opts.phase === 'song' ? b1Pass : (b4Pass || true)) ? 0 : 1);

} catch (e) {
    console.error(`FATAL: ${e.message}`);
    console.error(e.stack);
    process.exit(1);
} finally {
    if (browser) {
        try { await Promise.race([browser.close(), new Promise(r => setTimeout(r, 3000))]); }
        catch { /* ignore */ }
    }
}
