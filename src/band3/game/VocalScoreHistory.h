#pragma once
#include <vector>

class VocalScoreCache {
public:
    VocalScoreCache()
        : mHitPercentage(0), mFramePoints(0), mPhrasePointsCap(0), mUncappedFramePoints(0),
          mVibratoPoints(0), mTargetPitch(0), mOctaveOffset(0), mVoiced(0),
          mSpamming(0), unk22(0), mVocalEnergy(0) {}
    ~VocalScoreCache() {}

    float GetHitPercentage() const { return mHitPercentage; }

    float mHitPercentage; // 0x0
    float mFramePoints; // 0x4
    float mPhrasePointsCap; // 0x8
    float mUncappedFramePoints; // 0xc
    float mVibratoPoints; // 0x10
    float mTargetPitch; // 0x14
    float mTargetPitchMs; // 0x18
    int mOctaveOffset; // 0x1c
    bool mVoiced; // 0x20
    bool mSpamming; // 0x21
    bool unk22; // 0x22
    float mVocalEnergy; // 0x24
};

class VocalScoreHistory {
public:
    VocalScoreHistory(int, int);
    ~VocalScoreHistory();

    void AddScore(float, float);
    void SetOctaveOffset(int);
    float CalculateSum(float) const;
    void Reset();
    void BiasLastScore(float);
    int GetOctaveOffset() const;

    float unk0;
    std::vector<float> mScores; // 0x4
    int unkc;
    int unk10;
    int unk14;
    float unk18;
    int mOctaveOffset; // 0x1c
    bool unk20;
};