#!/usr/bin/env node
/**
 * _w3m1-analyze.mjs — decompose a frame-trace JSONL by counter for M1 verdicts.
 *
 * Windows the trace by screen (`scr`) and reports, for the song-start window
 * (game_screen + the tail of song_select where the prime fires) and the
 * splash->hub window, every frame >16ms decomposed by counter:
 *   dt = lp + lpu + fetchMs + dtaMs + objMs + primeMs + texMs + meshMs + pipeMs + inflMs + residue
 *
 * t10_go  = primeMs > 16ms in ANY frame whose window includes song start
 * a5_go   = splash->hub long frame(s) dominated by pipe/mesh/tex creation, >100ms class
 *
 * Usage: node _w3m1-analyze.mjs <trace.jsonl> [label]
 */
import { readFileSync } from 'fs';

const path = process.argv[2];
const LABEL = process.argv[3] || '';
const lines = readFileSync(path, 'utf8').split('\n');
const rows = [];
for (const ln of lines) {
  const s = ln.trim();
  if (!s || s.startsWith('#') || s.startsWith('ERR') || s.startsWith('EVAL')) continue;
  try { rows.push(JSON.parse(s)); } catch {}
}

const CTR = ['lp','lpu','fetchMs','dtaMs','objMs','primeMs','texMs','meshMs','pipeMs','inflMs'];
const g = (r,k) => +(r[k]||0);
function residue(r) {
  let s = g(r,'dt');
  for (const k of CTR) s -= g(r,k);
  return +s.toFixed(2);
}
function decomp(r) {
  const parts = [];
  for (const k of CTR) { const v = g(r,k); if (v >= 0.5) parts.push(`${k}=${v.toFixed(1)}`); }
  const res = residue(r);
  parts.push(`residue=${res.toFixed(1)}`);
  let extra = '';
  if (g(r,'texN')) extra += ` texN=${r.texN}`;
  if (g(r,'meshN')) extra += ` meshN=${r.meshN}`;
  if (g(r,'pipeN')) extra += ` pipeN=${r.pipeN}`;
  if (g(r,'st')) extra += ` st=${r.st}`;
  if (g(r,'ld')) extra += ` ld=${r.ld}`;
  if (r.objWNm && r.objWNm !== '') extra += ` worstObj='${r.objWNm}'(${g(r,'objWMs').toFixed(0)}ms)`;
  return { parts: parts.join(' '), extra };
}

console.log(`\n#### ${LABEL} ####  total frames=${rows.length}`);
if (!rows.length) { console.log('NO ROWS'); process.exit(0); }

// screen timeline (first frame index per screen-run)
let prev = null; const timeline = [];
for (const r of rows) { if (r.scr !== prev) { timeline.push([r.f, r.scr]); prev = r.scr; } }
console.log('SCREEN TIMELINE (frame -> screen):');
for (const [f, scr] of timeline) console.log(`  f=${f}  ${scr}`);

// -------- general worst frames over the whole trace --------
function topN(list, n) { return list.slice().sort((a,b)=>g(b,'dt')-g(a,'dt')).slice(0,n); }
console.log('\nGLOBAL WORST 20 FRAMES (dt desc):');
for (const r of topN(rows, 20)) {
  const d = decomp(r);
  console.log(`  f=${String(r.f).padStart(6)} scr=${(r.scr||'?').padEnd(20)} dt=${g(r,'dt').toFixed(1).padStart(7)}  ${d.parts}${d.extra}`);
}

// -------- counter maxima across whole trace --------
console.log('\nCOUNTER MAXIMA (worst single frame per counter):');
for (const k of ['primeMs','pipeMs','meshMs','texMs','dtaMs','objMs','fetchMs','inflMs','lpu','lp']) {
  let best = null;
  for (const r of rows) if (!best || g(r,k) > g(best,k)) best = r;
  console.log(`  ${k.padEnd(8)} max=${g(best,k).toFixed(1).padStart(8)} ms  @f=${best.f} scr=${best.scr}  (dt=${g(best,'dt').toFixed(1)})`);
}

// -------- window helpers --------
function windowByScreens(screens, padBefore=0, padAfter=0) {
  // returns frames whose scr is in `screens`, optionally padded by N frames
  // before the first / after the last matching frame index.
  const idxs = rows.map((r,i)=>[i,r]).filter(([i,r])=>screens.includes(r.scr)).map(([i])=>i);
  if (!idxs.length) return [];
  const lo = Math.max(0, Math.min(...idxs) - padBefore);
  const hi = Math.min(rows.length-1, Math.max(...idxs) + padAfter);
  return rows.slice(lo, hi+1);
}

