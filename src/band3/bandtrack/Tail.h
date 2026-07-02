#pragma once
#include "bandtrack/DelayLine.h"
#include "bandtrack/GemRepTemplate.h"
#include "math/Interp.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"

class Tail {
public:
    /** Left-hand-slide (portamento) parameters for a tail. Absent from Bank 5
        DWARF - member names are semantic, derived from Bank 8 usage: the start/end
        offsets and length feed ATanInterpolator::Reset(start, end, 0, length), and
        Eval(trackY) then bends the tail sideways in UpdateVerts. */
    struct SlideInfo {
        SlideInfo() : mSliding(0), mStartOffset(0), mEndOffset(0), mLength(0) {}
        /** this tail is a left-hand slide (GameGem::LeftHandSlide) */
        bool mSliding; // 0x0
        /** lateral offset at the start of the slide */
        float mStartOffset; // 0x4
        /** lateral offset at the end of the slide */
        float mEndOffset; // 0x8
        /** slide length in track-Y units (== tail length) */
        float mLength; // 0xc
    };

    Tail(GemRepTemplate &);
    virtual ~Tail();

    void Hit();
    void Release();
    void Done();
    void Poll(float, float, float);
    void SetDuration(float, float, float);
    void SetType(Symbol, bool);
    void
    Init(int, const Transform &, bool, Symbol, RndGroup *, const SlideInfo &, Tail *);
    void ReleaseMeshes();
    void ConfigureMeshes(Tail *);
    void UpdateVerts(float, bool);
    void MoveSlot(const Transform &);
    void HandleMistake();

    RndGroup *mGroup; // 0x4
    RndMesh *mTail1; // 0x8
    RndMesh *mTail2; // 0xc
    /** track-Y where the tail currently begins (Bank 5 also stored mTailEnd at
        0x14; Bank 8 dropped it and tracks the length in mTailLength instead) */
    float mTailBegin; // 0x10
    Symbol mType; // 0x14 - normal/star/unison/invisible
    int mState; // 0x18 - state?
    int mSlot; // 0x1c
    GemRepTemplate &mTemplate; // 0x20
    RndMesh *mTailGeomOwner; // 0x24
    bool mUpdateGeometry; // 0x28
    DelayLine<float, 300> mWhammy; // 0x2c
    float mWaveTime; // 0x4e0 - whammy wave phase accumulator
    float mLastWhammyVal; // 0x4e4 - pulse-smoothed whammy
    float mLastWhammyAlpha; // 0x4e8 - alpha-smoothed whammy
    /** semantic name (Bank-8-new): tail length in track-Y (end - begin) */
    float mTailLength; // 0x4ec
    /** semantic name: last mTailLength UpdateVerts ran with (dirty check) */
    float mLastTailLength; // 0x4f0
    /** semantic name: last "active" flag UpdateVerts ran with (dirty check) */
    bool mLastActive; // 0x4f4
    SlideInfo mSlideInfo; // 0x4f8
    ATanInterpolator mInterpolator; // 0x508
};
