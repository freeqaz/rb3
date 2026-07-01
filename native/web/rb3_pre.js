// rb3-web pre-js. Two jobs run before Emscripten's WASM is instantiated:
//
// 1) Replace Emscripten's "missing function: X" abort-stub imports with silent
//    no-op stubs so the RB3 matched-fork's static initializers don't trap with
//    `unreachable` when they reach Wii-only globals (PlatformMgr, TheNetSession,
//    TheMC, TheSynth, RndMesh::sLastCollide, etc.). See the long block-comment
//    below for the full history.
//
// 2) Open the W4b asset cache (IndexedDB), version-check it against
//    /api/version, drop it on mismatch, and bulk-load all (path -> bytes) rows
//    into a sync-readable JS map (`window.__rb3IdbCache`). The native
//    `WebAssetsFetchSync` shim (native/src/native_file.cpp) checks this map
//    BEFORE issuing the sync XHR — a hit writes bytes straight into MEMFS
//    (no network round-trip), a miss falls through to XHR and queues an async
//    IDB write back via `window.__rb3CachePut`. The cache pre-warm races the
//    WASM download in parallel; if it isn't ready yet by the time the engine
//    starts asking for assets, the misses just fall through to XHR as before.
//
// Background on (1): rb3-web links with `-Wl,--allow-undefined`, leaving those
// off-path references as wasm imports. Emscripten generates a JS env entry
// for each undefined import that calls `abort('missing function: ...')`. The
// engine's missing_stubs.js patches Module.abort/onAbort from
// onRuntimeInitialized — too late for static initializers (they run DURING
// runtime init). Even if onAbort is patched earlier, Emscripten's abort()
// ALWAYS throws after calling onAbort and the wasm catches that as
// `unreachable`. The only clean fix is to replace the abort-stub IMPORTS
// themselves before wasm instantiation, so the calls return harmlessly.
//
// We use Module.instantiateWasm to gain access to the imports object before
// the wasm runs, scan env.* for anything whose source contains "missing
// function", and swap each with a no-op returning 0. This mirrors
// rb3-native's dta_link_stubs.s weak-no-op strategy.