function reportWindow(name, frames, threshold=16) {
  console.log(`\n===== WINDOW: ${name} =====  (${frames.length} frames)`);
  if (!frames.length) { console.log('  (empty)'); return { longFrames: [], maxPrime: 0, maxCreate: 0, maxDt: 0 }; }
  const longs = frames.filter(r => g(r,'dt') > threshold).sort((a,b)=>g(b,'dt')-g(a,'dt'));
  console.log(`  frames >${threshold}ms: ${longs.length}`);
  for (const r of longs.slice(0, 25)) {
    const d = decomp(r);
    console.log(`    f=${String(r.f).padStart(6)} scr=${(r.scr||'?').padEnd(18)} dt=${g(r,'dt').toFixed(1).padStart(7)}  ${d.parts}${d.extra}`);
  }
  let maxPrime=0, maxCreate=0, maxDt=0, maxDtFrame=null;
  for (const r of frames) {
    maxPrime = Math.max(maxPrime, g(r,'primeMs'));
    maxCreate = Math.max(maxCreate, g(r,'pipeMs')+g(r,'meshMs')+g(r,'texMs'));
    if (g(r,'dt') > maxDt) { maxDt = g(r,'dt'); maxDtFrame = r; }
  }
  console.log(`  SUMMARY: maxDt=${maxDt.toFixed(1)}ms  maxPrimeMs=${maxPrime.toFixed(1)}  max(pipe+mesh+tex)=${maxCreate.toFixed(1)}`);
  if (maxDtFrame) {
    const d = decomp(maxDtFrame);
    console.log(`  WORST FRAME f=${maxDtFrame.f}: ${d.parts}${d.extra}`);
  }
  return { longFrames: longs, maxPrime, maxCreate, maxDt, maxDtFrame };
}

// -------- (a) SONG START window: tail of song_select + meta_loading + game_screen first frames --------
// song start = the frames around entering game_screen where StandardStream::Play() fires.
const gameFrames = rows.map((r,i)=>[i,r]).filter(([i,r])=>r.scr==='game_screen').map(([i])=>i);
let songStart;
if (gameFrames.length) {
  const first = Math.min(...gameFrames);
  // include ~120 frames before game_screen (covers part_difficulty/meta_loading where prime may fire)
  // and the first ~5s of gameplay (~300 frames @60fps, but debug build slower; cap at all game frames present)
  const lo = Math.max(0, first - 150);
  songStart = rows.slice(lo);
} else {
  songStart = windowByScreens(['part_difficulty_screen','meta_loading_screen'], 60, 60);
}
const ssRep = reportWindow('SONG START (pre-game tail + gameplay)', songStart, 16);

// -------- (b) SPLASH -> MAIN_HUB transition --------
// frames from the LAST splash_screen frame through the first ~120 main_hub frames.
const splashIdx = rows.map((r,i)=>[i,r]).filter(([i,r])=>r.scr==='splash_screen').map(([i])=>i);
const hubIdx = rows.map((r,i)=>[i,r]).filter(([i,r])=>r.scr==='main_hub_screen').map(([i])=>i);
let hubWin = [];
if (splashIdx.length && hubIdx.length) {
  const lo = Math.max(0, Math.max(...splashIdx) - 5); // last few splash frames
  const hi = Math.min(rows.length-1, Math.min(...hubIdx) + 150); // first ~150 hub frames
  hubWin = rows.slice(lo, hi+1);
} else {
  hubWin = windowByScreens(['main_hub_screen'], 10, 0);
}
const hubRep = reportWindow('SPLASH -> MAIN_HUB', hubWin, 16);

// -------- dtaMs informational --------
console.log('\n===== dtaMs (informational) =====');
const dtaLongs = rows.filter(r=>g(r,'dtaMs')>0.5).sort((a,b)=>g(b,'dtaMs')-g(a,'dtaMs')).slice(0,12);
if (!dtaLongs.length) console.log('  dtaMs is ZERO on all frames (S1 wire-in did not fire on web, or no main-thread DTA parse seen)');
for (const r of dtaLongs) console.log(`    f=${r.f} scr=${r.scr} dtaMs=${g(r,'dtaMs').toFixed(1)} dt=${g(r,'dt').toFixed(1)}`);

// -------- VERDICTS --------
console.log('\n===== VERDICTS =====');
const t10_go = ssRep.maxPrime > 16;
// a5: hub long frame dominated by creation work and >100ms class
const hubCreateDominant = hubRep.maxDtFrame ? ((g(hubRep.maxDtFrame,'pipeMs')+g(hubRep.maxDtFrame,'meshMs')+g(hubRep.maxDtFrame,'texMs')) > 0.5*g(hubRep.maxDtFrame,'dt')) : false;
const a5_go = hubRep.maxDt > 100 && (hubCreateDominant || hubRep.maxCreate > 50);
console.log(`  t10_go = ${t10_go}   (song-start maxPrimeMs=${ssRep.maxPrime.toFixed(1)}ms; >16 => true)`);
console.log(`  a5_go  = ${a5_go}    (hub maxDt=${hubRep.maxDt.toFixed(1)}ms, max(pipe+mesh+tex)=${hubRep.maxCreate.toFixed(1)}ms, createDominant=${hubCreateDominant})`);
