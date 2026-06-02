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

#ifdef __EMSCRIPTEN__
// Web-only: the sidecar files are not bundled into the wasm/preload (180MB).
// They are served live by native/web/server.py and fetched ON DEMAND into
// MEMFS the first time a given XMA SFX plays. WebAssetsFetchSync is the same
// synchronous-XHR hook native_file.cpp uses for the rest of the web asset path
// (engine-exported header; resolves via milo-engine's PUBLIC src include dir).
#include "platform/WebAssets.h"
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

// Sidecar directory, relative to the current working dir (the engine chdir's to
// RB3_DATA at boot, so "sfx/gen/xma_pcm" sits next to the banks). Override with
// RB3_SFX_PCM_DIR (absolute or cwd-relative).
inline std::string SidecarDir() {
    if (const char *d = getenv("RB3_SFX_PCM_DIR")) return std::string(d);
    return std::string("sfx/gen/xma_pcm");
}

// Try to load a sidecar for the given raw XMA payload. Returns a SidecarPCM with
// data==nullptr if none is found (caller falls back to skipping the sample).
inline SidecarPCM TryLoad(const void *xmaData, int sizeBytes, int sampleRate) {
    SidecarPCM out;
    if (!xmaData || sizeBytes <= 0) return out;

    uint64_t key = PayloadKey(xmaData, sizeBytes, sampleRate);
    char keyHex[24];
    std::snprintf(keyHex, sizeof(keyHex), "%016llx",
                  static_cast<unsigned long long>(key));
    std::string path = SidecarDir() + "/" + keyHex + ".pcm";

    FILE *f = std::fopen(path.c_str(), "rb");
#ifdef __EMSCRIPTEN__
    // Web on-demand fetch: under emcc the sidecar lives on the dev server, not
    // in MEMFS, so the first open misses. Fetch it (one synchronous XHR per
    // distinct SFX — sidecars are 19KB-1.5MB) into MEMFS and retry. A warm
    // MEMFS (already-fetched this session) skips the XHR entirely. Mirrors the
    // miss-then-fetch ordering in native_file.cpp. Compiled out natively.
    if (!f) {
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
#endif
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
