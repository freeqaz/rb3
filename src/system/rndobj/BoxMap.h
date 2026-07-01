#pragma once
#include "math/Vec.h"
#include "math/Color.h"
#include "rndobj/Lit.h"

template <class T1, int T2>
class BoxLightArray {
public:
    BoxLightArray() : mNumElements(0) {}
    void Clear() { mNumElements = 0; }
    bool CanAddEntry() const { return mNumElements < T2; }
    T1 *AddEntry() { return &mArray[mNumElements++]; }
    void RemoveEntry() { mNumElements--; }
    unsigned int NumElements() const { return mNumElements; }
    const T1 &operator[](int idx) const { return mArray[idx]; }

    T1 mArray[T2]; // 0x0
    unsigned int mNumElements;
};

class BoxMapLighting {
public:
    // size 0x1c
    struct LightParams_Directional {
        Vector3 mDirection; // 0x0
        Hmx::Color mColor; // 0xc
    };

    // size 0x24
    struct LightParams_Point {
        Vector3 mLightPos; // 0x0
        Hmx::Color mColor; // 0xc
        float mRange; // 0x1c
        float mFalloffStart; // 0x20
    };

    // size 0x50
    // Member names recovered from Bank 5 DWARF (LightParams_Spot + its inlined
    // CachedData sub-struct); Bank 8 flattened the cache fields directly into
    // the struct and reordered mColor ahead of the second Vector3.
    struct LightParams_Spot {
        Vector3 mDirection; // 0x0
        Hmx::Color mColor; // 0xc
        Vector3 mTipPosition; // 0x1c - cone apex (mLightPos - mDirection*tipOffset)
        float mCosTheta; // 0x28 - cone cosine threshold
        float mOneOverOneSubCos; // 0x2c - 1/(1-mCosTheta)
        float mOneOverRange2x; // 0x30 - 1/(2*mRange)
        float mTipOverRange2x; // 0x34 - tipOffset/(2*mRange)
        Vector3 mLightPos; // 0x38 - authored light position (xfm.v)
        float mRange; // 0x44 - beam length
        float mRadiusTop; // 0x48 - top beam radius
        float mRadiusBottom; // 0x4c - bottom beam radius
    };

    BoxMapLighting();
    void Clear();
    bool QueueLight(RndLight *, float);
    bool CacheData(LightParams_Spot &);
    void ApplyQueuedLights(Hmx::Color *, const Vector3 *) const;

    void ApplyLight(Hmx::Color *, const LightParams_Directional &) const;
    void ApplyLight(Hmx::Color *, const LightParams_Point &, const Vector3 &) const;
    void
    ApplyLight(Hmx::Color *, const BoxLightArray<BoxMapLighting::LightParams_Spot, 50> &, const Vector3 &)
        const;

    unsigned int NumQueuedLights() const {
        return mQueued_Directional.NumElements() + mQueued_Point.NumElements()
            + mQueued_Spot.NumElements();
    }

    bool ParamsAt(LightParams_Directional *&pd) {
        if (mQueued_Directional.CanAddEntry()) {
            pd = mQueued_Directional.AddEntry();
            return true;
        } else
            return false;
    }
    bool ParamsAt(LightParams_Point *&pt) {
        if (mQueued_Point.CanAddEntry()) {
            pt = mQueued_Point.AddEntry();
            return true;
        } else
            return false;
    }
    bool ParamsAt(LightParams_Spot *&ps) {
        if (mQueued_Spot.CanAddEntry()) {
            ps = mQueued_Spot.AddEntry();
            return true;
        } else
            return false;
    }

    static Vector3 sAxisDir[6];

    BoxLightArray<LightParams_Directional, 50> mQueued_Directional; // 0x0
    BoxLightArray<LightParams_Point, 50> mQueued_Point; // 0x57c
    BoxLightArray<LightParams_Spot, 50> mQueued_Spot; // 0xc88
};