(function() {
    var origModule = (typeof Module === 'undefined') ? {} : Module;
    var stubWarned = {};
    var stubCount = 0;

    function makeStub(name) {
        return function() {
            if (!stubWarned[name]) {
                console.warn('[rb3-stub] ' + name);
                stubWarned[name] = true;
            }
            return 0;
        };
    }

    var origInstantiate = origModule.instantiateWasm;
    origModule.instantiateWasm = function(imports, successCallback) {
        // Walk env.* and replace any function whose source contains
        // "missing function" — that's Emscripten's auto-generated
        // abort stub for unresolved --allow-undefined imports.
        for (var modName in imports) {
            if (!imports.hasOwnProperty(modName)) continue;
            var mod = imports[modName];
            if (!mod || typeof mod !== 'object') continue;
            for (var name in mod) {
                if (!mod.hasOwnProperty(name)) continue;
                var v = mod[name];
                if (typeof v !== 'function') continue;
                var src = '';
                try { src = String(v); } catch (e) { src = ''; }
                if (src.indexOf('missing function') !== -1) {
                    mod[name] = makeStub(modName + '.' + name);
                    stubCount++;
                }
            }
        }
        console.log('[rb3-pre] replaced ' + stubCount + ' missing-function abort stubs');

        if (origInstantiate) {
            return origInstantiate.call(this, imports, successCallback);
        }
        // Default Emscripten path — fetch + compile + instantiate ourselves.
        var wasmPath = (origModule.locateFile ? origModule.locateFile('rb3-web.wasm') : 'rb3-web.wasm');
        // Prefer STREAMING compilation: compile the 28MB wasm while it downloads
        // instead of download-then-compile (the old arrayBuffer path). The server
        // sends Content-Type: application/wasm (+ Content-Encoding br/gz, which the
        // browser transparently decompresses into the streaming compiler). This
        // attacks the pre-boot wasm-compile phase (measured bimodal 0.5–8s in
        // loadperf-profile.mjs). Falls back to arrayBuffer if streaming is
        // unavailable or the MIME type isn't honoured.
        var t0 = (typeof performance !== 'undefined' && performance.now) ? performance.now() : 0;
        function viaArrayBuffer() {
            return fetch(wasmPath)
                .then(function(resp) { return resp.arrayBuffer(); })
                .then(function(bytes) { return WebAssembly.instantiate(bytes, imports); })
                .then(function(result) { successCallback(result.instance, result.module); });
        }
        if (typeof WebAssembly.instantiateStreaming === 'function') {
            return WebAssembly.instantiateStreaming(fetch(wasmPath), imports)
                .then(function(result) {
                    console.log('[rb3-pre] wasm instantiateStreaming ' + ((performance.now() - t0) | 0) + 'ms');
                    successCallback(result.instance, result.module);
                })
                .catch(function(err) {
                    console.warn('[rb3-pre] instantiateStreaming failed (' + err + '); falling back to arrayBuffer');
                    return viaArrayBuffer();
                });
        }
        return viaArrayBuffer().catch(function(err) {
            console.error('[rb3-pre] instantiateWasm failed: ' + err);
        });
    };

    if (typeof Module === 'undefined') {
        globalThis.Module = origModule;
    } else {
        Module = origModule;
    }

    // -----------------------------------------------------------------------
    // Generic URL-param -> ENV bridge (incremental-load-perf PLAN.md T1).
    //
    // The existing per-flag bridge lives in native/src/main_web.cpp
    // (ApplyUrlLoaderEnv: ?loaderBudgetMs=8 -> RB3_LOADER_BUDGET_MS, a fixed
    // allowlist). To let ANY RB3_* flag be toggled in a browser without a
    // rebuild or a code edit per flag, this parses a single generic param:
    //
    //     ?env=RB3_FRAME_TRACE=/trace.jsonl;RB3_BC_TEX_OFF=1
    //
    // Pairs are ';'-separated, each "NAME=VALUE". We seed Module.ENV (Emscripten
    // copies it into `environ` before main, so the wasm's getenv() sees it) AND
    // stash the same map on window.__rb3ExtraEnv as a hook for any C++-side
    // draining a later wave may add (the existing main_web.cpp ApplyUrlLoaderEnv
    // is a fixed per-flag allowlist; this generic path complements it). Names
    // are restricted to /^RB3_[A-Z0-9_]+$/ so a stray param can't clobber
    // arbitrary process env (e.g. PATH).
    try {
        var __envParam = new URLSearchParams(window.location.search).get('env');
        if (__envParam) {
            origModule.ENV = origModule.ENV || {};
            window.__rb3ExtraEnv = window.__rb3ExtraEnv || {};
            __envParam.split(';').forEach(function(pair) {
                if (!pair) return;
                var eq = pair.indexOf('=');
                var name = (eq < 0 ? pair : pair.slice(0, eq)).trim();
                var val = (eq < 0 ? '1' : pair.slice(eq + 1)).trim();
                // RB3_* flags pass freely; a fixed second list admits the
                // engine's draw-path diagnostic probes (read via getenv in
                // milo-native-engine Rnd_Wgpu_RB3.cpp) so visual bugs can be
                // probed in-browser without a rebuild. Anything else (PATH,
                // HOME, ...) is still rejected.
                var dbgProbes = /^(SKIN_PROBE|SHARD_CATCH|SKIN_CLAMP_PROBE|GEM_VTX|SLOT_PROBE|CAM_DBG|XBONE|XBONE_TRACK|BAND_ANIM_PROBE|REBIND_DRAW_SKINPOS|REBIND_DRAW_FLING)$/;
                if (!/^RB3_[A-Z0-9_]+$/.test(name) && !dbgProbes.test(name)) {
                    console.warn('[rb3-pre] ignoring non-RB3 env param: ' + name);
                    return;
                }
                window.__rb3ExtraEnv[name] = val;
                origModule.ENV[name] = val;
                console.log('[rb3-pre] env ' + name + '=' + val + ' (from ?env)');
            });
        }
    } catch (e) {
        console.warn('[rb3-pre] ?env bridge failed: ' + e);
    }

    // -----------------------------------------------------------------------
    // A1 (incremental-load-perf PLAN.md T6) — manifest size/existence oracle.
    //
    // The async-open seam (native_file.cpp WebPendingFile/WebRangeFile) needs a
    // SYNCHRONOUS answer to "does this asset exist, and how big is it?" the
    // instant the engine's File ctor runs, with zero network. /api/manifest
    // already enumerates every curated asset with its size (server.py:414-435);
    // we pre-warm it into window.__rb3ManifestSizes (Map<path, sizeBytes>) racing
    // the wasm download — the same pattern as the IDB cache below. The engine's
    // WebAssetsManifestLoad() copies this Map across the wasm boundary in one
    // EM_ASM at WebAssetsInit(); if it isn't ready in time, the engine falls back
    // to a one-shot synchronous /api/manifest fetch (no correctness loss, just a
    // slightly later first answer). Keys are server-relative ('ui/gen/x.milo_xbox',
    // 'songs/x/x.mogg', '(..)/(..)/system/run/...'); the engine de-mangles the
    // '(..)/' system prefix to match its request form.
    window.__rb3ManifestSizes = new Map();
    window.__rb3ManifestReady = 0;
    (function preWarmManifest() {
        try {
            fetch('/api/manifest')
                .then(function(r) { return r.ok ? r.json() : null; })
                .then(function(j) {
                    if (j && j.files) {
                        for (var i = 0; i < j.files.length; i++) {
                            window.__rb3ManifestSizes.set(j.files[i].path, j.files[i].size);
                        }
                        console.log('[rb3-pre] manifest oracle: ' + window.__rb3ManifestSizes.size + ' assets');
                    }
                    window.__rb3ManifestReady = 1;
                })
                .catch(function(e) {
                    console.warn('[rb3-pre] manifest prewarm failed: ' + e);
                    window.__rb3ManifestReady = 1;
                });
        } catch (e) {
            console.warn('[rb3-pre] manifest prewarm threw: ' + e);
            window.__rb3ManifestReady = 1;
        }
    })();

    // -----------------------------------------------------------------------
    // W4b — IndexedDB asset cache.
    //
    // Design constraints. The engine's WebAssetsFetchSync is a synchronous
    // call (the matched-fork's File ctor expects bytes on return), but IDB
    // itself is fundamentally async. We can't read IDB inside a sync XHR
    // path. The workaround is to pre-warm a JS Map at boot:
    //   - Open the DB now (before WASM).
    //   - Version-check against /api/version; if the stored version mismatches
    //     the live one, clear the object store (forces a fresh fetch).
    //   - Read every row into `window.__rb3IdbCache` (a Map<string, Uint8Array>).
    //   - Once that's done, set `window.__rb3IdbReady = 1`. The native shim
    //     checks readiness AND map hit synchronously.
    //   - The pre-warm runs concurrently with the WASM download; if it isn't
    //     ready in time, the native shim falls through to XHR (cache misses,
    //     no harm done).
    // The cache key is the server-relative path (e.g. "ui/gen/foo.milo_xbox"),
    // matching what gets sent to /api/file/<path>.
    // -----------------------------------------------------------------------

    var DB_NAME = 'rb3-web-assets';
    var DB_VERSION = 1;
    var STORE = 'files';
    var META_STORE = 'meta';
    var VERSION_KEY = 'asset-version';

    window.__rb3IdbCache = new Map();
    window.__rb3IdbReady = 0;
    window.__rb3CacheStats = { hits: 0, misses: 0, bytesFromCache: 0, bytesFetched: 0, puts: 0, writeErrors: 0 };
    window.__rb3IdbVersion = '';
    var sDb = null;
    var sPendingWrites = [];

    function openDb() {
        return new Promise(function(resolve, reject) {
            var req = indexedDB.open(DB_NAME, DB_VERSION);
            req.onupgradeneeded = function(e) {
                var db = e.target.result;
                if (!db.objectStoreNames.contains(STORE))   db.createObjectStore(STORE);
                if (!db.objectStoreNames.contains(META_STORE)) db.createObjectStore(META_STORE);
            };
            req.onsuccess = function(e) { resolve(e.target.result); };
            req.onerror = function(e) { reject(e.target.error); };
        });
    }

    function metaGet(db, key) {
        return new Promise(function(resolve, reject) {
            var tx = db.transaction(META_STORE, 'readonly');
            var req = tx.objectStore(META_STORE).get(key);
            req.onsuccess = function() { resolve(req.result); };
            req.onerror = function() { reject(req.error); };
        });
    }
    function metaPut(db, key, val) {
        return new Promise(function(resolve, reject) {
            var tx = db.transaction(META_STORE, 'readwrite');
            tx.objectStore(META_STORE).put(val, key);
            tx.oncomplete = function() { resolve(); };
            tx.onerror = function() { reject(tx.error); };
        });
    }
    function clearStore(db, store) {
        return new Promise(function(resolve, reject) {
            var tx = db.transaction(store, 'readwrite');
            tx.objectStore(store).clear();
            tx.oncomplete = function() { resolve(); };
            tx.onerror = function() { reject(tx.error); };
        });
    }
    // Q6 (incremental-load-perf PLAN §5 T5) — IDB pre-warm cap.
    //
    // loadAllRows() pulls every cached row into window.__rb3IdbCache, a JS Map
    // held in the renderer's ArrayBuffer heap, BEFORE wasm even starts. The
    // synchronous warm-boot path (native_file.cpp cacheTryHit) needs that Map
    // because IDB can't be read synchronously inside the File ctor. But a hovered
    // preview/gameplay mogg is 31-36 MB; hover ten songs and the NEXT boot would
    // eager-load ~360 MB into JS memory — a latent OOM (the same heap-growth class
    // as the runtime-put OOM FIX above, but on the boot path).
    //
    // Fix: exclude oversized rows (and .mogg keys, which are always oversized)
    // from the eager Map. These are NEVER on the synchronous boot-critical path —
    // the warm-boot Map exists to satisfy the small milo/texture/dta opens the App
    // ctor does; large moggs are opened later (preview hover / song start), off the
    // boot frame. An excluded row stays in IDB (not deleted); its first
    // session-touch misses cacheTryHit and falls to the network exactly as a
    // never-cached asset's first touch does today (one fetch → MEMFS-resident for
    // the rest of the session → written back through to IDB), so correctness is
    // unchanged. Only the unbounded eager-load is removed. Q2/Q3/A1 (range moggs +
    // hover prefetch) are the real fix for the mogg fetch cost; this just stops the
    // cache from OOMing the renderer at boot.
    var PREWARM_MAX_ROW_BYTES = 4 * 1024 * 1024; // 4 MB — boot-critical assets are
                                                 // well under this; moggs are far over.
    function isOversizedKey(key) {
        // .mogg (and any future large-media key) is excluded by name as a belt-and-
        // suspenders even if its byteLength were somehow missing.
        return typeof key === 'string' && /\.mogg$/i.test(key);
    }
    function loadAllRows(db) {
        return new Promise(function(resolve, reject) {
            var tx = db.transaction(STORE, 'readonly');
            var store = tx.objectStore(STORE);
            var bytes = 0, rows = 0, skipped = 0, skippedBytes = 0;
            var cursorReq = store.openCursor();
            cursorReq.onsuccess = function(e) {
                var c = e.target.result;
                if (!c) {
                    resolve({ rows: rows, bytes: bytes, skipped: skipped, skippedBytes: skippedBytes });
                    return;
                }
                var v = c.value;
                if (v && v.byteLength != null) {
                    if (v.byteLength > PREWARM_MAX_ROW_BYTES || isOversizedKey(c.key)) {
                        // Oversized: leave it in IDB, keep it out of the heap Map.
                        skipped++;
                        skippedBytes += v.byteLength;
                    } else {
                        var u8 = v instanceof Uint8Array ? v : new Uint8Array(v);
                        window.__rb3IdbCache.set(c.key, u8);
                        bytes += u8.byteLength;
                        rows++;
                    }
                }
                c.continue();
            };
            cursorReq.onerror = function() { reject(cursorReq.error); };
        });
    }

    function flushPendingWrites() {
        if (!sDb || sPendingWrites.length === 0) return;
        var batch = sPendingWrites;
        sPendingWrites = [];
        try {
            var tx = sDb.transaction(STORE, 'readwrite');
            var store = tx.objectStore(STORE);
            for (var i = 0; i < batch.length; i++) {
                store.put(batch[i].bytes, batch[i].key);
            }
            tx.onerror = function() { window.__rb3CacheStats.writeErrors += batch.length; };
        } catch (e) {
            window.__rb3CacheStats.writeErrors += batch.length;
        }
    }

    // Single-row put — called by the native shim (via EM_ASM) after a successful
    // sync XHR (cachePutAfterFetch) AND by the engine bundle unpack
    // (bundleCacheWriteThrough). The write to IDB is fire-and-forget; we batch by
    // deferring the actual transaction commit to the next microtask so a burst of
    // misses shares one tx.
    //
    // OOM FIX: do NOT retain the bytes in the in-memory __rb3IdbCache map here.
    // Every caller of __rb3CachePut has ALREADY written the file to MEMFS (the
    // sync path opens it after the XHR; the bundle path fwrites it before caching),
    // so for the rest of THIS session every re-open is served by MEMFS and the
    // in-memory entry is never read again (cacheTryHit is only consulted on a MEMFS
    // miss = the first open). The only consumer of the in-memory map is the
    // *next* session's synchronous warm-boot path, which is repopulated from IDB by
    // loadAllRows() — not by these runtime puts. Retaining a 2nd copy of every
    // fetched asset in the renderer's ArrayBuffer heap therefore bought nothing and
    // grew unbounded: navigating to song_select pulls ~470 milos/textures (~190 MB),
    // a full redundant duplicate of MEMFS, which OOM-crashed the renderer
    // ("Page crashed" / hang-on-black) the moment the song-select milo burst landed.
    // Keep only the IDB write-through so warm boots still benefit.
    window.__rb3CachePut = function(path, bytes) {
        if (!path || !bytes) return;
        window.__rb3CacheStats.puts++;
        if (!sDb) return;
        // Take a copy — the caller's HEAP view may move on next allocation. The
        // copy is referenced only until flushPendingWrites() hands it to IDB, then
        // released (no in-memory map retention — see OOM FIX above).
        var copy = new Uint8Array(bytes.byteLength);
        copy.set(bytes);
        sPendingWrites.push({ key: path, bytes: copy });
        // Flush soon — Promise.resolve is the smallest deferral that still
        // lets a burst of misses share a transaction.
        if (sPendingWrites.length === 1) {
            Promise.resolve().then(flushPendingWrites);
        }
    };

    // Pre-warm. Race the WASM download.
    (function preWarm() {
        var versionPromise = fetch('/api/version')
            .then(function(r) { return r.json(); })
            .then(function(j) { return j && j.version ? String(j.version) : ''; })
            .catch(function() { return ''; });

        Promise.all([openDb(), versionPromise]).then(function(arr) {
            var db = arr[0];
            var liveVersion = arr[1];
            sDb = db;
            return metaGet(db, VERSION_KEY).then(function(storedVersion) {
                window.__rb3IdbVersion = liveVersion;
                if (!liveVersion) {
                    console.log('[rb3-idb] no version from server — skipping cache load');
                    window.__rb3IdbReady = 1;
                    return;
                }
                if (storedVersion !== liveVersion) {
                    console.log('[rb3-idb] version changed (' + storedVersion + ' -> ' + liveVersion + ') — clearing cache');
                    return clearStore(db, STORE).then(function() {
                        return metaPut(db, VERSION_KEY, liveVersion);
                    }).then(function() {
                        window.__rb3IdbReady = 1;
                        console.log('[rb3-idb] cache cleared, version pinned, 0 rows loaded');
                    });
                }
                return loadAllRows(db).then(function(stats) {
                    window.__rb3IdbReady = 1;
                    var msg = '[rb3-idb] loaded ' + stats.rows + ' rows (' + (stats.bytes/1048576).toFixed(2) + ' MB) — version=' + liveVersion;
                    if (stats.skipped)
                        msg += '; skipped ' + stats.skipped + ' oversized row(s) (' + (stats.skippedBytes/1048576).toFixed(2) + ' MB, lazy via network on first touch)';
                    console.log(msg);
                });
            });
        }).catch(function(err) {
            console.warn('[rb3-idb] init failed: ' + err + ' — disabling cache');
            window.__rb3IdbReady = 1;  // Don't keep callers waiting forever.
        });
    })();

    // =======================================================================
    // B3 — IndexedDB SAVE persistence (separate from the asset cache above).
    //
    // The C2 IPersistBackend Read(key,buf,len) is SYNCHRONOUS (called mid-App-
    // ctor by MetaPanel's RB3SaveLoadGlobalOptions re-apply) but IndexedDB is
    // async — same async->sync problem the asset cache solves above, so we use
    // the SAME prefetch-to-JS-Map bridge. The C++ WebIdbBackend
    // (native/src/rb3_save_web.cpp) reads window.__rb3SaveCache synchronously
    // via EM_ASM_INT and writes via window.__rb3SavePut.
    //
    // CRITICAL: a SEPARATE DB ('rb3-web-saves') with NO version gate. The asset
    // cache above clearStore()s its files on an asset version-bump; saves must
    // NEVER be wiped by that, so they live in their own database. The two save
    // blobs are tiny (globaloptions.bin ~tens of bytes, gameplayopts_0.bin = 9
    // bytes), so the prewarm completes ~instantly and wins the race with the
    // App ctor.
    //
    // The whole block is wrapped in try/catch and every async path resolves
    // __rb3SaveReady=1 (even on failure) so a missing/blocked IndexedDB
    // degrades to no-persistence (empty Map, first-run defaults) and NEVER
    // blocks boot.
    // =======================================================================
    (function() {
        var SAVE_DB_NAME = 'rb3-web-saves';
        var SAVE_DB_VERSION = 1;
        var SAVE_STORE = 'saves';

        window.__rb3SaveCache = new Map();
        window.__rb3SaveReady = 0;
        window.__rb3SaveStats = { puts: 0, writeErrors: 0, rows: 0 };
        var sSaveDb = null;
        var sSavePending = [];

        function openSaveDb() {
            return new Promise(function(resolve, reject) {
                var req = indexedDB.open(SAVE_DB_NAME, SAVE_DB_VERSION);
                req.onupgradeneeded = function(e) {
                    var db = e.target.result;
                    if (!db.objectStoreNames.contains(SAVE_STORE))
                        db.createObjectStore(SAVE_STORE);
                };
                req.onsuccess = function(e) { resolve(e.target.result); };
                req.onerror = function(e) { reject(e.target.error); };
            });
        }

        function loadAllSaveRows(db) {
            return new Promise(function(resolve, reject) {
                var tx = db.transaction(SAVE_STORE, 'readonly');
                var store = tx.objectStore(SAVE_STORE);
                var rows = 0;
                var cursorReq = store.openCursor();
                cursorReq.onsuccess = function(e) {
                    var c = e.target.result;
                    if (!c) { resolve(rows); return; }
                    var v = c.value;
                    if (v && v.byteLength != null) {
                        var u8 = v instanceof Uint8Array ? v : new Uint8Array(v);
                        window.__rb3SaveCache.set(c.key, u8);
                        rows++;
                    }
                    c.continue();
                };
                cursorReq.onerror = function() { reject(cursorReq.error); };
            });
        }

        // Deferred batched put — mirrors flushPendingWrites (asset block, ~186-220).
        function flushSavePending() {
            if (!sSaveDb || sSavePending.length === 0) return;
            var batch = sSavePending;
            sSavePending = [];
            try {
                var tx = sSaveDb.transaction(SAVE_STORE, 'readwrite');
                var store = tx.objectStore(SAVE_STORE);
                for (var i = 0; i < batch.length; i++)
                    store.put(batch[i].bytes, batch[i].key);
                tx.onerror = function() { window.__rb3SaveStats.writeErrors += batch.length; };
            } catch (e) {
                window.__rb3SaveStats.writeErrors += batch.length;
            }
        }

        // Single-key put — called by the WebIdbBackend (via EM_ASM) on Write().
        // The Map.set is synchronous (the engine's sync Read sees it immediately
        // on the next boot AND in-session); the IDB transaction is a deferred
        // microtask so a burst of writes shares one tx.
        window.__rb3SavePut = function(key, bytes) {
            if (!key || !bytes) return;
            // Take a copy — the caller's HEAP view may move on next allocation.
            var copy = new Uint8Array(bytes.byteLength);
            copy.set(bytes);
            window.__rb3SaveCache.set(key, copy);
            window.__rb3SaveStats.puts++;
            if (!sSaveDb) return;  // No DB (fallback) — Map-only, lost on reload.
            sSavePending.push({ key: key, bytes: copy });
            if (sSavePending.length === 1)
                Promise.resolve().then(flushSavePending);
        };

        // Page-lifecycle save trigger. visibilitychange:hidden fires reliably
        // before a tab is closed/backgrounded (pagehide is the belt-and-
        // suspenders for navigation). The wasm main loop polls
        // __rb3SaveRequested each frame and runs RB3SaveSaveGlobalOptions().
        try {
            document.addEventListener('visibilitychange', function() {
                if (document.visibilityState === 'hidden')
                    window.__rb3SaveRequested = 1;
            });
            window.addEventListener('pagehide', function() {
                window.__rb3SaveRequested = 1;
            });
        } catch (e) { /* no document (worker) — harmless */ }

        // Pre-warm. Race the WASM download; cursor all rows into the Map then
        // flip __rb3SaveReady=1. Any failure path also sets ready=1 with an
        // empty Map (graceful no-persistence; never blocks boot).
        try {
            openSaveDb().then(function(db) {
                sSaveDb = db;
                return loadAllSaveRows(db).then(function(rows) {
                    window.__rb3SaveStats.rows = rows;
                    window.__rb3SaveReady = 1;
                    console.log('[rb3-idb-save] loaded ' + rows + ' save row(s)');
                });
            }).catch(function(err) {
                console.warn('[rb3-idb-save] init failed: ' + err + ' — no persistence this session');
                window.__rb3SaveReady = 1;
            });
        } catch (e) {
            // indexedDB undefined / threw synchronously — graceful no-persistence.
            console.warn('[rb3-idb-save] indexedDB unavailable — no persistence this session');
            window.__rb3SaveReady = 1;
        }
    })();

    // =======================================================================
    // D5 — SESSION-TELEMETRY WEB EGRESS (consumer half; Task F / SESSION_
    // TELEMETRY_DESIGN.md §7).
    //
    // The wasm recorder (native/src/rb3_session_trace.cpp) is the PRODUCER: it
    // owns serialization and publishes a FROZEN window-global contract that this
    // block drains over HTTP. We are the CONSUMER — batching + network only.
    //
    //   window.__rb3Sid      string  — C++-minted session id; the POST route key
    //                                  (/api/telemetry/<sid>). Set once mid-boot
    //                                  by WebEgressPublishSidAndReadToggle (EM_ASM).
    //   window.__rb3Trace    Array   — pending NDJSON-string CHUNKS to drain. Each
    //                                  element is one-or-more '\n'-terminated lines
    //                                  (the recorder pushes a multi-line chunk per
    //                                  ring drain to amortize the EM crossing).
    //   window.__rb3TraceOn  bool    — master toggle. Default ON (always-on); we
    //                                  set it FALSE up-front on URL opt-out
    //                                  (?trace=0 / ?notrace), which the recorder
    //                                  reads back (EM_ASM_INT) to disarm itself.
    //
    // This runs in pre-js — before _main, so the URL opt-out is honored before
    // the recorder reads __rb3TraceOn, and __rb3Trace exists before the first
    // push. Same-origin (relative URLs): the server's COOP/COEP impose no extra
    // requirement and there is no CSP. The server handler
    // (server.py do_POST -> _handle_telemetry_post) expects exactly
    //   POST /api/telemetry/<sid>   Content-Type: application/x-ndjson   body=NDJSON
    // and its localhost gate passes for same-origin dev. We match that contract.
    //
    // The whole block is best-effort: any failure leaves capture disarmed and
    // never blocks boot.
    // =======================================================================
    (function() {
        // ---- URL opt-out (?trace=0 / ?notrace). Always-on otherwise. -------
        // Set __rb3TraceOn FIRST so the C++ recorder reads the opt-out and never
        // arms. Force false only on opt-out; leave an explicit value alone; else
        // default-arm.
        var traceOff = false;
        try {
            var qs = new URLSearchParams(location.search);
            var traceParam = qs.get('trace');     // ?trace=0 / =false / =off
            var noTrace = qs.has('notrace');       // ?notrace (presence)
            if (noTrace ||
                (traceParam !== null &&
                 (traceParam === '0' || traceParam === 'false' || traceParam === 'off'))) {
                traceOff = true;
            }
        } catch (e) { /* no URLSearchParams — default on */ }

        if (traceOff) {
            window.__rb3TraceOn = false;
            console.log('[rb3-trace] disabled via URL (?trace=0 / ?notrace)');
        } else if (typeof window.__rb3TraceOn === 'undefined') {
            window.__rb3TraceOn = true;             // always-on default
        }

        // Ensure the producer array exists before the recorder creates it, so a
        // push never lands on `undefined` and our drain can splice safely.
        window.__rb3Trace = window.__rb3Trace || [];

        // ---- Tunables ------------------------------------------------------
        var FLUSH_MS = 5000;                     // periodic timer cadence
        var MAX_RETAIN_BYTES = 5 * 1024 * 1024;  // bounded retry buffer (~5MB)
        var BEACON_CAP = 60 * 1024;              // headroom under the ~64KB cap

        // Single in-flight POST guard: keeps ordering simple and avoids parallel
        // writers against the stdlib ThreadingHTTPServer (it funnels to one
        // SQLite writer thread). New chunks accumulate during the POST.
        var posting = false;
        var droppedBytes = 0;
        var droppedWarned = false;

        function endpoint() {
            return '/api/telemetry/' + encodeURIComponent(window.__rb3Sid);
        }
        function pendingBytes(chunks) {
            var n = 0;
            for (var i = 0; i < chunks.length; i++) n += chunks[i].length;
            return n;
        }

        // Re-prepend chunks after a failed POST so a transient server-down doesn't
        // lose data — bounded: if the retain buffer would exceed MAX_RETAIN_BYTES,
        // drop OLDEST (front) chunks until back under cap, warn once. Recent/crash
        // window is preferred over stale.
        function reprepend(chunks) {
            var t = window.__rb3Trace;
            if (!Array.isArray(t)) { t = window.__rb3Trace = []; }
            for (var i = chunks.length - 1; i >= 0; i--) t.unshift(chunks[i]);
            var total = pendingBytes(t);
            var dropped = 0;
            while (total > MAX_RETAIN_BYTES && t.length > 1) {
                var gone = t.shift();
                total -= gone.length;
                dropped += gone.length;
            }
            if (dropped > 0) {
                droppedBytes += dropped;
                if (!droppedWarned) {
                    console.warn('[rb3-trace] retain buffer over ' +
                        (MAX_RETAIN_BYTES / 1048576).toFixed(0) +
                        'MB while server unreachable — dropping oldest telemetry (' +
                        droppedBytes + ' bytes so far)');
                    droppedWarned = true;
                }
            }
        }

        // Periodic flusher: drain ALL pending chunks into one NDJSON body + POST.
        function flush() {
            if (!window.__rb3TraceOn) return;
            if (!window.__rb3Sid) return;        // recorder hasn't minted sid yet
            if (posting) return;                 // single in-flight
            var t = window.__rb3Trace;
            if (!Array.isArray(t) || t.length === 0) return;

            // Splice OUT pending chunks so events pushed during the POST queue up
            // separately and ship next tick. Chunks are already '\n'-terminated
            // NDJSON fragments — concatenation is valid NDJSON (no extra seps).
            var batch = t.splice(0, t.length);
            var body = batch.join('');
            if (!body) return;

            posting = true;
            fetch(endpoint(), {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-ndjson' },
                body: body,
                keepalive: true
            }).then(function(r) {
                posting = false;
                if (!r || !r.ok) reprepend(batch);   // 4xx/5xx -> retry next tick
            }).catch(function() {
                posting = false;
                reprepend(batch);                    // network fail -> retry next tick
            });
        }

        // ---- Teardown — beacon the tail on hide/close ----------------------
        // visibilitychange:hidden is the PRIMARY tail flush (fires reliably on
        // tab-hide/close while the page can still send); pagehide is the belt-
        // and-suspenders for navigation. sendBeacon survives unload where fetch
        // may not; it hard-rejects bodies over the UA cap (~64KB) by returning
        // false, so we split the tail into multiple <=60KB beacons.
        function beaconTail() {
            if (!window.__rb3TraceOn) return;
            if (!window.__rb3Sid) return;
            // Drain the C++ recorder ring into window.__rb3Trace FIRST so the last
            // <30 frames (accumulated since the last periodic BOOT_RUNNING flush)
            // are captured in this final beacon rather than lost on unload. Guarded
            // — the wasm export may not exist yet very early in boot.
            try {
                if (typeof Module !== 'undefined' && Module._rb3_trace_flush)
                    Module._rb3_trace_flush();
            } catch (e) { /* pre-runtime / unload race — ignore */ }
            var t = window.__rb3Trace;
            if (!Array.isArray(t) || t.length === 0) return;
            if (!navigator.sendBeacon) { flush(); return; }  // no beacon -> fetch
            var batch = t.splice(0, t.length);
            var url = endpoint();

            // Pack chunks into <=BEACON_CAP bodies; one beacon each. Always take
            // at least one chunk so we make progress even if a single chunk > cap.
            var i = 0;
            while (i < batch.length) {
                var parts = [];
                var size = 0;
                do {
                    parts.push(batch[i]);
                    size += batch[i].length;
                    i++;
                } while (i < batch.length && (size + batch[i].length) <= BEACON_CAP);
                var body = parts.join('');
                try {
                    var ok = navigator.sendBeacon(
                        url, new Blob([body], { type: 'application/x-ndjson' }));
                    if (!ok) {
                        // Beacon refused (over cap / queue full) -> keepalive fetch.
                        fetch(url, {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/x-ndjson' },
                            body: body,
                            keepalive: true
                        }).catch(function() { /* unload — nothing else to do */ });
                    }
                } catch (e) { /* unload race — ignore */ }
            }
        }

        // ---- Wire it up ----------------------------------------------------
        try {
            setInterval(flush, FLUSH_MS);
            document.addEventListener('visibilitychange', function() {
                if (document.visibilityState === 'hidden') beaconTail();
            });
            window.addEventListener('pagehide', beaconTail);
            console.log('[rb3-trace] egress consumer armed (on=' +
                (window.__rb3TraceOn ? '1' : '0') + ', flush=' + FLUSH_MS + 'ms)');
        } catch (e) {
            console.warn('[rb3-trace] egress wiring failed: ' + e);
        }
    })();
})();

