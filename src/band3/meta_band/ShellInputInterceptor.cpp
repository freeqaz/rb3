#include "meta_band/ShellInputInterceptor.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/ModifierMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "ui/UI.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

ShellInputInterceptor::ShellInputInterceptor(BandUserMgr *mgr)
    : mBandUserMgr(mgr), mButtonDownSwitch(1), mButtonUpSwitch(1) {
    mTime.Start();
    for (int i = 0; i < 4; i++)
        mLastUpDown[i] = 0;
}

DataNode ShellInputInterceptor::OnMsg(const ButtonDownMsg &msg) {
    LocalBandUser *user = BandUserMgr::GetLocalBandUser(msg.GetUser());
    if (mButtonDownSwitch) {
        JoypadAction filteredAction = FilterAction(user, msg.GetAction());
        JoypadAction rawAction = msg.GetAction();
        if (filteredAction != rawAction) {
            static ButtonDownMsg new_msg(nullptr, kPad_Xbox_A, kAction_Confirm, 0);
            new_msg[0] = user;
            new_msg[1] = msg.GetButton();
            new_msg[2] = filteredAction;
            new_msg[3] = msg.GetPadNum();
            mButtonDownSwitch = false;
            TheUI.Handle(new_msg, false);
            mButtonDownSwitch = true;
            return 0;
        }
    }
    if (IsDoubleStrum(user, msg.GetButton())) {
        return 1;
    } else
        return DataNode(kDataUnhandled, 0);
}

DataNode ShellInputInterceptor::OnMsg(const ButtonUpMsg &msg) {
    LocalBandUser *user = BandUserMgr::GetLocalBandUser(msg.GetUser());
    if (mButtonUpSwitch) {
        JoypadAction filteredAction = FilterAction(user, msg.GetAction());
        JoypadAction rawAction = msg.GetAction();
        if (filteredAction != rawAction) {
            static ButtonUpMsg new_msg(nullptr, kPad_Xbox_A, kAction_Confirm, 0);
            new_msg[0] = user;
            new_msg[1] = msg.GetButton();
            new_msg[2] = filteredAction;
            new_msg[3] = msg.GetPadNum();
            mButtonUpSwitch = false;
            TheUI.Handle(new_msg, false);
            mButtonUpSwitch = true;
            return 0;
        }
    }
    return DataNode(kDataUnhandled, 0);
}

JoypadAction
ShellInputInterceptor::FilterAction(LocalBandUser *pUser, JoypadAction action) {
    int padNum = pUser->GetPadNum();
    Symbol type = JoypadControllerTypePadNum(padNum);
    if (type == none)
        return action;
    JoypadData *data = JoypadGetPadData(padNum);
    if (data->mIsDrum) {
        int padShift = JoypadTypePadShiftButton(type);
        int cymbalShift = JoypadTypeCymbalShiftButton(type);
        if (!TheModifierMgr->IsModifierActive(mod_drum_surface_navigation)) {
            if ((data->mButtons & (1 << padShift))
                || (data->mButtons & (1 << cymbalShift))) {
                return kAction_None;
            }
        } else if ((data->mButtons & (1 << padShift))
                   || (data->mButtons & (1 << cymbalShift))) {
            switch (action) {
            case kAction_Option:
                return kAction_Down;
            case kAction_WiiHomeMenu:
                return kAction_Up;
            }
        }
    }
    if (JoypadTypeHasLeftyFlip(type)) {
        GameplayOptions *opts = pUser->GetGameplayOptions();
        MILO_ASSERT(opts, 0x98);
        if (opts->GetLefty()) {
            if (action == kAction_Up)
                return kAction_Down;
            if (action == kAction_Down)
                return kAction_Up;
            if (action == kAction_Left)
                return kAction_Right;
            if (action == kAction_Right)
                return kAction_Left;
        }
    }
    return action;
}

bool ShellInputInterceptor::IsDoubleStrum(LocalBandUser *pUser, int button) {
    mTime.Split();
    if (button == kPad_DUp || button == kPad_DDown) {
        MILO_ASSERT(pUser && pUser->IsLocal(), 0xAC);
        int slot = pUser->GetLocalBandUser()->GetPadNum();
        float diff = Timer::CyclesToMs(mTime.mCycles) - mLastUpDown[slot];
        mLastUpDown[slot] = Timer::CyclesToMs(mTime.mCycles);
        if (diff < 50.0f) {
            return true;
        }
    }
    return false;
}

BEGIN_HANDLERS(ShellInputInterceptor)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_CHECK(0xBE)
END_HANDLERS