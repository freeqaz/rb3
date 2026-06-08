import { launchBrowser, engineState, createCapture } from './lib/core.mjs';

const PORT = 8421;
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const { browser, page } = await launchBrowser(PORT, { query: 'debug=true' });
const cap = createCapture(page, { filter: /Rand\.cpp|THREAD-NOTIFY|NewFile|AudioDevice|Preview|MainThread|FAIL/ });

let randAsserts = 0, lastScreen = '', maxFrame = 0;
page.on('console', m => { if (/Rand\.cpp Line: 111/.test(m.text())) randAsserts++; });

const deadline = Date.now() + 300000;   // 5 min
let closed = false;
page.on('close', () => { closed = true; });
try {
  while (Date.now() < deadline) {
    if (closed) { console.log('PAGE CLOSED (crash)'); break; }
    let s;
    try { s = await engineState(page); } catch (e) { console.log('engineState threw:', e.message); break; }
    if (s.screen !== lastScreen) { console.log(`t=${((Date.now()-(deadline-300000))/1000).toFixed(1)}s screen='${s.screen}' booted=${s.booted} frame=${s.frame} songs=${s.songs} randAsserts=${randAsserts}`); lastScreen = s.screen; }
    if (s.frame > maxFrame) maxFrame = s.frame;
    if (['song_select_screen','main_hub_screen','splash_screen'].includes(s.screen)) {
      console.log(`REACHED ${s.screen} frame=${s.frame} songs=${s.songs}`);
      if (s.screen === 'song_select_screen') break;
    }
    await sleep(1000);
  }
} catch (e) { console.log('LOOP ERROR:', e.message); }
console.log(`FINAL: lastScreen='${lastScreen}' maxFrame=${maxFrame} randAsserts=${randAsserts} closed=${closed}`);
await Promise.race([browser.close(), sleep(3000)]);
process.exit(0);
