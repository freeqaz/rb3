#include "meta_band/UIStats.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "os/System.h"
#include "utl/MemMgr.h"

UIStats gUIStats;
UIStats *TheUIStats = &gUIStats;

UIStats::UIStats() {}

void UIStats::Init() {
    void *mem = _MemAlloc(0x10000, 0);
    unkb0 = mem;
    unkb4 = mem;
    unkb8 = 0;
    unka8 = 0;
    unk1c = false;
    unkac = SystemMs();
}

void UIStats::Terminate() {
    if (unkb0) {
        _MemFree(unkb0);
        unkb0 = 0;
    }
    unkb4 = 0;
}

void UIStats::Poll() {}

void UIStats::DropScreen(UIScreen *screen) {
    unkb4 = unkb0;
    unkb8 = 0;
    unka8++;
}

void UIStats::MaybePublish(UIScreen *from) {}

void UIStats::EventLog(unsigned int pad, unsigned int but, unsigned int state) {
    MILO_ASSERT(but < 32, 0x139);
    MILO_ASSERT(pad < 8, 0x13B);
    MILO_ASSERT(state < 2, 0x13D);
    int now = SystemMs();
    unsigned int elapsed = (unsigned int)(now - unkac) >> 4;
    if (elapsed > 0x7FFFFF) elapsed = 0x7FFFFF;
    unsigned int packed = (elapsed | ((but << 23) & 0x0F800000)) | (((pad << 28) & 0x70000000) | (state << 31));
    *(unsigned int *)unkb4 = (unsigned short)packed;
    int count = unkb8 + 1;
    unkb8 = count;
    unkb4 = (char *)unkb4 + 4;
    if ((unkb8 & 0x3FFF) == 0) {
        unkb4 = unkb0;
    }
    unkac = now;
}

DataNode UIStats::OnMsg(const ButtonDownMsg &msg) {
    EventLog(msg.GetPadNum(), msg.GetButton(), 0);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const ButtonUpMsg &msg) {
    EventLog(msg.GetPadNum(), msg.GetButton(), 1);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const JoypadConnectionMsg &msg) {
    MILO_ASSERT(msg.GetUser(), 0x166);
    EventLog(msg.GetUser()->GetPadNum(), 0x18, msg->Int(3) != 0);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const UIComponentFocusChangeMsg &) {
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const UIScreenChangeMsg &msg) {
    MaybePublish(msg.GetOldScreen());
    return DataNode(kDataUnhandled, 0);
}

BEGIN_HANDLERS(UIStats)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MESSAGE(JoypadConnectionMsg)
    HANDLE_MESSAGE(UIComponentFocusChangeMsg)
    HANDLE_MESSAGE(UIScreenChangeMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x183)
END_HANDLERS