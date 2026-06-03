#!/usr/bin/env node
/**
 * analyze-cpuprofile.mjs — summarize a V8 .cpuprofile (from loadperf-profile.mjs
 * or Chrome DevTools) without opening a GUI. Aggregates self-time by function and
 * by category (wasm / JS glue / GC / idle) so you can see where boot time goes.
 *
 * Usage: node scripts/web/analyze-cpuprofile.mjs <boot.cpuprofile> [--top 30] [--grep <substr>]
 */
import { readFileSync } from 'fs';

const argv = process.argv.slice(2);
const file = argv.find(a => !a.startsWith('--'));
const TOP = parseInt((argv[argv.indexOf('--top') + 1]) || '30', 10) || 30;
const GREP = argv.indexOf('--grep') !== -1 ? argv[argv.indexOf('--grep') + 1] : null;
if (!file) { console.error('usage: analyze-cpuprofile.mjs <boot.cpuprofile>'); process.exit(1); }

const prof = JSON.parse(readFileSync(file, 'utf8'));
const nodes = prof.nodes || (prof.head ? flattenHead(prof.head) : []);
const byId = new Map();
for (const n of nodes) byId.set(n.id, n);

// Self time per node from samples + timeDeltas (microseconds).
const selfUs = new Map();
const samples = prof.samples || [];
const deltas = prof.timeDeltas || [];
let totalUs = 0;
for (let i = 0; i < samples.length; i++) {
    const id = samples[i];
    const dt = Math.max(0, deltas[i] || 0);
    totalUs += dt;
    selfUs.set(id, (selfUs.get(id) || 0) + dt);
}

const cat = (frame) => {
    const fn = frame.functionName || '(anonymous)';
    const url = frame.url || '';
    if (fn === '(idle)') return 'idle';
    if (fn === '(program)') return 'program';
    if (fn === '(garbage collector)') return 'gc';
    if (/wasm/.test(url) || /^(\$|wasm-function)/.test(fn) || frame.url === '' && /^[a-z_]/.test(fn) && !url) {
        // CDP names wasm frames with url '' and the demangled C++ name (dev -g2).
    }
    if (/wasm/.test(url)) return 'wasm';
    if (url.endsWith('.wasm')) return 'wasm';
    if (url.endsWith('.js')) return 'js-glue';
    if (!url && fn !== '(root)') return 'wasm'; // dev build: wasm fns carry no url
    return 'other';
};

// Aggregate self-time by function-name and by category.
const fnAgg = new Map();
const catAgg = new Map();
for (const [id, us] of selfUs) {
    const n = byId.get(id);
    if (!n) continue;
    const f = n.callFrame || n; // .cpuprofile (nodes[].callFrame) vs legacy (head)
    const name = (f.functionName || '(anonymous)') + (f.url ? ` @${f.url.split('/').pop()}` : '');
    fnAgg.set(name, (fnAgg.get(name) || 0) + us);
    const c = cat(f);
    catAgg.set(c, (catAgg.get(c) || 0) + us);
}

function flattenHead(head) {
    const out = []; const stack = [head];
    while (stack.length) { const n = stack.pop(); out.push(n); for (const c of (n.children || [])) stack.push(c); }
    return out;
}

const pct = (us) => `${(100 * us / totalUs).toFixed(1)}%`;
const ms = (us) => `${(us / 1000).toFixed(0)}ms`;

console.log(`CPU profile: ${file}`);
console.log(`total sampled: ${(totalUs / 1e6).toFixed(2)}s   samples: ${samples.length}`);
console.log('='.repeat(70));
console.log('BY CATEGORY (self time):');
for (const [c, us] of [...catAgg].sort((a, b) => b[1] - a[1]))
    console.log(`  ${c.padEnd(10)} ${ms(us).padStart(8)}  ${pct(us).padStart(7)}`);
console.log('-'.repeat(70));
const busy = totalUs - (catAgg.get('idle') || 0);
console.log(`BUSY (non-idle): ${ms(busy)}  ${pct(busy)}`);
console.log('='.repeat(70));
const sorted = [...fnAgg].sort((a, b) => b[1] - a[1]);
const filtered = GREP ? sorted.filter(([n]) => n.toLowerCase().includes(GREP.toLowerCase())) : sorted;
console.log(`TOP ${TOP} FUNCTIONS BY SELF TIME${GREP ? ` (grep="${GREP}")` : ''}:`);
for (const [name, us] of filtered.slice(0, TOP))
    console.log(`  ${ms(us).padStart(8)}  ${pct(us).padStart(7)}  ${name.slice(0, 80)}`);
