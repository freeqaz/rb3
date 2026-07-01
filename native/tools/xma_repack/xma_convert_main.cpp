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

// W5-T2: also encode a compact <hexkey>.ogg (libvorbis) next to each .pcm so the
// web wire ships ~10% of the raw PCM bytes. Uses the already-linked libav.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

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

// Quality knob for the vorbis sidecars: maps to ffmpeg's `-q:a` (libvorbis VBR
// 0..10). Plan §4.2 picked q4 (~11% of PCM, well above the source XMA fidelity).
// Override via RB3_OGG_QUALITY for A/B tuning.
double ogg_quality() {
    if (const char* q = getenv("RB3_OGG_QUALITY")) {
        double v = atof(q);
        if (v >= -1.0 && v <= 10.0) return v;
    }
    return 4.0;
}

// Encode interleaved int16 PCM -> in-memory Ogg/Vorbis (libvorbis VBR @ quality).
// Returns the encoded bytes, or empty on failure (caller still has the .pcm).
// The runtime stb_vorbis decoder recovers numSamples/channels/rate straight from
// the stream, so this carries the SAME per-channel sample count / rate / channels
// the .pcm header records (the decode is the single source of truth).
std::vector<uint8_t> encode_vorbis_ogg(const int16_t* pcm, int perChanSamples,
                                       int channels, int sampleRate, double quality) {
    std::vector<uint8_t> result;
    if (!pcm || perChanSamples <= 0 || channels < 1 || channels > 8 || sampleRate <= 0)
        return result;

    const AVCodec* enc = avcodec_find_encoder_by_name("libvorbis");
    if (!enc) {
        fprintf(stderr, "ogg: libvorbis encoder unavailable in this libav build\n");
        return result;
    }

    AVCodecContext* cctx = avcodec_alloc_context3(enc);
    if (!cctx) return result;

    cctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // libvorbis only accepts planar float
    cctx->sample_rate = sampleRate;
    av_channel_layout_default(&cctx->ch_layout, channels);
    // VBR quality: ffmpeg maps -q:a N to global_quality = N * FF_QP2LAMBDA.
    cctx->flags |= AV_CODEC_FLAG_QSCALE;
    cctx->global_quality = (int)lround(quality * FF_QP2LAMBDA);

    AVFormatContext* ofmt = nullptr;
    AVStream* st = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    bool headerWritten = false;
    uint8_t* dynbuf = nullptr;

    auto cleanup = [&](bool freeDyn) {
        if (frame) av_frame_free(&frame);
        if (pkt) av_packet_free(&pkt);
        if (ofmt) {
            if (ofmt->pb) {
                int sz = avio_close_dyn_buf(ofmt->pb, &dynbuf);
                ofmt->pb = nullptr;
                if (freeDyn && sz >= 0 && dynbuf) {
                    result.assign(dynbuf, dynbuf + sz);
                }
                if (dynbuf) { av_free(dynbuf); dynbuf = nullptr; }
            }
            avformat_free_context(ofmt);
            ofmt = nullptr;
        }
        if (cctx) avcodec_free_context(&cctx);
    };

    if (avcodec_open2(cctx, enc, nullptr) < 0) { cleanup(false); return result; }

    if (avformat_alloc_output_context2(&ofmt, nullptr, "ogg", nullptr) < 0 || !ofmt) {
        cleanup(false); return result;
    }
    if (avio_open_dyn_buf(&ofmt->pb) < 0) { cleanup(false); return result; }

    st = avformat_new_stream(ofmt, nullptr);
    if (!st) { cleanup(false); return result; }
    if (avcodec_parameters_from_context(st->codecpar, cctx) < 0) { cleanup(false); return result; }
    st->time_base = (AVRational){1, sampleRate};

    if (avformat_write_header(ofmt, nullptr) < 0) { cleanup(false); return result; }
    headerWritten = true;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (!pkt || !frame) { cleanup(false); return result; }

    // Vorbis is variable-frame; use the encoder's preferred frame size (or a
    // sane default if it reports 0). Each frame is planar float per channel.
    int frameSize = cctx->frame_size > 0 ? cctx->frame_size : 1024;
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = sampleRate;
    av_channel_layout_copy(&frame->ch_layout, &cctx->ch_layout);
    frame->nb_samples = frameSize;
    if (av_frame_get_buffer(frame, 0) < 0) { cleanup(false); return result; }

    auto write_packets = [&]() -> bool {
        while (true) {
            int ret = avcodec_receive_packet(cctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
            if (ret < 0) return false;
            pkt->stream_index = st->index;
            av_packet_rescale_ts(pkt, cctx->time_base.num ? cctx->time_base
                                                          : (AVRational){1, sampleRate},
                                 st->time_base);
            int w = av_interleaved_write_frame(ofmt, pkt);
            av_packet_unref(pkt);
            if (w < 0) return false;
        }
    };

    bool ok = true;
    int64_t pts = 0;
    int pos = 0;
    while (pos < perChanSamples && ok) {
        int n = perChanSamples - pos;
        if (n > frameSize) n = frameSize;
        if (av_frame_make_writable(frame) < 0) { ok = false; break; }
        frame->nb_samples = n;
        for (int c = 0; c < channels; c++) {
            float* dst = reinterpret_cast<float*>(frame->data[c]);
            for (int s = 0; s < n; s++)
                dst[s] = pcm[(size_t)(pos + s) * channels + c] * (1.0f / 32768.0f);
        }
        frame->pts = pts;
        pts += n;
        pos += n;
        if (avcodec_send_frame(cctx, frame) < 0) { ok = false; break; }
        if (!write_packets()) { ok = false; break; }
    }
    // Flush.
    if (ok) {
        if (avcodec_send_frame(cctx, nullptr) < 0) ok = false;
        else if (!write_packets()) ok = false;
    }
    if (ok && headerWritten) {
        if (av_write_trailer(ofmt) < 0) ok = false;
    }

    cleanup(ok); // on success, harvests the dyn-buf bytes into `result`
    if (!ok) result.clear();
    return result;
}

} // namespace

