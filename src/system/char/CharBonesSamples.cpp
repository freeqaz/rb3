#include "char/CharBonesSamples.h"
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "decomp.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"

int gVer;

DECOMP_FORCEACTIVE(
    CharBonesSamples,
    __FILE__,
    "set.mNumSamples == mNumSamples",
    "CharBonesSamples are already compressed.",
    "CharBonesSamples are already compressed, can't remove bones."
)

int CharBonesSamples::FracToSample(float *frac) const {
    if (mNumSamples < 2) {
        *frac = 0.0f;
        return 0;
    }
    float inputFrac = *frac;
    ClampEq(*frac, 0.0f, 1.0f);
    int total = Max((int)mFrames.size(), (int)mNumSamples);
    float scaledPos = *frac * (float)(total - 1);
    *frac = scaledPos;
    int sampleIdx = (int)scaledPos;
    if (sampleIdx >= total - 1) {
        *frac = 0.0f;
        return mNumSamples - 1;
    }
    *frac = scaledPos - (float)sampleIdx;
    if (mFrames.size() != 0) {
        float frame = mFrames[sampleIdx];
        float nextFrame = mFrames[sampleIdx + 1];
        float interpFrame = (*frac * (nextFrame - frame)) + frame;
        sampleIdx = (int)interpFrame;
        *frac = interpFrame - (float)sampleIdx;
    }
    if (sampleIdx < 0 || sampleIdx >= mNumSamples) {
        MILO_NOTIFY_ONCE(
            "FracToSample: sample is %d, clip only has %d samples, frac was %g, is %g",
            sampleIdx,
            mNumSamples,
            inputFrac,
            *frac
        );
        sampleIdx = 0;
    }
    if (!(*frac >= 0.0f && *frac < 1.0f)) {
        MILO_NOTIFY_ONCE("FracToSample: frac is %g, outside of 0 and 1", *frac);
        *frac = 0.0f;
    }
    return sampleIdx;
}

CharBonesSamples::CharBonesSamples() : mNumSamples(0), mPreviewSample(0), mRawData(0) {}

CharBonesSamples::~CharBonesSamples() { _MemFree(mRawData); }

void CharBonesSamples::Set(
    const std::vector<CharBones::Bone> &bones, int i, CharBones::CompressionType ty
) {
    ClearBones();
    SetCompression(ty);
    mNumSamples = i;
    AddBones(bones);
    _MemFree(mRawData);
    mRawData = (char *)_MemAlloc(AllocateSize(), 0);
    mFrames.clear();
}

void CharBonesSamples::Clone(const CharBonesSamples &samp) {
    Set(samp.mBones, samp.mNumSamples, samp.mCompression);
    memcpy(mRawData, samp.mRawData, AllocateSize());
    mFrames = samp.mFrames;
}

FORCE_LOCAL_INLINE
int CharBonesSamples::AllocateSize() { return mTotalSize * mNumSamples; }
END_FORCE_LOCAL_INLINE

void CharBonesSamples::RotateBy(CharBones &bones, int i) {
    mStart = &mRawData[mTotalSize * i];
    CharBones::RotateBy(bones);
}

void CharBonesSamples::RotateTo(CharBones &bones, float f1, int i, float f2) {
    mStart = &mRawData[mTotalSize * i];
    CharBones::RotateTo(bones, (1.0f - f2) * f1);
    if (f2 > 0.0f) {
        mStart = &mRawData[mTotalSize * (i + 1)];
        CharBones::RotateTo(bones, f2 * f1);
    }
}

void CharBonesSamples::ScaleAddSample(CharBones &bones, float f1, int i, float f2) {
    mStart = &mRawData[mTotalSize * i];
    CharBones::ScaleAdd(bones, (1.0f - f2) * f1);
    if (f2 > 0.0f) {
        mStart = &mRawData[mTotalSize * (i + 1)];
        CharBones::ScaleAdd(bones, f2 * f1);
    }
}

