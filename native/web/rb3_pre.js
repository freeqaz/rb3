// rb3-web pre-js: replace Emscripten's "missing function: X" abort-stub
// imports with silent no-op stubs so the RB3 matched-fork's static initializers
// don't trap with `unreachable` when they reach Wii-only globals (PlatformMgr,
// TheNetSession, TheMC, TheSynth, RndMesh::sLastCollide, etc.).
//
// Background: rb3-web links with `-Wl,--allow-undefined`, leaving those
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
        return fetch(wasmPath)
            .then(function(resp) { return resp.arrayBuffer(); })
            .then(function(bytes) { return WebAssembly.instantiate(bytes, imports); })
            .then(function(result) {
                successCallback(result.instance, result.module);
            })
            .catch(function(err) {
                console.error('[rb3-pre] instantiateWasm failed: ' + err);
            });
    };

    if (typeof Module === 'undefined') {
        globalThis.Module = origModule;
    } else {
        Module = origModule;
    }
})();
