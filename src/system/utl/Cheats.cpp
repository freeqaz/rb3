#include "utl/Cheats.h"
#include "os/Joypad.h"
#include "os/UserMgr.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "rndwii/Rnd.h"
#include "utl/Symbols.h"

CheatsManager *gCheatsManager;
static bool sKeyCheatsEnabled = true;
bool gDisable;

CheatsManager::CheatsManager() : mKeyCheatsEnabled(sKeyCheatsEnabled) {
    mLastButtonTime.Start();
    SystemConfig()->FindData("cheats_buffer", mMaxBuffer, true);
}

void CheatsManager::CallCheatScript(bool b1, DataArray *arr2, LocalUser *user, bool b2) {
    if (!user && TheUserMgr) {
        std::vector<LocalUser *> localUsers;
        TheUserMgr->GetLocalUsers(localUsers);
        for (std::vector<LocalUser *>::iterator it = localUsers.begin();
             it != localUsers.end();
             ++it) {
            if ((*it)->GetPadNum() == -1)
                break;
            JoypadData *data = JoypadGetPadData((*it)->GetPadNum());
            int dataType = data->mType;
            if (!b1 || !b2 || (unsigned int)(dataType - 1) <= 2U || (unsigned int)(dataType - 0x13) <= 2U) {
                user = *it;
                break;
            }
        }
    }
    if (user) {
        JoypadData *padData = JoypadGetPadData(user->GetPadNum());
        int mType = padData->mType;
        if (b1 && b2 && mType != kJoypadDigital && mType != kJoypadAnalog
            && mType != kJoypadWiiCore && mType != kJoypadWiiFS
            && mType != kJoypadWiiClassic && mType != kJoypadDualShock) {
            return;
        }
        DataVariable("cheat_pad") = user ? user->GetPadNum() : 0;
        LogCheat(user ? user->GetPadNum() : -1, b1, arr2);
        if (b1) {
            int i = 2;
            for (; arr2->Node(i).Type() != kDataCommand && i < arr2->Size(); i++)
                ;
            if (i < arr2->Size()) {
                arr2->ExecuteScript(i, nullptr, nullptr, 1);
            }
        } else
            arr2->Execute();
        Hmx::Object *uiObj = ObjectDir::Main()->Find<Hmx::Object>("ui", true);
        static Message msg("cheat_invoked", 0, 0);
        msg[0] = b1;
        msg[1] = DataNode(arr2, kDataArray);
        uiObj->Handle(msg, false);
    }
}

BEGIN_HANDLERS(CheatsManager)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(KeyboardKeyMsg)
    HANDLE_CHECK(0x115)
END_HANDLERS

void InitQuickJoyCheats(const DataArray *arr, CheatsManager::ShiftMode shiftmode) {
    for (int i = 1; i < arr->Size(); i++) {
        DataArray *curArr = arr->Array(i);
        if (curArr->Int(0) >= 0 && curArr->Int(0) < 0x18) {
            QuickJoyCheat cheat;
            cheat.unk0 = curArr->Int(0);
            cheat.unk4 = curArr;
            gCheatsManager->AddQuickJoyCheat(cheat, shiftmode);
        } else
            MILO_LOG("Error in quick_cheats: %s is not a valid button\n", curArr->Str(0));
    }
}

void InitLongJoyCheats(const DataArray *arr) {
    for (int i = 1; i < arr->Size(); i++) {
        DataArray *curArr = arr->Array(i);
        DataArray *buttonArr = curArr->Array(0);
        if (buttonArr->Size() > 0x10) {
            MILO_LOG("Too many buttons in long cheat, max %d\n", 0x10);
        } else {
            LongJoyCheat cheat;
            bool push = true;
            for (int j = 0; j < buttonArr->Size(); j++) {
                int curBtn = buttonArr->Int(j);
                if (curBtn > 0x17U) {
                    MILO_LOG("Error in long_cheats: %s is not a valid button\n", curBtn);
                    push = false;
                    break;
                }
                cheat.unk0.push_back(curBtn);
            }
            if (push) {
                cheat.unkc = curArr->Command(1);
                gCheatsManager->AddLongJoyCheat(cheat);
            }
        }
    }
}

