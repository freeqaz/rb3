#include "synth/VorbisReader.h"
#include "obj/DataFile.h"
#include "KeyChain.h"
#include "synth/Synth.h"
#include "utl/BufStream.h"
#include "os/Endian.h"
#include "decomp.h"

namespace {
    static unsigned char gKey[256];
    static int gCipher = -1;
    static int gKeySize = 16;
    static unsigned char gRB1Key[16] = { 0x37, 0xB2, 0xE2, 0xB9, 0x1C, 0x74, 0xFA, 0x9E,
                                         0x38, 0x81, 0x08, 0xEA, 0x36, 0x23, 0xDB, 0xE4 };
}

#define VORBIS_FAIL(name, err)                                                           \
    MILO_WARN("Ogg Vorbis failure: %s, error code %i", name, err);

void VorbisReader::setupCypher(int moggVersion) {
    char script[256];
    unsigned char masterKey[256];
#ifdef HX_NATIVE
    // On native, bypass the DTA obfuscation for masterKey initialization.
    // The matched path passes (int)masterKey through DTA pointer math as an
    // anti-tamper measure; on LP64 that truncates the 64-bit pointer and
    // corrupts the key. Call getMasher directly to populate masterKey instead.
    KeyChain::getMasher(masterKey);
    MILO_LOG("MOGG_DBG: setupCypher entry moggVersion=0x%X mKeyIndex=%ld mMagicA=0x%lX mMagicB=0x%lX\n",
             moggVersion, (long)mKeyIndex, (long)mMagicA, (long)mMagicB);
    MILO_LOG("MOGG_DBG: mNonce=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", mNonce[_i]);
    MILO_LOG("\nMOGG_DBG: mKeyMask=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", mKeyMask[_i]);
    MILO_LOG("\nMOGG_DBG: masterKey[0:16]=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", masterKey[_i]);
    MILO_LOG("\n");
#else
    DataArray *arr = DataReadString("{Na 42 'O32'}");
    unsigned int iEval = arr->Evaluate(0).Int();
    arr->Release();

    char i6 = (iEval % 13);
    i6 = i6 + 'A';
    sprintf(script, "{%c %d %c}", i6, (int)masterKey ^ iEval, i6);
    DataArray *buf118Arr = DataReadString(script);
    buf118Arr->Evaluate(0);
    buf118Arr->Release();
#endif
    KeyChain::getKey(mKeyIndex, gKey, masterKey);
#ifdef HX_NATIVE
    MILO_LOG("MOGG_DBG: gKey AFTER getKey=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", gKey[_i]);
    MILO_LOG("\n");
#endif
    TheSynth->mGrinder.GrindArray(mMagicA, mMagicB, gKey, 0x10, moggVersion);
#ifdef HX_NATIVE
    MILO_LOG("MOGG_DBG: gKey AFTER GrindArray=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", gKey[_i]);
    MILO_LOG("\n");
#endif
    for (int i = 0; i < 16; i++) {
        gKey[i] ^= mKeyMask[i];
    }
#ifdef HX_NATIVE
    MILO_LOG("MOGG_DBG: gKey FINAL (XOR mKeyMask)=");
    for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", gKey[_i]);
    MILO_LOG("\n");
#endif
    int ret = ctr_start(gCipher, mNonce, gKey, gKeySize, 0, mCtrState);
    memset(gKey, 0, gKeySize);
    MILO_ASSERT(ret == 0, 0xAA);

#ifdef HX_NATIVE
    extern int magicNumberGeneratorNative(int idx, int mode);
    mMagicHashA = magicNumberGeneratorNative(mMagicA, 1);
    mMagicHashB = magicNumberGeneratorNative(mMagicB, 2);
#else
    sprintf(script, "{ha %d 1}", mMagicA);
    DataArray *magicGenA = DataReadString(script);
    mMagicHashA = magicGenA->Evaluate(0).Int();
    magicGenA->Release();

    sprintf(script, "{ha %d 2}", mMagicB);
    DataArray *magicGenB = DataReadString(script);
    mMagicHashB = magicGenB->Evaluate(0).Int();
    magicGenB->Release();
#endif
}

