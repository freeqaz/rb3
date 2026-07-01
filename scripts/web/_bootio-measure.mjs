// _bootio-measure.mjs — Step-0 boot-I/O attribution + appctor-timing harness for
// handoff 02-boot-sync-read. Boots the DEBUG web build with the given URL params,
// captures the [boot-io-stats] stderr line printed at appctor_done, and reads the
// appctor_start -> appctor_done delta from window.rb3BootPhaseLog.
//
// Usage:
//   node scripts/web/_bootio-measure.mjs --port 8432 [--query "bootNoResidencySkip=1"] [--label A]
//
// Prints one summary line; chain runs to A/B the residency-skip + loader knobs.
import { waitForServer, launchBrowser, createCapture, waitForBoot, cleanup, engineState }
    from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const PORT = parseInt(arg('port', '8432'), 10) || 8432;
const EXTRA = arg('query', '');
const LABEL = arg('label', '');
// Always debug build (?debug=true) + always-on stats; merge any extra knobs.
const query = ['debug=true', 'bootIoStats=1', EXTRA].filter(Boolean).join('&');

(async () => {
    await waitForServer(PORT, 30000);
    const { browser, page } = await launchBrowser(PORT, { noGoto: true, query });
    let ioStatsLine = null;
    page.on('console', (msg) => {
        const t = msg.text();
        if (t.includes('[boot-io-stats')) ioStatsLine = t;
    });
    const url = `http://127.0.0.1:${PORT}/?${query}`;
    const t0 = Date.now();
    await page.goto(url, { waitUntil: 'domcontentloaded', timeout: 30000 });
    // Wait for appBooted (App ctor finished) — generous, the box is loaded.
    await waitForBoot(page, 180000).catch(() => {});
    const phases = await page.evaluate(() => window.rb3BootPhaseLog || []).catch(() => []);
    const st = await engineState(page).catch(() => ({}));
    await cleanup(browser);

    const find = (n) => { const e = phases.find(p => p[0] === n); return e ? e[1] : null; };
    const aStart = find('appctor_start');
    const aDone = find('appctor_done');
    const appctorS = (aStart != null && aDone != null) ? ((aDone - aStart) / 1000).toFixed(2) : 'n/a';
    const navToDoneS = aDone != null ? (aDone / 1000).toFixed(2) : 'n/a';
    const wall = ((Date.now() - t0) / 1000).toFixed(1);

    console.log('========================================================');
    console.log(`[bootio${LABEL ? ' ' + LABEL : ''}] query=${query}`);
    console.log(`  appctor_start->done = ${appctorS}s   (nav->appctor_done ${navToDoneS}s, wall ${wall}s)`);
    console.log(`  final screen='${st.screen || '?'}' frame=${st.frame ?? '?'}`);
    if (ioStatsLine) console.log(`  ${ioStatsLine}`);
    else console.log('  (no [boot-io-stats] line captured — RB3_BOOT_IO_STATS off or appctor not reached)');
    console.log('  PHASES: ' + phases.map(p => `${p[0]}@${(p[1] / 1000).toFixed(1)}`).join(' '));
    console.log('========================================================');
})();
