#include "char/CharLipSync.h"
#include "decomp.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PropKeys.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

INIT_REVS(CharLipSync)

CharLipSync::Generator::Generator() : mLipSync(0), mLastCount(0), mWeights() {}

void CharLipSync::Generator::Init(CharLipSync *sync) {
    mLipSync = sync;
    mLipSync->mData.resize(0);
    mWeights.resize(mLipSync->mVisemes.size());
    for (int i = 0; i < mWeights.size(); i++) {
        mWeights[i].unk0 = 0;
        mWeights[i].unk1 = 0;
    }
    mLastCount = mLipSync->mData.size();
    mLipSync->mData.push_back(0);
    mLipSync->mFrames = 0;
}

void CharLipSync::Generator::AddWeight(int visemeIdx, float weight) {
    float scaled = weight * 255.0f;
    float clamped = Clamp(0.0f, 255.0f, scaled + 0.5f);
    unsigned char val = clamped;
    if (mWeights[visemeIdx].unk0 != val || mWeights[visemeIdx].unk1 != val) {
        unsigned char idx = (unsigned char)visemeIdx;
        mLipSync->mData.push_back(idx);
        mLipSync->mData.push_back(val);
        mWeights[visemeIdx].unk0 = mWeights[visemeIdx].unk1;
        mWeights[visemeIdx].unk1 = val;
    }
}

void CharLipSync::Generator::NextFrame() {
    int count = (mLipSync->mData.size() - mLastCount - 1) / 2;
    MILO_ASSERT(count >= 0 && count < 256, 0x40);
    mLipSync->mData[mLastCount] = count;
    mLastCount = mLipSync->mData.size();
    mLipSync->mData.push_back(0);
    mLipSync->mFrames++;
}

void CharLipSync::Generator::Finish() {
    mLipSync->mData.pop_back();
    std::vector<bool> bools;
    bools.resize(mLipSync->mVisemes.size());
    for (int i = 0; i < bools.size(); i++)
        bools[i] = false;

    std::vector<unsigned char VECTOR_SIZE_LARGE> &data = mLipSync->mData;
    int idx = 0;
    for (int i = 0; i < mLipSync->mFrames; i++) {
        int count = data[idx++];
        MILO_ASSERT(count <= mLipSync->mVisemes.size(), 0x57);
        for (int j = 0; j < count; j++) {
            int viseme = data[idx++];
            MILO_ASSERT(viseme < mLipSync->mVisemes.size(), 0x5B);
            if (data[idx++] != 0) {
                bools[viseme] = true;
            }
        }
    }

    for (int i = 0; i < bools.size(); i) {
        if (!bools[i]) {
            bools.erase(bools.begin() + i);
            RemoveViseme(i);
        } else
            i++;
    }
}

void CharLipSync::Generator::RemoveViseme(int visemeIdx) {
    CharLipSync *lipSync = mLipSync;
    lipSync->mVisemes.erase(lipSync->mVisemes.begin() + visemeIdx);

    int cur = 0;
    int i = 0;
    lipSync = mLipSync;
    while (i < mLipSync->mFrames) {
        int j = 0;
        int count = lipSync->mData[cur++];
        while (j < count) {
            if (lipSync->mData[cur] >= visemeIdx) {
                lipSync->mData[cur]--;
                MILO_ASSERT(lipSync->mData[cur] < mLipSync->mVisemes.size(), 0x83);
            }
            cur += 2;
            j++;
        }
        i++;
    }
}

CharLipSync::PlayBack::PlayBack()
    : mLipSync(0), mPropAnim(0), mClips(0), mIndex(0), mOldIndex(0), mFrame(-1) {}

void CharLipSync::PlayBack::Set(CharLipSync *lipsync, ObjectDir *dir) {
    mClips = dir;
    if (lipsync->GetPropAnim()) {
        mPropAnim = lipsync->GetPropAnim();
        MILO_ASSERT(mPropAnim->GetRate() == RndAnimatable::k30_fps, 0xA4);
        std::vector<PropKeys *> &keys = mPropAnim->mPropKeys;
        mWeights.resize(keys.size());
        int idx = 0;
        for (std::vector<PropKeys *>::iterator it = keys.begin(); it != keys.end();
             ++it) {
            String str((*it)->mProp->Str(0));
            ObjPtr<CharClip> &clip = mWeights[idx].unk0;
            clip = mClips->Find<CharClip>(str.c_str(), false);
            if (!clip)
                MILO_WARN("could not find %s", str.c_str());
            idx++;
        }
    } else {
        mLipSync = lipsync;
        mWeights.resize(mLipSync->mVisemes.size());
        for (int i = 0; i < mWeights.size(); i++) {
            ObjPtr<CharClip> &clip = mWeights[i].unk0;
            clip = mClips->Find<CharClip>(mLipSync->mVisemes[i].c_str(), false);
            if (!clip) {
                MILO_WARN("could not find %s", mLipSync->mVisemes[i].c_str());
            }
        }
    }
}

