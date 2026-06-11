// rb3_xma_sidecar.h — runtime loader for offline-converted XMA->PCM sidecars.
//
// Web has no runtime ffmpeg, so kXMA SFX are converted OFFLINE to little-endian
// 16-bit PCM sidecar files (native/tools/xma_repack/rb3-xma-convert). At play
// time, when an RB3 SampleInst sees a kXMA SampleData, it asks this loader for
// the matching sidecar (keyed by a bank-independent content hash of the raw XMA
// payload) and plays that PCM instead. Works native AND web (HX_NATIVE).
//
// Sidecar key + file format must stay in lockstep with the converter
// (native/tools/xma_repack/milo_sample_scan.h payload_key + xma_convert_main.cpp).
#pragma once

#ifdef HX_NATIVE

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h> // stat (native sidecar-dir auto-probe)
#include <vector>     // ogg read-into-memory buffer

// W5-T2: vorbis SFX sidecars. The offline converter emits a <hexkey>.ogg next to
// each <hexkey>.pcm (libvorbis q4, ~10% of the raw PCM bytes), keyed by the SAME
// PayloadKey. We try the .ogg first and decode it with vendored stb_vorbis into
// the identical SidecarPCM contract, falling back to .pcm on miss/decode-fail.
//
// stb_vorbis.h is header-only by default; the SINGLE TU that defines
// RB3_STB_VORBIS_IMPL (rb3_sampleinst_native.cpp) compiles the implementation so
// the decoder symbols link exactly once. All other includers (App.cpp,
// main_native.cpp) get just the declarations. We use only the pulldata
// open-memory API (we read the file/MEMFS bytes ourselves), so push-data + the
// stdio file API are compiled out to keep the impl lean.
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#ifndef RB3_STB_VORBIS_IMPL
#define STB_VORBIS_HEADER_ONLY
#endif
#include "stb_vorbis.h"
#ifdef STB_VORBIS_HEADER_ONLY
#undef STB_VORBIS_HEADER_ONLY
#endif

#ifdef __EMSCRIPTEN__
// Web-only: the sidecar files are not bundled into the wasm/preload (180MB).
// They are served live by native/web/server.py and fetched ON DEMAND into
// MEMFS the first time a given XMA SFX plays. WebAssetsFetchSync is the same
// synchronous-XHR hook native_file.cpp uses for the rest of the web asset path
// (engine-exported header; resolves via milo-engine's PUBLIC src include dir).
#include "platform/WebAssets.h"
#include <map>
#include <set>
#include <vector>
#endif

namespace rb3_xma {

// FNV-1a over the raw codec payload, mixed with size + sample rate. MUST match
// milo_scan::payload_key in the offline converter exactly.
inline uint64_t PayloadKey(const void *data, int sizeBytes, int sampleRate) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < sizeBytes; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(sizeBytes));
    h *= 1099511628211ULL;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(sampleRate));
    h *= 1099511628211ULL;
    return h;
}

// Decoded PCM owned by the caller (free() the data). Empty (data==nullptr) if
// no sidecar exists for this payload.
struct SidecarPCM {
    int16_t *data = nullptr; // interleaved 16-bit LE, malloc'd
    int numSamples = 0;      // per-channel sample count
    int numChannels = 0;
    int sampleRate = 0;
    int byteSize = 0;        // total PCM byte size
};

// W5-T2 flag: prefer the compact <hex>.ogg sidecar (default ON). Set
// RB3_SFX_OGG_OFF=1 to force the legacy raw .pcm path (A/B arm). getenv-once.
// The web URL bridge (?env=RB3_SFX_OGG_OFF=1) sets this getenv before boot.
inline bool SfxOggEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        sEnabled = 1; // default ON
        if (const char *e = getenv("RB3_SFX_OGG_OFF"))
            if (e[0] && e[0] != '0')
                sEnabled = 0;
    }
    return sEnabled != 0;
}