void CharBonesSamples::Print() {
    TheDebug << MakeString(
        "samples: %d size: %d address: %x compression %d\n",
        mNumSamples,
        AllocateSize(),
        (int)mRawData,
        mCompression
    );
    if (mNumSamples == 0) {
        TheDebug << "Bones:\n";
        for (int i = 0; i < mBones.size(); i++) {
            TheDebug << "   " << mBones[i].name << "\n";
        }
    }
    for (int i = 0; i < mNumSamples; i++) {
        TheDebug << i << ")\n";
        mStart = &mRawData[mTotalSize * i];
        CharBones::Print();
    }
}

void CharBonesSamples::Relativize(CharClip *clip) {
    std::vector<Bone> &bones = mBones;
    if (bones.empty())
        return;

    for (int sample = mNumSamples - 1; sample >= 0; sample--) {
        Bone *bone = &bones[0];
        mStart = mRawData + sample * mTotalSize;

        if (mCompression >= kCompressVects) {
            for (ShortVector3 *pos = (ShortVector3 *)mStart;
                 pos < (ShortVector3 *)(mStart + mOffsets[TYPE_QUAT]); pos++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                Vector3 evalPos;
                clip->EvaluateChannel(&evalPos, channel, startBeat);
                float sx = (float)pos->x * (1.0f / 32767.0f) * 1300.0f;
                float sz = (float)pos->z * (1.0f / 32767.0f) * 1300.0f;
                float sy = (float)pos->y * (1.0f / 32767.0f) * 1300.0f;
                Vector3 v;
                v.x = sx - evalPos.x;
                v.y = sy - evalPos.y;
                v.z = sz - evalPos.z;
                pos->Set(v);
                bone++;
            }
        } else {
            for (Vector3 *pos = (Vector3 *)mStart;
                 pos < (Vector3 *)(mStart + mOffsets[TYPE_QUAT]); pos++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                Vector3 evalPos;
                clip->EvaluateChannel(&evalPos, channel, startBeat);
                pos->x -= evalPos.x;
                pos->y -= evalPos.y;
                pos->z -= evalPos.z;
                bone++;
            }
        }

        if (mCompression >= kCompressQuats) {
            for (ByteQuat *quat = (ByteQuat *)(mStart + mOffsets[TYPE_QUAT]);
                 quat < (ByteQuat *)(mStart + mOffsets[TYPE_ROTX]); quat++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                Hmx::Quat evalQuat;
                clip->EvaluateChannel(&evalQuat, channel, startBeat);
                Hmx::Matrix3 evalMat, curMat;
                MakeRotMatrix(evalQuat, evalMat);
                FastInvert(evalMat, evalMat);
                Hmx::Quat tempQuat;
                quat->ToQuat(tempQuat);
                MakeRotMatrix(tempQuat, curMat);
                Multiply(curMat, evalMat, curMat);
                tempQuat.Set(curMat);
                quat->Set(tempQuat);
                bone++;
            }
            for (short *rot = (short *)(mStart + mOffsets[TYPE_ROTX]);
                 rot < (short *)(mStart + mOffsets[TYPE_END]); rot++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                float evalRot;
                clip->EvaluateChannel(&evalRot, channel, startBeat);
                float rotVal = (float)*rot * (1.0f / 1638.4f);
                *rot = MakeShortAng(LimitAng(rotVal - evalRot));
                bone++;
            }
        } else if (mCompression != kCompressNone) {
            for (ShortQuat *quat = (ShortQuat *)(mStart + mOffsets[TYPE_QUAT]);
                 quat < (ShortQuat *)(mStart + mOffsets[TYPE_ROTX]); quat++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                Hmx::Quat evalQuat;
                clip->EvaluateChannel(&evalQuat, channel, startBeat);
                Hmx::Matrix3 evalMat, curMat;
                MakeRotMatrix(evalQuat, evalMat);
                FastInvert(evalMat, evalMat);
                Hmx::Quat tempQuat;
                quat->ToQuat(tempQuat);
                MakeRotMatrix(tempQuat, curMat);
                Multiply(curMat, evalMat, curMat);
                tempQuat.Set(curMat);
                quat->Set(tempQuat);
                bone++;
            }
            for (short *rot = (short *)(mStart + mOffsets[TYPE_ROTX]);
                 rot < (short *)(mStart + mOffsets[TYPE_END]); rot++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                float evalRot;
                clip->EvaluateChannel(&evalRot, channel, startBeat);
                float rotVal = (float)*rot * (1.0f / 1638.4f);
                *rot = MakeShortAng(LimitAng(rotVal - evalRot));
                bone++;
            }
        } else {
            for (Hmx::Quat *quat = (Hmx::Quat *)(mStart + mOffsets[TYPE_QUAT]);
                 quat < (Hmx::Quat *)(mStart + mOffsets[TYPE_ROTX]); quat++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                Hmx::Quat evalQuat;
                clip->EvaluateChannel(&evalQuat, channel, startBeat);
                Hmx::Matrix3 evalMat, curMat;
                MakeRotMatrix(evalQuat, evalMat);
                FastInvert(evalMat, evalMat);
                MakeRotMatrix(*quat, curMat);
                Multiply(curMat, evalMat, curMat);
                quat->Set(curMat);
                bone++;
            }
            for (float *rot = (float *)(mStart + mOffsets[TYPE_ROTX]);
                 rot < (float *)(mStart + mOffsets[TYPE_END]); rot++) {
                float startBeat = clip->StartBeat();
                void *channel = clip->GetChannel(bone->name);
                float evalRot;
                clip->EvaluateChannel(&evalRot, channel, startBeat);
                *rot = LimitAng(*rot - evalRot);
                bone++;
            }
        }
    }
}

