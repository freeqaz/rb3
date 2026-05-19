#include "bandobj/CharKeyHandMidi.h"
#include "os/Debug.h"
#include "utl/Symbols.h"

CharKeyHandMidi::CharKeyHandMidi()
    : mIKObject(this, 0), mFirstSpot(this, 0), mSecondSpot(this, 0),
      unk64(0), unk68(5), unk74(5), unk78(true), unk7c(this, 0), unk88(0.0f), mIsRightHand(true) {
    unk6c.resize(5);
    for (int i = 0; i < 5; i++) {
        unk6c[i] = 0;
    }
    unk4c.resize(26);
    unk54.resize(26);
    Enter();
}

CharKeyHandMidi::~CharKeyHandMidi() {}

void CharKeyHandMidi::EndTest() {
    if (mIKObject) {
        mIKObject->ReleaseFinger(CharIKFingers::kFingerThumb);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerIndex);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerMiddle);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerRing);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerPinky);
    }
}

void CharKeyHandMidi::UnkeyFinger(CharIKFingers::FingerNum finger) {
    MILO_ASSERT(finger >= 0 && finger < CharIKFingers::kFingerNone, 0x16a);
    mIKObject->ReleaseFinger(finger);
    unk6c[finger] = 0;
    unk74++;
}

CharIKFingers::FingerNum CharKeyHandMidi::DefaultSelectFinger(KeyboardKey key) {
    for (int i = 1; i < 5; i++) {
        if (unk6c[i] == 0) {
            KeyFinger((CharIKFingers::FingerNum)i, key);
            return (CharIKFingers::FingerNum)i;
        }
    }
    if (unk6c[0] == 0) {
        KeyFinger(CharIKFingers::kFingerThumb, key);
        return CharIKFingers::kFingerThumb;
    }
    return CharIKFingers::kFingerNone;
}

BEGIN_HANDLERS(CharKeyHandMidi)
    HANDLE(fingers_up, OnFingersUp)
    HANDLE(fingers_down, OnFingersDown)
    HANDLE_ACTION(run_test, RunTest())
    HANDLE_ACTION(end_test, EndTest())
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x319)
END_HANDLERS

BEGIN_PROPSYNCS(CharKeyHandMidi)
    SYNC_PROP(ik_object, mIKObject)
    SYNC_PROP(first_spot, mFirstSpot)
    SYNC_PROP(second_spot, mSecondSpot)
    SYNC_PROP(is_right_hand, mIsRightHand)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS