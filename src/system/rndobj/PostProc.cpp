#include "rndobj/PostProc.h"
#include "decomp.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Rnd.h"
#include "rndobj/HiResScreen.h"
#include "math/Utl.h"
#include "math/Rand.h"
#include "obj/Task.h"
#include "rndobj/Utl.h"
#include "utl/Messages.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "bandobj/BandDirector.h"

RndPostProc *RndPostProc::sCurrent = 0;
DOFOverrideParams RndPostProc::sDOFOverride;

INIT_REVS(RndPostProc)

RndPostProc::RndPostProc()
    : mPriority(1.0f), mBloomColor(1.0f, 1.0f, 1.0f, 0.0f), mBloomThreshold(4.0f),
      mBloomIntensity(0.0f), mBloomGlare(0), mBloomStreak(0),
      mBloomStreakAttenuation(0.9f), mBloomStreakAngle(0.0f), mLuminanceMap(this, 0),
      mForceCurrentInterp(0), mColorXfm(), mPosterLevels(0.0f), mPosterMin(1.0f),
      mKaleidoscopeComplexity(0.0f), mKaleidoscopeSize(0.5f), mKaleidoscopeAngle(0.0f),
      mKaleidoscopeRadius(0.0f), mKaleidoscopeFlipUVs(1), mFlickerModBounds(0.0f, 0.0f),
      mFlickerTimeBounds(0.001f, 0.007f), mFlickerSeconds(0.0f, 0.0f),
      mColorModulation(1.0f), mNoiseBaseScale(32.0f, 24.0f), mNoiseTopScale(1.35914f),
      mNoiseIntensity(0.0f), mNoiseStationary(0), mNoiseMidtone(1), mNoiseMap(this, 0),
      mTrailThreshold(1.0f), mTrailDuration(0.0f), mBlendVec(1.0f, 1.0f, 0.0f),
      mEmulateFPS(30.0f), mLastRender(0.0f), mHallOfTimeType(0), mHallOfTimeRate(0.0f),
      mHallOfTimeColor(1.0f, 1.0f, 1.0f, 0.0f), mHallOfTimeMix(0.0f),
      mMotionBlurWeight(1.0f, 1.0f, 1.0f, 0.0f), mMotionBlurBlend(0.0f),
      mMotionBlurVelocity(1), mGradientMap(this, 0), mGradientMapOpacity(0.0f),
      mGradientMapIndex(0.0f), mGradientMapStart(0.0f), mGradientMapEnd(1.0f),
      mRefractMap(this, 0), mRefractDist(0.05f), mRefractScale(1.0f, 1.0f),
      mRefractPanning(0.0f, 0.0f), mRefractVelocity(0.0f, 0.0f), mRefractAngle(0.0f),
      mChromaticAberrationOffset(0.0f), mChromaticSharpen(0),
      mVignetteColor(0.0f, 0.0f, 0.0f, 0.0f), mVignetteIntensity(0.0f) {
    mColorXfm.Reset();
}

RndPostProc::~RndPostProc() {
    Unselect();
    if (TheRnd->GetPostProcOverride() == this)
        TheRnd->SetPostProcOverride(0);
}

void RndPostProc::Select() {
    if (sCurrent != this) {
        if (sCurrent)
            sCurrent->OnUnselect();
        sCurrent = this;
        OnSelect();
    }
}

void RndPostProc::Unselect() {
    if (sCurrent == this) {
        sCurrent->OnUnselect();
        sCurrent = 0;
    }
}

void RndPostProc::Reset() {
    if (sCurrent) {
        sCurrent->OnUnselect();
        sCurrent = 0;
    }
}

void RndPostProc::OnSelect() {
    TheRnd->RegisterPostProcessor(this);
    Handle(selected_msg, false);
}

void RndPostProc::OnUnselect() {
    TheRnd->UnregisterPostProcessor(this);
    Handle(unselected_msg, false);
}

RndPostProc *RndPostProc::Current() { return sCurrent; }

