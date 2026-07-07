#pragma once
#include "game/BandUser.h"
#include "meta_band/SessionMgr.h"
#include "net/NetSession.h"
#include "net/VoiceChatMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Timer.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"
#include <list>

// Values from the HandlePassiveMessage switch (setup_message_* handlers).
// An empty enum's valid value range is 0..1, so loading/storing types 2-4
// through it is UB — clang -O2 (wasm) folds those switch cases to
// `unreachable`, which killed the tab when award toasts fired at song end.
enum PassiveMessageType {
    kPassiveMessageText = 0,
    kPassiveMessageIcon = 1,
    kPassiveMessageCareerStep = 2,
    kPassiveMessageCareerGoal = 3,
    kPassiveMessageCareerMulti = 4
};

class PassiveMessage {
public:
    PassiveMessage(
        DataArray *a,
        PassiveMessageType t,
        Symbol s,
        int i4,
        int i5,
        int i6,
        int i7,
        int i8,
        const char *c1,
        const char *c2,
        const char *c3,
        int i48,
        bool b4c
    )
        : mText(a), mType(t), mChannel(s), unk10(i4), unk14(i6), unk18(i7), mMeterAnimValue(i8),
          unk20(i5), unk24(c1), unk30(c2), unk3c(c3), unk48(i48), unk4c(b4c) {
        mText->AddRef();
    }
    virtual ~PassiveMessage() { mText->Release(); }

    void AddAnim(int delta) {
        // The earned-accomplishment coalesce path (GetAndPreProcessFirstMessage)
        // sums (mMeterAnimValue - unk14) across every queued accomplishment. When
        // >= 4 are earned at once (e.g. a full-combo expert pass earns many
        // "first time" goals simultaneously), the running fan-meter delta goes
        // negative, tripping this debug-only invariant. The RETAIL (non-debug)
        // build compiles MILO_ASSERT out and proceeds with a negative meter (a
        // cosmetic over-fill at worst); on native we run with asserts ON, so
        // mirror retail here rather than OSFatal — otherwise the popups screen's
        // accomplishment toast aborts before it can hand off to the score-detail
        // (coop_endgame) screen. Wii build is byte-identical (HX_NATIVE-gated).
        // Same rationale + precedent as MILO_FAIL_DTA in os/Debug.h.
#ifndef HX_NATIVE
        MILO_ASSERT(mMeterAnimValue >= 0, 0x4A);
#endif
        mMeterAnimValue += delta;
    }

    DataArray *mText; // 0x4
    PassiveMessageType mType; // 0x8
    Symbol mChannel; // 0xc
    int unk10; // 0x10
    int unk14;
    int unk18;
    int mMeterAnimValue; // 0x1c
    int unk20;
    String unk24;
    String unk30;
    String unk3c;
    int unk48;
    bool unk4c;
};

class PassiveMessageQueue {
public:
    PassiveMessageQueue(Hmx::Object *o) : mMessageDuration(2000.0f), mCallback(o) {}
    virtual ~PassiveMessageQueue() {}
    virtual PassiveMessage *GetAndPreProcessFirstMessage();
    virtual void AddMessage(PassiveMessage *);

    void SetMessageDuration(float);
    void Poll();
    void HandlePassiveMessage(PassiveMessage *);
    void MakeIntoCoalescedGoalMessage(PassiveMessage *, int);
    bool RemoveLowerPriorityMessage(PassiveMessage *);

    float mMessageDuration; // 0x4
    std::list<PassiveMessage *> mQueue; // 0x8
    Hmx::Object *mCallback; // 0x10
    Timer mTimer; // 0x14
};

class PassiveMessagesPanel : public UIPanel {
public:
    PassiveMessagesPanel();
    OBJ_CLASSNAME(PassiveMessagesPanel);
    OBJ_SET_TYPE(PassiveMessagesPanel);
    NEW_OBJ(PassiveMessagesPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~PassiveMessagesPanel();
    virtual void SetTypeDef(DataArray *);
    virtual void Poll();

    void QueueMessage(DataArray *, PassiveMessageType, Symbol, int);
    void PostSetup();
    void SetShowMessages(bool);
    PassiveMessageQueue *GetMessageQueue() const { return mMessageQueue; }

    DataNode OnQueueMessage(DataArray *);

    class PassiveMessenger *mMessenger; // 0x38
    PassiveMessageQueue *mMessageQueue; // 0x3c
    bool mShowMessages; // 0x40
};

class PassiveMessenger : public Hmx::Object {
public:
    PassiveMessenger();
    virtual ~PassiveMessenger();
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    void TriggerMessage(
        DataArray *,
        PassiveMessageType,
        LocalBandUser *,
        bool,
        Symbol,
        int = -1,
        int = 0,
        int = 0,
        int = 0,
        int = 0,
        const char * = gNullStr,
        const char * = gNullStr,
        const char * = gNullStr,
        int = 0
    );
    void TriggerSkipSongMsg();
    void TriggerInviteFailedMsg();
    void TriggerRemoteUserLeftMsg(const char *);
    void TriggerEarnedAccomplishmentMsg(
        LocalBandUser *,
        Symbol,
        Symbol,
        int,
        int,
        int,
        int,
        int,
        const char *,
        const char *,
        const char *,
        int
    );
    void TriggerEarnedCampaignLevelMsg(LocalBandUser *, Symbol);
    void TriggerCompletedAccomplishmentCategoryMsg(LocalBandUser *, Symbol);
    void TriggerCompletedAccomplishmentGroupMsg(LocalBandUser *, Symbol);
    void TriggerVoiceChatDisabledMsg();
    void TriggerSetlistSongsRemovedMsg(int);
    bool HasMessages() const;

    DataNode OnMsg(const RemoteUserLeftMsg &);
    DataNode OnMsg(const VoiceChatDisabledMsg &);
    DataNode OnMsg(const SessionDisconnectedMsg &);
    DataNode OnMsg(const InviteSentMsg &);
    DataNode OnMsg(const InviteReceivedMsg &);

    bool unk1c; // 0x1c
    Timer mTimer; // 0x20
};

extern PassiveMessenger *ThePassiveMessenger;