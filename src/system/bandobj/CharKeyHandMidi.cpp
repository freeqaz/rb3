#include "bandobj/CharKeyHandMidi.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"
#include <algorithm>

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

void CharKeyHandMidi::Enter() {
    for (int i = 0; i < 5; i++) {
        if (unk6c[i] != 0) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
    }
    unk5c.clear();
    RndPollable::Enter();
}

void CharKeyHandMidi::SetName(const char *name, ObjectDir *dir) {
    Hmx::Object::SetName(name, dir);
    unk7c = dynamic_cast<Character *>(dir);
}

void CharKeyHandMidi::RunTest() {
    unk78 = true;
    if (mIKObject) {
        mIKObject->SetFinger(unk4c[25], unk54[25], CharIKFingers::kFingerPinky);
        mIKObject->SetFinger(unk4c[13], unk54[13], CharIKFingers::kFingerThumb);
    }
}

CharIKFingers::FingerNum CharKeyHandMidi::FindPreferredFinger(
    KeyboardKey setToKey, KeyboardKey lastKeyDown, CharIKFingers::FingerNum lastFingerDown
) {
    if (setToKey == lastKeyDown)
        return lastFingerDown;
    if (lastKeyDown == 0 || lastFingerDown == CharIKFingers::kFingerNone)
        return CharIKFingers::kFingerMiddle;
    int distance = abs(setToKey - lastKeyDown);
    if (setToKey > lastKeyDown) {
        if (mIsRightHand) {
            if (lastFingerDown == CharIKFingers::kFingerPinky)
                return CharIKFingers::kFingerPinky;
            int finger;
            if (distance <= 2)
                finger = lastFingerDown + 1;
            else if (distance <= 5)
                finger = lastFingerDown + 2;
            else if (distance <= 7)
                finger = lastFingerDown + 3;
            else
                return CharIKFingers::kFingerPinky;
            if (finger > 4)
                finger = CharIKFingers::kFingerPinky;
            return (CharIKFingers::FingerNum)finger;
        } else {
            if (lastFingerDown == CharIKFingers::kFingerThumb)
                return CharIKFingers::kFingerThumb;
            int finger;
            if (distance <= 2)
                finger = lastFingerDown - 1;
            else if (distance <= 5)
                finger = lastFingerDown - 2;
            else if (distance <= 7)
                finger = lastFingerDown - 3;
            else
                return CharIKFingers::kFingerThumb;
            if (finger < 0)
                finger = CharIKFingers::kFingerThumb;
            return (CharIKFingers::FingerNum)finger;
        }
    } else {
        if (mIsRightHand) {
            if (lastFingerDown == CharIKFingers::kFingerThumb)
                return CharIKFingers::kFingerThumb;
            int finger;
            if (distance <= 2)
                finger = lastFingerDown - 1;
            else if (distance <= 5)
                finger = lastFingerDown - 2;
            else if (distance <= 7)
                finger = lastFingerDown - 3;
            else
                return CharIKFingers::kFingerThumb;
            if (finger < 0)
                finger = CharIKFingers::kFingerThumb;
            return (CharIKFingers::FingerNum)finger;
        } else {
            if (lastFingerDown == CharIKFingers::kFingerPinky)
                return CharIKFingers::kFingerPinky;
            int finger;
            if (distance <= 2)
                finger = lastFingerDown + 1;
            else if (distance <= 5)
                finger = lastFingerDown + 2;
            else if (distance <= 7)
                finger = lastFingerDown + 3;
            else
                return CharIKFingers::kFingerPinky;
            if (finger > 4)
                finger = CharIKFingers::kFingerPinky;
            return (CharIKFingers::FingerNum)finger;
        }
    }
}

bool CharKeyHandMidi::IsBlackKey(KeyboardKey key) {
    switch (key) {
    case 2:
    case 4:
    case 7:
    case 9:
    case 0xb:
    case 0xe:
    case 0x10:
    case 0x13:
    case 0x15:
    case 0x17:
        return true;
    default:
        return false;
    }
}

void CharKeyHandMidi::EndTest() {
    if (mIKObject) {
        mIKObject->ReleaseFinger(CharIKFingers::kFingerThumb);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerIndex);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerMiddle);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerRing);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerPinky);
    }
}

DataNode CharKeyHandMidi::OnFingersUp(DataArray *msg) {
    KeyboardKey key = (KeyboardKey)msg->Int(2);
    MILO_ASSERT(key > kNoKey && key <= kKeyC4, 0x65);
    for (int i = 0; i < 5; i++) {
        if (unk6c[i] == (KeyboardKey)(key - kNoKey)) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
    }
    std::remove(unk5c.begin(), unk5c.end(), (KeyboardKey)(key - kNoKey));
    return 0;
}

DataNode CharKeyHandMidi::OnFingersDown(DataArray *msg) {
    KeyboardKey key = (KeyboardKey)msg->Int(2);
    MILO_ASSERT(key > kNoKey && key <= kKeyC4, 0x81);
    unk5c.push_back((KeyboardKey)(key - kNoKey));
    return 0;
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

bool CharKeyHandMidi::KeyFinger(CharIKFingers::FingerNum finger, KeyboardKey setToKey) {
    if (!(finger >= 0 && finger < CharIKFingers::kFingerNone)) {
        TheDebug.Notify(MakeString("CharKeyHandMidi: Trying to key non-existent finger"));
        return false;
    }
    if ((unsigned int)(setToKey - 1) > 0x18) {
        TheDebug.Notify(MakeString("CharKeyHandMidi: Trying to put finger on non-existent key"));
        return false;
    }
    if (setToKey == unk6c[0]) return false;
    if (setToKey == unk6c[1]) return false;
    if (setToKey == unk6c[2]) return false;
    if (setToKey == unk6c[3]) return false;
    if (setToKey == unk6c[4]) return false;
    mIKObject->SetFinger(unk4c[setToKey], unk54[setToKey], finger);
    unk6c[finger] = setToKey;
    unk68 = finger;
    unk64 = setToKey;
    unk74--;
    return true;
}

void CharKeyHandMidi::UnkeyFinger(CharIKFingers::FingerNum finger) {
    MILO_ASSERT(finger >= 0 && finger < CharIKFingers::kFingerNone, 0x16a);
    mIKObject->ReleaseFinger(finger);
    unk6c[finger] = 0;
    unk74++;
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