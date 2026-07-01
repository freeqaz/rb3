// GPU-op interposition repro (NO rebuild). Uses page.addInitScript to wrap
// navigator.gpu.requestAdapter / adapter.requestDevice BEFORE any page JS runs,
// then wraps device.createBuffer/createTexture/createBindGroup/createBindGroupLayout/
// createRenderPipeline + queue.submit + queue.writeBuffer/writeTexture to:
//   - install an 'uncapturederror' listener (logs WGPU-UNCAPTURED ...)
//   - keep a permanent pushErrorScope('validation')/('out-of-memory')
//   - count + log every op with its size/label, catch exceptions
//   - record the LAST successful + FIRST failing GPU op with running counts.
//
// All instrumentation logs to console with a 'WGPU-OP' / 'WGPU-FAIL' / 'WGPU-UNCAPTURED'
// prefix, captured by the Node side and dumped at the end (or at crash).
//
// Run with the bash sandbox DISABLED for real GPU.
import { chromium } from 'playwright';
import { writeFileSync } from 'fs';

const PORT = parseInt(process.env.PORT || '8421', 10);
const QUERY = process.env.QUERY || '';
const OUT = process.env.OUT || '/tmp/rb3-web/wgpu-interpose';
const RUN = process.env.RUN || '0';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const INIT = `
(() => {
  if (!navigator.gpu) return;
  const W = (window.__wgpu = { ops: 0, buffers: 0, textures: 0, bindGroups: 0,
    bgls: 0, pipelines: 0, submits: 0, bufBytes: 0, texBytes: 0,
    lastOk: null, firstFail: null, biggestBuf: 0, biggestTex: 0,
    bufByLabel: {}, texByLabel: {}, errors: [] });
  const log = (...a) => { try { console.log(...a); } catch(e){} };

  function fmtBuf(d) {
    return { label: d && d.label || '', size: d && d.size, usage: d && d.usage };
  }
  function texBytes(d) {
    if (!d || !d.size) return 0;
    const s = d.size;
    const w = s.width || s[0] || 0, h = s.height || s[1] || 1, dl = s.depthOrArrayLayers || s[2] || 1;
    // approx bytes (assume 4bpp unless format hints otherwise)
    let bpp = 4;
    const f = (d.format||'')+'';
    if (f.includes('rgba16')) bpp = 8;
    else if (f.includes('rgba32')) bpp = 16;
    else if (f.includes('depth')) bpp = 4;
    else if (f.includes('bc') || f.includes('etc') || f.includes('astc')) bpp = 1;
    else if (f.includes('r8')) bpp = 1;
    return w*h*dl*bpp;
  }

  function wrapDevice(dev) {
    if (!dev || dev.__wrapped) return dev;
    dev.__wrapped = true;
    try {
      dev.addEventListener('uncapturederror', (e) => {
        const msg = (e.error && e.error.message) || String(e.error);
        log('WGPU-UNCAPTURED', (e.error && e.error.constructor && e.error.constructor.name)||'', msg.slice(0,400));
        W.errors.push('UNCAPTURED: ' + msg.slice(0,400));
      });
    } catch(e) {}
    // NOTE: we deliberately do NOT keep a permanent open error scope — an open
    // scope CAPTURES the error and prevents it from firing 'uncapturederror',
    // which would hide it. Instead we rely on the 'uncapturederror' listener
    // (above) to surface every device-level validation / oom error, plus
    // try/catch around each create call for synchronous throws.

    const wrapCreate = (name, kind, sizeFn) => {
      const orig = dev[name];
      if (typeof orig !== 'function') return;
      dev[name] = function(desc) {
        W.ops++;
        const label = (desc && desc.label) || '';
        let sz = 0; try { sz = sizeFn ? sizeFn(desc) : 0; } catch(e){}
        const opRec = { op: name, kind, label, size: sz, n: W.ops };
        let res;
        try {
          res = orig.call(this, desc);
        } catch (err) {
          opRec.error = String(err && err.message || err);
          if (!W.firstFail) W.firstFail = opRec;
          log('WGPU-FAIL', JSON.stringify(opRec));
          W.errors.push('THROW ' + name + ': ' + opRec.error);
          throw err;
        }
        // tally
        if (kind === 'buffer') { W.buffers++; W.bufBytes += sz; if (sz>W.biggestBuf) W.biggestBuf=sz; W.bufByLabel[label]=(W.bufByLabel[label]||0)+1; }
        if (kind === 'texture') { W.textures++; W.texBytes += sz; if (sz>W.biggestTex) W.biggestTex=sz; W.texByLabel[label]=(W.texByLabel[label]||0)+1; }
        if (kind === 'bindGroup') W.bindGroups++;
        if (kind === 'bgl') W.bgls++;
        if (kind === 'pipeline') W.pipelines++;
        W.lastOk = opRec;
        // Log only notable ops to avoid flooding: big buffers/textures, or every 200th op
        if (sz > 4*1024*1024 || (W.ops % 250 === 0)) {
          log('WGPU-OP', JSON.stringify({ ...opRec, bytesMB: +(sz/1048576).toFixed(2),
            tot: { buf: W.buffers, tex: W.textures, bg: W.bindGroups, bgl: W.bgls, pl: W.pipelines,
                   bufMB: +(W.bufBytes/1048576).toFixed(1), texMB: +(W.texBytes/1048576).toFixed(1) } }));
        }
        return res;
      };
    };

    wrapCreate('createBuffer', 'buffer', (d) => Number(d && d.size || 0));
    wrapCreate('createTexture', 'texture', texBytes);
    wrapCreate('createBindGroup', 'bindGroup', null);
    wrapCreate('createBindGroupLayout', 'bgl', null);
    wrapCreate('createRenderPipeline', 'pipeline', null);

    // queue wrap
    try {
      const q = dev.queue;
      if (q && !q.__wrapped) {
        q.__wrapped = true;
        const oSubmit = q.submit;
        q.submit = function(cmds) { W.submits++; try { return oSubmit.call(this, cmds); }
          catch(err){ log('WGPU-FAIL', JSON.stringify({op:'submit', error:String(err&&err.message||err), n:W.submits})); throw err; } };
        const oWB = q.writeBuffer;
        q.writeBuffer = function(buf, off, data, doff, sz) {
          try { return oWB.apply(this, arguments); }
          catch(err){ log('WGPU-FAIL', JSON.stringify({op:'writeBuffer', error:String(err&&err.message||err)})); throw err; } };
      }
    } catch(e){}

    // expose a snapshot getter
    window.__wgpuSnapshot = () => JSON.parse(JSON.stringify({
      ops: W.ops, buffers: W.buffers, textures: W.textures, bindGroups: W.bindGroups,
      bgls: W.bgls, pipelines: W.pipelines, submits: W.submits,
      bufMB: +(W.bufBytes/1048576).toFixed(1), texMB: +(W.texBytes/1048576).toFixed(1),
      biggestBufMB: +(W.biggestBuf/1048576).toFixed(2), biggestTexMB: +(W.biggestTex/1048576).toFixed(2),
      lastOk: W.lastOk, firstFail: W.firstFail,
      bufByLabel: W.bufByLabel, texByLabel: W.texByLabel,
      errors: W.errors.slice(-30),
    }));
    log('WGPU-WRAP installed on device');
  }

  const gpu = navigator.gpu;
  const origReqAdapter = gpu.requestAdapter.bind(gpu);
  gpu.requestAdapter = async function(opts) {
    const a = await origReqAdapter(opts);
    if (a && !a.__wrapped) {
      a.__wrapped = true;
      const origReqDevice = a.requestDevice.bind(a);
      a.requestDevice = async function(desc) {
        log('WGPU-REQDEVICE', JSON.stringify({ requiredLimits: desc && desc.requiredLimits || null,
          features: desc && desc.requiredFeatures || null }));
        const dev = await origReqDevice(desc);
        wrapDevice(dev);
        return dev;
      };
    }
    return a;
  };
})();
`;

