import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve } from 'path';
const PORT = 8766;
const OUT = '/tmp/sustain-shots';
mkdirSync(OUT, { recursive: true });
const getScreen = (p) => p.evaluate(() => window.rb3CurrentScreen || '');
const browser = await chromium.launch({ headless: !process.env.DISPLAY, args:['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan','--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration','--ozone-platform=x11','--mute-audio'] });
const ctx = await browser.newContext({ viewport:{width:1280,height:720} });
const page = await ctx.newPage();
page.on('console', m=>{ const t=m.text(); if(/TAIL/.test(t)) console.log(t); });
await page.goto(`http://127.0.0.1:${PORT}/`, { waitUntil:'domcontentloaded', timeout:30000 });
let dl=Date.now()+300000;
while(Date.now()<dl){ if(await page.evaluate(()=>window.rb3AppBooted||0)>=1) break; await new Promise(r=>setTimeout(r,500)); }
dl=Date.now()+180000;
while(Date.now()<dl){ if(await getScreen(page)==='splash_screen') break; await new Promise(r=>setTimeout(r,500)); }
await new Promise(r=>setTimeout(r,2000));
await page.locator('#rb3-canvas').click({force:true});
await page.keyboard.press('Space');
const waitS = async (targets, to=30000)=>{ const d=Date.now()+to; while(Date.now()<d){ const s=await getScreen(page); if(targets.includes(s)) return s; await new Promise(r=>setTimeout(r,250)); } return await getScreen(page); };
for(let i=0;i<6;i++){ if(await getScreen(page)==='main_hub_screen')break; await page.keyboard.press('Enter'); await new Promise(r=>setTimeout(r,1500)); }
await waitS(['main_hub_screen']);
for(let i=0;i<5;i++){ await page.keyboard.press('Enter'); await new Promise(r=>setTimeout(r,1500)); const c=await getScreen(page); if(c!=='main_hub_screen')break; }
await waitS(['song_select_screen','song_select_enter_screen']);
if(await getScreen(page)==='song_select_enter_screen') await waitS(['song_select_screen']);
await new Promise(r=>setTimeout(r,3000));
await page.evaluate(()=>{ window.rb3WebTargetSong='20thcenturyboy'; });
for(let i=0;i<4;i++){ await page.keyboard.down('Enter'); await new Promise(r=>setTimeout(r,120)); await page.keyboard.up('Enter'); const s=await waitS(['part_difficulty_screen'],12000); if(s==='part_difficulty_screen')break; await new Promise(r=>setTimeout(r,1500)); }
await waitS(['part_difficulty_screen']);
for(let i=0;i<5;i++){ await page.keyboard.down('Enter'); await new Promise(r=>setTimeout(r,150)); await page.keyboard.up('Enter'); await new Promise(r=>setTimeout(r,1200)); if(await getScreen(page)!=='part_difficulty_screen')break; }
await waitS(['game_screen'],240000);
console.log('GAME REACHED at', ((Date.now())/1000)|0);
// snap every 1.5s for 60s
for(let k=0;k<40;k++){
  await new Promise(r=>setTimeout(r,1500));
  const s=await getScreen(page);
  const path=resolve(OUT, `s_${String(k).padStart(2,'0')}_${s}.png`);
  try{ await page.locator('#rb3-canvas').screenshot({path}); }catch(e){}
}
console.log('done');
await browser.close();