// Sidecar directory. Override with RB3_SFX_PCM_DIR (absolute or cwd-relative);
// otherwise resolved per-platform below.
inline std::string SidecarDir() {
    if (const char *d = getenv("RB3_SFX_PCM_DIR")) return std::string(d);
#ifdef __EMSCRIPTEN__
    // Web: the path is virtual. server.py rewrites any ".../xma_pcm/<hex>.pcm"
    // request to the real sidecar dir, and the on-demand fetch in TryLoad anchors
    // it under /data (MEMFS is empty until that first fetch, so do NOT probe it).
    return std::string("sfx/gen/xma_pcm");
#else
    // Native: the sidecars live in a DERIVED tree that is NOT next to the banks
    // (cwd is RB3_DATA = .../orig-assets/extracted, but the sidecars are at
    // .../orig-assets/derived/sfx_pcm). Resolve once by probing a few known
    // locations relative to cwd so SFX play out-of-box without an env override.
    static const std::string resolved = [] {
        const char *candidates[] = {
            "sfx/gen/xma_pcm",             // legacy: sidecars dropped beside the banks
            "../derived/sfx_pcm",          // repo layout: extracted/ + derived/ are siblings
            "orig-assets/derived/sfx_pcm", // run from the repo root
        };
        struct stat st;
        for (const char *c : candidates)
            if (stat(c, &st) == 0 && S_ISDIR(st.st_mode))
                return std::string(c);
        return std::string("sfx/gen/xma_pcm"); // legacy default if nothing resolves
    }();
    return resolved;
#endif
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------------------
// Q8 (incremental-load): per-bank async sidecar prefetch.
//
// Without this, the first play of each distinct XMA SFX does one BLOCKING sync
// XHR (19KB-1.5MB) on the wasm main thread (the WebAssetsFetchSync miss path in
// TryLoad) — one frame hitch per distinct SFX. With it: the first sidecar
// request from a bank triggers an ASYNC fetch (WebAssetsFetch, off the main
// thread, lands in MEMFS) of every sidecar that bank converted, so by the time
// later SFX in that bank play the bytes are already resident and the sync miss
// path short-circuits (warm MEMFS). The sync fetch stays as the per-key
// fallback for the very first request (and any mispredicted/uncovered key).
//
// The converter (native/tools/xma_repack) emits manifest.txt next to the
// sidecars: tab-separated `bank<TAB>sample<TAB>fmt<TAB>...<TAB>keyHex`. We load
// it once (small, one sync fetch) to learn each bank's key set, then prefetch
// per bank on demand. RB3_XMA_PREFETCH_OFF disables (default ON).
//
// State lives in inline-function-local statics (single instance across TUs per
// the inline-function ODR), so this stays header-only — no .cpp / CMake change.
inline bool PrefetchEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        sEnabled = 1; // default ON
        if (const char *e = getenv("RB3_XMA_PREFETCH_OFF"))
            if (e[0] && e[0] != '0')
                sEnabled = 0;
    }
    return sEnabled != 0;
}

// keyHex -> bank name (parsed from manifest.txt), and bank name -> all its
// keyHexes. Loaded lazily on the first sidecar request.
struct BankIndex {
    std::map<std::string, std::string> keyToBank;
    std::map<std::string, std::vector<std::string> > bankToKeys;
    std::set<std::string> prefetchedBanks;  // banks already async-prefetched
    std::set<std::string> prefetchedKeys;   // keys already async-requested
    bool loaded = false;
    bool loadFailed = false;
};
inline BankIndex &TheBankIndex() {
    static BankIndex idx;
    return idx;
}

// MEMFS server-path for a sidecar key (relative; resolves under /data cwd).
// W5-T2: prefetch the same extension the warm-load path will open first — .ogg
// when the vorbis sidecars are enabled, else the legacy raw .pcm — so the
// already-resident bytes match the format the sync miss path expects.
inline std::string SidecarServerPath(const std::string &keyHex) {
    const char *ext = SfxOggEnabled() ? ".ogg" : ".pcm";
    return SidecarDir() + "/" + keyHex + ext;
}

