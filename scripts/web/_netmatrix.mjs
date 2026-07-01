#!/usr/bin/env node
/**
 * matrix.mjs — network-restricted benchmark matrix for the incremental-load effort.
 *
 * Drives ONE scripted journey under a fixed CDP throttle, cold cache:
 *   boot -> intro -> title(splash) -> main_hub -> song_select
 *   -> 3 cold hovers (deep, distinct songs) -> start a song
 *
 * Per phase records: wall ms, longest rAF gap, #gaps>100ms, frozen-Σ (gaps>250ms),
 * plus CDP network request count + bytes. Also pulls the MEMFS frame-trace JSONL
 * (RB3_FRAME_TRACE=/trace.jsonl via ?env=) so each long gap can be attributed to a
 * subsystem counter (fetch/dta/obj/prime/tex/mesh/pipe ms).
 *
 * Usage:
 *   node matrix.mjs --port 8441 --out DIR --mbps 20 --rtt 40 --label c1 [--release] [--run 1]
 */
import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'fs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i !== -1 && i + 1 < argv.length ? argv[i + 1] : d; };
const has = (k) => argv.includes(`--${k}`);
const PORT = parseInt(arg('port', '8441'), 10);
const OUT = arg('out', '/tmp/rb3perf-netmatrix/run');
const MBPS = parseFloat(arg('mbps', '20'));
const RTT = parseFloat(arg('rtt', '40'));
const LABEL = arg('label', 'cX');
const RUN = arg('run', '1');
const RELEASE = !has('debug'); // default release path (?-less); --debug for debug build
mkdirSync(OUT, { recursive: true });
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// ---- in-page rAF instrument: every gap with its perf.now timestamp ----
function instrument() {
  window.__wf = { perf0: performance.now(), epoch0: Date.now(), raf: [], milestones: {}, screenFirst: {} };
  const wf = window.__wf;
  const mark = (k) => { if (wf.milestones[k] === undefined) wf.milestones[k] = performance.now(); };
  let last = -1;
  function tick() {
    const t = performance.now();
    if (last >= 0) wf.raf.push([+t.toFixed(1), +(t - last).toFixed(1)]);
    last = t;
    if ((window.rb3AppBooted || 0) >= 1) mark('appBooted');
    const s = window.rb3CurrentScreen || '';
    if (s && wf.screenFirst[s] === undefined) wf.screenFirst[s] = t;
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}

const stateOf = (page) => page.evaluate(() => ({
  screen: window.rb3CurrentScreen || '', frame: window.rb3FrameCount || 0,
  booted: window.rb3AppBooted || 0, song: window.rb3HighlightedSong || '',
  now: performance.now(),
})).catch(() => ({ screen: '', frame: 0, booted: 0, song: '', now: 0 }));

async function press(page, key, holdMs = 200, gapMs = 420) {
  try { await page.keyboard.down(key); await sleep(holdMs); await page.keyboard.up(key); await sleep(gapMs); } catch {}
}

async function waitScreen(page, pred, timeoutMs, label) {
  const dl = Date.now() + timeoutMs; let last = '';
  while (Date.now() < dl) {
    const s = await stateOf(page);
    if (s.screen !== last) { console.log(`    [${((Date.now()-T0)/1000).toFixed(1)}s] ${label}: '${s.screen}' frame=${s.frame}`); last = s.screen; }
    if (pred(s)) return s;
    await sleep(250);
  }
  return null;
}

// freeze stats within a perf.now window [p0,p1]
function freezeIn(raf, p0, p1) {
  const inWin = raf.filter(g => g[0] >= p0 && g[0] <= p1).map(g => g[1]);
  return {
    frames: inWin.length,
    wallMs: +(p1 - p0).toFixed(0),
    longest: +inWin.reduce((m, g) => Math.max(m, g), 0).toFixed(1),
    over100: inWin.filter(g => g > 100).length,
    over250: inWin.filter(g => g > 250).length,
    frozenSum: +inWin.reduce((a, g) => a + Math.max(0, g - 250), 0).toFixed(0), // Σ(gap-250) for gaps>250
    frozen33: +inWin.reduce((a, g) => a + Math.max(0, g - 33), 0).toFixed(0),    // legacy comparator
    top: inWin.slice().sort((a, b) => b - a).slice(0, 8),
  };
}

const T0 = Date.now();
// RB3_EXTRA_ENVQ lets a gate driver append flags (e.g. RB3_LOADER_READAHEAD=0)
// to the ?env= bridge without forking this harness. Semicolon-joined.
const ENVQ = [`RB3_FRAME_TRACE=/trace.jsonl`, process.env.RB3_EXTRA_ENVQ].filter(Boolean).join(';');
const debugParam = RELEASE ? '' : 'debug=true';
const params = [debugParam, `env=${encodeURIComponent(ENVQ)}`].filter(Boolean).join('&');
const query = params ? `?${params}` : '';

const browser = await chromium.launch({
  headless: !process.env.DISPLAY,
  args: ['--no-sandbox', '--enable-unsafe-webgpu', '--use-angle=vulkan',
    '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
    '--ozone-platform=x11', '--mute-audio', '--autoplay-policy=no-user-gesture-required'],
});
// fresh context = cold profile; no storageState => empty IDB/cache
const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
const page = await ctx.newPage();

// CDP: throttle + network event capture
const cdp = await ctx.newCDPSession(page);
await cdp.send('Network.enable');
await cdp.send('Network.emulateNetworkConditions', {
  offline: false, latency: RTT,
  downloadThroughput: (MBPS * 1024 * 1024) / 8,
  uploadThroughput: (MBPS * 1024 * 1024) / 8,
});
// also clear IDB explicitly for this origin to guarantee cold
await cdp.send('Storage.clearDataForOrigin', {
  origin: `http://127.0.0.1:${PORT}`, storageTypes: 'all',
}).catch(() => {});

// network log: {ts(perf.now-ish via Date), url, status, encodedBytes, range}
const net = [];
const reqMeta = new Map();
cdp.on('Network.requestWillBeSent', (e) => {
  const range = (e.request.headers && (e.request.headers.Range || e.request.headers.range)) || '';
  reqMeta.set(e.requestId, { url: e.request.url, range, t0: Date.now() });
});
cdp.on('Network.loadingFinished', (e) => {
  const m = reqMeta.get(e.requestId);
  if (m) net.push({ epoch: Date.now(), url: m.url, range: m.range, bytes: e.encodedDataLength || 0, dur: Date.now() - m.t0 });
});
cdp.on('Network.loadingFailed', (e) => {
  const m = reqMeta.get(e.requestId);
  if (m) net.push({ epoch: Date.now(), url: m.url, range: m.range, bytes: 0, failed: e.errorText });
});

const cons = [];
page.on('console', m => cons.push({ epoch: Date.now(), text: m.text() }));
await page.addInitScript(instrument);

console.log(`[${LABEL} run${RUN}] ${MBPS} Mbps / ${RTT} ms RTT  build=${RELEASE?'release':'debug'}  query='${query}'`);

const navEpoch = Date.now();
await page.goto(`http://127.0.0.1:${PORT}/${query}`, { waitUntil: 'domcontentloaded', timeout: 120000 });

// epoch->perf.now offset: snapshot once shortly after boot for windowing in perf.now space
// We window phases in perf.now (screenFirst) where possible; otherwise approximate via epoch deltas.

// ---- BOOT ----
const reachedBoot = await waitScreen(page, s => ['intro_movie_screen','splash_screen','main_hub_screen'].includes(s.screen), 420000, 'boot');
await page.locator('#rb3-canvas').click({ force: true }).catch(()=>{});
await sleep(500);

// ---- INTRO -> SPLASH(title) ----
for (let i = 0; i < 30; i++) { const sc = (await stateOf(page)).screen; if (sc === 'splash_screen' || sc === 'main_hub_screen') break; await press(page, 'Space', 200, 600); }
await waitScreen(page, s => ['splash_screen','main_hub_screen'].includes(s.screen), 180000, 'splash');
await sleep(1200);

// ---- SPLASH -> MAIN_HUB ----
for (let i = 0; i < 18 && (await stateOf(page)).screen !== 'main_hub_screen'; i++) await press(page, 'Space', 250, 600);
const reachedHub = await waitScreen(page, s => s.screen === 'main_hub_screen', 180000, 'main_hub');
await sleep(1500);

// ---- MAIN_HUB -> SONG_SELECT ----
const hovers = [];
let startSong = null;
let anchor = null;
if (reachedHub) {
  for (let i = 0; i < 14 && (await stateOf(page)).screen !== 'song_select_screen'; i++) await press(page, 'Enter', 220, 500);
  await waitScreen(page, s => s.screen === 'song_select_screen', 90000, 'song_select');
  await sleep(3500); // settle; first preview may fire on highlighted song

  // ---- 3 COLD HOVERS on distinct deep songs ----
  // Navigate deep, then for each hover: jump several rows to a fresh song, dwell
  // long enough for the ~1s preview debounce to fire + cold mogg stream under throttle.
  // also capture an epoch->perf.now anchor so we can map the CDP net timeline
  // (Date.now epoch) onto the rAF gap timeline (perf.now).
  anchor = await page.evaluate(() => ({ epoch: Date.now(), perf: performance.now() }));
  for (let i = 0; i < 6; i++) await press(page, 'ArrowDown', 130, 200);
  await sleep(1500);
  for (let h = 0; h < 3; h++) {
    const before = await stateOf(page);
    const hp0 = before.now;
    // jump to a fresh song (several rows so it's definitely non-resident)
    for (let i = 0; i < 4; i++) await press(page, 'ArrowDown', 130, 200);
    const settled = await stateOf(page);
    const dwellMs = 9000; // preview debounce + cold mogg under throttle (3G needs headroom)
    await sleep(dwellMs);
    const after = await stateOf(page);
    const hp1 = after.now;
    const raf = await page.evaluate(() => window.__wf.raf);
    const fz = freezeIn(raf, hp0, hp1);
    hovers.push({ idx: h, song: after.song, fromSong: before.song, p0: hp0, p1: hp1, ...fz });
    console.log(`  COLD hover#${h}: song='${after.song}' (from '${before.song}') ${JSON.stringify(fz)}`);
  }

  // ---- START A SONG ----
  const startP0 = (await stateOf(page)).now;
  for (let i = 0; i < 6 && !['part_difficulty_screen','game_screen','genre_screen'].includes((await stateOf(page)).screen); i++) await press(page, 'Enter', 220, 600);
  const afterStartScreen = (await stateOf(page)).screen;
  // advance through difficulty if present
  for (let i = 0; i < 6 && (await stateOf(page)).screen !== 'game_screen'; i++) await press(page, 'Enter', 220, 700);
  const reachedGame = await waitScreen(page, s => s.screen === 'game_screen', 120000, 'start_song');
  await sleep(7000); // dwell in gameplay so the gameplay mogg streams under throttle
  const startP1 = (await stateOf(page)).now;
  const raf2 = await page.evaluate(() => window.__wf.raf);
  startSong = { reached: !!reachedGame, midScreen: afterStartScreen, p0: startP0, p1: startP1, ...freezeIn(raf2, startP0, startP1) };
  console.log(`  START song: reached_game=${!!reachedGame} ${JSON.stringify(startSong)}`);
}

// ---- collect everything ----
const wf = await page.evaluate(() => window.__wf);
const raf = wf.raf;
const sf = wf.screenFirst;
const ms = wf.milestones;

// Phase windows in perf.now space (using screenFirst)
function phase(a, b) {
  const p0 = sf[a], p1 = sf[b];
  if (p0 === undefined || p1 === undefined) return null;
  return { from: a, to: b, ...freezeIn(raf, p0, p1) };
}
const phases = {
  boot_appBooted: ms.appBooted !== undefined ? { from:'nav', to:'appBooted', wallMs:+(ms.appBooted - wf.perf0).toFixed(0), ...freezeIn(raf, wf.perf0, ms.appBooted) } : null,
  intro_to_splash: phase('intro_movie_screen','splash_screen'),
  splash_to_hub: phase('splash_screen','main_hub_screen'),
  hub_to_songsel: phase('main_hub_screen','song_select_screen'),
};

// Pull the MEMFS frame trace
let traceText = '';
try {
  traceText = await page.evaluate(() => {
    try { return FS.readFile('/trace.jsonl', { encoding: 'utf8' }); } catch (e) { return 'ERR:' + e; }
  });
} catch (e) { traceText = 'EVAL_ERR:' + e; }

// Net summary: total + by category
const isMogg = u => /\.mogg/.test(u);
const isMilo = u => /\.milo/.test(u);
const isBundle = u => /\/api\/bundle/.test(u);
const sumBytes = (pred) => net.filter(n => pred(n.url)).reduce((a, n) => a + (n.bytes||0), 0);
const netSummary = {
  totalReq: net.length,
  totalBytes: net.reduce((a,n)=>a+(n.bytes||0),0),
  moggReq: net.filter(n=>isMogg(n.url)).length,
  moggRangeReq: net.filter(n=>isMogg(n.url) && n.range).length,
  moggBytes: sumBytes(isMogg),
  miloReq: net.filter(n=>isMilo(n.url)).length,
  miloBytes: sumBytes(isMilo),
  bundleReq: net.filter(n=>isBundle(n.url)).length,
  bundleBytes: sumBytes(isBundle),
  failed: net.filter(n=>n.failed).length,
};

// Correlate the worst gap of each cold hover with the network requests in flight
// during it. Maps perf.now (raf gaps) <-> epoch (CDP net) via `anchor`.
function gapNetCorrelation(hoverList) {
  if (!anchor) return [];
  const off = anchor.epoch - anchor.perf; // epoch = perf + off
  return hoverList.map(h => {
    // worst gap end-time in perf.now: find the gap == longest within [p0,p1]
    const gaps = raf.filter(g => g[0] >= h.p0 && g[0] <= h.p1);
    const worst = gaps.reduce((m, g) => (g[1] > (m ? m[1] : 0) ? g : m), null);
    if (!worst) return { idx: h.idx, longest: h.longest, inFlight: [] };
    const gapEndEpoch = worst[0] + off;
    const gapStartEpoch = gapEndEpoch - worst[1];
    // requests whose [t0(start)..epoch(finish)] overlaps the gap window
    const inFlight = net.filter(n => {
      const fin = n.epoch; const start = fin - (n.dur||0);
      return start <= gapEndEpoch && fin >= gapStartEpoch;
    }).map(n => ({ url: n.url.replace(/^https?:\/\/[^/]+/, ''), range: n.range, bytes: n.bytes, dur: n.dur }));
    return { idx: h.idx, song: h.song, worstGapMs: worst[1], gapEndEpoch, inFlight: inFlight.slice(0, 8) };
  });
}
const hoverGapNet = gapNetCorrelation(hovers);

const result = {
  label: LABEL, run: RUN, MBPS, RTT, build: RELEASE?'release':'debug',
  appBootedSec: ms.appBooted !== undefined ? +((ms.appBooted - wf.perf0)/1000).toFixed(2) : null,
  screenFirstSec: Object.fromEntries(Object.entries(sf).map(([k,v]) => [k, +((v - wf.perf0)/1000).toFixed(2)])),
  phases, hovers, startSong, hoverGapNet, netSummary,
};

writeFileSync(`${OUT}/result.json`, JSON.stringify(result, null, 2));
writeFileSync(`${OUT}/raf.json`, JSON.stringify({ perf0: wf.perf0, raf, screenFirst: sf, milestones: ms }));
writeFileSync(`${OUT}/net.ndjson`, net.map(n => JSON.stringify(n)).join('\n') + '\n');
writeFileSync(`${OUT}/console.ndjson`, cons.map(e => JSON.stringify(e)).join('\n') + '\n');
writeFileSync(`${OUT}/trace.jsonl`, traceText);
console.log(`[${LABEL} run${RUN}] DONE  appBooted=${result.appBootedSec}s  net=${netSummary.totalReq}req/${(netSummary.totalBytes/1e6).toFixed(1)}MB  moggRange=${netSummary.moggRangeReq}`);
console.log(JSON.stringify(result, null, 2));
await Promise.race([browser.close(), sleep(3000)]);
process.exit(0);
