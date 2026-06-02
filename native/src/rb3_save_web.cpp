// RB3 Web Port — B3 IndexedDB-backed persistence backend.
//
// Slots an IndexedDB-backed IPersistBackend behind the EXISTING C2
// IPersistBackend interface (rb3_save_native.h) with ZERO ProfileMgr-glue
// changes — the ProfileMgr load/save bodies in rb3_save_native.cpp are reused
// verbatim (that TU is compiled into rb3-web alongside this one). Only the
// storage layer is swapped, via RB3SetPersistBackend().
//
// WHY prefetch-to-JS-Map (NOT IDBFS+syncfs, NOT Asyncify):
//   The C2 Read(key,buf,len) is SYNCHRONOUS — it is called mid-App-ctor (via
//   MetaPanel's in-ctor RB3SaveLoadGlobalOptions re-apply, MetaPanel.cpp:332),
//   but IndexedDB is fundamentally async. The async->sync bridge is already
//   proven in production for assets: native/web/rb3_pre.js pre-warms a
//   Map<string,Uint8Array> from IDB BEFORE wasm init and sets a ready flag;
//   native_file.cpp reads it synchronously via EM_ASM_INT. We mirror that for
//   saves with a SEPARATE DB ('rb3-web-saves') so an asset version-bump never
//   wipes saves. The two blobs are tiny (globaloptions.bin ~tens of bytes,
//   gameplayopts_0.bin = 9 bytes), so the prewarm completes instantly and
//   always wins the race with the App ctor.
//   IDBFS rejected: milo-native-engine has no IDBFS/syncfs support and syncfs
//   is async — wrong shape for the sync boot Read. Asyncify rejected: unwinding
//   the stack inside Read() mid-ctor is fragile and pointless for ~50 bytes.
//
// The EM_ASM_INT/HEAPU8.set pointer idiom is copied from native_file.cpp:66-95.
// The JS layer (window.__rb3SaveCache / __rb3SaveReady / __rb3SavePut) lives in
// native/web/rb3_pre.js.

#ifdef HX_WEB

#include "rb3_save_native.h"

#include <emscripten/em_asm.h>

namespace {

// WebIdbBackend — IPersistBackend over the JS save cache (window.__rb3SaveCache),
// which is pre-warmed from the 'rb3-web-saves' IndexedDB store before wasm init.
//
// Read() enforces an EXACT size match (byteLength === len), identical to
// HostFsBackend: a same-build blob is always exactly `len` bytes (rev-locked),
// so any mismatch -> reject. This keeps a stale/foreign/short blob from ever
// reaching the rev-asserting LoadGlobalOptions deserializer.
class WebIdbBackend : public IPersistBackend {
public:
    bool Read(const char *key, void *buf, int len) override {
        if (len <= 0)
            return false;
        // EM_ASM_INT returns 1 on a size-matched hit (bytes already copied into
        // the wasm heap at bufPtr), else 0. Pointer/HEAP idiom from
        // native_file.cpp:66-95.
        int ok = EM_ASM_INT({
            try {
                if (!window.__rb3SaveReady) return 0;
                if (!window.__rb3SaveCache) return 0;
                var key = UTF8ToString($0);
                var b = window.__rb3SaveCache.get(key);
                // EXACT-size gate — reject anything whose length doesn't match
                // the rev-locked expected size. Mirrors HostFsBackend::Read.
                if (!b || b.byteLength !== $2) return 0;
                HEAPU8.set(b, $1);
                return 1;
            } catch (e) {
                console.log('[rb3-idb-save] read failed: ' + e);
                return 0;
            }
        }, key, buf, len);
        return ok != 0;
    }

    bool Write(const char *key, const void *buf, int len) override {
        if (len <= 0)
            return false;
        // Synchronous into the JS Map (the IDB flush is a deferred microtask
        // put inside __rb3SavePut). HEAPU8.subarray gives a zero-copy view that
        // __rb3SavePut immediately copies, so the bytes are captured before any
        // subsequent wasm allocation moves the heap.
        EM_ASM({
            try {
                if (!window.__rb3SavePut) return;
                var key = UTF8ToString($0);
                window.__rb3SavePut(key, HEAPU8.subarray($1, $1 + $2));
            } catch (e) {
                console.log('[rb3-idb-save] write failed: ' + e);
            }
        }, key, buf, len);
        return true;
    }
};

WebIdbBackend gWebIdbBackend;

} // namespace

// Install the IDB-backed backend so gPersist is the web backend BEFORE the App
// ctor (and MetaPanel's in-ctor RB3SaveLoadGlobalOptions re-apply) fires any
// Read. Called from main_web.cpp BOOT_APP_CTOR, before `new App`.
void RB3InstallWebPersistBackend() { RB3SetPersistBackend(&gWebIdbBackend); }

#endif // HX_WEB
