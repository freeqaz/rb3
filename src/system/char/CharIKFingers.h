#pragma once
#include "char/CharWeightable.h"
#include "char/CharPollable.h"
#include "obj/ObjPtr_p.h"
#include "rndobj/Trans.h"
#include "rndobj/Highlightable.h"
#include "char/Character.h"
#include "obj/ObjVector.h"
#include "char/CharCollide.h"

/** "Pins fingers to world positions" */
class CharIKFingers : public RndHighlightable,
                      public CharWeightable,
                      public CharPollable {
public:
    enum FingerNum {
        kFingerThumb,
        kFingerIndex,
        kFingerMiddle,
        kFingerRing,
        kFingerPinky,
        kFingerNone
    };

    class FingerDesc {
    public:
        FingerDesc()
            : mIsControlled(0), mDestPos(0, 0, 0), mAltDestPos(0, 0, 0), mBone1(0),
              mBone2(0), mBone3(0), mBoneTip(0), mFBlendInFrames(0),
              mFBlendOutFrames(0), mResetFingerRots(1) {}
        ~FingerDesc() {}
        bool mIsControlled; // 0x0
        float mLength; // 0x4
        // Bank 5 DWARF documents a single Vector3 mDestPos in this span; Bank 8
        // added a second slot. Caller evidence (CharKeyHandMidi) shows SetFinger's
        // v1 = the key press position and v2 = the same point offset ~1 unit
        // deeper along the key (-forward). mDestPos (v1) is the primary IK
        // destination: it drives the hand-move distance check in SetFinger and the
        // hand-placement centroid in MoveHand; MoveFinger then bends to whichever
        // of the two is nearer the current fingertip (mAltDestPos = the
        // Bank-8-added alternate press point).
        Vector3 mDestPos; // 0x8
        Vector3 mAltDestPos; // 0x14
        ObjPtr<RndTransformable> mBone1; // 0x20
        ObjPtr<RndTransformable> mBone2; // 0x2c
        ObjPtr<RndTransformable> mBone3; // 0x38
        ObjPtr<RndTransformable> mBoneTip; // 0x44
        float mBone2DestAngle; // 0x50
        float mBone3DestAngle; // 0x54
        float mBone2CurAngle; // 0x58
        float mBone3CurAngle; // 0x5c
        int mFBlendInFrames; // 0x60
        int mFBlendOutFrames; // 0x64
        bool mResetFingerRots; // 0x68
        Vector3 mDestForwardVector; // 0x6c
        Vector3 mCurForwardVector; // 0x78
        bool mDestChanged; // 0x84
    };

    CharIKFingers();
    virtual ~CharIKFingers();
    virtual void Highlight();
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    OBJ_CLASSNAME(CharIKFingers);
    OBJ_SET_TYPE(CharIKFingers);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetName(const char *, class ObjectDir *);

    void MeasureLengths();
    void SetFinger(Vector3, Vector3, FingerNum);
    void ReleaseFinger(FingerNum);
    void CalculateHandDest(int, int);
    void CalculateFingerDest(FingerNum);
    void MoveFinger(FingerNum);
    void FixSingleFinger(RndTransformable *, RndTransformable *, RndTransformable *);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(CharIKFingers)
    static void Init() { REGISTER_OBJ_FACTORY(CharIKFingers) }

    ObjPtr<RndTransformable> mHand; // 0x28
    ObjPtr<RndTransformable> mForeArm; // 0x34
    ObjPtr<RndTransformable> mUpperArm; // 0x40
    int mBlendInFrames; // 0x4c
    int mBlendOutFrames; // 0x50
    bool mResetHandDest; // 0x54
    bool mResetCurHandTrans; // 0x55
    Transform mCurHandTrans; // 0x58
    Transform mDestHandTrans; // 0x88
    float mFingerCurledLength; // 0xb8
    Vector3 mDestForwardVector; // 0xbc
    Vector3 mCurForwardVector; // 0xc8
    /** "Starting hand offset from keyboard." */
    Vector3 mHandKeyboardOffset; // 0xd4
    Hmx::Matrix3 mtx; // 0xe0
    /** "how much to move forward when pinky or thumb is engaged" */
    float mHandMoveForward; // 0x104
    /** "how much to rotate the hand (radians) when pinky is engaged" */
    float mHandPinkyRotation; // 0x108
    /** "how much to rotate the hand (radians) when thumb is engaged" */
    float mHandThumbRotation; // 0x10c
    /** "x offset for right/left hands from average destination position for fingers" */
    float mHandDestOffset; // 0x110
    /** "Does this run the right or left hand?" */
    bool mIsRightHand; // 0x114
    bool mMoveHand; // 0x115
    bool mIsSetup; // 0x116
    std::vector<FingerDesc> mFingers; // 0x118
    float mInv2ab; // 0x120
    float mAAPlusBB; // 0x124
    /** "This trans will be set to the desired hand position." */
    ObjPtr<RndTransformable> mOutputTrans; // 0x128
    /** "A keyboard bone so we can calculate in local space. use rh/lh targets." */
    ObjPtr<RndTransformable> mKeyboardRefBone; // 0x134
};
