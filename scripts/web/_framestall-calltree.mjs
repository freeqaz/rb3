#!/usr/bin/env node
/**
 * _framestall-calltree.mjs — roll up a V8 .cpuprofile by INCLUSIVE (total) time
 * per function, and show the immediate-parent attribution of (program)/(idle)
 * samples. Self-time hides cost in -O0 wasm where deep frames collapse into
 * (program); inclusive time + parent context reveals which named subsystem owns
 * the unattributed time.
 *
 * Usage: node _framestall-calltree.mjs <profile.cpuprofile> [--grep S] [--top N]
 *        [--focus FNNAME]   (show children of the node(s) matching FNNAME)
 */
import { readFileSync } from 'fs';
const argv = process.argv.slice(2);
const file = argv.find(a => !a.startsWith('--'));
const TOP = parseInt(arg('--top', '40'), 10);
const GREP = arg('--grep', null);
const FOCUS = arg('--focus', null);
function arg(n, d) { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : d; }
if (!file) { console.error('usage: _framestall-calltree.mjs <profile.cpuprofile>'); process.exit(1); }

const prof = JSON.parse(readFileSync(file, 'utf8'));
const nodes = prof.nodes;
const byId = new Map(); for (const n of nodes) byId.set(n.id, n);
const nameOf = (n) => { const f = n.callFrame || n; return (f.functionName || '(anonymous)'); };

// parent map (nodes only carry children)
const parent = new Map();
for (const n of nodes) for (const c of (n.children || [])) parent.set(c, n.id);

// self-time per node id (us)
const selfUs = new Map();
const samples = prof.samples || [];
const deltas = prof.timeDeltas || [];
let totalUs = 0;
for (let i = 0; i < samples.length; i++) { const id = samples[i]; const dt = Math.max(0, deltas[i] || 0); totalUs += dt; selfUs.set(id, (selfUs.get(id) || 0) + dt); }

// inclusive time: a node's inclusive = its self + sum(children inclusive).
// Compute via post-order over the children tree (rooted at the (root) node).
const inclUs = new Map();
function computeIncl(id) {
    const n = byId.get(id);
    let total = selfUs.get(id) || 0;
    for (const c of (n.children || [])) total += computeIncl(c);
    inclUs.set(id, total);
    return total;
}
// find root(s) (no parent)
const roots = nodes.filter(n => !parent.has(n.id)).map(n => n.id);
for (const r of roots) computeIncl(r);

// aggregate inclusive by function NAME (sum across all nodes of that name — but
// that double counts recursion; for our purpose top-level-per-name is fine since
// the engine fns aren't deeply recursive). Use max-per-call-path approach: sum
// inclusive of nodes whose name != any ancestor's name (avoid recursion double).
function ancestorHasName(id, name) {
    let p = parent.get(id);
    while (p != null) { if (nameOf(byId.get(p)) === name) return true; p = parent.get(p); }
    return false;
}
const inclByName = new Map();
const selfByName = new Map();
for (const n of nodes) {
    const nm = nameOf(n);
    selfByName.set(nm, (selfByName.get(nm) || 0) + (selfUs.get(n.id) || 0));
    if (!ancestorHasName(n.id, nm)) inclByName.set(nm, (inclByName.get(nm) || 0) + (inclUs.get(n.id) || 0));
}

const ms = (us) => (us / 1000).toFixed(0);
const pct = (us) => (100 * us / Math.max(1, totalUs)).toFixed(1);

console.log(`profile: ${file}  total=${(totalUs / 1e6).toFixed(1)}s samples=${samples.length}`);

if (FOCUS) {
    // Show immediate children (callees) of every node named FOCUS, aggregated.
    console.log(`\nCHILDREN (callees) of "${FOCUS}" — where its inclusive time goes:`);
    const childAgg = new Map(); let focusIncl = 0, focusSelf = 0;
    for (const n of nodes) {
        if (nameOf(n) !== FOCUS) continue;
        focusIncl += inclUs.get(n.id) || 0; focusSelf += selfUs.get(n.id) || 0;
        for (const c of (n.children || [])) { const cm = nameOf(byId.get(c)); childAgg.set(cm, (childAgg.get(cm) || 0) + (inclUs.get(c) || 0)); }
    }
    console.log(`  ${FOCUS}: inclusive=${ms(focusIncl)}ms self=${ms(focusSelf)}ms`);
    for (const [nm, us] of [...childAgg].sort((a, b) => b[1] - a[1]).slice(0, TOP))
        console.log(`    ${ms(us).padStart(7)}ms ${pct(us).padStart(5)}%  ${nm.slice(0, 80)}`);
    // also show the PARENTS of FOCUS nodes (callers)
    const parAgg = new Map();
    for (const n of nodes) { if (nameOf(n) !== FOCUS) continue; const p = parent.get(n.id); if (p != null) { const pm = nameOf(byId.get(p)); parAgg.set(pm, (parAgg.get(pm) || 0) + (inclUs.get(n.id) || 0)); } }
    console.log(`  CALLERS of ${FOCUS}:`);
    for (const [nm, us] of [...parAgg].sort((a, b) => b[1] - a[1]).slice(0, 12)) console.log(`    ${ms(us).padStart(7)}ms  ${nm.slice(0, 70)}`);
    process.exit(0);
}

console.log(`\nTOP ${TOP} by INCLUSIVE time${GREP ? ` (grep=${GREP})` : ''}:`);
let rows = [...inclByName].sort((a, b) => b[1] - a[1]);
if (GREP) rows = rows.filter(([n]) => n.toLowerCase().includes(GREP.toLowerCase()));
for (const [nm, us] of rows.slice(0, TOP))
    console.log(`  incl=${ms(us).padStart(7)}ms ${pct(us).padStart(5)}%  self=${ms(selfByName.get(nm) || 0).padStart(6)}ms  ${nm.slice(0, 70)}`);

// Parent attribution of (program) and (idle): who is on the stack just above them?
for (const target of ['(program)', '(idle)']) {
    const parAgg = new Map(); let tot = 0;
    for (const n of nodes) {
        if (nameOf(n) !== target) continue;
        const self = selfUs.get(n.id) || 0; if (self === 0) continue; tot += self;
        const p = parent.get(n.id); const pm = p != null ? nameOf(byId.get(p)) : '(no parent)';
        parAgg.set(pm, (parAgg.get(pm) || 0) + self);
    }
    if (tot === 0) continue;
    console.log(`\nPARENTS of ${target} self-samples (total ${ms(tot)}ms = ${pct(tot)}%):`);
    for (const [nm, us] of [...parAgg].sort((a, b) => b[1] - a[1]).slice(0, 20))
        console.log(`  ${ms(us).padStart(7)}ms ${(100 * us / tot).toFixed(1).padStart(5)}%  ${nm.slice(0, 70)}`);
}