VorbisReader::VorbisReader(
    File *vorbisFile, bool expectMap, StandardStream *stream, bool b4
)
    : mNumChannels(-1), mSampleRate(-1), mFile(vorbisFile), mHeadersRead(0),
      mReadBuffer(0), mEnableReads(1), unk38(0), unk3c(0), mDone(0), mStream(stream),
      mOggSync(0), mOggStream(0), mVorbisInfo(0), mVorbisComment(0), mVorbisDsp(0),
      mVorbisBlock(0), unk98(0), mSeekTarget(-1), mSamplesToSkip(0), mHdrSize(0),
      mHdrBuf(0), mCtrState(0), unke0(b4), unke1(0), unke2(0), mFail(0),
      mThreadBufferStart(-1), unkf8(0) {
    MILO_ASSERT(mFile, 0xE6);
    if (expectMap) {
        mHdrBuf = new char[60000];
        mFile->ReadAsync(mHdrBuf, 60000);
        mFail = mFile->Fail();
    }
    mOggSync = new ogg_sync_state();
    ogg_sync_init(mOggSync);
}

VorbisReader::~VorbisReader() {
    delete[] mHdrBuf;
    mHdrBuf = 0;
    if (mOggStream)
        ogg_stream_clear(mOggStream);
    if (mVorbisBlock)
        vorbis_block_clear(mVorbisBlock);
    if (mVorbisDsp)
        vorbis_dsp_clear(mVorbisDsp);
    if (mVorbisComment)
        vorbis_comment_clear(mVorbisComment);
    if (mVorbisInfo)
        vorbis_info_clear(mVorbisInfo);
    ogg_sync_clear(mOggSync);
    RELEASE(mVorbisBlock);
    RELEASE(mVorbisDsp);
    RELEASE(mVorbisComment);
    RELEASE(mVorbisInfo);
    RELEASE(mOggStream);
    RELEASE(mOggSync);
    RELEASE(mCtrState);
}

#ifndef HX_NATIVE
void VorbisReader::Poll(float until) {
    if (!mFail && !unk3c && CheckHmxHeader() && !mDone && (mSeekTarget < 0 || DoSeek())) {
        DoFileRead();
        unke2 = mFile->Eof();
        if (mHeadersRead < 3) {
            while (TryReadHeader())
                ;
            if (mHeadersRead >= 3) {
                mNumChannels = mVorbisInfo->channels;
                mSampleRate = mVorbisInfo->rate;
                Init();
                InitDecoder();
            }
        } else {
            Timer timer;
            timer.Start();
            bool b1 = !unke0;
            while (Timer::CyclesToMs(timer.mCycles) < until || b1) {
                b1 = false;
                TryConsumeData();
                if (!TryDecode())
                    return;
                DoFileRead();
                timer.Split();
            }
        }
    }
}
#else // HX_NATIVE

static void Decrypt(VorbisReader *reader, unsigned char *data, int bytes,
                    symmetric_CTR *ctrState, int magicHashA, int magicHashB) {
    if (!ctrState)
        return;

    // Step 1: Decrypt the entire buffer in-place using AES-CTR (stream cipher)
    unsigned char *tmp = new unsigned char[bytes];
    ctr_decrypt(data, tmp, bytes, ctrState);
    memcpy(data, tmp, bytes);
    delete[] tmp;

    // Step 2: Scan for all HMXA page headers and apply anti-tamper reversal.
    // v0xE encryption replaces OggS with HMXA and XORs bytes 12-15 and 20-23
    // with magicHash values. The XOR was designed for big-endian (Xbox 360),
    // so we must byte-swap the hash values on little-endian before XORing.
    if (magicHashA != 0 || magicHashB != 0) {
        unsigned int xorA = __builtin_bswap32((unsigned int)magicHashA);
        unsigned int xorB = __builtin_bswap32((unsigned int)magicHashB);
        for (int i = 0; i <= bytes - 4; i++) {
            if (data[i] == 'H' && data[i+1] == 'M'
                && data[i+2] == 'X' && data[i+3] == 'A') {
                data[i]   = 'O';
                data[i+1] = 'g';
                data[i+2] = 'g';
                data[i+3] = 'S';
                if (i + 16 <= bytes) {
                    unsigned int *ui = (unsigned int *)&data[i + 12];
                    *ui ^= xorA;
                }
                if (i + 24 <= bytes) {
                    unsigned int *ui = (unsigned int *)&data[i + 20];
                    *ui ^= xorB;
                }
            }
        }
    }
}

