// Verify band animation: navigate to gameplay, then capture 6 screenshots ~0.25s
// apart (within one camera shot dwell) to confirm character poses change frame to
// frame (animated) rather than frozen (T-pose).
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
const __dirname = dirname(fileURLToPath(import.meta.url));
const port=parseInt(process.argv[2]||'8834',10);
const OUT=resolve(__dirname,'results/anim-verify'); mkdirSync(OUT,{recursive:true});
const browser = await chromium.launch({ headless: !process.env.DISPLAY,
  args:['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11','--mute-audio'] });
const ctx = await browser.newContext({ viewport:{width:1280,height:720} });
const page = await ctx.newPage();
const gs=p=>p.evaluate(()=>window.rb3CurrentScreen||'');
await page.goto(`http://127.0.0.1:${port}/`,{waitUntil:'domcontentloaded',timeout:30000});
let b=0,dl=Date.now()+120000; while(Date.now()<dl){b=await page.evaluate(()=>window.rb3AppBooted||0);if(b>=1)break;await new Promise(r=>setTimeout(r,500));}
let s=''; dl=Date.now()+60000; while(Date.now()<dl){s=await gs(page);if(s==='splash_screen')break;await new Promise(r=>setTimeout(r,400));}
await new Promise(r=>setTimeout(r,2000)); await page.locator('#rb3-canvas').click({force:true}); await page.keyboard.press('Space');
for(let i=0;i<8&&s==='splash_screen';i++){await page.keyboard.press('Enter');await new Promise(r=>setTimeout(r,1000));s=await gs(page);}
dl=Date.now()+30000; while(Date.now()<dl){s=await gs(page);if(s==='main_hub_screen')break;await new Promise(r=>setTimeout(r,300));}
await new Promise(r=>setTimeout(r,2500));
for(let i=0;i<6;i++){await page.keyboard.press('Enter');await new Promise(r=>setTimeout(r,1200));s=await gs(page);if(s.startsWith('song_select'))break;}
dl=Date.now()+30000; while(Date.now()<dl){s=await gs(page);if(s==='song_select_screen')break;await new Promise(r=>setTimeout(r,300));}
await page.evaluate(()=>{window.rb3WebTargetSong='20thcenturyboy';}); await new Promise(r=>setTimeout(r,1500));
for(let i=0;i<4;i++){await page.keyboard.down('Enter');await new Promise(r=>setTimeout(r,120));await page.keyboard.up('Enter');await new Promise(r=>setTimeout(r,1500));s=await gs(page);if(s==='part_difficulty_screen')break;}
dl=Date.now()+30000; while(Date.now()<dl){s=await gs(page);if(s==='part_difficulty_screen')break;await new Promise(r=>setTimeout(r,300));}
await new Promise(r=>setTimeout(r,2000));
for(let i=0;i<5;i++){await page.keyboard.down('Enter');await new Promise(r=>setTimeout(r,150));await page.keyboard.up('Enter');await new Promise(r=>setTimeout(r,1200));s=await gs(page);if(s!=='part_difficulty_screen')break;}
dl=Date.now()+240000; while(Date.now()<dl){s=await gs(page);if(s==='game_screen')break;await new Promise(r=>setTimeout(r,400));}
// stabilize then burst-capture 8 frames 250ms apart
await new Promise(r=>setTimeout(r,8000));
for(let k=0;k<8;k++){
  const f=await page.evaluate(()=>window.rb3FrameCount||0);
  await page.locator('#rb3-canvas').screenshot({path:resolve(OUT,`burst_${k}_f${f}.png`)});
  await new Promise(r=>setTimeout(r,250));
}
console.log('captured burst at screen='+(await gs(page)));
await browser.close();