// Load + parse manifest.txt once (small sync fetch). Returns false if absent (a
// checkout without converted sidecars) so prefetch silently no-ops.
inline bool EnsureManifestLoaded() {
    BankIndex &idx = TheBankIndex();
    if (idx.loaded || idx.loadFailed)
        return idx.loaded;

    std::string rel = SidecarDir() + "/manifest.txt";
    std::string memfs = rel;
    if (!memfs.empty() && memfs[0] != '/')
        memfs = "/data/" + memfs;

    FILE *f = std::fopen(rel.c_str(), "rb");
    if (!f) {
        // Not yet in MEMFS — one small sync fetch (manifest is a few KB-100s KB,
        // tiny vs the per-sidecar 19KB-1.5MB it lets us prefetch ahead of need).
        if (WebAssetsFetchSync(memfs.c_str()))
            f = std::fopen(rel.c_str(), "rb");
    }
    if (!f) {
        idx.loadFailed = true;
        return false;
    }

    std::string content;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        content.append(buf, n);
    std::fclose(f);

    // Parse: bank<TAB>...<TAB>keyHex per line; skip '#' comment header.
    size_t pos = 0;
    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        if (eol == std::string::npos)
            eol = content.size();
        std::string line = content.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.empty() || line[0] == '#')
            continue;
        size_t firstTab = line.find('\t');
        size_t lastTab = line.rfind('\t');
        if (firstTab == std::string::npos || lastTab == firstTab)
            continue;
        std::string bank = line.substr(0, firstTab);
        std::string keyHex = line.substr(lastTab + 1);
        // trim trailing CR/whitespace from keyHex
        while (!keyHex.empty() &&
               (keyHex.back() == '\r' || keyHex.back() == ' ' || keyHex.back() == '\t'))
            keyHex.pop_back();
        if (bank.empty() || keyHex.empty())
            continue;
        idx.keyToBank[keyHex] = bank;
        idx.bankToKeys[bank].push_back(keyHex);
    }
    idx.loaded = true;
    return true;
}

// On the first sidecar request from a bank, async-prefetch the whole bank's
// sidecars so later SFX from it are already MEMFS-resident. Cheap + idempotent.
inline void MaybePrefetchBank(const std::string &keyHex) {
    if (!PrefetchEnabled())
        return;
    if (!EnsureManifestLoaded())
        return;
    BankIndex &idx = TheBankIndex();
    std::map<std::string, std::string>::const_iterator bit = idx.keyToBank.find(keyHex);
    if (bit == idx.keyToBank.end())
        return; // key not in manifest — sync fallback handles it
    const std::string &bank = bit->second;
    if (!idx.prefetchedBanks.insert(bank).second)
        return; // already prefetched this bank
    std::map<std::string, std::vector<std::string> >::const_iterator kit =
        idx.bankToKeys.find(bank);
    if (kit == idx.bankToKeys.end())
        return;
    for (size_t i = 0; i < kit->second.size(); i++) {
        const std::string &k = kit->second[i];
        if (!idx.prefetchedKeys.insert(k).second)
            continue; // already requested
        WebAssetsFetch(SidecarServerPath(k).c_str()); // async, lands in MEMFS
    }
}
#endif // __EMSCRIPTEN__