bool VorbisReader::DoFileRead() {
    bool ret = false;
    if (mFail)
        return false;

    int queuedBytes = mOggSync->fill - mOggSync->returned;
    if (mEnableReads && !mReadBuffer && !mFile->Eof() && queuedBytes < 0x10000) {
        mReadBuffer = ogg_sync_buffer(mOggSync, 0x4000);
        mFile->ReadAsync(mReadBuffer, 0x4000);
        mFail = mFile->Fail();
        ret = true;
    }

    int bytes = 0;
    if (!mFail && mReadBuffer && mFile->ReadDone(bytes) && !unk38) {
        mFail = mFile->Fail();
        if (mFail)
            return false;
        MILO_ASSERT(bytes > 0, 0x1F9);
        static int sDbgDecryptCallCount = 0;
        if (sDbgDecryptCallCount < 3) {
            MILO_LOG("MOGG_DBG: DoFileRead got %d bytes BEFORE decrypt[0:32]=", bytes);
            int _lim = bytes < 32 ? bytes : 32;
            for (int _i = 0; _i < _lim; _i++)
                MILO_LOG("%02x", ((unsigned char*)mReadBuffer)[_i]);
            MILO_LOG("\n");
        }
        ::Decrypt(this, (unsigned char *)mReadBuffer, bytes, mCtrState, mMagicHashA, mMagicHashB);
        if (sDbgDecryptCallCount < 3) {
            MILO_LOG("MOGG_DBG: AFTER decrypt[0:32]=");
            int _lim = bytes < 32 ? bytes : 32;
            for (int _i = 0; _i < _lim; _i++)
                MILO_LOG("%02x", ((unsigned char*)mReadBuffer)[_i]);
            MILO_LOG(" (ASCII: %c%c%c%c)\n",
                     ((unsigned char*)mReadBuffer)[0],
                     ((unsigned char*)mReadBuffer)[1],
                     ((unsigned char*)mReadBuffer)[2],
                     ((unsigned char*)mReadBuffer)[3]);
            sDbgDecryptCallCount++;
        }
        ogg_sync_wrote(mOggSync, bytes);
        mReadBuffer = 0;
        ret = true;
    }
    mFail = mFile->Fail();
    return ret;
}

void VorbisReader::Poll(float until) {
    static int sPollCallCount = 0;
    static int sLastHeadersRead = -1;
    sPollCallCount++;
    if (mHeadersRead != sLastHeadersRead) {
        MILO_LOG("VORBIS_DBG: Poll #%d hdr=%d fail=%d done=%d\n",
                 sPollCallCount, mHeadersRead, (int)mFail, (int)mDone);
        MILO_LOG("VORBIS_DBG:   ch=%d rate=%d eof=%d queued=%d\n",
                 mNumChannels, mSampleRate, (int)mFile->Eof(), QueuedInputBytes());
        sLastHeadersRead = mHeadersRead;
    }
    if (!mFail && !unk3c && CheckHmxHeader() && !mDone && (mSeekTarget < 0 || DoSeek())) {
        DoFileRead();
        unke2 = mFile->Eof();
        if (mHeadersRead < 3) {
            while (TryReadHeader())
                ;
            if (mHeadersRead >= 3) {
                MILO_LOG("VORBIS_DBG: All 3 headers read! ch=%d rate=%d — entering audio decode phase\n",
                         mVorbisInfo->channels, mVorbisInfo->rate);
                mNumChannels = mVorbisInfo->channels;
                mSampleRate = mVorbisInfo->rate;
                unke4.resize(mNumChannels);
                Init();
                InitDecoder();
            }
        } else {
            Timer timer;
            timer.Start();
            bool first = !unke0;
            while (Timer::CyclesToMs(timer.mCycles) < until || first) {
                first = false;
                // Step 1: Push any decoded PCM to ring buffers
                {
                    float **pcm;
                    int pcmAvail = vorbis_synthesis_pcmout(mVorbisDsp, &pcm);
                    if (pcmAvail > 0) {
                        int consumed = ConsumeData((void **)pcm, pcmAvail,
                                                   mVorbisDsp->granulepos - pcmAvail);
                        vorbis_synthesis_read(mVorbisDsp, consumed);
                        if (consumed == 0)
                            break; // Ring buffer full — wait for audio callback to drain
                    }
                }
                // Step 2: Decode more Vorbis blocks
                if (!TryDecode()) {
                    // No packet available — on native there's no background decode
                    // thread, so read more file data and retry before giving up.
                    if (!DoFileRead())
                        break; // Can't read more (EOF, error, or ogg buffer full)
                    if (!TryDecode())
                        break; // Still no packet — give up this poll cycle
                }
                // Step 3: Feed raw data for next iteration
                DoFileRead();
                timer.Split();
            }
        }
    }
}

#endif // HX_NATIVE

