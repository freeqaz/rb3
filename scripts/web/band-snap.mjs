// Rapid-snap probe: navigate to gameplay, then snap a screenshot every 0.7s for
// ~30s, capturing many camera shots so at least some frame the band.
import { chromium } from 'playwright';
import { mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
const __dirname = dirname(fileURLToPath(import.meta.url));
const port = parseInt(process.argv[2]||'8834',10);
const OUT = resolve(__dirname,'results/band-snap'); mkdirSync(OUT,{recursive:true});
const browser = await chromium.launch({ headless: !process.env.DISPLAY,
  args:['--no-sandbox','--enable-unsafe-webgpu','--use-angle=vulkan',
        '--enable-features=Vulkan,VulkanFromANGLE,WebAssemblyJSPromiseIntegration',
        '--ozone-platform=x11','--mute-audio'] });
const ctx = await browser.newContext({ viewport:{width:1280,height:720} });
const page = await ctx.newPage();
const skinByFrame = {};
page.on('console', m=>{ const t=m.text(); const mm=t.match(/DIAG_SKIN frame=(\d+) totalDraws=\d+ skinnedDraws=(\d+)/); if(mm) skinByFrame[mm[1]]=parseInt(mm[2]); });
const getScreen=p=>p.evaluate(()=>window.rb3CurrentScreen||'');
const getFrame=p=>p.evaluate(()=>window.rb3FrameCount||0);
await page.goto(`http://127.0.0.1:${port}/`,{waitUntil:'domcontentloaded',timeout:30000});
let booted=0,dl=Date.now()+120000; while(Date.now()<dl){booted=await page.evaluate(()=>window.rb3AppBooted||0);if(booted>=1)break;await new Promise(r=>setTimeout(r,500));}
let s=''; dl=Date.now()+60000; while(Date.now()<dl){s=await getScreen(page);if(s==='splash_screen')break;await new Promise(r=>setTimeout(r,400));}
await new Promise(r=>setTimeout(r,2000));
await page.locator('#rb3-canvas').click({force:true});
await page.keyboard.press('Space');
for(let i=0;i<8&&s==='splash_screen';i++){await page.keyboard.press('Enter');await new Promise(r=>setTimeout(r,1000));s=await getScreen(page);}
// main_hub -> song_select
dl=Date.now()+30000; while(Date.now()<dl){s=await getScreen(page);if(s==='main_hub_screen')break;await new Promise(r=>setTimeout(r,300));}
await new Promise(r=>setTimeout(r,2500));
for(let i=0;i<6;i++){await page.keyboard.press('Enter');await new Promise(r=>setTimeout(r,1200));s=await getScreen(page);if(s.startsWith('song_select'))break;}
dl=Date.now()+30000; while(Date.now()<dl){s=await getScreen(page);if(s==='song_select_screen')break;await new Promise(r=>setTimeout(r,300));}
await page.evaluate(()=>{window.rb3WebTargetSong='20thcenturyboy';});
await new Promise(r=>setTimeout(r,1500));
for(let i=0;i<4;i++){await page.keyboard.down('Enter');await new Promise(r=>setTimeout(r,120));await page.keyboard.up('Enter');await new Promise(r=>setTimeout(r,1500));s=await getScreen(page);if(s==='part_difficulty_screen')break;}
dl=Date.now()+30000; while(Date.now()<dl){s=await getScreen(page);if(s==='part_difficulty_screen')break;await new Promise(r=>setTimeout(r,300));}
await new Promise(r=>setTimeout(r,2000));
for(let i=0;i<5;i++){await page.keyboard.down('Enter');await new Promise(r=>setTimeout(r,150));await page.keyboard.up('Enter');await new Promise(r=>setTimeout(r,1200));s=await getScreen(page);if(s!=='part_difficulty_screen')break;}
// wait for game/tv3
dl=Date.now()+240000; while(Date.now()<dl){s=await getScreen(page);if(s==='game_screen'||s.startsWith('tv3'))break;await new Promise(r=>setTimeout(r,400));}
console.log('reached:',s,'frame',await getFrame(page));
// rapid snap
for(let k=0;k<45;k++){
  const f=await getFrame(page);
  await page.locator('#rb3-canvas').screenshot({path:resolve(OUT,`snap_${String(k).padStart(2,'0')}_f${f}.png`)});
  await new Promise(r=>setTimeout(r,700));
}
// report skin by frame around snaps
const frames=Object.keys(skinByFrame).map(Number).sort((a,b)=>a-b);
const maxF=frames.length?frames[frames.length-1]:0;
let withChars=0,total=0; for(const fr of frames){total++; if(skinByFrame[fr]>0)withChars++;}
console.log(`skin frames: ${total}, withChars(skin>0): ${withChars} (${(100*withChars/Math.max(1,total)).toFixed(0)}%), maxFrame=${maxF}`);
console.log('sample skin>40 frames:', frames.filter(fr=>skinByFrame[fr]>40).slice(0,10).map(fr=>`${fr}:${skinByFrame[fr]}`).join(' '));
await browser.close();