// Open a sidecar file by full path, returning a FILE* (caller fclose's) or null.
// On web, a MEMFS miss triggers one synchronous XHR into MEMFS and a retry — the
// same miss-then-fetch ordering native_file.cpp uses; compiled out natively.
// keyHex is only used to drive the per-bank async prefetch (web).
inline FILE *OpenSidecarFile(const std::string &path, const char *keyHex) {
    FILE *f = std::fopen(path.c_str(), "rb");
#ifdef __EMSCRIPTEN__
    if (!f) {
        // Q8: kick an async prefetch of this key's whole bank (idempotent) so the
        // NEXT distinct SFX from this bank is already MEMFS-resident and skips the
        // blocking sync XHR below. The current key may already be in flight from a
        // prior MaybePrefetchBank — but it is not guaranteed ready yet, so we still
        // do the sync fetch as the per-key miss fallback (it short-circuits if the
        // async prefetch already landed these bytes in MEMFS).
        if (keyHex)
            MaybePrefetchBank(std::string(keyHex));

        // WebAssetsFetchSync writes the fetched bytes to the MEMFS path it is
        // handed and mkdir's the ABSOLUTE parent chain (/data/sfx/...). The
        // default web SidecarDir() is relative ("sfx/gen/xma_pcm"), which fopen
        // resolves against the FS cwd (/data). Anchor the fetch path under /data
        // so the mkdir/writeFile and this fopen agree on one absolute location.
        std::string fetchPath = path;
        if (!fetchPath.empty() && fetchPath[0] != '/')
            fetchPath = "/data/" + fetchPath;
        if (WebAssetsFetchSync(fetchPath.c_str()))
            f = std::fopen(path.c_str(), "rb");
    }
#else
    (void)keyHex;
#endif
    return f;
}

// Read an entire open file into a byte buffer (used for the whole-stream ogg
// decode). Returns false on read error.
inline bool ReadAllBytes(FILE *f, std::vector<uint8_t> &buf) {
    if (std::fseek(f, 0, SEEK_END) != 0) return false;
    long sz = std::ftell(f);
    if (std::fseek(f, 0, SEEK_SET) != 0) return false;
    if (sz <= 0) return false;
    buf.resize(static_cast<size_t>(sz));
    return std::fread(buf.data(), 1, buf.size(), f) == buf.size();
}

// Decode a whole in-memory Ogg/Vorbis stream into the SidecarPCM contract
// (16-bit interleaved malloc'd buffer). Returns an empty SidecarPCM on any
// decode failure so the caller falls back to .pcm. The vorbis sidecar carries no
// RB3PCM01 header — channels/rate come from the stream itself (the converter
// encoded the SAME per-channel sample count / rate / channels as the .pcm).
inline SidecarPCM DecodeOggBuffer(const uint8_t *bytes, int len) {
    SidecarPCM out;
    if (!bytes || len <= 0) return out;
    int err = 0;
    stb_vorbis *v = stb_vorbis_open_memory(bytes, len, &err, nullptr);
    if (!v) return out;
    stb_vorbis_info info = stb_vorbis_get_info(v);
    int ch = info.channels;
    int sr = static_cast<int>(info.sample_rate);
    if (ch <= 0 || ch > 8 || sr <= 0) {
        stb_vorbis_close(v);
        return out;
    }
    unsigned int perChan = stb_vorbis_stream_length_in_samples(v);
    if (perChan == 0 || perChan > 0x7fffffffu / static_cast<unsigned int>(ch)) {
        stb_vorbis_close(v);
        return out;
    }
    int total = static_cast<int>(perChan) * ch; // total interleaved shorts
    int16_t *pcm = static_cast<int16_t *>(std::malloc(total * sizeof(int16_t)));
    if (!pcm) {
        stb_vorbis_close(v);
        return out;
    }
    // get_samples_short_interleaved fills up to num_shorts and returns the number
    // of per-channel frames written; it may stop a frame or two short of the
    // reported stream length, so zero-pad any tail and trust the written count.
    int framesWritten =
        stb_vorbis_get_samples_short_interleaved(v, ch, pcm, total);
    stb_vorbis_close(v);
    if (framesWritten <= 0) {
        std::free(pcm);
        return out;
    }
    if (framesWritten < static_cast<int>(perChan)) {
        int writtenShorts = framesWritten * ch;
        std::memset(pcm + writtenShorts, 0,
                    (total - writtenShorts) * sizeof(int16_t));
    }
    out.data = pcm;
    out.numSamples = static_cast<int>(perChan); // per-channel count
    out.numChannels = ch;
    out.sampleRate = sr;
    out.byteSize = total * static_cast<int>(sizeof(int16_t));
    return out;
}

