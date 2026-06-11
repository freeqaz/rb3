// Wave-4 flag A/B opt-out gate. For each --env flagset, boot -> song_select and
// assert no pageerror / WASM trap. Usage:
//   node _w4-optout-check.mjs --port 8448 --env "RB3_LOADER_READAHEAD=0" --label readahead_off
import { launchBrowser, createCapture, navigateTo, engineState, cleanup, SCREENS, waitForServer } from './lib/core.mjs';

const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(`--${k}`); return i >= 0 ? argv[i+1] : d; };
const PORT = parseInt(arg('port', '8448'), 10);
const ENV = arg('env', '');
const LABEL = arg('label', 'optout');

const query = `env=${encodeURIComponent(ENV)}`;
await waitForServer(PORT);
const { browser, page } = await launchBrowser(PORT, { query });
const cap = createCapture(page);
let ok = false, err = null;
try {
  const reached = await navigateTo(page, cap, SCREENS.SONG_SELECT);
  const st = await engineState(page);
  const songCount = await page.evaluate(() => window.rb3SongCount || 0).catch(() => 0);
  const pageErrors = (cap.errors || []).length;
  const trap = (cap.logs || []).filter(m => /function signature mismatch|call_indirect|RuntimeError|abort\(/.test(typeof m === 'string' ? m : (m.text||''))).length;
  ok = reached === SCREENS.SONG_SELECT && pageErrors === 0 && trap === 0 && songCount > 0;
  console.log(JSON.stringify({ label: LABEL, env: ENV, reached, songCount, pageErrors, trap, ok }));
} catch (e) {
  err = String(e);
  console.log(JSON.stringify({ label: LABEL, env: ENV, ok: false, error: err }));
} finally {
  await cleanup(browser);
}
process.exit(ok ? 0 : 1);
