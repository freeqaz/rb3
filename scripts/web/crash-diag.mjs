import { chromium } from 'playwright';
import { appendFileSync, writeFileSync } from 'fs';
const milo = 'ui/track/gen/tracksystem.milo_xbox';
const lf='/tmp/rb3-crash-diag.log'; writeFileSync(lf,'');
const log=s=>{appendFileSync(lf,s+'\n');process.stdout.write(s+'\n');};
const browser = await chromium.launch({ headless:true, args:['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan','--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration','--ozone-platform=x11','--mute-audio','--js-flags=--max-old-space-size=4096'] });
const page = await (await browser.newContext({viewport:{width:1280,height:720}})).newPage();
const t0=Date.now(); const el=()=>((Date.now()-t0)/1000).toFixed(1);
page.on('console', m=>{ const t=m.text(); if(/abort|OOM|enlarge|memory|RangeError|Cannot|RuntimeError|unreachable|trap|exception|Maximum|wasm/i.test(t)) log(`[${el()}s ${m.type()}] ${t}`); });
page.on('pageerror', e=>log(`[${el()}s PAGEERR] ${e.message}`));
page.on('crash', ()=>log(`[${el()}s] CRASH`));
// capture CDP crash detail
const client = await page.context().newCDPSession(page);
await client.send('Log.enable').catch(()=>{});
client.on('Log.entryAdded', e=>log(`[${el()}s CDPLOG ${e.entry.level}] ${e.entry.text}`));
await page.goto(`http://127.0.0.1:8421/?milo=${encodeURIComponent(milo)}`,{waitUntil:'domcontentloaded',timeout:30000});
let crashed=false;
for(let i=0;i<30;i++){ await new Promise(r=>setTimeout(r,2000)); const ok=await page.evaluate(()=>1).catch(()=>{crashed=true;return null}); if(crashed){log(`[${el()}s] page unreachable`);break;} }
await browser.close().catch(()=>{});
log('DIAG DONE');