void CharLipSync::PlayBack::Reset() {
    mIndex = 0;
    mFrame = -1;
    for (int i = 0; i < mWeights.size(); i++) {
        Weight &weight = mWeights[i];
        weight.unk10 = 0;
        weight.unk14 = 0;
        weight.unkc = 0;
    }
}

void CharLipSync::PlayBack::Poll(float time) {
    RndPropAnim *propAnim = mPropAnim;
    if (propAnim) {
        float frame = 30.0f * time;
        if (TheLoadMgr.mEditMode) {
            propAnim->SetFrame(frame, 1.0f);
        }
        RndPropAnim *pa = mPropAnim;
        int i = 0;
        for (std::vector<PropKeys *>::iterator it = pa->mPropKeys.begin();
             it != pa->mPropKeys.end(); ++it) {
            (*it)->FloatAt(frame, mWeights[i].unk14);
            i++;
        }
        return;
    }
    CharLipSync *lipSync = mLipSync;
    int frames = lipSync->mFrames;
    if (frames >= 2) {
        float frame = 30.0f * time;
        int frameIdx = (int)(float)ceil(frame);
        float frac = frame - (float)(frameIdx - 1);
        if (frameIdx < 1) {
            frameIdx = 1;
            frac = 0.0f;
        } else if (frameIdx >= frames) {
            frameIdx = frames - 1;
            frac = 0.9999999f;
        }
        if (frameIdx < mFrame) {
            Reset();
        }
        if (mFrame < frameIdx) {
            float conv = 1.0f / 255.0f;
            do {
                mOldIndex = mIndex++;
                int count = lipSync->mData[mOldIndex];
                if (count != 0) {
                    for (int i = count; i != 0; i--) {
                        int idx = lipSync->mData[mIndex++];
                        Weight &w = mWeights[idx];
                        w.unkc = w.unk10;
                        int val = lipSync->mData[mIndex++];
                        w.unk10 = (float)val * conv;
                        w.unk14 = Interp(w.unkc, w.unk10, frac);
                    }
                }
                mFrame++;
            } while (mFrame < frameIdx);
        } else if (mFrame >= 0 && mFrame == frameIdx) {
            int idx = mOldIndex + 1;
            int count = lipSync->mData[mOldIndex];
            if (count != 0) {
                for (int i = count; i != 0; i--) {
                    int wIdx = lipSync->mData[idx];
                    idx += 2;
                    Weight &w = mWeights[wIdx];
                    w.unk14 = Interp(w.unkc, w.unk10, frac);
                }
            }
        }
    }
}

CharLipSync::CharLipSync() : mPropAnim(this), mFrames(0) {}

CharLipSync::~CharLipSync() {}

SAVE_OBJ(CharLipSync, 0x155)

BEGIN_LOADS(CharLipSync)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mVisemes;
    bs >> mFrames;
    bs >> mData;
    if (gRev != 0)
        bs >> mPropAnim;
END_LOADS

DECOMP_FORCEACTIVE(
    CharLipSync,
    "; song: ",
    "\n",
    "(visemes\n",
    "   ",
    ")\n",
    "(frames ; @ 30fps\n",
    "   ( ",
    " "
)

void CharLipSync::Parse(DataArray *arr) {
    DataArray *visemesArr = arr->FindArray("visemes");
    mVisemes.resize(visemesArr->Size() - 1);
    for (int i = 1; i < visemesArr->Size(); i++) {
        mVisemes[i - 1] = visemesArr->Str(i);
    }
    Generator gen;
    gen.Init(this);
    DataArray *framesArr = arr->FindArray("frames");
    for (int i = 1; i < framesArr->Size(); i++) {
        DataArray *innerArr = framesArr->Array(i);
        for (int j = 0; j < innerArr->Size(); j++) {
            gen.AddWeight(j, innerArr->Float(j));
        }
        gen.NextFrame();
    }
    gen.Finish();
}

BEGIN_COPYS(CharLipSync)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharLipSync)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mVisemes)
        COPY_MEMBER(mFrames)
        COPY_MEMBER(mData)
        COPY_MEMBER(mPropAnim)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_PROPSYNCS(CharLipSync)
    {
        static Symbol _s("frames");
        if (sym == _s && (_op & kPropGet))
            return PropSync(mFrames, _val, _prop, _i + 1, _op);
    }
    SYNC_PROP_SET(duration, Duration(), )
    {
        static Symbol _s("visemes");
        if (sym == _s && (_op & (kPropGet | kPropSize)))
            return PropSync(mVisemes, _val, _prop, _i + 1, _op);
    }
    SYNC_PROP(prop_anim, mPropAnim)
END_PROPSYNCS

BEGIN_HANDLERS(CharLipSync)
    HANDLE(parse, OnParse)
    HANDLE(parse_array, OnParseArray)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x1E3)
END_HANDLERS

DataNode CharLipSync::OnParse(DataArray *arr) {
    FilePath fp(arr->Str(2));
    DataArray *read = DataReadFile(fp.c_str(), true);
    if (read) {
        Parse(read);
        read->Release();
    }
    return 0;
}

DataNode CharLipSync::OnParseArray(DataArray *arr) {
    DataArray *read = arr->Array(2);
    if (read) {
        Parse(read);
        read->Release();
    }
    return 0;
}