void VorbisReader::Seek(int sample) {
    CritSecTracker tracker(this);
    MILO_ASSERT(mHeadersRead == 3, 0x1CC);
    MILO_ASSERT(mEnableReads, 0x1CD);
    MILO_ASSERT(sample >= 0, 0x1CE);
    mSeekTarget = sample;
    mDone = false;
    unke1 = false;
}

#ifndef HX_NATIVE
bool VorbisReader::DoFileRead() {
    bool ret = false;
    if (mFail)
        return false;
    else {
        if (mEnableReads && !mReadBuffer && !mFile->Eof()
            && QueuedInputBytes() < 0x10000) {
            mReadBuffer = ogg_sync_buffer(mOggSync, 0x4000);
            {
                static Timer *_t = AutoTimer::GetTimer("synth_poll");
                if (_t) _t->Stop();
            }
            mFile->ReadAsync(mReadBuffer, 0x4000);
            {
                static Timer *_t = AutoTimer::GetTimer("synth_poll");
                if (_t) _t->Start();
            }
            mFail = mFile->Fail();
            ret = true;
        }
        int bytes = 0;
        if (!mFail && mReadBuffer && mFile->ReadDone(bytes) && !unk38) {
            mFail = mFile->Fail();
            if (mFail)
                return false;
            MILO_ASSERT(bytes > 0, 0x1F9);
            Decrypt((unsigned char *)mReadBuffer, bytes);
            ogg_sync_wrote(mOggSync, bytes);
            mReadBuffer = 0;
            ret = true;
        }
        mFail = mFile->Fail();
    }
    return ret;
}

void VorbisReader::Decrypt(unsigned char *data, int bytes) {
    if (!mCtrState)
        return;
    int i = 0;
    int n = 0;
    while (i += n, i < bytes) {
        const int dataLen = 1024;
        unsigned char buf2[dataLen];
        unsigned char buf1[dataLen];

        int n = Min(bytes - i, dataLen);

        memcpy(buf1, data + i, n);
        int ret = ctr_decrypt(buf1, buf2, n, mCtrState);
        unsigned char *after = buf2;
        if ((mMagicHashA != 0 || mMagicHashB != 0)
            && (after[0] == 'H' && after[1] == 'M' && after[2] == 'X' && after[3] == 'A'
            )) {
            after[0] = 'O';
            after[2] = 'g';
            after[1] = 'g';
            after[3] = 'S';
            if (n >= 16) {
                unsigned int *ui = (unsigned int *)&after[12];
                *ui ^= mMagicHashA;
            }
            if (n >= 24) {
                unsigned int *ui = (unsigned int *)&after[20];
                *ui ^= mMagicHashB;
            }
        }
        MILO_ASSERT(ret == 0, 0x234);
        memcpy(data + i, buf2, n);
        i += n;
    }
}
#endif // !HX_NATIVE

#define kMaxHeader 60000

bool VorbisReader::CheckHmxHeader() {
    if (!mHdrBuf)
        return true;
    else {
        int bytes;
        if (mFile->ReadDone(bytes)) {
            BufStream bs(mHdrBuf, 60000, true);
            int version;
            bs >> version;
            bs >> mHdrSize;
            MILO_ASSERT(version >= 10, 0x24A);
            MILO_ASSERT(version <= 16, 0x24B);
            MILO_ASSERT(mHdrSize <= kMaxHeader, 0x24C);
            MILO_ASSERT(mHdrSize >= 0, 0x24D);
            mMap.Read(bs);
            mKeyIndex = 0;
            memset(mKeyMask, 0, sizeof(mKeyMask));
            mMagicA = mMagicB = 0;
            mMagicHashA = mMagicHashB = 0;
            if (version - 0xCU <= 4) {
                bs.Read(mNonce, sizeof(mNonce));
                bs >> mMagicA;
                bs >> mMagicB;
                unsigned char stuff[16];
                bs.Read(stuff, sizeof(stuff));
                bs.Read(stuff, sizeof(stuff));
                bs >> mKeyIndex;
                mKeyIndex = mKeyIndex % 6 + 6;
#ifdef HX_NATIVE
                MILO_LOG("MOGG_DBG: pre-HvDecrypt stuff=");
                for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", stuff[_i]);
                MILO_LOG(" mKeyIndex(raw)=%ld\n", (long)mKeyIndex);
#endif
                TheSynth->mGrinder.HvDecrypt(stuff, mKeyMask, version);
#ifdef HX_NATIVE
                MILO_LOG("MOGG_DBG: post-HvDecrypt mKeyMask=");
                for (int _i = 0; _i < 16; _i++) MILO_LOG("%02x", mKeyMask[_i]);
                MILO_LOG("\n");
#endif
                gCipher = register_cipher(&rijndael_desc);
                MILO_ASSERT(gCipher >= 0, 0x279);
                mCtrState = new symmetric_CTR();
                int keyIndex = mKeyIndex;
                MILO_ASSERT_RANGE( keyIndex, 0, KeyChain::getNumKeys(), 0x27E);
                setupCypher(version);
            } else if (version == 0xB) {
                bs.Read(mNonce, sizeof(mNonce));
                gCipher = register_cipher(&rijndael_desc);
                MILO_ASSERT(gCipher >= 0, 0x287);
                mCtrState = new symmetric_CTR();
                int ret = ctr_start(gCipher, mNonce, gRB1Key, gKeySize, 0, mCtrState);
                MILO_ASSERT(ret == 0, 0x28F);
            } else
                MILO_NOTIFY_ONCE("old mogg version!");
            delete[] mHdrBuf;
            mHdrBuf = 0;
            mFile->Seek(mHdrSize, BinStream::kSeekBegin);
        }
        mFail = mFile->Fail();
        return !mHdrBuf;
    }
}

