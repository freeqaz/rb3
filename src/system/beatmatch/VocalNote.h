#pragma once
#include "utl/Str.h"
#include "obj/Data.h"
#include "utl/MBT.h"
#include "utl/TempoMap.h"

class VocalNote {
public:
    VocalNote()
        : mPhrase(-1), mBeginPitch(0), mEndPitch(0), mMs(0), mTick(0), mDurationMs(0),
          mDurationTicks(0), mPhraseEnd(0), mUnpitchedPhrase(0), mUnpitchedNote(0),
          mUnpitchedEasy(0), mPitchRangeEnd(0), mPlayerMask(0), mBends(0), mLyricShift(0),
          mAllowCombine(1) {}

    int GetTick() const { return mTick; }
    void SetNoteTime(float ms, int tick) {
        mMs = ms;
        mTick = tick;
    }
    void SetStartPitch(int pitch) { mBeginPitch = pitch; }
    void SetEndPitch(int pitch) { mEndPitch = pitch; }
    float GetDurationMs() const { return mDurationMs; }
    float GetMs() const { return mMs; }
    unsigned short GetDurationTicks() const { return mDurationTicks; }
    bool IsUnpitched() const { return mUnpitchedNote; }
    void SetPhraseEnd(bool b) { mPhraseEnd = b; }
    bool LyricShift() const { return mLyricShift; }

    void SetDurationTime(float ms, int tick) {
        mDurationMs = ms;
        mDurationTicks = tick;
    }
    void SetBends(bool bends) { mBends = bends; }
    void SetText(const char *text) { mText = text; }

    int StartPitch() const { return mBeginPitch; }
    int EndPitch() const { return mEndPitch; }
    int EndTick() const { return mTick + mDurationTicks; }
    float EndMs() const { return mMs + mDurationMs; }
    bool PlayableBy(int) const;

    int mPhrase; // 0x0
    int mBeginPitch; // 0x4
    int mEndPitch; // 0x8
    float mMs; // 0xc
    int mTick; // 0x10
    float mDurationMs; // 0x14
    unsigned short mDurationTicks; // 0x18
    String mText; // 0x1c
    bool mPhraseEnd; // 0x28
    bool mUnpitchedPhrase; // 0x29
    bool mUnpitchedNote; // 0x2a
    bool mUnpitchedEasy; // 0x2b
    bool mPitchRangeEnd; // 0x2c
    unsigned char mPlayerMask; // 0x2d
    bool mBends; // 0x2e
    bool mLyricShift; // 0x2f
    bool mAllowCombine; // 0x30
};

class VocalPhrase {
public:
    VocalPhrase();
    VocalPhrase(const VocalPhrase &phrase)
        : mStartMs(phrase.mStartMs), mDurationMs(phrase.mDurationMs),
          mStartTick(phrase.mStartTick), mDurationTicks(phrase.mDurationTicks),
          mNoteStart(phrase.mNoteStart), mNoteEnd(phrase.mNoteEnd),
          mHasPitchedNotes(phrase.mHasPitchedNotes), mPitchRangeEnd(phrase.mPitchRangeEnd),
          unk1a(phrase.unk1a), mRangeShiftTicks(phrase.mRangeShiftTicks),
          mRangeShiftDur(phrase.mRangeShiftDur), mMinPitch(phrase.mMinPitch),
          mMaxPitch(phrase.mMaxPitch), mPlayerMask(phrase.mPlayerMask),
          mTambourinePhrase(phrase.mTambourinePhrase), mFreeStyleStartMs(phrase.mFreeStyleStartMs),
          mFreeStyleEndMs(phrase.mFreeStyleEndMs) {}

    bool Diff() const { return (mNoteEnd - mNoteStart) == 0; }

    float mStartMs; // 0x0
    float mDurationMs; // 0x4
    int mStartTick; // 0x8
    int mDurationTicks; // 0xc
    int mNoteStart; // 0x10
    int mNoteEnd; // 0x14
    bool mHasPitchedNotes; // 0x18
    bool mPitchRangeEnd; // 0x19
    bool unk1a; // 0x1a
    int mRangeShiftTicks; // 0x1c
    float mRangeShiftDur; // 0x20
    float mMinPitch; // 0x24
    float mMaxPitch; // 0x28
    unsigned char mPlayerMask; // 0x2c
    bool mTambourinePhrase; // 0x2d
    float mFreeStyleStartMs; // 0x30
    float mFreeStyleEndMs; // 0x34
};

class SongData;

class VocalNoteList {
public:
    VocalNoteList(SongData *);
    void Clear();
    void CopyPhrasesFrom(const VocalNoteList *);
    void CopyLyricPhrases();
    void AddNote(const VocalNote &);
    void NotesDone(const TempoMap &, bool);
    void DeterminePhraseTimes(const TempoMap &);
    void Finalize();
    void DetermineFreestyleSections();
    void AddTambourineGem(int);
    void SetFreestyleSections(const std::vector<std::pair<float, float> > &);
    void GenerateLegalFreestyleSections(std::vector<std::pair<float, float> > &) const;
    void RemoveInvalidFreestyleSections();
    void UpdatePitchRangeTickDelimited(int, int, float &, float &);
    void AddLyricShift(float);
    void StartPlayerPhrase(int, int);
    void EndPlayerPhrase(int, int);
    VocalNote *NextNote(float) const;
    const VocalNote *NoteAt(float) const;
    float PitchAt(float) const;
    void CapLastFreestyleSection(float);
    void GetPracticePhrases(std::vector<VocalPhrase> &, int, int) const;
    void GetPracticePhrases2(std::vector<VocalPhrase> &, int, int) const;
    int GetNumPracticePhrases(const std::vector<VocalPhrase> &) const;
    static bool
    IsIllegalFreestyleSection(DataArray *, const std::pair<float, float> &);

    const char *PrintTick(int tick) const;
    Symbol GetTrackName() const { return mTrackName; }
    void SetTrackName(Symbol name) { mTrackName = name; }
    const std::vector<VocalNote> &GetNotes() const { return mNotes; }
    std::vector<VocalPhrase> &GetPhrases() { return mPhrases; }
    std::vector<VocalPhrase> &GetLyricPhrases() { return mLyricPhrases; }
    int HasNoteInRange(int, int) const;

    std::vector<VocalPhrase> mPhrases; // 0x0
    std::vector<VocalPhrase> mLyricPhrases; // 0x8
    std::vector<VocalNote> mNotes; // 0x10
    std::vector<int> mTambourineGems; // 0x18
    std::vector<std::pair<float, float> > mFreestyleSections; // 0x20
    Symbol mTrackName; // 0x28
    SongData *mSongData; // 0x2c
    DataArray *mFreestyleMinDuration; // 0x30
    DataArray *mFreestylePad; // 0x34
};