float RndPostProc::BloomIntensity() const {
    if (mBloomGlare != 0 && TheHiResScreen.mActive != 0) {
        return mBloomIntensity / 3.0f;
    }
    return mBloomIntensity;
}

bool RndPostProc::DoGradientMap() const {
    bool ret = false;

    if ((mGradientMapOpacity > 0.0f) && mGradientMap.mPtr != 0) {
        ret = 1;
    }
    return ret;
}

bool RndPostProc::DoRefraction() const {
    bool ret = false;

    if (mRefractMap.mPtr != 0 && (0.0f != mRefractDist)) {
        ret = 1;
    }
    return ret;
}

bool RndPostProc::DoVignette() const { return mVignetteIntensity != 0.0f; }

bool RndPostProc::HallOfTime() const { return mHallOfTimeRate != 0.0f; }

// fn_80624B04
void RndPostProc::UpdateTimeDelta() {
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float val150 = secs - mLastRender;
    mDeltaSecs = val150;
    mDeltaSecs = Clamp(0.0f, 1.0f, val150);
    mLastRender = secs;
}

void RndPostProc::DoPost() {
    UpdateTimeDelta();
    UpdateColorModulation();
    UpdateBlendPrevious();
}

// fn_80624BB4
void RndPostProc::UpdateColorModulation() {
    if (mFlickerTimeBounds.x > 0 && mFlickerTimeBounds.y > 0 && mFlickerModBounds.y > 0) {
        float fy = mFlickerSeconds.y;
        if (mFlickerSeconds.x >= fy) {
            float diff = mFlickerSeconds.x - fy;
            mFlickerSeconds.x = diff;
            mFlickerSeconds.x = Max(mFlickerSeconds.x, 0.0f);
            mColorModulation =
                1.0f - RandomFloat(mFlickerModBounds.x, mFlickerModBounds.y);
            mFlickerSeconds.y = RandomFloat(mFlickerTimeBounds.x, mFlickerTimeBounds.y);
            mFlickerSeconds.y = Max(mFlickerSeconds.x, mFlickerSeconds.y);
        }
        mFlickerSeconds.x += mDeltaSecs;
    } else
        mColorModulation = 1.0f;
}

void RndPostProc::UpdateBlendPrevious() {
    bool shouldBlend =
        mTrailThreshold < 1.0f && mTrailDuration > 0.0f && !TheHiResScreen.IsActive();
    if (shouldBlend) {
        MILO_ASSERT(mTrailDuration > 0.f, 0xf6);
        mBlendVec.x = mTrailThreshold;
        mBlendVec.y = mDeltaSecs / mTrailDuration;
        mBlendVec.z = 0.3333333333f;
    }
    return;
}

bool RndPostProc::BlendPrevious() const {
    return mTrailThreshold < 1.0f && mTrailDuration > 0.0f && !TheHiResScreen.IsActive();
}

DataNode RndPostProc::OnAllowedNormalMap(const DataArray *) {
    return GetNormalMapTextures(Dir());
}

SAVE_OBJ(RndPostProc, 524)