DECOMP_FORCEACTIVE(
    CharBonesSamples,
    "numSamples > 0",
    "firstSample >= 0 && firstSample < mNumSamples",
    "(firstSample + numSamples) >= 0 && (firstSample + numSamples) < mNumSamples",
    "mNumSamples >= 0"
)

void CharBonesSamples::SetVer(int ver) {
    MILO_ASSERT(ver < 13, 0x22B);
    gVer = ver;
}

DECOMP_FORCEACTIVE(CharBonesSamples, "0")

#define VER 16

void CharBonesSamples::Load(BinStream &bs) {
    bs >> gVer;
    MILO_ASSERT(gVer > 12 && gVer <= VER, 0x2A0);
    LoadHeader(bs);
    LoadData(bs);
}

void CharBonesSamples::ReadCounts(BinStream& bs, int i2){
    int i = 0;
    int numTypesToRead = Min(7, i2);
    for(; i < numTypesToRead; i++){
        bs >> mCounts[i];
    }
    for(int numTypesRead = i; numTypesRead < i2; numTypesRead++){
        int tmp;
        bs >> tmp;
        MILO_ASSERT((tmp - mCounts[NUM_TYPES - 1]) == 0, 0x2B2);
    }
    for(; i < 7; i++){
        mCounts[i] = 0;
    }
}

