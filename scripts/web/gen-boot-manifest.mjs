#!/usr/bin/env node
/**
 * gen-boot-manifest.mjs — derive the R3 boot-assets manifest from a cold-boot
 * network waterfall (netperf-suite.mjs's network-waterfall.json).
 *
 * The boot bundle (server.py /api/bundle/boot) pre-packs the .milo_xbox working
 * set the App ctor reads BEFORE the first interactive frame, so those reads land
 * in warm MEMFS instead of freezing the wasm main thread on a synchronous XHR
 * (see docs/native/web-perf-roadmap/R3-boot-bundle-expansion.md).
 *
 * This generator keeps the manifest a MEASURED artifact (not hand-authored): it
 * reads one (or a union of several) cold-boot waterfall(s), filters to the
 * .milo_xbox requests, drops the venue/vignette/song milos (R2's per-screen
 * territory — those are fetched in the FIRST menu transition, not at boot), and
 * writes the sorted unique path list to native/web/boot-assets.manifest.
 *
 * USAGE:
 *   node scripts/web/gen-boot-manifest.mjs <waterfall.json> [<waterfall2.json> ...]
 *   node scripts/web/gen-boot-manifest.mjs /tmp/rb3-web/netperf-baseline/low-boot-run1/network-waterfall.json
 *
 * Pass several waterfalls (e.g. from --runs 3) to take the UNION, guarding
 * against run-to-run lazy-load timing drift.
 *
 * It re-runs offline against committed waterfalls in CI; diff successive
 * generations to catch boot-graph drift (a new sync miss = a manifest gap).
 */
import { readFileSync, writeFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const MANIFEST = resolve(__dirname, '..', '..', 'native', 'web', 'boot-assets.manifest');

// R2 territory: venue / vignette backdrops + song moggs/milos are fetched in the
// first menu transition, NOT during boot-to-first-screen. The boot waterfall
// should not contain them, but a path-prefix denylist is a cheap guard against a
// timing-shifted lazy load sneaking a venue milo into the boot set (which would
// bloat the boot bundle and delay first paint for a file boot doesn't need).
const DENY_PREFIXES = ['world/venue/', 'world/vignette/', 'songs/'];

const files = process.argv.slice(2);
if (files.length === 0) {
    console.error('usage: gen-boot-manifest.mjs <network-waterfall.json> [...more]');
    process.exit(2);
}

const set = new Set();
let denied = 0;
for (const f of files) {
    const wf = JSON.parse(readFileSync(f, 'utf8'));
    for (const r of wf.requests || []) {
        const url = r.url || '';
        if (!url.startsWith('/api/file/') || !url.endsWith('.milo_xbox')) continue;
        // Only count files that actually arrived (200 / OK). A 404 (e.g. a
        // patchcreator milo absent from this checkout) must NOT enter the
        // manifest — the server would just skip it, but a clean manifest is
        // diff-reviewable and avoids a spurious "missing" log every boot.
        if (r.status != null && r.status !== 200) continue;
        const rel = url.slice('/api/file/'.length);
        if (DENY_PREFIXES.some((p) => rel.startsWith(p))) { denied++; continue; }
        set.add(rel);
    }
}

const list = [...set].sort();
const header = [
    '# RB3 web boot-assets manifest (R3) — GENERATED, do not hand-edit.',
    '# Source: scripts/web/gen-boot-manifest.mjs <network-waterfall.json>',
    '# The .milo_xbox working set the App ctor reads before the first interactive',
    '# screen, served as one async bundle by server.py /api/bundle/boot so those',
    '# reads hit warm MEMFS instead of freezing the wasm thread on a sync XHR.',
    '# Venue/vignette/song milos are excluded (R2 per-screen prefetch territory).',
    `# ${list.length} files. Regenerate when the boot graph changes.`,
];
writeFileSync(MANIFEST, header.join('\n') + '\n' + list.join('\n') + '\n');

console.log(`gen-boot-manifest: ${list.length} boot milos -> ${MANIFEST}`);
if (denied) console.log(`  (excluded ${denied} venue/vignette/song milo request(s) — R2 territory)`);
