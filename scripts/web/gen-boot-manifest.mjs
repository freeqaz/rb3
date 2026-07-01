#!/usr/bin/env node
/**
 * gen-boot-manifest.mjs — derive the R3 boot-assets manifest AND the A2
 * per-screen dependency manifests from a network waterfall (a netperf-suite /
 * screen-netlog CDP capture: { requests: [{ url, status, ... }] }).
 *
 * BOOT MODE (default):
 *   node scripts/web/gen-boot-manifest.mjs <waterfall.json> [<waterfall2.json> ...]
 * Writes native/web/boot-assets.manifest = the .milo_xbox working set the App
 * ctor reads BEFORE the first interactive frame, served as one async bundle by
 * server.py /api/bundle/boot so those reads land in warm MEMFS instead of
 * freezing the wasm main thread on a synchronous XHR
 * (docs/native/web-perf-roadmap/R3-boot-bundle-expansion.md).
 *
 * SCREEN MODE (A2, incremental-load-perf PLAN.md T9):
 *   node scripts/web/gen-boot-manifest.mjs --screen <name> \
 *        --screen-tags <screenA,screenB> <netlog.json> [...]
 *   node scripts/web/gen-boot-manifest.mjs --screen <name> \
 *        --enter-marker <substr> [--exit-marker <substr>] <netlog.json> [...]
 * Writes native/web/screen-<name>.manifest = the EXTRA .milo_xbox/.dta a screen
 * ENTER reads that are NOT already in the boot bundle (those are MEMFS-resident
 * by the time any screen transition runs, so re-bundling them is pure waste).
 *
 * The screen's read set is selected one of two ways from the screen-netlog.mjs
 * capture (whose requests carry a `screen` tag = the UI screen current when the
 * read fired, plus synthetic "MARKER:enter:<screen>" boundary requests):
 *   --screen-tags A,B  → requests whose `screen` tag is A or B (PRECISE; this is
 *                        the read SET that screen's bundle should warm — e.g. the
 *                        main_hub panel milos load while screen=='splash_screen',
 *                        so the main_hub bundle is built with --screen-tags
 *                        splash_screen). Preferred.
 *   --enter-marker S [--exit-marker S2] → the time-ordered request slice between
 *                        the markers (fallback when tags are unavailable).
 *
 * Both modes:
 *   - filter to /api/file/ requests with a bundlable ext (.milo_xbox, .dta);
 *   - drop non-200 requests (a 404 must NOT enter a manifest — the server would
 *     skip it, but a clean manifest is diff-reviewable);
 *   - drop venue/vignette/song milos via DENY_PREFIXES (large, lazily fetched,
 *     not worth blocking a transition on — they stream in during dwell);
 *   - take the UNION across several input files (guards run-to-run lazy-load
 *     timing drift — pass several captures, e.g. from --runs 3);
 *   - write a sorted unique, commented, diff-reviewable list.
 *
 * Re-run offline against committed waterfalls in CI; diff successive generations
 * to catch graph drift (a new sync miss = a manifest gap).
 */
import { readFileSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const WEB_DIR = resolve(__dirname, '..', '..', 'native', 'web');
const BOOT_MANIFEST = resolve(WEB_DIR, 'boot-assets.manifest');

// R2/A2 territory split: venue / vignette backdrops + song moggs/milos are big
// and fetched lazily during dwell, NOT on the blocking transition frame. Keeping
// them out of any bundle avoids bloating it + delaying the screen's first paint.
const DENY_PREFIXES = ['world/venue/', 'world/vignette/', 'songs/'];
// Bundlable extensions: scene graphs + the DTA family the engine sync-opens. Both
// compress well and are exactly what the bundle's warm-MEMFS unpack helps with.
const BUNDLABLE = ['.milo_xbox', '.dta'];

// ---- arg parsing ----------------------------------------------------------
const argv = process.argv.slice(2);
function takeOpt(name) {
    const i = argv.indexOf(name);
    if (i < 0) return null;
    const v = argv[i + 1];
    argv.splice(i, 2);
    return v;
}
const screenName = takeOpt('--screen');
const screenTags = (takeOpt('--screen-tags') || '').split(',').map((s) => s.trim()).filter(Boolean);
const enterMarker = takeOpt('--enter-marker');
const exitMarker = takeOpt('--exit-marker');
const files = argv.filter((a) => !a.startsWith('--'));

if (files.length === 0) {
    console.error('usage:');
    console.error('  gen-boot-manifest.mjs <network-waterfall.json> [...more]');
    console.error('  gen-boot-manifest.mjs --screen <name> --screen-tags <A,B> <netlog.json> [...]');
    console.error('  gen-boot-manifest.mjs --screen <name> [--enter-marker S] [--exit-marker S] <netlog.json> [...]');
    process.exit(2);
}

function relOf(url) {
    if (!url || !url.startsWith('/api/file/')) return null;
    if (!BUNDLABLE.some((e) => url.endsWith(e))) return null;
    return url.slice('/api/file/'.length);
}

// In screen mode, restrict each capture to the request slice between the enter
// marker (transition START) and the exit marker (or end). A marker is matched by
// substring on the request url. With no enter marker we take the WHOLE capture
// (useful for a netlog that was already trimmed to one transition by the harness).
function screenSlice(requests) {
    if (!enterMarker) return requests;
    const start = requests.findIndex((r) => (r.url || '').includes(enterMarker));
    if (start < 0) return [];
    let end = requests.length;
    if (exitMarker) {
        const e = requests.findIndex((r, i) => i > start && (r.url || '').includes(exitMarker));
        if (e >= 0) end = e;
    }
    return requests.slice(start, end);
}

const set = new Set();
let denied = 0, nonOk = 0, slices = 0;
for (const f of files) {
    const wf = JSON.parse(readFileSync(f, 'utf8'));
    let requests = wf.requests || [];
    // Screen mode selection: --screen-tags (precise, by the read's screen tag) is
    // preferred; --enter-marker (time slice) is the fallback. Boot mode uses the
    // whole capture.
    if (screenName && screenTags.length) {
        requests = requests.filter((r) => screenTags.includes(r.screen));
        slices += requests.length ? 1 : 0;
    } else if (screenName) {
        requests = screenSlice(requests);
        slices += requests.length ? 1 : 0;
    }
    for (const r of requests) {
        const rel = relOf(r.url || '');
        if (!rel) continue;
        if (r.status != null && r.status !== 200) { nonOk++; continue; }
        if (DENY_PREFIXES.some((p) => rel.startsWith(p))) { denied++; continue; }
        set.add(rel);
    }
}

let list = [...set].sort();

// Screen mode: subtract the boot manifest. Anything the boot bundle already
// delivers is MEMFS-resident before any transition, so re-bundling it per screen
// is dead weight (and risks double-unpacking the same milo).
let bootSubtracted = 0;
if (screenName) {
    let boot = new Set();
    try {
        for (const line of readFileSync(BOOT_MANIFEST, 'utf8').split('\n')) {
            const t = line.trim();
            if (t && !t.startsWith('#')) boot.add(t);
        }
    } catch { /* no boot manifest yet — subtract nothing */ }
    const before = list.length;
    list = list.filter((p) => !boot.has(p));
    bootSubtracted = before - list.length;
}

const OUT = screenName ? resolve(WEB_DIR, `screen-${screenName}.manifest`) : BOOT_MANIFEST;
const header = screenName
    ? [
        `# RB3 web screen-${screenName} dependency manifest (A2) — GENERATED, do not hand-edit.`,
        '# Source: scripts/web/gen-boot-manifest.mjs --screen ' + screenName + ' <netlog.json>',
        `# The extra .milo_xbox/.dta the '${screenName}' screen ENTER reads (boot-bundle`,
        '# entries already MEMFS-resident are subtracted), served as one async bundle by',
        '# server.py /api/bundle/screen/' + screenName + ' fired at transition-START so the',
        '# reads hit warm MEMFS instead of freezing the wasm thread on a sync XHR.',
        '# Venue/vignette/song milos are excluded (lazily streamed during dwell).',
        `# ${list.length} files. Regenerate when this screen's load graph changes.`,
    ]
    : [
        '# RB3 web boot-assets manifest (R3) — GENERATED, do not hand-edit.',
        '# Source: scripts/web/gen-boot-manifest.mjs <network-waterfall.json>',
        '# The .milo_xbox working set the App ctor reads before the first interactive',
        '# screen, served as one async bundle by server.py /api/bundle/boot so those',
        '# reads hit warm MEMFS instead of freezing the wasm thread on a sync XHR.',
        '# Venue/vignette/song milos are excluded (R2 per-screen prefetch territory).',
        `# ${list.length} files. Regenerate when the boot graph changes.`,
    ];
writeFileSync(OUT, header.join('\n') + '\n' + list.join('\n') + '\n');

console.log(`gen-boot-manifest: ${list.length} ${screenName ? `screen-${screenName}` : 'boot'} entries -> ${OUT}`);
if (screenName && bootSubtracted) console.log(`  (subtracted ${bootSubtracted} entr(y/ies) already in the boot bundle)`);
if (denied) console.log(`  (excluded ${denied} venue/vignette/song request(s))`);
if (nonOk) console.log(`  (excluded ${nonOk} non-200 request(s))`);
if (screenName && enterMarker) console.log(`  (sliced ${slices}/${files.length} capture(s) on enter-marker '${enterMarker}')`);
