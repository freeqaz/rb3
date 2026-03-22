#pragma once
#include "char/CharBonesMeshes.h"
#include "char/CharClip.h"
#include "char/CharDriver.h"
#include "math/Color.h"
#include "rndobj/Trans.h"
#include <vector>

class DistEntry {
public:
    DistEntry() {}
    ~DistEntry() {}

    float beat; // 0x0
    std::vector<Vector3> bones; // 0x4
    float facing[4]; // 0xc
};

class ClipDistMap {
public:
    class Array2d {
    public:
        Array2d() {}
        void Resize(int, int);
        int CalcWidth();
        int CalcHeight();
        int Width() { return mWidth; }
        int Height() { return mHeight; }
        float &operator()(int i, int j) { return mData[i + j * mWidth]; }

        int mWidth; // 0x0
        int mHeight; // 0x4
        float *mData; // 0x8
    };

    class Node {
    public:
        float curBeat;
        float nextBeat;
        float err;
    };

    ClipDistMap(CharClip *, CharClip *, float, float, int, const DataArray *);
    void FindDists(float, DataArray *);
    void FindNodes(float, float, float);
    void SetNodes(Node *, Node *);
    void Draw(float, float, CharDriver *);
    void DrawDot(float, float, float, float, const Hmx::Color &);
    bool ClipBeat(CharClip *, CharDriver *, bool);
    float BeatA(int);
    float BeatB(int);
    inline bool BeatAligned(int, int);
    bool LocalMin(int, int);
    bool FindBestNode(float, float, float, Node &);
    void FindBestNodeRecurse(float, float, float, float, float);
    void
    GenerateDistEntry(CharBonesMeshes &, DistEntry &, float, CharClip *, const std::vector<RndTransformable *> &);
    CharClip *ClipA() const { return mClipA; }
    CharClip *ClipB() const { return mClipB; }

    CharClip *mClipA; // 0x0
    CharClip *mClipB; // 0x4
    const DataArray *mWeightData; // 0x8
    float mAStart; // 0xc
    float mAEnd; // 0x10
    float mBStart; // 0x14
    int mSamplesPerBeat; // 0x18
    float mWorstErr; // 0x1c
    float mLastMinErr; // 0x20
    float mBeatAlign; // 0x24
    int mBeatAlignOffset; // 0x28
    int mBeatAlignPeriod; // 0x2c
    float mBlendWidth; // 0x30
    int mNumSamples; // 0x34
    Array2d mDists; // 0x38
    std::vector<Node> mNodes; // 0x44

protected:
    int CalcWidth();
    int CalcHeight();
};

struct DistMapNodeSort {
    bool operator()(const ClipDistMap::Node &n1, const ClipDistMap::Node &n2) const {
        return n1.curBeat < n2.curBeat ? true : false;
    }
};