bool VorbisReader::TryReadHeader() {
    if (!mOggStream) {
        ogg_page page;
        int pageOut = ogg_sync_pageout(mOggSync, &page);
        if (pageOut < 0) {
            VORBIS_FAIL("StreamInit", pageOut);
#ifdef HX_NATIVE
            // Persistent page-sync failure means the data is corrupt (likely
            // bad mogg decryption). Mark the reader failed so the single-threaded
            // native Poll loop can break out instead of spinning forever.
            mFail = true;
            return false;
#endif
        }
        if (pageOut > 0) {
            mOggStream = new ogg_stream_state();
            ogg_stream_init(mOggStream, ogg_page_serialno(&page));
            ogg_stream_pagein(mOggStream, &page);
            mVorbisInfo = new vorbis_info();
            vorbis_info_init(mVorbisInfo);
            mVorbisComment = new vorbis_comment();
            vorbis_comment_init(mVorbisComment);
        } else
            return false;
    }
    if (mHeadersRead == 3)
        return false;
    else {
        ogg_packet packet;
        if (TryReadPacket(packet)) {
            int vorbisErr =
                vorbis_synthesis_headerin(mVorbisInfo, mVorbisComment, &packet);
            if (vorbisErr < 0)
                VORBIS_FAIL("HeaderIn", vorbisErr);
            mHeadersRead++;
            return true;
        } else
            return false;
    }
}

void VorbisReader::InitDecoder() {
    MILO_ASSERT(!mVorbisDsp && !mVorbisBlock, 0x2D1);
    MILO_ASSERT(mHeadersRead == 3, 0x2D2);
    mVorbisDsp = new vorbis_dsp_state();
    vorbis_synthesis_init(mVorbisDsp, mVorbisInfo);
    mVorbisBlock = new vorbis_block();
    vorbis_block_init(mVorbisDsp, mVorbisBlock);
}

bool VorbisReader::TryConsumeData() {
    int ret = 0;
    float **pcm;
    int pcmErr = vorbis_synthesis_pcmout(mVorbisDsp, &pcm);
    if (pcmErr < 0) {
        VORBIS_FAIL("PCMOut", pcmErr);
    }
    if (pcmErr > 0) {
        ret = ConsumeData((void **)pcm, pcmErr, mVorbisDsp->granulepos - pcmErr);
        vorbis_synthesis_read(mVorbisDsp, ret);
    }
    return ret;
}