inline void CheatsManager::RebuildKeyCheatsForMode() {
    mKeyCheatPtrsMode.clear();
    for (std::vector<KeyCheat>::iterator it = mKeyCheats.begin(); it != mKeyCheats.end();
         ++it) {
        DataArray *modesArr = it->unk8->FindArray(modes, false);
        if (!modesArr || modesArr->Contains(mSymMode)) {
            mKeyCheatPtrsMode.push_back(&*it);
        }
    }
    for (int i = 0; i < 2; i++) {
        mJoyCheatPtrsMode[i].clear();
        for (std::vector<QuickJoyCheat>::iterator it = mQuickJoyCheats[i].begin();
             it != mQuickJoyCheats[i].end();
             ++it) {
            DataArray *modesArr = it->unk4->FindArray(modes, false);
            if (!modesArr || modesArr->Contains(mSymMode)) {
                mJoyCheatPtrsMode[i].push_back(&*it);
            }
        }
    }
}

void InitKeyCheats(const DataArray *arr) {
    for (int i = 1; i < arr->Size(); i++) {
        KeyCheat keyCheat;
        DataArray *cheat = arr->Array(i);
        if (cheat->Type(0) == kDataInt) {
            if (cheat->Int(0) < 0) {
                MILO_LOG(
                    "Error in quick_cheats: %i is not a valid key code\n", cheat->Int(0)
                );
                continue;
            } else {
                keyCheat.unk0 = cheat->Int(0);
            }
        } else {
            const char *key = cheat->Str(0);
            if (strlen(key) > 1) {
                MILO_LOG("Error in quick_cheats: %s is not a valid key\n", key);
                continue;
            } else {
                keyCheat.unk0 = key[0];
            }
        }
        keyCheat.unk5 = false;
        keyCheat.unk4 = false;
        int subIndex = 1;
        for (; subIndex < cheat->Size() && cheat->Type(subIndex) == kDataSymbol;
             subIndex++) {
            Symbol keyModifier = cheat->Sym(subIndex);
            if (keyModifier == ctrl)
                keyCheat.unk4 = true;
            else if (keyModifier == alt)
                keyCheat.unk5 = true;
            else {
                MILO_WARN("Unknown modifier symbol in cheat: %s", cheat->Sym(subIndex));
            }
        }
        MILO_ASSERT(cheat->Type( subIndex ) == kDataString, 0x251);
        keyCheat.unk8 = cheat;
        gCheatsManager->AddKeyCheat(keyCheat);
    }
    gCheatsManager->RebuildKeyCheatsForMode();
}

void EnableKeyCheats(bool b) {
    sKeyCheatsEnabled = b;
    if (gCheatsManager)
        gCheatsManager->mKeyCheatsEnabled = b;
}

bool GetEnabledKeyCheats() { return sKeyCheatsEnabled; }

DataNode SetKeyCheatsEnabled(DataArray *arr) {
    EnableKeyCheats(arr->Int(1));
    return 0;
}

DataNode OnSetCheatMode(DataArray *arr) {
    SetCheatMode(arr->Sym(1));
    return 0;
}

DataNode OnGetCheatMode(DataArray *arr) { return gCheatsManager->mSymMode; }

bool CheatsInitialized() { return gCheatsManager; }
DataNode MemHuntStatsDF(DataArray *) { return 0; }