(async () => {
  const browser = await chromium.launch({
    headless: true,
    args: [
      '--enable-unsafe-webgpu',
      '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
      '--use-angle=vulkan',
      '--no-sandbox',
      '--enable-dawn-features=allow_unsafe_apis',
    ],
  });
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 720 } });
  const page = await ctx.newPage();
  await page.addInitScript(INIT);

  const gpuLog = [];
  page.on('console', m => {
    const t = m.text();
    if (/^WGPU-|device.*lost|uncaptured|validation|out of memory|out-of-memory/i.test(t))
      gpuLog.push(`[${m.type()}] ${t}`.slice(0, 500));
  });
  let crashed = false;
  page.on('crash', () => { crashed = true; gpuLog.push('*** page.on(crash) FIRED ***'); });
  page.on('pageerror', e => gpuLog.push('PAGEERROR: ' + e.message.slice(0, 300)));

  const url = `http://localhost:${PORT}/` + (QUERY ? `?${QUERY}` : '');
  await page.goto(url, { waitUntil: 'domcontentloaded' }).catch(e => gpuLog.push('goto: ' + e.message));

  const screenOf = () => page.evaluate(() => window.rb3CurrentScreen || '').catch(() => 'ERR');
  const snap = () => page.evaluate(() => (window.__wgpuSnapshot ? window.__wgpuSnapshot() : null)).catch(() => null);
  const press = async (k) => { try { await page.keyboard.down(k); await sleep(220); await page.keyboard.up(k); await sleep(180); } catch {} };

  for (let i = 0; i < 200; i++) { const s = await screenOf(); if (['intro_movie_screen','splash_screen','main_hub_screen'].includes(s)) break; await sleep(500); }
  try { await page.locator('#rb3-canvas').click({ force: true }); } catch {}
  for (let i = 0; i < 18 && (await screenOf()) !== 'main_hub_screen' && !crashed; i++) await press('Space');
  const hub = await screenOf();
  const snapHub = await snap();   // resource counts at main_hub (before song_select)

  // Now the dangerous transition: main_hub -> song_select.
  for (let i = 0; i < 14 && !crashed && (await screenOf()) !== 'song_select_screen'; i++) await press('Enter');
  for (let i = 0; i < 16 && !crashed; i++) await sleep(500);
  const snapAfter = crashed ? null : await snap();   // counts after (if it survived)
  const finalScreen = crashed ? '(CRASHED)' : await screenOf();

  await sleep(400);
  const result = {
    run: RUN, query: QUERY, crashed, reachedHub: hub, finalScreen,
    snapshotAtHub: snapHub, snapshotAfter: snapAfter,
    gpuLog: gpuLog,
  };
  console.log(JSON.stringify(result, null, 2));
  try { writeFileSync(`${OUT}-run${RUN}.json`, JSON.stringify(result, null, 2)); } catch {}
  await browser.close().catch(() => {});
  process.exit(0);
})().catch(e => { console.error('ERR', e.message, e.stack); process.exit(2); });
