#include "os/Joypad.h"
#include "os/Joypad_Wii.h"
#include "os/PlatformMgr.h"
#include "os/UserMgr.h"
#include "os/Debug.h"
#include "obj/Data.h"
#include "usbwii/UsbWii.h"
#include "RVL_SDK/revolution/wpad/WPAD.h"
#include <vector>

extern "C" {
    void KPADInit();
    void KPADSetConnectCallback(int chan, void (*cb)(long, long));
    void KPADDisableDPD(int chan);
    void WPADSetSyncDeviceCallback(void (*cb)(long, long));
    void WPADSetAutoSleepTime(int minutes);
    void WPADStartSyncDevice();
}

struct WiiJoypad {
    JoypadType mType;        // 0x0
    JoypadType mPrevType;    // 0x4
    int mWPADSlot;           // 0x8
    int mUSBSlotNum;         // 0xC
    int mReconnectCountdown; // 0x10
    bool mDPDEnabled;        // 0x14

    WiiJoypad() {}
    ~WiiJoypad() {}

    void Init();
    void Terminate();
    void SetJoypadType(JoypadType type);
    void Connect(JoypadType type, int slot);
    void Disconnect();
    bool CheckConnection(JoypadType *typeOut) const;
    void AttemptReconnect();
    void EnableDPD(bool enable);
    void Read(
        unsigned int *buttons, char *a, char *b, char *c, char *d, char *e, char *f,
        unsigned int *g, unsigned char *h
    );
};

namespace {
    static UsbWii *gUSBWii;
    static bool gDisabled;
    static bool gWiiJoypadCommonInitialized;
    static bool gWiiJoypadUseHeapMemory;
    static WiiJoypad gWiiJoypads[4];
    static int gUSBSlotJoypadNums[4];

    static WiiJoypad *GetWiiJoypad(int padNum);
    static void SetUSBSlotInUse(int slot, int padNum);
    static void ClearUSBSlotInUse(int slot);
    static int FindUSBSlotByJoypadNum(int padNum);
    static void ResetUSBSlotJoypadNums();
    static void InitWiiJoypads();
    static JoypadType GetJoypadTypeFromWiiType(unsigned long wiiType);
    static void WiiJoypadSetDataFormat(long slot, JoypadType type);
    static bool WiiJoypadCheckDataFormatCompatible(long slot, JoypadType type);
    static JoypadType WiiJoypadCheckTypeAtSlot(int slot, unsigned long *devTypeOut);
    static void WiiJoypadSyncCallback(long result, long unk);
    static void WiiJoypadConnectCallback(long slot, long connected);
    static void *WiiJoypadWPADAlloc(unsigned long size);
    static bool WiiJoypadWPADFree(void *ptr);
    static unsigned int DeOpposeDPadButtons(unsigned int btns);
    static unsigned int StickToDPad(float stickX, float stickY, float threshold);
    static bool IsAnyCompatibleSlotOpen(JoypadType type);
    static bool IsAnyOpenSlotSeekingType(JoypadType type);
    static void AddWiiJoypadUSB(int slot);
    static void AddWiiJoypad(int slot);
    static void TranslateButtons(
        unsigned int *out, unsigned int btns, JoypadType oldType, JoypadType newType
    );
}

void JoypadInit() {
    if (gUSBWii == NULL) {
        DataArray *joypadCfg = SystemConfig("joypad");
        joypadCfg->FindData("disable", gDisabled, true);
        if (!gDisabled) {
            InitWiiJoypads();
            UsbWii *newUsb = ::new UsbWii();
            gUSBWii = newUsb;
            JoypadWiiInitCommon(true);
            WPADSetSyncDeviceCallback(WiiJoypadSyncCallback);
            for (int i = 0; i < 4; i++) {
                KPADSetConnectCallback(i, WiiJoypadConnectCallback);
            }
            WPADSetAutoSleepTime(0xf);
            JoypadInitCommon(joypadCfg);
            JoypadReset();
        }
    }
}

void JoypadReset() {
    ResetAllUsersPads();
    std::vector<LocalUser *> users;
    if (TheUserMgr != NULL) {
        TheUserMgr->GetLocalUsers(users);
    }
    int joypadOff = 0;
    int userOff = 0;
    unsigned int padNum = 0;
    do {
        if (padNum < users.size()) {
            AssociateUserAndPad(*(LocalUser **)((char *)users.data() + userOff), padNum);
        }
        gWiiJoypads[padNum].Init();
        padNum += 1;
        userOff += 4;
        joypadOff += 0x18;
    } while ((int)padNum < 4);
}

void JoypadTerminate() {
    if (!gDisabled) {
        WiiJoypad *joypad = gWiiJoypads;
        for (int i = 0; i < 4; i++, joypad++) {
            WPADDisconnect(i);
            joypad->Terminate();
        }
        delete gUSBWii;
        gUSBWii = NULL;
    }
}

void JoypadPoll() {
    if (gDisabled || ThePlatformMgr.mHomeMenuWii->mHomeMenuActive) {
        JoypadKeepEverythingAlive();
        return;
    }
    gUSBWii->Poll();
    JoypadPollCommon();
}