DataNode ForceGPHangRecoveryDF(DataArray *arr) {
    if (!gbDbgRequestHangRecovery && !gbDbgRequestForcedHang) {
        bool b3 = false;
        bool b2 = false;
        bool b1 = false;
        if (arr && arr->Size() > 1) {
            b1 = true;
        }
        if (b1 && arr->Node(1).Type() == kDataInt) {
            b2 = true;
        }
        if (b2 && arr->Int(1) != 0) {
            b3 = true;
        }
        if (b3) {
            gbDbgRequestForcedHang = true;
            OSReport("GPHangDebug: Requesting forced hang.\n");
        } else {
            gbDbgRequestHangRecovery = true;
            OSReport("GPHangDebug: Requesting hang recovery.\n");
        }
    }
    return 0;
}

void CheatsInit() {
    SystemConfig()->FindData("disable_cheats", gDisable, true);
    if (!gDisable) {
        MILO_ASSERT(gCheatsManager == null, 0x2A5);
        gCheatsManager = new CheatsManager();
        JoypadSubscribe(gCheatsManager);
        KeyboardSubscribe(gCheatsManager);
        DataArray *cheatCfg = SystemConfig("quick_cheats");
        InitQuickJoyCheats(cheatCfg->FindArray("left"), CheatsManager::kLeftShift);
        InitQuickJoyCheats(cheatCfg->FindArray("right"), CheatsManager::kRightShift);
        InitKeyCheats(cheatCfg->FindArray("keyboard"));
        InitLongJoyCheats(SystemConfig("long_cheats"));
        DataRegisterFunc("set_key_cheats_enabled", SetKeyCheatsEnabled);
        DataRegisterFunc("set_cheat_mode", OnSetCheatMode);
        DataRegisterFunc("get_cheat_mode", OnGetCheatMode);
        DataRegisterFunc("mem_hunt_stats", MemHuntStatsDF);
        DataRegisterFunc("gp_hang", ForceGPHangRecoveryDF);
    }
}

void CheatsTerminate() {
    if (!gDisable) {
#ifdef HX_NATIVE
        // CheatsInit() is SKIPPED on native (System.cpp:724 — cheats.dta binds
        // punctuation keys the lexer mis-parses, and cheats are irrelevant to
        // boot/render). gDisable stays at its zero-init false, so this branch
        // runs but gCheatsManager is null. The MILO_ASSERT would abort the
        // shutdown path; bail when the symmetric Init didn't run.
        if (!gCheatsManager)
            return;
#endif
        MILO_ASSERT(gCheatsManager, 0x2D0);
        JoypadUnsubscribe(gCheatsManager);
        KeyboardUnsubscribe(gCheatsManager);
        RELEASE(gCheatsManager);
    }
}

void LogCheat(int i, bool b, DataArray *a) {
    MILO_ASSERT(gCheatsManager, 0x2D9);
    gCheatsManager->Log(i, b, a);
}

void CallQuickCheat(DataArray *a, LocalUser *u) {
    MILO_ASSERT(gCheatsManager, 0x2DF);
    gCheatsManager->CallCheatScript(true, a, u, false);
}

void CheatsManager::Log(int i, bool b, DataArray *a) {
    CheatLog log;
    log.unk4 = i;
    log.unk0 = b;
    log.unk8 = DataNode(a, kDataArray);
    mBuffer.push_front(log);
    if (mBuffer.size() > mMaxBuffer) {
        mBuffer.pop_back();
    }
}

void AppendCheatsLog(char *c) {
    if (gCheatsManager)
        gCheatsManager->AppendLog(c);
}

void CheatsManager::AppendLog(char *c) {
    if (mBuffer.size() != 0) {
        strcat(c, "\n\nCheats Used");
        char buf[10] = "\n   %.30s";
        for (std::list<CheatLog>::iterator it = mBuffer.begin(); it != mBuffer.end();
             ++it) {
            String str;
            it->unk8.Print(str, true);
            strcat(c, MakeString(buf, str));
        }
        if (mMaxBuffer == mBuffer.size()) {
            strcat(c, "\n   ...");
        }
    }
}

