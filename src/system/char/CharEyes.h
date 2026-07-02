#pragma once
#include "obj/ObjPtr_p.h"
#include "rndobj/Highlightable.h"
#include "char/CharWeightable.h"
#include "char/CharPollable.h"
#include "char/CharEyeDartRuleset.h"
#include "char/CharInterest.h"
#include "rndobj/Trans.h"
#include "rndobj/Overlay.h"

class CharFaceServo;
class CharWeightSetter;
class CharLookAt;
class CharInterest;

/** "Moves a bunch of lookats around" */
class CharEyes : public RndHighlightable, public CharWeightable, public CharPollable {
public:
    class EyeDesc {
    public:
        EyeDesc(Hmx::Object *);
        EyeDesc(const EyeDesc &);
        EyeDesc &operator=(const EyeDesc &);

        /** "Eye to retarget" */
        ObjOwnerPtr<CharLookAt> mEye; // 0x0
        /** "corresponding upper lid bone, must rotate about Z" */
        ObjPtr<RndTransformable> mUpperLid; // 0xc
        /** "corresponding lower lid bone, must rotate about Z" */
        ObjPtr<RndTransformable> mLowerLid; // 0x18
        /** "optional - child of lower_lid, placed at edge of lower lid geometry.  It will
         * be used to detect and resolve interpenetration of the lids" */
        ObjPtr<RndTransformable> mLowerLidBlink; // 0x24
        /** "optional - child of upper_lid, placed at edge of upper lid geometry.  It will
         * be used to detect and resolve interpenetration of the lids" */
        ObjPtr<RndTransformable> mUpperLidBlink; // 0x30
    };

    class CharInterestState {
    public:
        CharInterestState(Hmx::Object *);
        CharInterestState(const CharInterestState &);
        CharInterestState &operator=(const CharInterestState &);
        void ResetState();
        void BeginRefractoryPeriod();
        bool IsInRefractoryPeriod();
        float RefractoryTimeRemaining();

        ObjOwnerPtr<CharInterest> mInterest; // 0x0
        float mLastTimeStamp; // 0xc
    };

    CharEyes();
    virtual ~CharEyes();
    virtual void Highlight();
    OBJ_CLASSNAME(CharEyes);
    OBJ_SET_TYPE(CharEyes);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    virtual void Enter();
    virtual void Exit();
    virtual void Replace(Hmx::Object *, Hmx::Object *);
    virtual void ListPollChildren(std::list<RndPollable *> &) const;

    RndTransformable *GetHead();
    RndTransformable *GetTarget();
    void ClearAllInterestObjects();
    void AddInterestObject(CharInterest *);
    /** "force a procedural blink for testing" */
    void ForceBlink();
    void ProceduralBlinkUpdate();
    void SetEnableBlinks(bool, bool);
    bool SetFocusInterest(CharInterest *, int);
    bool EyesOnTarget(float);
    void ToggleInterestsDebugOverlay();
    CharInterest *GetCurrentInterest();
    CharInterest *GetInterest(int idx) {
        return idx >= mInterests.size() ? ObjOwnerPtr<CharInterest>(nullptr)
                                        : mInterests[idx].mInterest;
    }
    void EnforceMinimumTargetDistance(const Vector3 &, const Vector3 &, Vector3 &);
    void UpdateOverlay();
    bool EitherEyeClamped();
    bool IsHeadIKWeightIncreasing();
    void DartUpdate();
    Vector3 GenerateDartOffset();
    void LidTrackAndClampingUpdate(EyeDesc &, float);
    void NextLook();
    int NumInterests() const { return mInterests.size(); }

    void SetInterestFilterFlags(int i) {
        mInterestFilterFlags = i;
        mInterestFiltersChanged = true;
    }

    void ClearInterestFilterFlags() { mInterestFilterFlags = mDefaultFilterFlags; }

    /** "for testing, this forces the current interest to a focus target" */
    DataNode OnToggleForceFocus(DataArray *);
    /** "for testing, this shows the debug overlay for interest objects" */
    DataNode OnToggleInterestOverlay(DataArray *);
    DataNode OnAddInterest(DataArray *);

    /** "globally disables eye darts for all characters" */
    static bool sDisableEyeDart;
    /** "globally disables eye jitter for all characters" */
    static bool sDisableEyeJitter;
    /** "globally disables use of interest objects for all characters" */
    static bool sDisableInterestObjects;
    /** "globally disables use of procedural blinks for all characters" */
    static bool sDisableProceduralBlink;
    /** "globally disables eye lid clamping on all characters" */
    static bool sDisableEyeClamping;

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(CharEyes)
    static void Init() { REGISTER_OBJ_FACTORY(CharEyes) }

