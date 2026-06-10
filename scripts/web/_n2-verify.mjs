#!/usr/bin/env node
/**
 * _n2-verify.mjs — N2 (mogg Range read-ahead + cross-open chunk reuse) verification.
 *
 * Drives a tight journey under CDP throttle (default c4: 4 Mbps / 150 ms RTT),
 * cold cache, capturing the FULL lifecycle (start+end) of every mogg Range request
 * so we can prove:
 *   (1) read-ahead: chunk N+1 is fetched DURING chunk N's consumption (overlap).
 *   (2) cross-open reuse: starting the song does NOT re-download the preview's
 *       early chunks (0-1).
 *   (3) cancel-mid-fetch: hover A, switch to B before A lands, x10 -> no crash/error.
 *
 * Usage:
 *   node _n2-verify.mjs --port 8446 [--mbps 4] [--rtt 150] [--out DIR] [--noreadahead] [--nocache]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const has = (k) => argv.includes(`--${k}`);
const PORT = parseInt(arg('port', '8446'), 10);
const MBPS = parseFloat(arg('mbps', '4'));
const RTT = parseFloat(arg('rtt', '150'));
const OUT = arg('out', '/tmp/rb3-n2-verify');
const NOREADAHEAD = has('noreadahead');
const NOCACHE = has('nocache');
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const T0 = Date.now();
const log = (m) => console.log(`  [${((Date.now() - T0) / 1000).toFixed(1)}s] ${m}`);

// env bridge: opt-out flags for the A/B runs
const DBG = has('cachedbg');
const envParts = [];
if (NOREADAHEAD) envParts.push('RB3_MOGG_READAHEAD_OFF=1');
if (NOCACHE) envParts.push('RB3_MOGG_CACHE_MB=0');
if (DBG) envParts.push('RB3_MOGG_CACHE_DBG=1');
const params = ['debug=true'];
// rb3_pre.js splits the ?env value on ';' (NOT ',') into NAME=VALUE pairs.
if (envParts.length) params.push(`env=${encodeURIComponent(envParts.join(';'))}`);
const query = `?${params.join('&')}`;

const stateOf = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
  booted: window.rb3AppBooted || 0, song: window.rb3HighlightedSong || '',
})).catch(() => ({ screen: '', frame: 0, booted: 0, song: '' }));

async function press(page, key, holdMs = 200, gapMs = 420) {
  try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(gapMs); } catch {}
}
async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await stateOf(page);
    if (s.screen !== last) { log(`${label}: '${s.screen}' frame=${s.frame} song='${s.song}'`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();

const cdp = await ctx.newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.emulateNetworkConditions', {
  offline: false, latency: RTT,
  downloadThroughput: (MBPS * 1024 * 1024) / 8,
  uploadThroughput: (MBPS * 1024 * 1024) / 8,
});

// Full request lifecycle capture (start + finish) so we can compute overlaps.
const reqMeta = new Map();
const net = []; // {url, range, t0, t1, bytes, failed}
cdp.on('Network.requestWillBeSent', (e) => {
  const range = (e.request.headers && (e.request.headers.Range || e.request.headers.range)) || '';
  reqMeta.set(e.requestId, { url: e.request.url, range, t0: Date.now() - T0 });
});
cdp.on('Network.loadingFinished', (e) => {
  const m = reqMeta.get(e.requestId);
  if (m) net.push({ ...m, t1: Date.now() - T0, bytes: e.encodedDataLength || 0 });
});
cdp.on('Network.loadingFailed', (e) => {
  const m = reqMeta.get(e.requestId);
  if (m) net.push({ ...m, t1: Date.now() - T0, bytes: 0, failed: e.errorText });
});

const errors = [];
const cacheLog = [];
page.on('pageerror', (err) => { errors.push(`PAGEERROR: ${err.message || err}`); console.log(`  [PAGE_ERROR] ${err.message || err}`); });
page.on('crash', () => { errors.push('CRASH'); console.log('  [CRASH]'); });
page.on('console', (m) => { const t = m.text(); if (t.includes('[moggcache]')) cacheLog.push({ t: Date.now() - T0, text: t }); });

log(`N2 verify: ${MBPS} Mbps / ${RTT} ms RTT  readahead=${NOREADAHEAD ? 'OFF' : 'ON'} cache=${NOCACHE ? 'OFF' : 'ON'}  query='${query}'`);
await page.goto(`http://127.0.0.1:${PORT}/${query}`, { waitUntil: 'domcontentloaded', timeout: 120000 });

// ---- BOOT -> SONG_SELECT ----
await waitScreen(page, s => ['intro_movie_screen', 'splash_screen', 'main_hub_screen'].includes(s.screen), 300000, 'boot');
await page.locator('#rb3-canvas').click({ force: true }).catch(() => {});
await sleep(500);
for (let i = 0; i < 30; i++) { const sc = (await stateOf(page)).screen; if (sc === 'splash_screen' || sc === 'main_hub_screen') break; await press(page, 'Space', 200, 600); }
await waitScreen(page, s => ['splash_screen', 'main_hub_screen'].includes(s.screen), 180000, 'splash');
await sleep(1200);
// splash -> main_hub: Start (Space) then a Confirm (Enter) chain; under low
// bandwidth the hub milos can take 45s+, so press, then WAIT, repeatedly.
for (let i = 0; i < 30 && (await stateOf(page)).screen !== 'main_hub_screen'; i++) {
  await press(page, i % 2 ? 'Enter' : 'Space', 200, 600);
  await waitScreen(page, s => s.screen === 'main_hub_screen', 8000, 'splash->hub');
}
const reachedHub = await waitScreen(page, s => s.screen === 'main_hub_screen', 180000, 'main_hub');
if (!reachedHub) { console.log('FAIL: never reached main_hub'); await finish(); }
await sleep(1500);
for (let i = 0; i < 20 && (await stateOf(page)).screen !== 'song_select_screen'; i++) {
  await press(page, 'Enter', 220, 500);
  await waitScreen(page, s => s.screen === 'song_select_screen', 6000, 'hub->sel');
}
const reachedSel = await waitScreen(page, s => s.screen === 'song_select_screen', 90000, 'song_select');
if (!reachedSel) { console.log('FAIL: never reached song_select'); await finish(); }
await sleep(3500);

// Helper: is a mogg Range request, and parse the byte offset -> chunk index (1MB chunks).
const isMoggRange = (n) => /\.mogg/.test(n.url) && /bytes=/.test(n.range);
const chunkOf = (n) => { const m = /bytes=(\d+)-/.exec(n.range); return m ? Math.floor(parseInt(m[1], 10) / (1 << 20)) : -1; };
const moggName = (n) => { const m = /\/([^/]+)\.mogg/.exec(n.url); return m ? m[1] : '?'; };

// ============ SCENARIO 3 (run first): CANCEL-MID-FETCH STRESS (UAF class) ====
// Hover a song (kicks a mogg range fetch incl. a read-ahead slot), then jump to
// another song BEFORE the chunk(s) can land -> WebRangeFile destroyed mid-fetch,
// both slots abandoned. Repeat. This is the exact UAF class the wave-2 review
// caught, now extended to the read-ahead slot. Run BEFORE committing to gameplay
// so it doesn't depend on game->back navigation.
const QUICK = has('quick');
console.log('\n--- SCENARIO 3: cancel-mid-fetch x10 (UAF stress) ---');
let cancelRounds = 0;
for (let r = 0; r < (QUICK ? 0 : 10); r++) {
  await press(page, 'ArrowDown', 80, 120);          // hover -> kicks fetch + read-ahead
  await sleep(250 + Math.floor(Math.random() * 400)); // partway through a 4s chunk at 4Mbps/150ms
  await press(page, 'ArrowDown', 80, 120);          // jump away -> destroys WebRangeFile mid-fetch
  await sleep(120);
  cancelRounds++;
  if (errors.length) { console.log(`  ABORT: error after round ${r}`); break; }
}
await sleep(2500); // let the abandoned fetches land + self-reclaim
console.log(`cancel-mid-fetch rounds=${cancelRounds}  errors=${errors.length}  -> ${errors.length === 0 ? 'NO CRASH/ERROR' : 'ERRORS: ' + errors.join('; ')}`);
await sleep(2000); // settle before the measured hover

// ============ SCENARIO 1: COLD HOVER (read-ahead + first-audio) ============
log('--- SCENARIO 1: cold hover (read-ahead overlap) ---');
const before = (await stateOf(page)).song;
for (let i = 0; i < 5; i++) await press(page, 'ArrowDown', 120, 320); // jump to a fresh deep song
await sleep(1500); // let the highlight SETTLE so the preview debounce targets THIS song
const hovered = await stateOf(page);
const hoverStartIdx = net.length; // start counting AFTER the settle -> only this song's chunks
const hoverT0 = Date.now() - T0;
log(`hovering song='${hovered.song}' (from '${before}')`);
await sleep(12000); // preview debounce + cold mogg stream under throttle
// previewSong = the mogg that actually streamed for the settled highlight
const hoverMoggs = net.slice(hoverStartIdx).filter(isMoggRange);
// pick the dominant mogg (most range reqs) so a tail-end fetch of the prior song doesn't win
const moggCounts = {};
for (const n of hoverMoggs) moggCounts[moggName(n)] = (moggCounts[moggName(n)] || 0) + 1;
const previewSong = Object.keys(moggCounts).sort((a, b) => moggCounts[b] - moggCounts[a])[0] || null;
log(`preview mogg='${previewSong}'  ${hoverMoggs.length} range reqs  (counts ${JSON.stringify(moggCounts)})`);

// ============ SCENARIO 2: START SAME SONG (cross-open reuse) ============
log('--- SCENARIO 2: start same song (cross-open chunk reuse) ---');
const gameStartIdx = net.length;
await press(page, 'Enter', 200, 800); // song_select -> part_difficulty
await waitScreen(page, s => ['part_difficulty_screen', 'game_screen'].includes(s.screen), 60000, 'difficulty');
for (let i = 0; i < 4 && (await stateOf(page)).screen !== 'game_screen'; i++) await press(page, 'Enter', 200, 900);
const reachedGame = await waitScreen(page, s => s.screen === 'game_screen', 120000, 'game');
await sleep(8000); // let the gameplay mogg stream
const gameMoggs = net.slice(gameStartIdx).filter(isMoggRange);
const gameSong = gameMoggs.length ? moggName(gameMoggs.find(n => moggName(n) === previewSong) || gameMoggs[0]) : null;
log(`gameplay mogg='${gameSong}'  ${gameMoggs.length} range reqs  reachedGame=${reachedGame ? 'yes' : 'no'}`);

// ============ ANALYSIS ============
// Read-ahead overlap: for the preview/gameplay mogg, do any two consecutive-chunk
// range requests overlap in time (N+1 started before N finished)?
function overlapAnalysis(reqs) {
  const byChunk = reqs.map(n => ({ chunk: chunkOf(n), t0: n.t0, t1: n.t1, dur: n.t1 - n.t0 })).filter(r => r.chunk >= 0).sort((a, b) => a.t0 - b.t0);
  let overlaps = 0; const pairs = [];
  for (let i = 0; i < byChunk.length; i++) {
    for (let j = i + 1; j < byChunk.length; j++) {
      const a = byChunk[i], b = byChunk[j];
      // overlap = b started before a finished, and they cover different chunks
      if (a.chunk !== b.chunk && b.t0 < a.t1 && a.t0 < b.t1) {
        overlaps++;
        if (pairs.length < 12) pairs.push({ a: a.chunk, b: b.chunk, aWin: [a.t0, a.t1], bWin: [b.t0, b.t1] });
      }
    }
  }
  return { count: byChunk.length, overlaps, pairs, chunks: byChunk.map(r => r.chunk) };
}

const allMogg = net.filter(isMoggRange);
const previewReqs = allMogg.filter(n => moggName(n) === previewSong && n.t0 < (gameStartIdx < net.length ? net[gameStartIdx]?.t0 ?? Infinity : Infinity));
const previewChunks = new Set(net.slice(hoverStartIdx, gameStartIdx).filter(isMoggRange).filter(n => moggName(n) === previewSong).map(chunkOf));
const gameReqs = net.slice(gameStartIdx).filter(isMoggRange).filter(n => moggName(n) === (gameSong || previewSong));
const gameChunks = gameReqs.map(chunkOf);
const redownloaded = gameChunks.filter(c => previewChunks.has(c));

const previewOverlap = overlapAnalysis(net.slice(hoverStartIdx, gameStartIdx).filter(isMoggRange).filter(n => moggName(n) === previewSong));
const gameOverlap = overlapAnalysis(gameReqs);

console.log('\n========== N2 ANALYSIS ==========');
console.log(`config: readahead=${NOREADAHEAD ? 'OFF' : 'ON'} cache=${NOCACHE ? 'OFF' : 'ON'}`);
console.log(`preview song='${previewSong}' chunks fetched: [${[...previewChunks].sort((a, b) => a - b).join(',')}]`);
console.log(`preview range-req overlaps (N+1 during N): ${previewOverlap.overlaps}  (chunk seq: [${previewOverlap.chunks.join(',')}])`);
if (previewOverlap.pairs.length) console.log(`  sample overlaps:`, JSON.stringify(previewOverlap.pairs.slice(0, 6)));
console.log(`gameplay song='${gameSong}' chunks fetched: [${[...new Set(gameChunks)].sort((a, b) => a - b).join(',')}]`);
console.log(`gameplay range-req overlaps: ${gameOverlap.overlaps}`);
console.log(`CROSS-OPEN: gameplay chunks that the preview ALSO fetched (re-downloads): [${redownloaded.sort((a, b) => a - b).join(',')}]  (count=${redownloaded.length})`);
console.log(`  -> reuse ${redownloaded.length === 0 ? 'GOOD (no re-download)' : 'chunks ' + redownloaded.join(',') + ' RE-FETCHED'}`);

const result = {
  config: { mbps: MBPS, rtt: RTT, readahead: !NOREADAHEAD, cache: !NOCACHE },
  previewSong, previewChunks: [...previewChunks].sort((a, b) => a - b),
  previewOverlaps: previewOverlap.overlaps, previewChunkSeq: previewOverlap.chunks,
  gameSong, gameChunks: [...new Set(gameChunks)].sort((a, b) => a - b),
  gameOverlaps: gameOverlap.overlaps,
  crossOpenRedownloads: redownloaded.sort((a, b) => a - b),
  cancelRounds, errors,
  reachedGame: !!reachedGame,
};
writeFileSync(`${OUT}/result.json`, JSON.stringify(result, null, 2));
writeFileSync(`${OUT}/net.ndjson`, net.map(n => JSON.stringify(n)).join('\n') + '\n');
if (cacheLog.length) {
  writeFileSync(`${OUT}/cache.log`, cacheLog.map(c => `${(c.t / 1000).toFixed(1)}s  ${c.text}`).join('\n') + '\n');
  console.log(`cache events: ${cacheLog.length} (${OUT}/cache.log)`);
}
console.log(`\nartifacts: ${OUT}/result.json  ${OUT}/net.ndjson`);

async function finish() {
  try { writeFileSync(`${OUT}/net.ndjson`, net.map(n => JSON.stringify(n)).join('\n') + '\n'); } catch {}
  await browser.close().catch(() => {});
  process.exit(0);
}
await browser.close().catch(() => {});
process.exit(0);
