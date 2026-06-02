// xma_convert_main.cpp — offline RB3/DC3 SFX XMA->PCM sidecar converter.
//
// Approach B (sidecar + runtime glue): for every kXMA SampleData blob embedded
// in the input `.milo_xbox` SFX banks, decode the Xbox-360 XMA2 payload to
// 16-bit little-endian PCM (via DC3's already-validated DecodeXMAToPCM, ffmpeg
// xma2 codec) and write a sidecar `.pcm` file into a DERIVED directory. The
// original banks are NEVER mutated — this avoids the milo container re-pack /
// offset-fixup risk entirely. The runtime SampleInst glue (rb3_sampleinst_native.cpp
// for RB3; the engine SampleInst path for DC3) loads the matching sidecar when
// it sees a kXMA sample, keyed by a bank-independent content hash of the XMA
// payload (so it works without knowing which bank the sample came from).
//
// Sidecar file layout (little-endian):
//   magic   "RB3PCM01" (8 bytes)
//   int32   sampleRate
//   int32   numSamples (mono, 16-bit)
//   int32   numChannels (always 1 for RB3; DC3 may pass 2)
//   int32   reserved (0)
//   int16[] interleaved PCM
// Sidecar filename: <hexkey>.pcm  where hexkey = payload_key(xma, size, rate).
//
// Also writes a human-readable manifest.txt (key -> bank/sample/format/samples).
//
// Build: native/tools/xma_repack/CMakeLists.txt (links pkg-config libav +
// DC3's XmaSampleDecoder.cpp compiled with HX_FFMPEG). Reproducible CLI:
//   rb3-xma-convert <out-dir> <bank.milo_xbox> [<bank2.milo_xbox> ...]
// or via the wrapper script scripts/assets/convert_xma_banks.sh.

#ifndef HX_FFMPEG
#error "xma-convert must be built with HX_FFMPEG (libav). See CMakeLists.txt."
#endif

#include "milo_sample_scan.h"
#include "platform/XmaSampleDecoder.h" // DecodeXMAToPCM (DC3, gated HX_FFMPEG)

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path, "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz > 0) {
        v.resize((size_t)sz);
        if (fread(v.data(), 1, (size_t)sz, f) != (size_t)sz) v.clear();
    }
    fclose(f);
    return v;
}

bool write_file(const std::string& path, const void* data, size_t len) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = (fwrite(data, 1, len, f) == len);
    fclose(f);
    return ok;
}

void put_le32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xff);
    v.push_back((x >> 8) & 0xff);
    v.push_back((x >> 16) & 0xff);
    v.push_back((x >> 24) & 0xff);
}

// RMS of int16 interleaved PCM, normalized to [0,1].
double pcm_rms(const int16_t* pcm, int count) {
    if (count <= 0) return 0.0;
    double acc = 0.0;
    for (int i = 0; i < count; i++) {
        double s = pcm[i] / 32768.0;
        acc += s * s;
    }
    return std::sqrt(acc / count);
}

const char* fmt_name(int f) {
    switch (f) {
        case milo_scan::kPCM: return "kPCM";
        case milo_scan::kBigEndPCM: return "kBigEndPCM";
        case milo_scan::kVAG: return "kVAG";
        case milo_scan::kXMA: return "kXMA";
        case milo_scan::kATRAC: return "kATRAC";
        case milo_scan::kMP3: return "kMP3";
        case milo_scan::kNintendoADPCM: return "kNintendoADPCM";
        default: return "?";
    }
}

const char* basename_of(const char* p) {
    const char* b = strrchr(p, '/');
    return b ? b + 1 : p;
}

} // namespace