DataNode CheatsManager::OnMsg(const KeyboardKeyMsg &msg) {
    if (!mKeyCheatsEnabled) {
        return DataNode(kDataUnhandled, 0);
    }
    int key = msg.GetKey();
    std::vector<KeyCheat *> cheats(mKeyCheatPtrsMode);
    for (std::vector<KeyCheat *>::iterator it = cheats.begin(); it != cheats.end();
         ++it) {
        KeyCheat *cur = *it;
        if (key == cur->unk0 && msg.GetCtrl() == cur->unk4
            && msg.GetAlt() == cur->unk5) {
            CallCheatScript(true, cur->unk8, nullptr, true);
        }
    }
    return DataNode(1);
}

bool CheatsManager::OnMsg(const ButtonDownMsg &msg) {
    User *user = msg.GetUser();
    if (!user)
        return 1;
    LocalUser *localUser = user->GetLocalUser();

    int padNum = localUser->GetPadNum();
    JoypadData *padData = JoypadGetPadData(padNum);
    unsigned int buttons = padData->mButtons;

    bool leftShift = (buttons & (1 << kPad_L1)) && (buttons & (1 << kPad_L2));
    bool rightShift = (buttons & (1 << kPad_R1)) && (buttons & (1 << kPad_R2));

    JoypadButton button = msg.GetButton();

    if ((JoypadGetPadData(localUser->GetPadNum())->mType == kJoypadWiiCore ||
         JoypadGetPadData(localUser->GetPadNum())->mType == kJoypadWiiFS ||
         JoypadGetPadData(localUser->GetPadNum())->mType == kJoypadWiiClassic) &&
        button == kPad_Select) {
        button = kPad_L1;
    }

    if (leftShift || rightShift) {
        std::vector<QuickJoyCheat *> &srcCheats = mJoyCheatPtrsMode[leftShift ? 0 : 1];
        std::vector<QuickJoyCheat *> cheats(srcCheats);
#ifdef HX_NATIVE
        for (auto it = cheats.begin(); it != cheats.end(); ++it) {
#else
        for (QuickJoyCheat **it = cheats.begin(); it != cheats.end(); ++it) {
#endif
            if (button == (*it)->unk0) {
                CallCheatScript(true, (*it)->unk4, localUser, true);
            }
        }
    }

    mLastButtonTime.Stop();
    if (Timer::CyclesToMs(mLastButtonTime.mCycles) > 2000.0f) {
        for (std::vector<LongJoyCheat>::iterator it = mLongJoyCheats.begin();
             it != mLongJoyCheats.end(); ++it) {
            it->unk8 = 0;
        }
    }

    mLastButtonTime.Restart();

    padNum = localUser->GetPadNum();
    if (JoypadIsShiftButton(padNum, button))
        return 1;

    for (std::vector<LongJoyCheat>::iterator it = mLongJoyCheats.begin();
         it != mLongJoyCheats.end(); ++it) {
        if (it->unk8 < 0 || (unsigned)it->unk8 > it->unk0.size()) {
            it->unk8 = 0;
        }
        if (button == it->unk0[it->unk8]) {
            it->unk8++;
            if ((unsigned)it->unk8 >= it->unk0.size()) {
                CallCheatScript(false, it->unkc, localUser, true);
                for (std::vector<LongJoyCheat>::iterator jt = mLongJoyCheats.begin();
                     jt != mLongJoyCheats.end(); ++jt) {
                    jt->unk8 = 0;
                }
                goto done;
            }
        } else {
            it->unk8 = 0;
        }
    }
    done:
    return 1;
}

void SetCheatMode(Symbol mode) {
    Symbol m = mode;
    CheatsManager *mgr = gCheatsManager;
    if (m != mgr->mSymMode) {
        mgr->mSymMode = m;
        mgr->RebuildKeyCheatsForMode();
    }
}

Symbol GetCheatMode() { return gCheatsManager->mSymMode; }