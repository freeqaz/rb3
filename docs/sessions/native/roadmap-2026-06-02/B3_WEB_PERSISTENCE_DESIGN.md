# B3 — Web Persistence Backend (behind the C2 IPersistBackend)

## Goal
Make RB3 global options + profile-0 GameplayOptions survive a browser reload, by
slotting an IndexedDB-backed `IPersistBackend` behind the **existing** C2 interface
with **zero** changes to the ProfileMgr-layer glue. No matched-fork edits; all
changes are native glue gated `#ifdef HX_WEB`.

## What already exists (read these first)
- `native/src/rb3_save_native.h` — `IPersistBackend { Read(key,buf,len); Write(key,buf,len); }`
  (synchronous, blob-by-key), `RB3PersistBackend()`, `RB3SetPersistBackend(IPersistBackend*)`,
  and the ProfileMgr glue `RB3SaveLoadGlobalOptions()` / `RB3SaveSaveGlobalOptions()`.
- `native/src/rb3_save_native.cpp` — `HostFsBackend` (fopen/fread/fwrite). **Read enforces an
  EXACT size match** (`fileSize != len` → reject) so a stale/foreign blob never reaches the
  rev-asserting deserializer. The web backend MUST keep this gate.
- The async↔sync problem is **already solved in this tree** for assets:
  - `native/web/rb3_pre.js:114-258` — pre-warms a JS `Map<string,Uint8Array>`
    (`window.__rb3IdbCache`) from IDB **before** wasm init, sets `window.__rb3IdbReady=1`,
    and defines `window.__rb3CachePut(path,bytes)` (batched fire-and-forget IDB write).
  - `native/src/native_file.cpp:39-86` — reads that Map **synchronously** via `EM_ASM_INT`
    and writes back via `EM_ASM`. This is the exact idiom to mirror.

## Two blockers that must be fixed (not just "add a backend")
1. **rb3_save_native.cpp is not in the web build.** It lives in the `NATIVE_SHIMS` set()
   (`native/CMakeLists.txt:354`, the file listed at :513), which feeds **rb3-native only**.
   The web target sources are `RB3_WEB_NATIVE_GLUE` (:619, consumed at :713) and grep shows
   `rb3_save_native.cpp` appears nowhere else. So today rb3-web links **no** IPersistBackend,
   no HostFsBackend, no load/save symbols → zero persistence.
2. **The web boot never calls load/save.** `main_web.cpp` BOOT_APP_CTOR (:351-369) does
   `new App(...)` then BOOT_RUNNING; it never calls `RB3SaveLoadGlobalOptions()` nor
   registers the exit save. Those wirings exist only in `main_native.cpp` RunGame
   (:648-650 exit save, :714 boot load).

## Recommended approach: pre-warmed JS Map (NOT Asyncify, NOT IDBFS)
Reconcile async IDB with the synchronous `Read()` at boot by mirroring the asset cache:
a tiny **separate** IDB store pre-warmed into a JS Map before wasm init; sync read from the
Map; fire-and-forget write-back. Two save blobs only (`globaloptions.bin` ≈ tens of bytes,
`gameplayopts_0.bin` = 9 bytes) → the prewarm is instant and always wins the race with the
App ctor.

Rejected alternatives:
- **Asyncify/JSPI block-on-promise** — `MILO_WEB_ASYNC`/JSPI exists (CMakeLists :810-814) but
  unwinding the stack inside `Read()` mid-App-ctor is fragile and unnecessary for ~50 bytes.
- **IDBFS mount + syncfs** — the engine has **no** IDBFS/syncfs support anywhere
  (grep of milo-native-engine = empty); syncfs is async and the boot read is sync. The
  JS-Map precedent is already proven in production for assets. Don't introduce a new mechanism.

### New: `native/web/rb3_save_idb.js` (a second `--pre-js`)
Structurally a trimmed copy of rb3_pre.js's IDB block, but a **distinct DB** so an asset
version-bump (which `clearStore`s the asset store, rb3_pre.js:240-247) never wipes saves:
- DB `rb3-web-saves`, store `saves`. Key = the same short filename keys the C2 layer already
  uses: `globaloptions.bin`, `gameplayopts_0.bin` (rb3_save_native.cpp:147-148).
- `preWarm()`: open DB, cursor every row into `window.__rb3SaveCache` (Map), set
  `window.__rb3SaveReady = 1`. **No version gate** — saves persist across builds; the C2
  reader's exact-size gate already rejects an incompatible blob and falls back to defaults.
- `window.__rb3SavePut(key, bytes)`: copy the HEAP view (it can move), `map.set`, push a
  deferred batched IDB `put` (mirror flushPendingWrites, rb3_pre.js:186-220).
- Wrap all of it in try/catch; on failure set `__rb3SaveReady = 1` with an empty map
  (graceful no-persistence, never blocks the boot).

### New: `native/src/rb3_save_web.cpp` (`#ifdef HX_WEB`)
```
class WebIdbBackend : public IPersistBackend {
  bool Read(key,buf,len):  EM_ASM_INT — if(!__rb3SaveReady) return 0;
     b=__rb3SaveCache.get(key); if(!b||b.byteLength!==len) return 0;   // EXACT-size gate
     HEAPU8.set(b, bufPtr); return 1;
  bool Write(key,buf,len): EM_ASM — if(!__rb3SavePut) return;
     __rb3SavePut(key, HEAPU8.subarray(bufPtr,bufPtr+len));            // fire-and-forget
};
static WebIdbBackend gWebIdbBackend;
void RB3InstallWebPersistBackend(){ RB3SetPersistBackend(&gWebIdbBackend); }
```
EM_ASM pointer/HEAP idiom copied verbatim from native_file.cpp:40-86.