void CharBonesSamples::LoadHeader(BinStream& bs){
    _MemFree(mRawData);
    int numBones; bs >> numBones;
    mBones.resize(numBones);
    if(gVer > 0xA){
        for(int i = 0; i < numBones; i++){
            bs >> mBones[i];
        }
    }
    else {
        for(int i = 0; i < numBones; i++){
            bs >> mBones[i].name;
        }
    }

    if(gVer > 9){
        ReadCounts(bs, gVer > 0xF ? 7 : 10);
        bs >> (int&)mCompression;
        int numSamples;
        bs >> numSamples;
        MILO_ASSERT(numSamples < 32767, 0x2D7);
        mNumSamples = numSamples;
    }
    else {
        int i;
        if (gVer > 5) {
            int count;
            if (gVer > 7) {
                count = 9;
            } else {
                count = 10;
                if (gVer > 6)
                    count = 6;
            }
            for(i = 0; i < count; i++){
                int sp14;
                bs >> sp14;
            }
            bs >> (int&)mCompression;
            int numSamples;
            bs >> numSamples;
            MILO_ASSERT(numSamples < 32767, 0x2F1);
            mNumSamples = numSamples;
        }
        else {
            int numSamples;
            bs >> numSamples;
            MILO_ASSERT(numSamples < 32767, 0x2FC);
            mNumSamples = numSamples;
            if(gVer > 3){
                bs >> (int&)mCompression;
            }
        }
        for(i = 0; i < 7; i++){
            mCounts[i] = 0;
        }
        for(i = 0; i < mBones.size(); i++){
            mCounts[CharBones::TypeOf(mBones[i].name) + 1]++;
        }
        for(i = 1; i < 7; i++){
            mCounts[i] += mCounts[i-1];
        }
    }

    if(gVer > 0xB){
        bs >> mFrames;
    }
    else mFrames.clear();
    RecomputeSizes();
    mRawData = (char*)_MemAlloc(AllocateSize(), 0);    
}

void CharBonesSamples::LoadData(BinStream& bs){
    if(gVer == 0xE){
        bool x; bs >> x;
    }
    for(int i = 0; i < mNumSamples; i++){
        SetStartFromRawData(Min(i, mNumSamples - 1));

        if(mCompression >= kCompressVects){
            short* offset = (short*)QuatOffset();
            for(short* p = (short*)Start(); p < offset; p += 3){
                bs >> p[0] >> p[1] >> p[2];
            }
        }
        else {
            Vector3* offset = (Vector3*)QuatOffset();
            for(Vector3* p = (Vector3*)Start(); p < offset; p++){
                bs >> *p;
            }
        }

        if(mCompression >= kCompressQuats){
            char* offset = RotXOffset();
            for(char* p = QuatOffset(); p < offset; p += 4){
                bs >> p[0] >> p[1] >> p[2] >> p[3];
            }
        }
        else if(mCompression != kCompressNone){
            short* offset = (short*)RotXOffset();
            for(short* p = (short*)QuatOffset(); p < offset; p += 4){
                bs >> p[0] >> p[1] >> p[2] >> p[3];
            }
        }
        else {
            Hmx::Quat* offset = (Hmx::Quat*)RotXOffset();
            for(Hmx::Quat* p = (Hmx::Quat*)QuatOffset(); p < offset; p++){
                bs >> *p;
            }
        }

        if(mCompression != kCompressNone){
            short* offset = (short*)EndOffset();
            for(short* p = (short*)RotXOffset(); p < offset; p++){
                bs >> *p;
            }
        }
        else {
            float* offset = (float*)EndOffset();
            for(float* p = (float*)RotXOffset(); p < offset; p++){
                bs >> *p;
            }
        }

        if((i & 0x7F) == 0x7F){
            while(bs.Eof() == TempEof){
                Timer::Sleep(0);
            }
        }
    }
}

void CharBonesSamples::SetPreview(int i) {
    int tmp = Clamp(0, mNumSamples - 1, i);
    MILO_ASSERT(mPreviewSample < 32767, 0x38B);
    mPreviewSample = tmp;
    mStart = &mRawData[mTotalSize * mPreviewSample];
}

BEGIN_PROPSYNCS(CharBonesSamples)
    SYNC_PROP(num_samples, mNumSamples)
    SYNC_PROP_SET(preview_sample, mPreviewSample, SetPreview(_val.Int()))
    SYNC_PROP(frames, mFrames)
    SYNC_PROP_SET(compression, mCompression, )
    else {
        gPropBones = this;
        if (sym == bones)
            return PropSync(mBones, _val, _prop, _i + 1, _op);
    }
END_PROPSYNCS
