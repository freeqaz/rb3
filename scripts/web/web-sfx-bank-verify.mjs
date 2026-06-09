/**
 * web-sfx-bank-verify.mjs — confirm the common SFX sound bank LOADS on web and
 * that UI/menu SFX actually play through the XMA->PCM sidecar path.
 *
 * Background: rb3-web used to SKIP the common sound bank ("audio-free W3a").
 * App.cpp now loads it on web like every other target; Xbox-360 kXMA samples
 * play via offline PCM sidecars served on demand by server.py (xma_pcm/...).
 *
 * What this checks from the browser console stream during a splash->main_hub
 * (->song_select) navigation that fires menu button SFX:
 *   - "sound bank done"            (bank load ran)  AND NOT "sound bank SKIPPED"
 *   - common_bank.milo_xbox fetch  (the bank reached MEMFS)
 *   - >=1 "playing XMA->PCM sidecar" (a SampleInst actually started)
 *   - 0  "no PCM sidecar"          (no coverage misses)
 *
 * Usage (start the server first):
 *   python3 native/web/server.py &
 *   node scripts/web/web-sfx-bank-verify.mjs [--port 8421] [--verbose]
 *
 * Exit 0 = bank loaded on web AND at least one SFX played with no sidecar miss.
 */
import {
    parseArgs, waitForServer, launchBrowser, createCapture,
    navigateTo, outputDir, saveLogs, saveJson, cleanup, SCREENS,
} from './lib/core.mjs';

const opts = parseArgs({ port: 8421, verbose: false });

function count(logs, needle) {
    return logs.filter(l => l.text.includes(needle)).length;
}

(async () => {
    await waitForServer(opts.port);
    // ?debug=true -> the no-store debug build (the one build.sh --debug deploys).
    const { browser, page } = await launchBrowser(opts.port, { query: 'debug=true' });
    const cap = createCapture(page, {
        verbose: opts.verbose,
        // Surface the audio-relevant lines even when not --verbose.
        filter: /sound bank|XMA->PCM|no PCM sidecar|common_bank|xma_pcm|AudioDevice/,
    });

    let reached = null;
    try {
        // Navigate to song_select — a Confirm chain through the menu that fires
        // button_confirm / button_back / scroll SFX from the common bank.
        reached = await navigateTo(page, cap, SCREENS.SONG_SELECT).catch(async () => {
            // song_select can be flaky headless; main_hub alone already fires SFX.
            return await navigateTo(page, cap, SCREENS.MAIN_HUB);
        });
        // Let a few more frames of SFX settle.
        await new Promise(r => setTimeout(r, 4000));
    } catch (e) {
        console.log(`  [nav] ${e.message}`);
    }

    const logs = cap.logs;
    const bankDone    = count(logs, 'sound bank done') > 0;
    const bankSkipped = count(logs, 'sound bank SKIPPED') > 0;
    const bankFetched = count(logs, 'common_bank.milo_xbox') > 0;
    const sfxPlayed   = count(logs, 'playing XMA->PCM sidecar');
    const sidecarFetch = count(logs, 'xma_pcm/');
    const sidecarMiss = count(logs, 'no PCM sidecar');

    const dir = outputDir('web-sfx-bank-verify', opts.out);
    saveLogs(logs, dir);
    const result = {
        reachedScreen: reached,
        bankLoadRan: bankDone, bankWasSkipped: bankSkipped, bankFetched,
        sfxPlayedCount: sfxPlayed, sidecarFetchCount: sidecarFetch, sidecarMissCount: sidecarMiss,
    };
    saveJson(result, dir);

    const pass = bankDone && !bankSkipped && sfxPlayed >= 1 && sidecarMiss === 0;
    console.log('\n=== web SFX bank verification ===');
    console.log(`  reached screen      : ${reached}`);
    console.log(`  bank load ran       : ${bankDone}  (skipped=${bankSkipped})`);
    console.log(`  common_bank fetched : ${bankFetched}`);
    console.log(`  SFX played (sidecar): ${sfxPlayed}`);
    console.log(`  sidecar fetches     : ${sidecarFetch}`);
    console.log(`  sidecar misses      : ${sidecarMiss}`);
    console.log(`  RESULT              : ${pass ? 'PASS' : 'FAIL'}`);

    await cleanup(browser);
    process.exit(pass ? 0 : 1);
})().catch((e) => { console.error(e); process.exit(1); });