## Save/load call ordering vs the ProfileMgr::Init clobber
The native ordering is already designed around two clobbers; replicate it on web:
1. **`ProfileMgr::Init()`** (run inside `MetaPanel::Init`, MetaPanel.cpp:288) resets options to
   defaults — notably `SetExcessVideoLag(0)` (ProfileMgr.cpp:111). So a load done *before*
   Init is wiped.
2. The matched fork already re-applies after Init: **MetaPanel.cpp:323-333** (HX_NATIVE arm,
   so it compiles for web too — HX_WEB defines HX_NATIVE per CMakeLists :736) calls
   `RB3SaveLoadGlobalOptions()` immediately after `TheProfileMgr.Init()`. **This is the
   authoritative load on web** — it runs after the clobber. Do not duplicate it earlier in a
   way that fights it.

Therefore in `main_web.cpp` BOOT_APP_CTOR:
- **Before** `new App(...)`: `RB3InstallWebPersistBackend();` (so gPersist is the web backend
  before any Read fires — the MetaPanel re-apply happens during the App ctor).
- **After** the App ctor returns: call `RB3SaveLoadGlobalOptions()` once more (idempotent;
  exact-size-gated; mirrors main_native.cpp:714, harmless belt-and-suspenders that also
  covers profile-0 GameplayOptions). Net effect = identical to native.

## Save trigger (exit) on web
`TheDebug` exit callbacks never fire in the browser (no `exit()`; the runtime lives via
`emscripten_exit_with_live_runtime`, main_web.cpp:478). Replace the exit-callback save with a
page-lifecycle hook in `main_web.cpp` BOOT_APP_CTOR:
- `EM_ASM({ addEventListener('pagehide', () => Module._rb3SaveOnExit()); addEventListener('visibilitychange', ...hidden → save); })`,
  plus export an `extern "C" rb3SaveOnExit()` that calls `RB3SaveSaveGlobalOptions()`.
- Because `Write()` is synchronous into the JS Map (the IDB flush is deferred), saving on
  `visibilitychange:hidden` (fires reliably before tab close on mobile/desktop) guarantees the
  Map + a queued IDB put exist before unload. Also save on each options-confirm if a hook is
  cheap (optional; defense in depth), but pagehide/visibilitychange is the baseline.

## CMake wiring (DESIGN ONLY — native/CMakeLists.txt is LOCKED this task)
For the implementer, add to `RB3_WEB_NATIVE_GLUE` (:619):
`${CMAKE_SOURCE_DIR}/src/rb3_save_native.cpp` and `${CMAKE_SOURCE_DIR}/src/rb3_save_web.cpp`.
And to rb3-web `target_link_options` (next to rb3_pre.js, :808):
`"SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/web/rb3_save_idb.js"`.
`rb3_save_native.cpp`'s `_MemAllocTemp`/FixedSizeSaveableStream/ProfileMgr deps already link
in rb3-web (ProfileMgr + meta compile there). HostFsBackend's fopen path is dead code under
web (we install WebIdbBackend), but the TU's gPersist plumbing + the ProfileMgr glue are what
we need; it compiles fine under emcc (POSIX stdio is MEMFS).

## Fallback when IndexedDB is unavailable
Three layers, all already implied by the design:
1. `rb3_save_idb.js` try/catch on `indexedDB.open` (private-mode/SAB-policy failures) → sets
   `__rb3SaveReady=1` with an empty Map → `Read()` returns false → C2 keeps ctor defaults
   (first-run path, identical to a missing file on native). No crash, no block.
2. `Write()` no-ops if `__rb3SavePut` is undefined (IDB never opened) — options just don't
   persist; gameplay is unaffected.
3. The exact-size gate is the last line of defense against a corrupt/foreign blob.

## Testing without manual browser clicking
`scripts/web/save-persistence-test.py` (Playwright or puppeteer headless Chrome — needs
WebGPU + COOP/COEP from server.py, already sent at server.py:50-51):
1. Serve via `python3 native/web/server.py`; open `/` headless.
2. Wait for `window.rb3AppBooted`, navigate to options and toggle a persisted option
   (drive via the existing keyboard-input path, or expose a tiny test-only `EM_ASM`/exported
   setter — same harness style as the W3c smokes that poll `window.rb3CurrentScreen`).
3. Trigger save: dispatch `visibilitychange` (hidden) or call `Module._rb3SaveOnExit()`.
4. Assert `window.__rb3SaveCache.get('globaloptions.bin')` is a Uint8Array of the expected
   size; assert `window.__rb3CacheStats`-style put count if added.
5. `page.reload()`; wait for boot; assert the toggled option survived (poll the same getter
   used in step 2).
Plus a fast **pure-node** unit (no browser): require `fake-indexeddb`, load the IDB helpers
from `rb3_save_idb.js`, round-trip a blob through put→prewarm→get. And a **fallback** test:
`Object.defineProperty(window,'indexedDB',{get:()=>undefined})` then boot → assert no crash,
`__rb3SaveReady===1`, empty Map.
In-tab manual check (documented): DevTools console `window.__rb3SaveCache`,
`indexedDB.databases()` shows `rb3-web-saves`.

## Out of scope / non-goals
- No server-side save store (server.py untouched). Saves are browser-local IDB.
- No resurrection of the Wii SaveLoadManager/MemcardMgr async state machine (same exclusion
  rationale as C2; it is never Poll()'d, native or web).
- No engine (milo-native-engine) edits — no MILO_ENGINE_PIN bump required.
- No matched-fork `src/**` edits: MetaPanel.cpp's re-apply (already present, HX_NATIVE) is
  reused as-is; all new code is native glue under `#ifdef HX_WEB`.