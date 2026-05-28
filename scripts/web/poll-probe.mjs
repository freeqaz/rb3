// Live load-progress probe: writes each sample to a log file immediately so
// progress is visible even while node buffers stdout. Usage:
//   node poll-probe.mjs <milo-path> <logfile> [maxSeconds] [screenshotPath]
import { chromium } from 'playwright';
import { appendFileSync, writeFileSync } from 'fs';
const milo = process.argv[2] || 'ui/track/gen/tracksystem.milo_xbox';
const logfile = process.argv[3] || '/tmp/rb3-poll.log';
const maxSec = parseInt(process.argv[4] || '180', 10);
const shotPath = process.argv[5] || '/tmp/rb3-canvas.png';
writeFileSync(logfile, '');
const log = (s) => { const line = s + '\n'; appendFileSync(logfile, line); process.stdout.write(line); };
const browser = await chromium.launch({ headless: true, args: ['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan','--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration','--ozone-platform=x11','--mute-audio'] });
const page = await (await browser.newContext({ viewport:{width:1280,height:720} })).newPage();
const t0=Date.now(); const el=()=>((Date.now()-t0)/1000).toFixed(1);
page.on('console', m=>{ const t=m.text(); if(/yielding load done|milo loaded|render loop|DirLoader returned null|no drawable|boot error/.test(t)) log(`[${el()}s] ${t}`); });
page.on('crash', ()=>log(`[${el()}s] CRASH`));
await page.goto(`http://127.0.0.1:8421/?milo=${encodeURIComponent(milo)}`,{waitUntil:'domcontentloaded',timeout:30000});
let done=false;
for(let i=0;i<maxSec/2;i++){
  await new Promise(r=>setTimeout(r,2000));
  const s=await page.evaluate(()=>({p:window.rb3LoadPolls||0,m:window.rb3MilosLoaded||0,f:window.rb3FrameCount||0})).catch(()=>null);
  if(!s){log(`[${el()}s] (crashed/unreachable)`);break;}
  log(`[${el()}s] polls=${s.p} milos=${s.m} frames=${s.f}`);
  if(s.m>=1&&s.f>=30){done=true;break;}
}
if(done){
  try { await page.locator('#rb3-canvas').screenshot({ path: shotPath }); log(`[${el()}s] screenshot saved ${shotPath}`); } catch(e){ log(`screenshot fail: ${e.message}`); }
}
await browser.close();
log(`DONE ok=${done}`);