BEGIN_COPYS(RndPostProc)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndPostProc)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPriority)
        COPY_MEMBER(mBloomIntensity)
        COPY_MEMBER(mBloomColor)
        COPY_MEMBER(mBloomThreshold)
        COPY_MEMBER(mBloomGlare)
        COPY_MEMBER(mBloomStreak)
        COPY_MEMBER(mBloomStreakAttenuation)
        COPY_MEMBER(mBloomStreakAngle)
        COPY_MEMBER(mLuminanceMap)
        COPY_MEMBER(mColorXfm)
        COPY_MEMBER(mFlickerModBounds)
        COPY_MEMBER(mFlickerTimeBounds)
        COPY_MEMBER(mNoiseBaseScale)
        COPY_MEMBER(mNoiseTopScale)
        COPY_MEMBER(mNoiseIntensity)
        COPY_MEMBER(mNoiseStationary)
        COPY_MEMBER(mNoiseMap)
        COPY_MEMBER(mNoiseMidtone)
        COPY_MEMBER(mTrailDuration)
        COPY_MEMBER(mTrailThreshold)
        COPY_MEMBER(mEmulateFPS)
        COPY_MEMBER(mPosterLevels)
        COPY_MEMBER(mPosterMin)
        COPY_MEMBER(mKaleidoscopeComplexity)
        COPY_MEMBER(mKaleidoscopeSize)
        COPY_MEMBER(mKaleidoscopeAngle)
        COPY_MEMBER(mKaleidoscopeRadius)
        COPY_MEMBER(mKaleidoscopeFlipUVs)
        COPY_MEMBER(mHallOfTimeRate)
        COPY_MEMBER(mHallOfTimeColor)
        COPY_MEMBER(mHallOfTimeMix)
        COPY_MEMBER(mHallOfTimeType)
        COPY_MEMBER(mMotionBlurBlend)
        COPY_MEMBER(mMotionBlurWeight)
        COPY_MEMBER(mMotionBlurVelocity)
        COPY_MEMBER(mGradientMap)
        COPY_MEMBER(mGradientMapIndex)
        COPY_MEMBER(mGradientMapOpacity)
        COPY_MEMBER(mGradientMapStart)
        COPY_MEMBER(mGradientMapEnd)
        COPY_MEMBER(mRefractMap)
        COPY_MEMBER(mRefractDist)
        COPY_MEMBER(mRefractScale)
        COPY_MEMBER(mRefractPanning)
        COPY_MEMBER(mRefractVelocity)
        COPY_MEMBER(mRefractAngle)
        COPY_MEMBER(mChromaticAberrationOffset)
        COPY_MEMBER(mChromaticSharpen)
        COPY_MEMBER(mVignetteColor)
        COPY_MEMBER(mVignetteIntensity)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(RndPostProc)
    LOAD_REVS(bs)
    ASSERT_REVS(37, 0)
    if (gRev == 16) {
        int dRev;
        bs >> dRev;
        MILO_ASSERT(dRev == 3, 667);
        float f;
        bool x;
        Vector3 v;
        int i;
        bs >> x >> v >> f >> i;
    } else
        LOAD_SUPERCLASS(Hmx::Object)
    LoadRev(bs, gRev);
END_LOADS

float RndPostProc::sMotionBlurBlendAmount;

DECOMP_FORCEACTIVE(PostProc, "%s can't load new %s version")

