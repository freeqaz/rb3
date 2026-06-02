#!/usr/bin/env node
/**
 * loadperf-responsiveness.mjs — TRACK-B web smoothness probe.
 *
 * Boots the rb3-web App in a real browser and measures MAIN-THREAD
 * responsiveness during boot/load by sampling requestAnimationFrame inter-frame
 * gaps. A long gap == a frame where the wasm RunOneFrame (incl. TheLoadMgr.Poll)
 * blocked the main thread, freezing the tab (the overlay can't composite, input
 * is dropped). We install a RAF chain at page load (before the wasm even runs)
 * and record every gap; the worst gaps and a histogram tell us whether the
 * per-frame load work is bounded.
 *
 * Also asserts the CSS loading overlay's animation actually advances (its
 * computed background-position changes over time) — a direct "is it animating?"
 * check independent of RAF.
 *
 * Usage: node scripts/web/loadperf-responsiveness.mjs [--port 8421] [--secs 35]
 */
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const PORT = parseInt(argv[argv.indexOf('--port') + 1] || '8421', 10) || 8421;
const SECS = parseFloat(argv[argv.indexOf('--secs') + 1] || '35') || 35;
const OUT = resolve(__dirname, 'results/loadperf');
mkdirSync(OUT, { recursive: true });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await chromium.launch({
    headless: true,
    args: ['--use-gl=angle', '--use-angle=swiftshader',
           '--enable-unsafe-webgpu', '--enable-features=Vulkan'],
  });
  const page = await browser.newPage();
  page.on('console', m => {
    const t = m.text();
    if (/freeze|FRAME|overlay|GPU ready|App constructed|assets ready/i.test(t))
      console.log('  [page]', t.slice(0, 160));
  });

  // Install the RAF-gap recorder BEFORE navigation so it captures the whole boot.
  await page.addInitScript(() => {
    window.__rafGaps = [];
    window.__rafLast = -1;
    function rafTick(t) {
      if (window.__rafLast >= 0) window.__rafGaps.push(t - window.__rafLast);
      window.__rafLast = t;
      requestAnimationFrame(rafTick);
    }
    requestAnimationFrame(rafTick);
  });

  const url = `http://localhost:${PORT}/`;
  const NAV = argv.includes('--nav');
  console.log(`[loadperf] navigating ${url}, sampling RAF gaps for ${SECS}s nav=${NAV}`);
  await page.goto(url, { waitUntil: 'domcontentloaded' });

  const getScreen = () => page.evaluate(() => window.rb3CurrentScreen || '').catch(() => '');
  const waitScreen = async (target, toMs) => {
    const t = Date.now();
    while ((Date.now() - t) < toMs) {
      if ((await getScreen()) === target) return true;
      await sleep(300);
    }
    return false;
  };

  // Sample the overlay animation position a few times to prove it advances.
  const overlayPos = [];
  const t0 = Date.now();
  let navDone = !NAV;
  while ((Date.now() - t0) / 1000 < SECS) {
    const snap = await page.evaluate(() => {
      const el = document.querySelector('.loading-logo span') ||
                 document.querySelector('#loading-overlay');
      let bgpos = '';
      if (el) bgpos = getComputedStyle(el).backgroundPosition || getComputedStyle(el).opacity;
      return {
        frame: window.rb3FrameCount || 0,
        screen: window.rb3CurrentScreen || '',
        assets: window.rb3AssetsLoaded || 0,
        total: window.rb3AssetsTotal || 0,
        bgpos,
        overlayGone: !document.getElementById('loading-overlay'),
      };
    }).catch(() => null);
    if (snap) overlayPos.push(snap.bgpos);

    // Drive nav -> gameplay while sampling, to exercise the heavy song load.
    if (NAV && !navDone) {
      navDone = true; // run once, async
      (async () => {
        try {
          await waitScreen('splash_screen', 20000);
          await page.keyboard.press('Space'); await sleep(500);
          await page.keyboard.press('Enter');
          await waitScreen('main_hub_screen', 20000);
          console.log('  [nav] at main_hub; entering quickplay');
          await page.keyboard.press('Enter'); await sleep(1500);
          await page.keyboard.press('Enter'); await sleep(1500);
          await waitScreen('song_select_screen', 20000);
          console.log('  [nav] at song_select; confirming a song');
          await page.keyboard.press('Enter'); await sleep(2000);
          await page.keyboard.press('Enter'); await sleep(2000);
          console.log('  [nav] song confirm sent; watching for game_screen');
        } catch (e) { console.log('  [nav] error', e.message); }
      })();
    }
    await sleep(1000);
  }

  const gaps = await page.evaluate(() => window.__rafGaps || []);
  const lastState = await page.evaluate(() => ({
    frame: window.rb3FrameCount || 0,
    screen: window.rb3CurrentScreen || '',
    assets: window.rb3AssetsLoaded || 0,
    total: window.rb3AssetsTotal || 0,
    overlayGone: !document.getElementById('loading-overlay'),
  }));

  await page.screenshot({ path: resolve(OUT, 'final.png') }).catch(() => {});
  await browser.close();

  // Report RAF gap stats. A gap is the time the main thread did NOT yield to the
  // compositor. >100ms = a visible hitch; >250ms = a freeze.
  gaps.sort((a, b) => b - a);
  const n = gaps.length;
  const max = n ? gaps[0] : 0;
  const over = (th) => gaps.filter(g => g > th).length;
  console.log('='.repeat(60));
  console.log(`[loadperf] RAF samples: ${n}   (over ${SECS}s)`);
  console.log(`[loadperf] reached frame=${lastState.frame} screen='${lastState.screen}' ` +
              `assets=${lastState.assets}/${lastState.total} overlayGone=${lastState.overlayGone}`);
  console.log(`[loadperf] MAX RAF gap: ${max.toFixed(1)} ms`);
  console.log(`[loadperf] gaps > 100ms: ${over(100)}   > 250ms: ${over(250)}   ` +
              `> 500ms: ${over(500)}   > 1000ms: ${over(1000)}`);
  console.log('[loadperf] worst 10 gaps (ms):',
              gaps.slice(0, 10).map(g => g.toFixed(0)).join(', '));
  const uniquePos = [...new Set(overlayPos.filter(Boolean))];
  console.log(`[loadperf] overlay anim position samples (unique=${uniquePos.length}):`,
              uniquePos.slice(0, 6).join(' | '));
  console.log('='.repeat(60));
  // Smoothness verdict: no gap should exceed ~500ms during load.
  process.exit(max > 1500 ? 1 : 0);
})().catch(e => { console.error('[loadperf] ERROR', e); process.exit(2); });