int main(int argc, char** argv) {
    // Optional leading mode flag. --dc3 selects DC3's SampleData layout (rev 0x10
    // with mCRC + per-blob numChannels) instead of RB3's (rev <= 0xE, mono, with
    // a SynthSample loop tail). Default (no flag) = RB3. Either way the sidecar
    // key + file format are identical, so one shared out-dir serves both engines.
    bool dc3 = false;
    int argi = 1;
    if (argc > 1 && std::strcmp(argv[1], "--dc3") == 0) { dc3 = true; argi = 2; }
    else if (argc > 1 && std::strcmp(argv[1], "--rb3") == 0) { dc3 = false; argi = 2; }
    if (getenv("XMA_CONVERT_DC3")) dc3 = true;

    if (argc - argi < 2) {
        fprintf(stderr,
                "usage: %s [--dc3|--rb3] <out-dir> <bank.milo_xbox> [<bank2> ...]\n"
                "  Decodes every kXMA SampleData blob to a <hexkey>.pcm sidecar\n"
                "  in <out-dir>; writes <out-dir>/manifest.txt. Originals untouched.\n"
                "  --dc3 : parse DC3 SampleData (rev 0x10, mCRC, per-blob channels)\n"
                "  --rb3 : parse RB3 SampleData (default; mono SFX banks)\n",
                argv[0]);
        return 2;
    }
    std::string outDir = argv[argi++];
    mkdir(outDir.c_str(), 0755); // ignore EEXIST

    // RB3 banks are mono; DC3 carries per-blob numChannels (used directly below).
    // For RB3 the channel count can still be overridden via RB3_XMA_CHANNELS.
    int channelsArg = 1;
    if (const char* c = getenv("RB3_XMA_CHANNELS")) channelsArg = atoi(c);
    if (channelsArg < 1) channelsArg = 1;

    std::string manifest;
    manifest += "# bank\tsample\tformat\tnumSamples\tsampleRate\tchannels\tdecodedSamples\trms\tkey\n";

    int totalBanks = 0, totalXma = 0, converted = 0, failed = 0, silent = 0;

    for (int ai = argi; ai < argc; ai++) {
        const char* bankPath = argv[ai];
        std::vector<uint8_t> bank = read_file(bankPath);
        if (bank.empty()) {
            fprintf(stderr, "WARN: cannot read or empty: %s\n", bankPath);
            continue;
        }
        std::vector<milo_scan::SampleBlob> blobs;
        bool scanned = dc3
            ? milo_scan::scan_bank_dc3(bank.data(), bank.size(), blobs)
            : milo_scan::scan_bank(bank.data(), bank.size(), blobs);
        if (!scanned) {
            fprintf(stderr, "WARN: not an uncompressed milo (skipped): %s\n", bankPath);
            continue;
        }
        totalBanks++;
        const char* bankName = basename_of(bankPath);

        int bankXma = 0;
        for (const auto& b : blobs) {
            if (b.format != milo_scan::kXMA) continue;
            bankXma++;
            totalXma++;

            // dataOffset is relative to the object stream (after the 0x810
            // ChunkInfo header); recover the absolute pointer into the file.
            uint32_t infoSize = milo_scan::rd_le32(bank.data() + 4);
            const uint8_t* xmaPtr = bank.data() + infoSize + b.dataOffset;

            // DC3: numChannels parsed per blob. RB3: mono (or RB3_XMA_CHANNELS).
            // The KEY never uses channels (matches the runtime PayloadKey), only
            // the payload bytes + sizeBytes + sampleRate.
            int chans = dc3 ? b.numChannels : channelsArg;
            if (chans < 1) chans = 1;

            uint64_t key = milo_scan::payload_key(xmaPtr, b.sizeBytes, b.sampleRate);

            void* pcm = nullptr;
            int pcmSize = 0;
            bool ok = DecodeXMAToPCM(xmaPtr, b.sizeBytes, b.numSamples,
                                     b.sampleRate, chans, &pcm, &pcmSize);
            if (!ok || !pcm || pcmSize <= 0) {
                fprintf(stderr,
                        "FAIL decode: %s :: %s (xma %d bytes, %d samples @ %d Hz)\n",
                        bankName, b.file.c_str(), b.sizeBytes, b.numSamples, b.sampleRate);
                failed++;
                if (pcm) free(pcm);
                continue;
            }

            int decodedSamples = pcmSize / (int)sizeof(int16_t); // total int16 values
            double rms = pcm_rms((const int16_t*)pcm, decodedSamples);

            // Validation 1: RMS > 0 (not pure silence). A bad extradata guess or
            // wrong codec id decodes to all-zero — reject those loudly.
            if (rms <= 0.0) {
                fprintf(stderr,
                        "FAIL silence: %s :: %s decoded to RMS=0 (%d samples)\n",
                        bankName, b.file.c_str(), decodedSamples);
                silent++;
                failed++;
                free(pcm);
                continue;
            }
            // Validation 2: decoded sample count sane vs stored mNumSamples.
            // XMA encodes in blocks so the decoded count can exceed mNumSamples
            // by up to ~one frame (512) per stream; warn (don't fail) if it is
            // wildly off (>2x or <0.5x), which signals a misparse.
            int perChanDecoded = decodedSamples / chans;
            if (b.numSamples > 0 &&
                (perChanDecoded > b.numSamples * 2 + 4096 ||
                 perChanDecoded < b.numSamples / 2)) {
                fprintf(stderr,
                        "WARN count: %s :: %s decoded=%d stored=%d (>2x mismatch)\n",
                        bankName, b.file.c_str(), perChanDecoded, b.numSamples);
            }

            // Sidecar: header + PCM. numChannels matches the decode (DC3 stereo
            // round-trips; the runtime reads it back as side.numChannels).
            std::vector<uint8_t> sidecar;
            sidecar.insert(sidecar.end(), {'R','B','3','P','C','M','0','1'});
            put_le32(sidecar, (uint32_t)b.sampleRate);
            put_le32(sidecar, (uint32_t)perChanDecoded);   // per-channel sample count
            put_le32(sidecar, (uint32_t)chans);
            put_le32(sidecar, 0);
            const uint8_t* pcmBytes = (const uint8_t*)pcm;
            sidecar.insert(sidecar.end(), pcmBytes, pcmBytes + pcmSize);

            char keyHex[24];
            snprintf(keyHex, sizeof(keyHex), "%016llx", (unsigned long long)key);
            std::string outPath = outDir + "/" + keyHex + ".pcm";
            if (!write_file(outPath, sidecar.data(), sidecar.size())) {
                fprintf(stderr, "FAIL write: %s\n", outPath.c_str());
                failed++;
                free(pcm);
                continue;
            }

            char line[512];
            snprintf(line, sizeof(line),
                     "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%.4f\t%s\n",
                     bankName, b.file.c_str(), fmt_name(b.format),
                     b.numSamples, b.sampleRate, chans, perChanDecoded, rms, keyHex);
            manifest += line;

            converted++;
            free(pcm);
        }
        printf("bank %-28s : %d XMA blobs (%zu total samples)\n",
               bankName, bankXma, blobs.size());
    }

    std::string manifestPath = outDir + "/manifest.txt";
    write_file(manifestPath, manifest.data(), manifest.size());

    printf("\n=== XMA->PCM conversion summary ===\n");
    printf("banks scanned     : %d\n", totalBanks);
    printf("kXMA blobs found  : %d\n", totalXma);
    printf("converted (sidecars): %d\n", converted);
    printf("failed            : %d (silent: %d)\n", failed, silent);
    printf("manifest          : %s\n", manifestPath.c_str());
    printf("out dir           : %s\n", outDir.c_str());

    return (failed == 0) ? 0 : 1;
}