// Try to load the compact <hex>.ogg vorbis sidecar. Empty SidecarPCM on miss or
// decode failure (caller falls back to .pcm). Web fetch-on-miss is handled by
// OpenSidecarFile exactly like the .pcm path.
inline SidecarPCM TryLoadOgg(const std::string &keyHex) {
    SidecarPCM out;
    std::string path = SidecarDir() + "/" + keyHex + ".ogg";
    FILE *f = OpenSidecarFile(path, keyHex.c_str());
    if (!f) return out;
    std::vector<uint8_t> bytes;
    bool ok = ReadAllBytes(f, bytes);
    std::fclose(f);
    if (!ok) return out;
    return DecodeOggBuffer(bytes.data(), static_cast<int>(bytes.size()));
}

// Try to load a sidecar for the given raw XMA payload. Returns a SidecarPCM with
// data==nullptr if none is found (caller falls back to skipping the sample).
// W5-T2: prefer the compact <hex>.ogg vorbis sidecar (default ON); on miss/
// decode-fail fall through to the legacy raw <hex>.pcm. RB3_SFX_OGG_OFF=1 skips
// the .ogg attempt entirely (A/B arm — byte-identical to the pre-W5 .pcm path).
inline SidecarPCM TryLoad(const void *xmaData, int sizeBytes, int sampleRate) {
    SidecarPCM out;
    if (!xmaData || sizeBytes <= 0) return out;

    uint64_t key = PayloadKey(xmaData, sizeBytes, sampleRate);
    char keyHex[24];
    std::snprintf(keyHex, sizeof(keyHex), "%016llx",
                  static_cast<unsigned long long>(key));

    if (SfxOggEnabled()) {
        SidecarPCM ogg = TryLoadOgg(std::string(keyHex));
        if (ogg.data) return ogg;
        // miss/decode-fail → fall through to the raw .pcm sidecar below.
    }

    std::string path = SidecarDir() + "/" + keyHex + ".pcm";
    FILE *f = OpenSidecarFile(path, keyHex);
    if (!f) return out;

    // Header: magic(8) sampleRate(i32) numSamples(i32) numChannels(i32) rsvd(i32)
    uint8_t hdr[24];
    if (std::fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        std::memcmp(hdr, "RB3PCM01", 8) != 0) {
        std::fclose(f);
        return out;
    }
    auto le32 = [](const uint8_t *p) -> int {
        return static_cast<int>(static_cast<uint32_t>(p[0]) |
                                (static_cast<uint32_t>(p[1]) << 8) |
                                (static_cast<uint32_t>(p[2]) << 16) |
                                (static_cast<uint32_t>(p[3]) << 24));
    };
    int sr = le32(hdr + 8);
    int ns = le32(hdr + 12);
    int ch = le32(hdr + 16);
    if (ns <= 0 || ch <= 0 || ch > 8) {
        std::fclose(f);
        return out;
    }
    int bytes = ns * ch * static_cast<int>(sizeof(int16_t));
    void *pcm = std::malloc(bytes);
    if (!pcm) {
        std::fclose(f);
        return out;
    }
    if (static_cast<int>(std::fread(pcm, 1, bytes, f)) != bytes) {
        std::free(pcm);
        std::fclose(f);
        return out;
    }
    std::fclose(f);

    out.data = static_cast<int16_t *>(pcm);
    out.numSamples = ns;
    out.numChannels = ch;
    out.sampleRate = sr;
    out.byteSize = bytes;
    return out;
}

} // namespace rb3_xma

#endif // HX_NATIVE