int main(int argc, char** argv) {
    // Optional leading mode flag. --dc3 selects DC3's SampleData layout (rev 0x10
    // with mCRC + per-blob numChannels) instead of RB3's (rev <= 0xE, mono, with
    // a SynthSample loop tail). Default (no flag) = RB3. Either way the sidecar
    // key + file format are identical, so one shared out-dir serves both engines.
    bool dc3 = false;
    bool oggDisabled = false; // W5-T2: skip the .ogg sibling (raw-.pcm-only run)
    int argi = 1;
    // Leading flags (any order before the out-dir): --dc3/--rb3, --no-ogg.
    while (argc > argi) {
        if (std::strcmp(argv[argi], "--dc3") == 0) { dc3 = true; argi++; }
        else if (std::strcmp(argv[argi], "--rb3") == 0) { dc3 = false; argi++; }
        else if (std::strcmp(argv[argi], "--no-ogg") == 0) { oggDisabled = true; argi++; }
        else break;
    }
    if (getenv("XMA_CONVERT_DC3")) dc3 = true;
    if (getenv("RB3_NO_OGG")) oggDisabled = true;
    const double oggQuality = ogg_quality();

    if (argc - argi < 2) {
        fprintf(stderr,
                "usage: %s [--dc3|--rb3] <out-dir> <bank.milo_xbox> [<bank2> ...]\n"
                "  Decodes every kXMA SampleData blob to a <hexkey>.pcm sidecar\n"
                "  AND a compact <hexkey>.ogg (libvorbis VBR) sibling in <out-dir>;\n"
                "  writes <out-dir>/manifest.txt. Originals untouched.\n"
                "  --dc3    : parse DC3 SampleData (rev 0x10, mCRC, per-blob channels)\n"
                "  --rb3    : parse RB3 SampleData (default; mono SFX banks)\n"
                "  --no-ogg : emit only .pcm (skip the vorbis sibling)\n"
                "  env RB3_OGG_QUALITY (default 4, 0..10) tunes the vorbis VBR level\n",
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
    int oggWritten = 0, oggFailed = 0;        // W5-T2 vorbis sidecar tallies
    long long oggBytes = 0, pcmBytes = 0;     // for the ratio in the summary

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
            const uint8_t* pcmRaw = (const uint8_t*)pcm;
            sidecar.insert(sidecar.end(), pcmRaw, pcmRaw + pcmSize);

            char keyHex[24];
            snprintf(keyHex, sizeof(keyHex), "%016llx", (unsigned long long)key);
            std::string outPath = outDir + "/" + keyHex + ".pcm";
            if (!write_file(outPath, sidecar.data(), sidecar.size())) {
                fprintf(stderr, "FAIL write: %s\n", outPath.c_str());
                failed++;
                free(pcm);
                continue;
            }

            // W5-T2: also emit the compact <hexkey>.ogg sibling (libvorbis VBR).
            // Same key, same per-channel sample count / rate / channels as the PCM
            // header — the runtime tries .ogg first, falls back to .pcm. A failed
            // encode is non-fatal: the .pcm above is the guaranteed fallback.
            if (!oggDisabled) {
                std::vector<uint8_t> ogg =
                    encode_vorbis_ogg((const int16_t*)pcm, perChanDecoded, chans,
                                      b.sampleRate, oggQuality);
                if (!ogg.empty()) {
                    std::string oggPath = outDir + "/" + keyHex + ".ogg";
                    if (write_file(oggPath, ogg.data(), ogg.size())) {
                        oggWritten++;
                        oggBytes += (long long)ogg.size();
                        pcmBytes += (long long)sidecar.size();
                    } else {
                        fprintf(stderr, "WARN ogg write: %s\n", oggPath.c_str());
                        oggFailed++;
                    }
                } else {
                    fprintf(stderr, "WARN ogg encode failed: %s :: %s\n",
                            bankName, b.file.c_str());
                    oggFailed++;
                }
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
    if (!oggDisabled) {
        double ratio = pcmBytes > 0 ? 100.0 * (double)oggBytes / (double)pcmBytes : 0.0;
        printf("ogg sidecars      : %d written, %d failed (q=%.1f)\n",
               oggWritten, oggFailed, oggQuality);
        printf("ogg vs pcm bytes  : %.2f MB ogg / %.2f MB pcm = %.1f%%\n",
               oggBytes / 1048576.0, pcmBytes / 1048576.0, ratio);
    }
    printf("manifest          : %s\n", manifestPath.c_str());
    printf("out dir           : %s\n", outDir.c_str());

    // ogg encode failures are non-fatal (the .pcm fallback is always present), so
    // they don't flip the exit code; only PCM decode/write failures do.
    return (failed == 0) ? 0 : 1;
}