    ObjVector<EyeDesc> mEyes; // 0x28
    ObjVector<CharInterestState> mInterests; // 0x34
    /** "the CharFaceServo if any, used to allow blinks through the eyelid following" */
    ObjPtr<CharFaceServo> mFaceServo; // 0x40
    /** "The weight setter for eyes tracking the camera" */
    ObjPtr<CharWeightSetter> mCamWeight; // 0x4c
    Vector3 mTarget; // 0x58
    int mDefaultFilterFlags; // 0x64 - mask
    /** "optional bone that serves as the reference for which direction the character is
     * looking.  If not set, one of the eyes will be used" */
    ObjPtr<RndTransformable> mViewDirection; // 0x68
    /** "optionally supply a head lookat to inform eyes what the head is doing.  used
     * primarily to coordinate eye lookats with head ones..." */
    ObjPtr<CharLookAt> mHeadLookAt; // 0x74
    /** "in degrees, the maximum angle we can offset the current view direction when
     * extrapolating for generated interests". Ranges from 0 to 90. */
    float mMaxExtrapolation; // 0x80
    /** "the minimum distance, in inches, that this interest can be from the eyes.
     *  If the interest is less than this distance, the eyes look in the same direction,
     * but projected out to this distance.  May be overridden per interest object." */
    float mMinTargetDist; // 0x84
    /** "affects rotation applied to upper lid when eyes rotate up". Ranges from 0 to 10.
     */
    float mUpperLidTrackUp; // 0x88
    /** "affects rotation applied to upper lid when eyes rotate down". Ranges from 0
     * to 10. */
    float mUpperLidTrackDown; // 0x8c
    /** "translates lower lids up/down when eyes rotate up". Ranges from 0 to 10. */
    float mLowerLidTrackUp; // 0x90
    /** "translates lower lids up/down when eyes rotate down". Ranges from 0 to 10. */
    float mLowerLidTrackDown; // 0x94
    /** "if checked, lower lid tracking is done by rotation instead of translation" */
    bool mLowerLidTrackRotate; // 0x98
    /** the "eye_status" debug overlay */
    RndOverlay *mOverlay; // 0x9c
    int mInterestFilterFlags; // 0xa0 - also a mask
    Vector3 mLastFacing; // 0xa4
    /** cos of the angle between the facing dir and the target dir last poll */
    float mLastCang; // 0xb0
    /** seconds spent looking at the current interest */
    float mLastLook; // 0xb4
    /** default max view cone cos when no interest overrides it: cos(30 deg) */
    float mMaxEyeCang; // 0xb8
    float mAvDelta; // 0xbc
    float mLastBlinkWeight; // 0xc0
    bool mBlinkDetect; // 0xc4
    bool mTargetTooClose; // 0xc5
    ObjPtr<CharInterest> mCurInterest; // 0xc8
    ObjPtr<CharInterest> mFocusInterest; // 0xd4
    int mCurFocusPriorityClass; // 0xe0
    bool mNewFocusInterest; // 0xe4
#ifdef MILO_DEBUG
    Vector3 mLastExtrapolatedDir; // 0xe8 - reference view direction (default: Y-axis)
#endif
    float mLastHeadIKWeight; // 0xf4
    CharEyeDartRuleset::EyeDartRulesetData mEyeDartRulesetData; // 0xf8
    bool mDarting; // 0x124
    float mDartNextEventTime; // 0x128
    int mDartsRemaining; // 0x12c
    Vector3 mCurDartOffset; // 0x130
    /** a procedural blink is currently in progress */
    bool mProceduralBlink; // 0x13c
    float mBlinkTimestamp; // 0x140
    int mBlinkWindowCount; // 0x144
    float mBlinkWindowSecsRemaining; // 0x148
    float mLastBlinkDetectTime; // 0x14c
    Vector3 mDelayedTarget; // 0x150
    bool mInterestFiltersChanged; // 0x15c
    /** semantic (non-DWARF) name - Bank-8-added member, set by SetEnableBlinks */
    bool mBlinksEnabled; // 0x15d
};

// class CharEyes : public RndHighlightable, public CharWeightable, public CharPollable {
//     // total size: 0xD0
// protected:
//     class ObjVector mEyes; // offset 0x28, size 0x10
//     class ObjVector mInterests; // offset 0x38, size 0x10
//     class ObjPtr mFaceServo; // offset 0x48, size 0xC
//     class ObjPtr mCamWeight; // offset 0x54, size 0xC
//     class Vector3 mTarget; // offset 0x60, size 0x10
//     class Vector3 mLastFacing; // offset 0x70, size 0x10
//     float mLastCang; // offset 0x80, size 0x4
//     float mLastLook; // offset 0x84, size 0x4
//     float mMaxEyeCang; // offset 0x88, size 0x4
//     float mAvDelta; // offset 0x8C, size 0x4
//     float mLastBlinkWeight; // offset 0x90, size 0x4
//     unsigned char mBlinkDetect; // offset 0x94, size 0x1
// };