UNPOOL_DATA
bool VorbisReader::TryDecode() {
    if (mFail)
        return false;
    if (QueuedOutputSamples() > 0)
        return false;
    if (!unk98 && TryReadPacket(mPendingPacket)) {
        unk98 = true;
    }
    if (unk98) {
        int pollErr;
        {
            START_AUTO_TIMER("vorbis_synthesis_poll_cpu");
            if (mVorbisBlock->synthesis_state == vorbis_block::vss_init) {
                START_AUTO_TIMER("vorbis_synthesis_vssinit_cpu");
                pollErr = vorbis_synthesis_poll(mVorbisBlock, &mPendingPacket);
            } else if (mVorbisBlock->synthesis_state == vorbis_block::vss_decode) {
                START_AUTO_TIMER("vorbis_synthesis_vssdecode_cpu");
                pollErr = vorbis_synthesis_poll(mVorbisBlock, &mPendingPacket);
            } else {
                START_AUTO_TIMER("vorbis_synthesis_vssmdct_cpu");
                pollErr = vorbis_synthesis_poll(mVorbisBlock, &mPendingPacket);
            }
        }
        if ((unsigned int)pollErr == OV_ENOTAUDIO) {
            unk98 = false;
        } else if (pollErr == -0x32) {
            return true;
        } else {
            if (pollErr < 0) {
                VORBIS_FAIL("Synthesis", pollErr);
            }
            unk98 = false;
            if (pollErr == 0) {
                START_AUTO_TIMER("vorbis_synthesis_blockin_cpu");
                int blockErr = vorbis_synthesis_blockin(mVorbisDsp, mVorbisBlock);
                if (blockErr < 0) {
                    VORBIS_FAIL("BlockIn", blockErr);
                }
                return true;
            }
        }
    } else if (unke2 && !mReadBuffer && QueuedOutputSamples() == 0 && !mDone) {
        EndData();
        mDone = true;
    }
    return false;
}
END_UNPOOL_DATA

bool VorbisReader::TryReadPacket(ogg_packet &pk) {
    MILO_ASSERT(mOggStream, 0x3AA);
    int err;
    while (true) {
        err = ogg_stream_packetout(mOggStream, &pk);
        if (err < 0) {
            VORBIS_FAIL("PacketOut", err);
        }
        if (err > 0)
            return true;
        ogg_page page;
        err = ogg_sync_pageout(mOggSync, &page);
        if (err > 0)
            ogg_stream_pagein(mOggStream, &page);
        if (err <= 0)
            return false;
    }
}

bool VorbisReader::DoSeek() {
    mEnableReads = false;
    DoFileRead();
    if (!mFail && !mReadBuffer) {
        int i1, i2;
        mMap.GetSeekPos(mSeekTarget, i1, i2);
        DoRawSeek(i1);
        mSamplesToSkip = mSeekTarget - i2;
        MILO_ASSERT(mSamplesToSkip >= 0, 0x3D0);
        mSeekTarget = -1;
        mEnableReads = true;
        return true;
    }
    return false;
}

void VorbisReader::DoRawSeek(int byte) {
    if (mReadBuffer) {
        mEnableReads = false;
        while (!mFail && mReadBuffer)
            DoFileRead();
        mEnableReads = true;
    }
    int streamErr = ogg_stream_reset(mOggStream);
    if (streamErr < 0)
        VORBIS_FAIL("StreamReset", streamErr);
    int syncErr = ogg_sync_reset(mOggSync);
    if (syncErr < 0)
        VORBIS_FAIL("SyncReset", syncErr);
    vorbis_block_clear(mVorbisBlock);
    int restartErr = vorbis_synthesis_restart(mVorbisDsp);
    if (restartErr < 0)
        VORBIS_FAIL("DspReset", restartErr);
    vorbis_block_init(mVorbisDsp, mVorbisBlock);
    unk98 = false;
    mFile->Seek(byte + mHdrSize, 0);
    if (mCtrState) {
        MILO_ASSERT(byte%16 == 0, 0x402);
        *(unsigned int*)mNonce = EndianSwap((unsigned int)(byte / 16));
        int ret = ctr_reinit(gCipher, mNonce, mCtrState);
        MILO_ASSERT(ret == 0, 0x405);
    }
    mDone = false;
    unke2 = false;
}

inline int VorbisReader::QueuedInputBytes() {
    return mOggSync->fill - mOggSync->returned;
}

int VorbisReader::QueuedOutputSamples() {
    if (mVorbisDsp) {
        START_AUTO_TIMER("vorbis_synthesis_pcmout_cpu");
        return vorbis_synthesis_pcmout(mVorbisDsp, nullptr);
    } else
        return 0;
}

void VorbisReader::Init() {
    MILO_ASSERT(mStream, 0x42D);
    mStream->InitInfo(mNumChannels, mSampleRate, true, -1);
}

int VorbisReader::ConsumeData(void **v, int i1, int i2) {
    MILO_ASSERT(mSeekTarget == -1, 0x444);
    if (((volatile int &)mSamplesToSkip) > 0) {
        int ret = Min(i1, mSamplesToSkip);
        mSamplesToSkip -= ret;
        return ret;
    } else {
        MILO_ASSERT(mStream, 0x44D);
        return mStream->ConsumeData(v, i1, i2);
    }
}