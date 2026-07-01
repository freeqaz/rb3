#pragma once
#include "GemManager.h"
#include "Track.h"
#include "bandobj/GemTrackDir.h"
#include "beatmatch/FillInfo.h"
#include "game/BandUser.h"
#include "rndobj/Anim.h"

class GemTrack : public Track {
public:
    class RangeShift {
    public:
        RangeShift(int i1, int i2, float f1, float f2)
            : mStartTick(i1), mEndTick(i2), mStartOffset(-1), mEndOffset(f1),
              mStartRange(-1), mEndRange(f2), mMaskDrawn(0), mMaskStartFrame(0),
              mMaskEndFrame(0) {}
        int mStartTick; // 0x0
        int mEndTick; // 0x4
        float mStartOffset; // 0x8
        float mEndOffset; // 0xc
        float mStartRange; // 0x10
        float mEndRange; // 0x14
        bool mMaskDrawn; // 0x18
        int mMaskStartFrame; // 0x1c
        int mMaskEndFrame; // 0x20
    };

    GemTrack(BandUser *);
    virtual ~GemTrack();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Init();
    virtual void PlayerInit();
    virtual void PostInit();

    virtual void SetBonusGems(bool);
    virtual void SetGemsEnabled(float);
    virtual void SetGemsEnabledByPlayer();
    virtual void UpdateGems();
    virtual float NextKickNoteMs() const;
    virtual Hmx::Object *GetSmasher(int);
    virtual void ResetSmashers(bool);
    virtual bool Lefty() const { return mTrackConfig.IsLefty(); }
    virtual void RebuildBeats();
    virtual void UpdateSlotXfms();
    virtual void UpdateShifts();
    virtual void RefreshCurrentShift();
    virtual void PlayKeyIntros();

    virtual void Poll(float);
    virtual void Jump(float);
    virtual void SetDir(RndDir *);
    virtual RndDir *GetDir() { return mTrackDir; }
    virtual BandTrack *GetBandTrack();
    virtual void SetSmasherGlowing(int, bool);
    virtual void PopSmasher(int);
    virtual void OnMissPhrase(int);
    virtual void RemovePlayer();
    virtual void UpdateLeftyFlip();

    GemManager *GetGemManager();
    void HandleNewSong();
    void ApplyShiftImmediately(const RangeShift &);
    void ResetFills(bool);
    void UpdateShiftsToTick(int);
    void CheckShifts(float, int);
    void UpdateFills();
    void ChangeDifficulty(Difficulty, int);
    void DropIn(int);
    void SetPlayerState(const PlayerState &);
    void DrawFill(FillInfo *, int, int);
    void RedrawTrackElements(float);
    void DrawTrackElements(int, int);
    void See(float, int);
    void Hit(float, int, int);
    void Miss(float, int, int);
    void Pass(int);
    void Ignore(int);
    void PartialHit(float, int, unsigned int, int);
    void FillHit(int, int);
    void SetFretButtonPressed(int, bool);
    void ReleaseGem(float, int);
    void SetInCoda(bool);
    void UpdateEffects(int);
    void OverrideRangeShift(float, float);
    void SetEnableSlot(int, bool);
    void DrawBeatLine(Symbol, int, int, bool);
    void DrawBeatLines(int, int);
    GemTrackDir *GetTrackDir() const { return mTrackDir; }
    bool ShiftsEnabled() const;
    float TickToOffset(int) const;
    float GetRange() const { return mRange; }
    float GetOffset() const { return mOffset; }

    bool mResetFills; // 0x68
    bool mUseFills; // 0x69
    ObjPtr<GemTrackDir> mTrackDir; // 0x6c
    int mLastTopTick; // 0x78
    int mLastBottomTick; // 0x7c
    GemManager *mGemManager; // 0x80
    PlayerState mPlayerState; // 0x84
    PlayerState mLastPlayerState; // 0x9c
    unsigned short mKickPassCounter; // 0xb4
    bool mUpdateShifting; // 0xb6
    bool mEnableShifting; // 0xb7
    std::vector<RangeShift> mRangeShifts; // 0xb8
    RangeShift *mCurrentRangeShift; // 0xc0
    float mRange; // 0xc4
    float mOffset; // 0xc8
    ObjPtr<RndAnimatable> mUpcomingShiftMaskAnim; // 0xcc
    int mBeatLineSubdivisionTicks; // 0xd8
    ObjPtrList<Task> mKeyIntroTasks; // 0xdc
};
