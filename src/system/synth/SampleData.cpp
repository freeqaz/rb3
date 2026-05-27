#include "synth/SampleData.h"
#include "os/PlatformMgr.h"
#include "utl/ChunkStream.h"
#include "utl/WaveFile.h"
#ifdef HX_NATIVE
#include <cstdlib>

// On native the game may not register a SampleData allocator (sAlloc/sFree are
// set by the platform audio layer, which the engine bypasses). Fall back to the
// host malloc/free so sample loading and teardown don't dereference a null
// function pointer.
static void *SampleDataAllocNative(int size, const char *) { return malloc(size); }
static void SampleDataFreeNative(void *p) { free(p); }
#endif

SampleDataAllocFunc SampleData::sAlloc = 0;
SampleDataFreeFunc SampleData::sFree = 0;
BinStream &operator>>(BinStream &bs, SampleMarker &m);

DECOMP_FORCEACTIVE(SampleData, __FILE__, "0")

void SampleData::SetAllocator(SampleDataAllocFunc a, SampleDataFreeFunc f) {
    sAlloc = a;
    sFree = f;
}

SampleData::SampleData() : mData(0), mMarkers() { Reset(); }

SampleData::~SampleData() {
#ifdef HX_NATIVE
    (sFree ? sFree : SampleDataFreeNative)(mData);
#else
    sFree(mData);
#endif
}

void SampleData::Load(BinStream &bs, const FilePath &fp) {
    Reset();
    int rev;
    bs >> rev;
    if (rev > 0xE) {
        if (rev - 0x3e9U <= 0x24606) {
            MILO_LOG("%s: loading old cached sample\n", fp);
            mSampleRate = rev;
            bs >> mSizeBytes;
            mFormat = kBigEndPCM;
            mNumSamples = mSizeBytes / 2;
#ifdef HX_NATIVE
            mData = (sAlloc ? sAlloc : SampleDataAllocNative)(mSizeBytes, fp.c_str());
#else
            mData = sAlloc(mSizeBytes, fp.c_str());
#endif
            MILO_ASSERT(!((uint)mData & 31), 0x6A);
            bs.Read(mData, mSizeBytes);
        } else
            MILO_WARN("can't load new SampleData: %s", fp);
    } else {
        int fmt;
        bs >> fmt >> mNumSamples >> mSampleRate >> mSizeBytes;
        bool b = true;
        mFormat = (Format)fmt;
        if (rev >= 0xB) {
            bs >> b;
        }
        if (b) {
            if (ThePlatformMgr.AreSFXEnabled()) {
#ifdef HX_NATIVE
                mData = (sAlloc ? sAlloc : SampleDataAllocNative)(mSizeBytes, fp.c_str());
#else
                mData = sAlloc(mSizeBytes, fp.c_str());
#endif
                ReadChunks(bs, mData, mSizeBytes, 0x8000);
            } else {
                bs.Seek(mSizeBytes, BinStream::kSeekCur);
                mNumSamples = 0;
                mSizeBytes = 0;
                mData = 0;
            }
        }
        if (rev >= 0xE)
            bs >> mMarkers;
    }
}

// https://decomp.me/scratch/XPqxP
void SampleData::LoadWAV(BinStream &bs, const FilePath &fp) {
    Reset();
    WaveFile wav(bs);
    if (wav.NumChannels() != 1) {
        MILO_WARN("Wave file %s has multiple channels", fp);
        return;
    } else if (wav.BitsPerSample() != 0x10) {
        MILO_WARN("Wave file %s is not 16-bit", fp);
        return;
    } else if (wav.Format() != 1) {
        MILO_WARN("Wave file %s is compressed", fp);
        return;
    } else {
        mFormat = kPCM;
        mNumSamples = wav.NumSamples();
        mSampleRate = wav.SamplesPerSec();
        mSizeBytes = SizeAs(kPCM);
#ifdef HX_NATIVE
        mData = (sAlloc ? sAlloc : SampleDataAllocNative)(mSizeBytes, fp.c_str());
#else
        mData = sAlloc(mSizeBytes, fp.c_str());
#endif
        WaveFileData wavdata(wav);
        wavdata.Read(mData, mSizeBytes);
        for (int i = 0; i < wav.NumMarkers(); i++) {
            mMarkers.push_back(SampleMarker(wav.mMarkers[i].mName, wav.mMarkers[i].mFrame)
            );
        }
    }
}

DECOMP_FORCEACTIVE(SampleData, "size % 192 == 0")

void SampleData::Reset() {
#ifdef HX_NATIVE
    (sFree ? sFree : SampleDataFreeNative)(mData);
#else
    sFree(mData);
#endif
    mData = 0;
    mFormat = kPCM;
    mSizeBytes = 0;
    mSampleRate = 0;
    mNumSamples = 0;
    mMarkers.clear();
}

int SampleData::SizeAs(Format fmt) const {
    switch (fmt) {
    case kPCM: {
        return mNumSamples * 2;
    }
    case kBigEndPCM: {
        return mNumSamples * 2;
    }
    case kVAG: {
        return ((mNumSamples + 0x6F) / 0x70) * 0x40;
    }
    case kATRAC: {
        return ((mNumSamples + 0x3FF) / 0x400) * 0xC0;
    }
    case kMP3: {
        return ((mNumSamples + 0x3FF) / 0x400) * 0xC0;
    }
    case kXMA: {
        MILO_WARN("don't know size as XMA");
        return mNumSamples / 5;
    }
    case kNintendoADPCM: {
        return (int)((float)(mNumSamples * 2) / 3.4f) + 0x60;
    }
    default: {
        MILO_ASSERT(0, 0x136);
        return 0;
    }
    }
}

int SampleData::NumMarkers() const { return mMarkers.size(); }
const SampleMarker &SampleData::GetMarker(int i) const { return mMarkers[i]; }

BinStream &operator>>(BinStream &bs, SampleMarker &m) {
    m.Load(bs);
    return bs;
}