void RndPostProc::LoadRev(BinStream &bs, int rev) {
    if (rev > 4) {
        if (rev > 0xA) {
            bs >> mBloomColor;
            if (rev < 0x18) {
                int dummy;
                bs >> dummy;
            }
            bs >> mBloomIntensity;
            bs >> mBloomThreshold;
        } else {
            Hmx::Color c;
            bs >> c;
            float minVal = c.red;
            if (minVal > c.green)
                minVal = c.green;
            if (minVal > c.blue)
                minVal = c.blue;
            if (minVal < 4.0f) {
                float range = 4.0f - minVal;
                c.red = (4.0f - c.red) / range;
                c.green = (4.0f - c.green) / range;
                c.blue = (4.0f - c.blue) / range;
                mBloomThreshold = c.alpha;
                c.alpha = 0.0f;
                mBloomColor = c;
            } else {
                mBloomColor.red = 1.0f;
                mBloomColor.green = 1.0f;
                mBloomColor.blue = 1.0f;
                mBloomColor.alpha = 0.0f;
                mBloomThreshold = c.alpha;
            }
            int dummy;
            bs >> dummy;
            bs >> mBloomIntensity;
            mBloomIntensity = std::sqrt(mBloomIntensity);
            int dummy2;
            bs >> dummy2;
        }
    }
    if (rev > 5) {
        bs >> mLuminanceMap;
    }
    if (rev > 6) {
        if (rev < 0x12) {
            bs >> mColorXfm.mColorXfm.m.x >> mColorXfm.mColorXfm.m.y
                >> mColorXfm.mColorXfm.m.z;
            bs >> mColorXfm.mColorXfm.v;
        } else {
            if (!mColorXfm.Load(bs)) {
                MILO_FAIL("%s can't load new %s version", PathName(this), ClassName());
            }
        }
        bs >> mFlickerModBounds >> mFlickerTimeBounds;
        if (rev < 9) {
            mFlickerModBounds.x = 1.0f - mFlickerModBounds.x;
            mFlickerModBounds.y = 1.0f - mFlickerModBounds.y;
        }
        if (rev < 0x1D) {
            mFlickerModBounds.x = 0.0f;
        }
        bs >> mNoiseBaseScale >> mNoiseTopScale >> mNoiseIntensity;
        if (rev > 0xC) {
            bs >> mNoiseStationary;
        }
        if (rev > 8) {
            bs >> mNoiseMap;
        }
        if (rev > 0x24) {
            bs >> mNoiseMidtone;
        } else {
            mNoiseMidtone = false;
        }
        if (rev < 0x12) {
            bs >> mColorXfm.mHue >> mColorXfm.mSaturation >> mColorXfm.mLightness
                >> mColorXfm.mContrast >> mColorXfm.mBrightness;
        }
    }
    if (rev > 7) {
        bs >> mTrailThreshold >> mTrailDuration >> mEmulateFPS;
    }
    if (rev > 9) {
        if (rev < 0x12) {
            bs >> mColorXfm.mLevelInLo >> mColorXfm.mLevelInHi;
            bs >> mColorXfm.mLevelOutLo >> mColorXfm.mLevelOutHi;
        }
        bs >> mPosterLevels;
    }
    if (rev > 0xD) {
        bs >> mPosterMin;
    }
    if (rev > 0xB) {
        if (rev < 0x16) {
            float complexity;
            bs >> complexity;
            if (complexity != 0.0f) {
                mKaleidoscopeComplexity = 2.0f;
            }
        } else {
            bs >> mKaleidoscopeComplexity >> mKaleidoscopeSize >> mKaleidoscopeAngle
                >> mKaleidoscopeRadius >> mKaleidoscopeFlipUVs;
        }
    }
    if (rev > 0xE && rev < 0x1F) {
        int dummy;
        bs >> dummy;
        if (rev < 0x11) {
            int dummy2;
            bs >> dummy2;
            ObjPtr<RndDrawable> dummyDraw(this, 0);
            bs >> dummyDraw;
        }
    }
    if (rev > 0x12) {
        bs >> mHallOfTimeRate;
        bs >> mHallOfTimeColor >> mHallOfTimeMix;
        if (rev > 0x13 && rev < 0x20) {
            bool hotType;
            bs >> hotType;
            mHallOfTimeType = hotType ? 1 : 0;
        } else if (rev > 0x1F) {
            bs >> mHallOfTimeType;
        }
    }
    if (rev > 0x14) {
        bs >> mMotionBlurBlend;
        if (rev > 0x1A) {
            bs >> mMotionBlurWeight;
            if (rev > 0x21) {
                bs >> mMotionBlurVelocity;
            }
        }
    }
    if (rev > 0x16) {
        bs >> mGradientMap >> mGradientMapOpacity >> mGradientMapIndex
            >> mGradientMapStart >> mGradientMapEnd;
    }
    if (rev < 0x18) {
        mBloomThreshold *= 4.0f;
    }
    if (rev > 0x18) {
        bs >> mRefractMap >> mRefractDist >> mRefractScale >> mRefractPanning
            >> mRefractAngle;
        if (rev > 0x1B) {
            bs >> mRefractVelocity;
        }
    }
    if (rev > 0x19) {
        bs >> mChromaticAberrationOffset;
        if (rev > 0x22) {
            bs >> mChromaticSharpen;
        }
    }
    if (rev > 0x1D) {
        bs >> mVignetteColor >> mVignetteIntensity;
    }
    if (rev > 0x20) {
        bs >> mBloomGlare;
    }
    if (rev > 0x23) {
        bs >> mBloomStreak >> mBloomStreakAttenuation >> mBloomStreakAngle;
    }
}

