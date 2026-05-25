#include "bandobj/CharKeyHandMidi.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "math/Vec.h"
#include "obj/Task.h"
#include "utl/BinStream.h"
#include "utl/MakeString.h"
#include "utl/Symbols.h"
#include <algorithm>

INIT_REVS(CharKeyHandMidi);

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

void CharKeyHandMidi::Highlight() {
    if (mFirstSpot) {
        if (mSecondSpot) goto draw;
    }
    return;
draw:
    UtilDrawSphere(mFirstSpot->WorldXfm().v, 1.0f, Hmx::Color(1.0f, 1.0f, 1.0f));
    UtilDrawSphere(mSecondSpot->WorldXfm().v, 1.0f, Hmx::Color(1.0f, 1.0f, 1.0f));
    for (int key = 1; key <= 0x19; key++) {
        if (IsBlackKey((KeyboardKey)key)) {
            UtilDrawSphere(unk4c[key], 0.15f, Hmx::Color(0.0f, 1.0f, 0.0f));
            UtilDrawSphere(unk54[key], 0.15f, Hmx::Color(1.0f, 1.0f, 0.0f));
        } else {
            UtilDrawSphere(unk4c[key], 0.15f, Hmx::Color(0.0f, 0.0f, 1.0f));
            UtilDrawSphere(unk54[key], 0.15f, Hmx::Color(1.0f, 0.0f, 1.0f));
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

#pragma fp_contract off
void CharKeyHandMidi::Poll() {
    if (!mFirstSpot)
        return;
    if (!mSecondSpot)
        return;

    if (unk7c && unk7c->mTeleported) {
        unk78 = true;
        if (mIKObject)
            mIKObject->mResetCurHandTrans = true;
    }

    if (unk78) {
        Transform &firstXfm = mFirstSpot->WorldXfm();
        Vector3 firstPos(firstXfm.v.x, firstXfm.v.y, firstXfm.v.z);

        Vector3 keyDir;
        Subtract(mSecondSpot->WorldXfm().v, firstPos, keyDir);
        float keyDist = Length(keyDir);
        Normalize(keyDir, keyDir);

        Vector3 upDir;
        Normalize(mFirstSpot->WorldXfm().m.z, upDir);

        Vector3 &forward = mFirstSpot->WorldXfm().m.y;

        float halfStep = keyDist / 28.0f;
        float fullStep = keyDist / 14.0f;

        float negFwdX = forward.x * -1.0f;
        float negFwdY = forward.y * -1.0f;
        float negFwdZ = forward.z * -1.0f;
        float tipX = negFwdX * 1.0f;
        float tipY = negFwdY * 1.0f;
        float tipZ = negFwdZ * 1.0f;

        float curX = firstPos.x + upDir.x * -0.4f;
        float curY = firstPos.y + upDir.y * -0.4f;
        float curZ = firstPos.z + upDir.z * -0.4f;

        float whiteX = keyDir.x * fullStep;
        float whiteY = keyDir.y * fullStep;
        float whiteZ = keyDir.z * fullStep;

        float blackX = upDir.x * 0.5f + (negFwdX * 2.0f + keyDir.x * halfStep);
        float blackY = upDir.y * 0.5f + (negFwdY * 2.0f + keyDir.y * halfStep);
        float blackZ = upDir.z * 0.5f + (negFwdZ * 2.0f + keyDir.z * halfStep);

        unk4c[1].Set(curX, curY, curZ);
        unk54[1].Set(curX + tipX, curY + tipY, curZ + tipZ);

        for (int key = 2; key <= 0x19; key++) {
            if (IsBlackKey((KeyboardKey)key)) {
                float px = curX + blackX;
                float py = curY + blackY;
                float pz = curZ + blackZ;
                unk4c[key].Set(px, py, pz);
                unk54[key].Set(px + tipX, py + tipY, pz + tipZ);
            } else {
                curX = curX + whiteX;
                curY = curY + whiteY;
                curZ = curZ + whiteZ;
                unk4c[key].Set(curX, curY, curZ);
                unk54[key].Set(curX + tipX, curY + tipY, curZ + tipZ);
            }
        }
        unk78 = false;
    }

    float now = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    if (now < unk88) {
        for (int i = 0; i < 5; i++) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
        unk5c.clear();
        unk88 = now;
        return;
    }
    unk88 = now;

    int numKeysDown = unk5c.size();
    if (numKeysDown > 5) {
        TheDebug.Notify(MakeString("Too many keyboard keys down in one poll: %d\n", numKeysDown));
        unk5c.clear();
        return;
    }

    if (mIsRightHand) {
        std::sort(unk5c.begin(), unk5c.end());
    } else {
        std::sort(unk5c.begin(), unk5c.end(), std::greater<int>());
    }

    if (numKeysDown <= 0)
        return;

    if (unk74 == 5) {
        switch (numKeysDown) {
        case 1: {
            KeyboardKey k0 = unk5c[0];
            CharIKFingers::FingerNum f =
                FindPreferredFinger(k0, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f != CharIKFingers::kFingerNone)
                KeyFinger(f, k0);
            break;
        }
        case 2: {
            KeyboardKey k0 = unk5c[0];
            KeyboardKey k1 = unk5c[1];
            CharIKFingers::FingerNum f0 =
                FindPreferredFinger(k0, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f0 > CharIKFingers::kFingerMiddle)
                f0 = CharIKFingers::kFingerMiddle;
            KeyFinger(f0, k0);
            KeyFinger(
                FindPreferredFinger(k1, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68), k1
            );
            break;
        }
        case 3: {
            KeyboardKey k0 = unk5c[0];
            KeyboardKey k1 = unk5c[1];
            KeyboardKey k2 = unk5c[2];
            KeyFinger(CharIKFingers::kFingerThumb, k0);
            CharIKFingers::FingerNum f1 =
                FindPreferredFinger(k1, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f1 == CharIKFingers::kFingerPinky)
                KeyFinger(CharIKFingers::kFingerRing, k1);
            else
                KeyFinger(f1, k1);
            KeyFinger(
                FindPreferredFinger(k2, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68), k2
            );
            break;
        }
        case 4:
            KeyFinger(CharIKFingers::kFingerThumb, unk5c[0]);
            KeyFinger(CharIKFingers::kFingerIndex, unk5c[1]);
            KeyFinger(CharIKFingers::kFingerMiddle, unk5c[2]);
            KeyFinger(CharIKFingers::kFingerPinky, unk5c[3]);
            break;
        case 5:
            KeyFinger(CharIKFingers::kFingerThumb, unk5c[0]);
            KeyFinger(CharIKFingers::kFingerIndex, unk5c[1]);
            KeyFinger(CharIKFingers::kFingerMiddle, unk5c[2]);
            KeyFinger(CharIKFingers::kFingerRing, unk5c[3]);
            KeyFinger(CharIKFingers::kFingerPinky, unk5c[4]);
            break;
        }
    } else {
        if (numKeysDown > unk74) {
            TheDebug.Notify(
                FormatString("Keyboard fingers: not enough free fingers to play a "
                              "note, please check the authoring!")
                    .Str()
            );
        }
        std::vector<CharIKFingers::FingerNum> usedFingers;
        bool allOk = true;
        KeyboardKey lastKey = (KeyboardKey)unk64;
        CharIKFingers::FingerNum lastFinger = (CharIKFingers::FingerNum)unk68;
        for (int i = 0; i < numKeysDown; i++) {
            CharIKFingers::FingerNum f =
                FindPreferredFinger(unk5c[i], lastKey, lastFinger);
            if (f == CharIKFingers::kFingerNone || unk6c[f] != 0) {
                allOk = false;
                break;
            }
            if (std::find(usedFingers.begin(), usedFingers.end(), f) != usedFingers.end()) {
                allOk = false;
                break;
            }
            usedFingers.push_back(f);
        }
        if (allOk) {
            for (int i = 0; i < numKeysDown; i++) {
                KeyFinger(usedFingers[i], unk5c[i]);
            }
        } else {
            for (int i = 0; i < numKeysDown; i++) {
                DefaultSelectFinger(unk5c[i]);
            }
        }
    }
    unk5c.clear();
}
#pragma fp_contract on

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

SAVE_OBJ(CharKeyHandMidi, 0x2E3)

BEGIN_COPYS(CharKeyHandMidi)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharKeyHandMidi)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mIKObject)
        COPY_MEMBER(mFirstSpot)
        COPY_MEMBER(mSecondSpot)
        COPY_MEMBER(mIsRightHand)
    END_COPYING_MEMBERS
END_COPYS

void CharKeyHandMidi::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mFirstSpot);
    changedBy.push_back(mSecondSpot);
}

BEGIN_LOADS(CharKeyHandMidi)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mIKObject;
    bs >> mFirstSpot;
    bs >> mSecondSpot;
    if (gRev > 1)
        bs >> mIsRightHand;
END_LOADS

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