// ─── SESSION-TELEMETRY TIER-1 REPLAY (web source) ────────────────────────────
// When the URL carries ?replay=<sid>, fetch the recorded session's NDJSON from
// GET /api/telemetry/<sid> (the server-side fetch-back, reconstructed from the
// SQLite `events` log) into window.__rb3ReplayData BEFORE the wasm reads it.
// rb3_replay.cpp's RB3ReplayInit() (called from the JoypadPoll hook, lazily) then
// reads this string via EM_ASM and parses the `in` events into its frame->bits
// table — identical data shape to the native RB3_REPLAY=<file> path.
//
// This is best-effort + compile-guarded; BROWSER RUN-VERIFICATION IS DEFERRED
// (no web build this wave). It only arms when ?replay= is present, so a normal
// web session is unaffected. The fetch is fire-and-forget: it kicks off at
// page-load and resolves into __rb3ReplayData well before boot reaches the first
// JoypadPoll. If it is still pending at first poll, replay simply stays inactive
// that run (RB3ReplayInit is once-only) — acceptable for v1 / deferred verify.
(function() {
    try {
        var m = /[?&]replay=([^&]+)/.exec(location.search || '');
        if (!m) return;
        var sid = decodeURIComponent(m[1]);
        if (!sid) return;
        window.__rb3ReplayData = '';   // present-but-empty until the fetch lands
        var url = '/api/telemetry/' + encodeURIComponent(sid);
        fetch(url, { method: 'GET', headers: { 'Accept': 'application/x-ndjson' } })
            .then(function(r) {
                if (!r.ok) throw new Error('HTTP ' + r.status);
                return r.text();
            })
            .then(function(text) {
                window.__rb3ReplayData = text || '';
                var lines = (text ? text.split('\n').length : 0);
                console.log('[rb3-replay] loaded session ' + sid + ' (' +
                    lines + ' lines) into window.__rb3ReplayData');
            })
            .catch(function(e) {
                console.warn('[rb3-replay] fetch of ' + url + ' failed: ' + e);
            });
    } catch (e) {
        console.warn('[rb3-replay] setup failed: ' + e);
    }
})();