BEGIN_HANDLERS(RndPostProc)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_ACTION(select, Select())
    HANDLE_ACTION(unselect, Unselect())
    HANDLE_ACTION(multi_select, OnSelect())
    HANDLE_ACTION(multi_unselect, OnUnselect())
    HANDLE_ACTION(
        interp,
        Interp(_msg->Obj<RndPostProc>(2), _msg->Obj<RndPostProc>(3), _msg->Float(4))
    )
    HANDLE(allowed_normal_map, OnAllowedNormalMap)
    HANDLE_CHECK(0x3BB)
END_HANDLERS

#pragma push
#pragma dont_inline on
BEGIN_PROPSYNCS(RndPostProc)
    SYNC_PROP(priority, mPriority)
    SYNC_PROP(bloom_color, mBloomColor)
    SYNC_PROP(bloom_threshold, mBloomThreshold)
    SYNC_PROP(bloom_intensity, mBloomIntensity)
    SYNC_PROP(bloom_glare, mBloomGlare)
    SYNC_PROP(bloom_streak, mBloomStreak)
    SYNC_PROP(bloom_streak_attenuation, mBloomStreakAttenuation)
    SYNC_PROP(bloom_streak_angle, mBloomStreakAngle)
    SYNC_PROP(luminance_map, mLuminanceMap)
    SYNC_PROP_MODIFY_ALT(hue, mColorXfm.mHue, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(saturation, mColorXfm.mSaturation, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(lightness, mColorXfm.mLightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(brightness, mColorXfm.mBrightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(contrast, mColorXfm.mContrast, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(in_lo, mColorXfm.mLevelInLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(in_hi, mColorXfm.mLevelInHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(out_lo, mColorXfm.mLevelOutLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY_ALT(out_hi, mColorXfm.mLevelOutHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP(num_levels, mPosterLevels)
    SYNC_PROP(min_intensity, mPosterMin)
    SYNC_PROP(kaleidoscope_complexity, mKaleidoscopeComplexity)
    SYNC_PROP(kaleidoscope_size, mKaleidoscopeSize)
    SYNC_PROP(kaleidoscope_angle, mKaleidoscopeAngle)
    SYNC_PROP(kaleidoscope_radius, mKaleidoscopeRadius)
    SYNC_PROP(kaleidoscope_flipUVs, mKaleidoscopeFlipUVs)
    SYNC_PROP(flicker_intensity, mFlickerModBounds)
    SYNC_PROP(flicker_secs_range, mFlickerTimeBounds)
    SYNC_PROP(noise_base_scale, mNoiseBaseScale)
    SYNC_PROP(noise_intensity, mNoiseIntensity)
    SYNC_PROP(noise_stationary, mNoiseStationary)
    SYNC_PROP(noise_midtone, mNoiseMidtone)
    SYNC_PROP(noise_map, mNoiseMap)
    SYNC_PROP(threshold, mTrailThreshold)
    SYNC_PROP(duration, mTrailDuration)
    SYNC_PROP(emulate_fps, mEmulateFPS)
    SYNC_PROP(hall_of_time_type, mHallOfTimeType)
    SYNC_PROP(hall_of_time_rate, mHallOfTimeRate)
    SYNC_PROP(hall_of_time_color, mHallOfTimeColor)
    SYNC_PROP(hall_of_time_mix, mHallOfTimeMix)
    SYNC_PROP(motion_blur_blend, mMotionBlurBlend)
    SYNC_PROP(motion_blur_weight, mMotionBlurWeight)
    SYNC_PROP(motion_blur_exposure, mMotionBlurWeight.alpha)
    SYNC_PROP(motion_blur_velocity, mMotionBlurVelocity)
    SYNC_PROP(gradient_map, mGradientMap)
    SYNC_PROP(gradient_map_opacity, mGradientMapOpacity)
    SYNC_PROP(gradient_map_index, mGradientMapIndex)
    SYNC_PROP(gradient_map_start, mGradientMapStart)
    SYNC_PROP(gradient_map_end, mGradientMapEnd)
    SYNC_PROP(refract_map, mRefractMap)
    SYNC_PROP(refract_dist, mRefractDist)
    SYNC_PROP(refract_scale, mRefractScale)
    SYNC_PROP(refract_panning, mRefractPanning)
    SYNC_PROP(refract_velocity, mRefractVelocity)
    SYNC_PROP(refract_angle, mRefractAngle)
    SYNC_PROP(chromatic_aberration_offset, mChromaticAberrationOffset)
    SYNC_PROP(chromatic_sharpen, mChromaticSharpen)
    SYNC_PROP(vignette_color, mVignetteColor)
    SYNC_PROP(vignette_intensity, mVignetteIntensity)
    SYNC_PROP(force_current_interp, mForceCurrentInterp)
END_PROPSYNCS
#pragma pop

void RndPostProc::Interp(const RndPostProc *from, const RndPostProc *to, float pct) {
    if (!from && !to)
        return;
    if (mForceCurrentInterp)
        return;

    if (!to) {
        to = from;
    } else if (!from) {
        from = to;
    }

    const RndPostProc *pick = pct > 0.0f ? to : from;

    mNoiseMidtone = pick->mNoiseMidtone;
    mNoiseStationary = pick->mNoiseStationary;
    mLuminanceMap = pick->mLuminanceMap.Ptr();
    mNoiseMap = pick->mNoiseMap.Ptr();
    mGradientMap = pick->mGradientMap.Ptr();
    mRefractMap = pick->mRefractMap.Ptr();
    mBloomGlare = pick->mBloomGlare;
    mMotionBlurVelocity = pick->mMotionBlurVelocity;
    mChromaticSharpen = pick->mChromaticSharpen;

    float toBloom = to->BloomIntensity();
    float fromBloom = from->BloomIntensity();
    ::Interp(fromBloom, toBloom, pct, mBloomIntensity);

    ::Interp(from->mBloomColor, to->mBloomColor, pct, mBloomColor);

    ::Interp(from->mBlendVec, to->mBlendVec, pct, mBlendVec);

    { float _a = from->mTrailDuration; mTrailDuration = pct * (to->mTrailDuration - _a) + _a; }
    { float _a = from->mTrailThreshold; mTrailThreshold = pct * (to->mTrailThreshold - _a) + _a; }

    float noisePct = pct;
    if (from != to && from->mNoiseMidtone != to->mNoiseMidtone
        && from->mNoiseIntensity != 0.0f && to->mNoiseIntensity != 0.0f) {
        noisePct = 1.0f;
    }
    {
        float _ay = from->mNoiseBaseScale.y, _ax = from->mNoiseBaseScale.x;
        float _ty = to->mNoiseBaseScale.y, _tx = to->mNoiseBaseScale.x;
        mNoiseBaseScale.y = noisePct * (_ty - _ay) + _ay;
        mNoiseBaseScale.x = noisePct * (_tx - _ax) + _ax;
    }
    { float _a = from->mNoiseTopScale; mNoiseTopScale = noisePct * (to->mNoiseTopScale - _a) + _a; }
    { float _a = from->mNoiseIntensity; mNoiseIntensity = noisePct * (to->mNoiseIntensity - _a) + _a; }

    { float _a = from->mKaleidoscopeComplexity; mKaleidoscopeComplexity = pct * (to->mKaleidoscopeComplexity - _a) + _a; }
    { float _a = from->mKaleidoscopeSize; mKaleidoscopeSize = pct * (to->mKaleidoscopeSize - _a) + _a; }
    { float _a = from->mKaleidoscopeAngle; mKaleidoscopeAngle = pct * (to->mKaleidoscopeAngle - _a) + _a; }
    { float _a = from->mKaleidoscopeRadius; mKaleidoscopeRadius = pct * (to->mKaleidoscopeRadius - _a) + _a; }
    mKaleidoscopeFlipUVs = pct >= 1.0f ? to->mKaleidoscopeFlipUVs : from->mKaleidoscopeFlipUVs;

    { float _a = from->mEmulateFPS; mEmulateFPS = pct * (to->mEmulateFPS - _a) + _a; }

    { float _a = from->mPosterLevels; mPosterLevels = pct * (to->mPosterLevels - _a) + _a; }
    { float _a = from->mPosterMin; mPosterMin = pct * (to->mPosterMin - _a) + _a; }

    { float _a = from->mColorModulation; mColorModulation = pct * (to->mColorModulation - _a) + _a; }

    { float _a = from->mColorXfm.mBrightness; mColorXfm.mBrightness = pct * (to->mColorXfm.mBrightness - _a) + _a; }
    { float _a = from->mColorXfm.mHue; mColorXfm.mHue = pct * (to->mColorXfm.mHue - _a) + _a; }
    { float _a = from->mColorXfm.mSaturation; mColorXfm.mSaturation = pct * (to->mColorXfm.mSaturation - _a) + _a; }
    { float _a = from->mColorXfm.mLightness; mColorXfm.mLightness = pct * (to->mColorXfm.mLightness - _a) + _a; }
    { float _a = from->mColorXfm.mContrast; mColorXfm.mContrast = pct * (to->mColorXfm.mContrast - _a) + _a; }
    ::Interp(from->mColorXfm.mLevelInLo, to->mColorXfm.mLevelInLo, pct, mColorXfm.mLevelInLo);
    ::Interp(from->mColorXfm.mLevelInHi, to->mColorXfm.mLevelInHi, pct, mColorXfm.mLevelInHi);
    ::Interp(from->mColorXfm.mLevelOutLo, to->mColorXfm.mLevelOutLo, pct, mColorXfm.mLevelOutLo);
    ::Interp(from->mColorXfm.mLevelOutHi, to->mColorXfm.mLevelOutHi, pct, mColorXfm.mLevelOutHi);
    mColorXfm.AdjustColorXfm();

    { float _a = from->mGradientMapOpacity; mGradientMapOpacity = pct * (to->mGradientMapOpacity - _a) + _a; }
    { float _a = from->mGradientMapIndex; mGradientMapIndex = pct * (to->mGradientMapIndex - _a) + _a; }
    { float _a = from->mGradientMapStart; mGradientMapStart = pct * (to->mGradientMapStart - _a) + _a; }
    { float _a = from->mGradientMapEnd; mGradientMapEnd = pct * (to->mGradientMapEnd - _a) + _a; }

    { float _a = from->mRefractDist; mRefractDist = pct * (to->mRefractDist - _a) + _a; }
    {
        float _ay = from->mRefractScale.y, _ty = to->mRefractScale.y;
        float _ax = from->mRefractScale.x, _tx = to->mRefractScale.x;
        mRefractScale.x = pct * (_tx - _ax) + _ax;
        mRefractScale.y = pct * (_ty - _ay) + _ay;
    }
    {
        float _ay = from->mRefractPanning.y, _ty = to->mRefractPanning.y;
        float _ax = from->mRefractPanning.x, _tx = to->mRefractPanning.x;
        mRefractPanning.x = pct * (_tx - _ax) + _ax;
        mRefractPanning.y = pct * (_ty - _ay) + _ay;
    }
    {
        float _ay = from->mRefractVelocity.y, _ty = to->mRefractVelocity.y;
        float _ax = from->mRefractVelocity.x, _tx = to->mRefractVelocity.x;
        mRefractVelocity.x = pct * (_tx - _ax) + _ax;
        mRefractVelocity.y = pct * (_ty - _ay) + _ay;
    }
    { float _a = from->mRefractAngle; mRefractAngle = pct * (to->mRefractAngle - _a) + _a; }

    {
        bool notMV = !TheBandDirector->IsMusicVideo();
        if (notMV) {
            { float _a = from->mMotionBlurBlend; mMotionBlurBlend = pct * (to->mMotionBlurBlend - _a) + _a; }
            ::Interp(from->mMotionBlurWeight, to->mMotionBlurWeight, pct, mMotionBlurWeight);
        }
    }

    if (pct == 0.0f) {
        mMotionBlurBlend = sMotionBlurBlendAmount;
    }

    { float _a = from->mChromaticAberrationOffset; mChromaticAberrationOffset = pct * (to->mChromaticAberrationOffset - _a) + _a; }

    ::Interp(from->mVignetteColor, to->mVignetteColor, pct, mVignetteColor);
    { float _a = from->mVignetteIntensity; mVignetteIntensity = pct * (to->mVignetteIntensity - _a) + _a; }

    {
        float _ay = from->mFlickerTimeBounds.y, _ax = from->mFlickerTimeBounds.x;
        float _ty = to->mFlickerTimeBounds.y, _tx = to->mFlickerTimeBounds.x;
        mFlickerTimeBounds.x = pct * (_tx - _ax) + _ax;
        mFlickerTimeBounds.y = pct * (_ty - _ay) + _ay;
    }
    {
        float _ay = from->mFlickerModBounds.y, _ax = from->mFlickerModBounds.x;
        float _ty = to->mFlickerModBounds.y, _tx = to->mFlickerModBounds.x;
        mFlickerModBounds.x = pct * (_tx - _ax) + _ax;
        mFlickerModBounds.y = pct * (_ty - _ay) + _ay;
    }

    if (from->mHallOfTimeRate != 0.0f) {
        mHallOfTimeType = from->mHallOfTimeType;
        mHallOfTimeRate = from->mHallOfTimeRate;
        mHallOfTimeColor = from->mHallOfTimeColor;
        mHallOfTimeMix = from->mHallOfTimeMix;
    } else {
        mHallOfTimeRate = 0.0f;
    }
}

bool RndPostProc::DoMotionBlur() const {
    bool ret = false;
    if (mMotionBlurBlend > 0.0f && mMotionBlurWeight.Pack() > 0
        && !TheHiResScreen.IsActive()) {
        ret = true;
    }
    return ret;
}

bool RndPostProc::ColorXfmEnabled() const {
    bool ret = false;
    if (mColorModulation != 1.0f
        || mColorXfm.mHue != 0.0f
        || mColorXfm.mSaturation != 0.0f
        || mColorXfm.mLightness != 0.0f
        || mColorXfm.mContrast != 0.0f
        || mColorXfm.mBrightness != 0.0f
        || mColorXfm.mLevelInLo.Pack() != 0
        || mColorXfm.mLevelOutLo.Pack() != 0
        || mColorXfm.mLevelInHi.Pack() != 0xffffff
        || mColorXfm.mLevelOutHi.Pack() != 0xffffff) {
        ret = true;
    }
    return ret;
}

ProcCounter::ProcCounter()
    : mProcAndLock(0), mCount(0), mSwitch(0), mOdd(0), mFPS(0), mEvenOddDisabled(0),
      mTriFrameRendering(0) {}

void ProcCounter::SetProcAndLock(bool pandl) {
    mProcAndLock = pandl;
    mCount = -1;
}

void ProcCounter::SetEvenOddDisabled(bool eod) {
    if (mEvenOddDisabled == eod)
        return;
    else
        mEvenOddDisabled = eod;
    if (mEvenOddDisabled)
        mCount = -1;
}

ProcessCmd ProcCounter::ProcCommands() {
    int count;
    int retCmd;

    if ((this->mProcAndLock != false) && (this->mCount == 0)) {
        return kProcessNone;
    }
    if (this->mEvenOddDisabled != false) {
        return kProcessAll;
    }
    count = this->mCount;
    retCmd = 0;
    switch (count) {
    case -1:
        this->mCount = -1;
        retCmd = 7;
        break;
    case 0:
        retCmd = 1;
        break;
    case 1:
        retCmd = 6;
        if (this->mTriFrameRendering != false) {
            retCmd = 4;
        }
        break;
    case 2:
        retCmd = 2;
        break;
    }
    short compare_value = (mTriFrameRendering != 0) ? 2 : 1;
    this->mCount++;
    count = this->mCount;

    if (count > compare_value) {
        this->mCount = 0;
    }
    return (ProcessCmd)retCmd;
}

DOFOverrideParams::DOFOverrideParams()
    : mDepthScale(1.0f), mDepthOffset(0.0f), mMinBlurScale(1.0f), mMinBlurOffset(0.0f),
      mMaxBlurScale(1.0f), mMaxBlurOffset(0.0f), mBlurWidthScale(1.0f) {}
