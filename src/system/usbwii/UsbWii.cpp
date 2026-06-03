#include "usbwii/UsbWii.h"
#include "utl/MemMgr.h"

static UsbWii *gsTheUsbWii;

// static u32 UsbWii::sUSBOpenCloseResult = 0;

void UsbWii::ClearDevice(int num) {
    UsbDevice *device = &sDevices[num];
    memset(device, 0, sizeof(UsbDevice));
    device->state = 0;
    device->type = kUsbNone;
    device->ledNum = -1;
    device->unk6 = 0;
}

void UsbWii::DbgVerifyNoBufferOverrun(int unk) {}

void UsbWii::KeepAlive(int unk) {}

bool UsbWii::AddDevice(HIDDevice *device, UsbType type) {
    UsbDevice *base;
    int off;
    // check if we already have this device in our list
    base = sDevices;
    for (off = 0; off < 0x400; off += 0x100) {
        if (*(HIDDevice **)((char *)base + off) == device) {
            ((UsbDevice *)((char *)base + off))->state = 0;
            return false;
        }
    }
    // if we don't, find a free spot and add the device
    base = sDevices;
    for (off = 0; off < 0x400; off += 0x100) {
        if (*(HIDDevice **)((char *)base + off) == NULL) {
            *(HIDDevice **)((char *)base + off) = device;
            ((UsbDevice *)((char *)base + off))->type = type;
            return true;
        }
    }
    // failed to add the device
    return false;
}

int UsbWii::UsbAttachHandler(_HIDClient *client, HIDDevice *device, unsigned long user) {
    if (!mDiscError) {
        if (user != NULL) {
            UsbType type = (UsbType)GetType(device);
            if (type != kUsbNone && (type > kUsbNone && type < kUsbTypeMax)) {
                return AddDevice(device, (UsbType)type);
            }
            return 0;
        }
        for (int i = 0; i < 4; i++) {
            if (sDevices[i].dd == device) {
                ClearDevice(i);
            }
        }
    }
    return 0;
}

int UsbWii::GetType(HIDDevice *device) {
    // Harmonix Music Systems / Mad Catz vendor ID
    if (device->vid == 0x1BAD) {
        unsigned short pid = device->pid;
        if (pid == 0x3138) {
            // MIDI Pro Adapter, Drums
            return kUsbMidiDrums;
        }
        if (pid < 0x3138) {
            if (pid >= 0x3000) {
                if (pid >= 0x3100) {
                    if (pid >= 0x3110) goto lbl_drums_rb2;
                    goto lbl_drums;  // [0x3100, 0x3110)
                }
                if (pid >= 0x3010) goto lbl_guitar_rb2;
                goto lbl_guitar;  // [0x3000, 0x3010)
            } else {
                if (pid == 5) goto lbl_drums;  // shared with [0x3100, 0x3110)
                if (pid >= 5) goto vid_check;
                if (pid < 4) goto vid_check;
                // pid==4: goto lbl_guitar
            }
        } else {
            // pid > 0x3138
            if (pid == 0x3338) goto lbl_mpa;
            if (pid > 0x3338) {
                if (pid < 0x3530) {
                    if (pid >= 0x3430) goto lbl_mustang;
                    goto lbl_midi_keyboard;  // (0x3338, 0x3430)
                } else {
                    if (pid >= 0x3630) goto vid_check;
                    goto lbl_squire;
                }
            } else {
                // (0x3138, 0x3338)
                if (pid >= 0x3330) goto lbl_midi_keyboard;  // [0x3330, 0x3338)
                if (pid >= 0x3200) goto vid_check;
                goto lbl_drums_rb2;  // (0x3138, 0x3200)
            }
        }
        lbl_drums_rb2: return kUsbDrumsRb2;          // shared: [0x3110, 0x3138) and (0x3138, 0x3200)
        lbl_drums: return kUsbDrums;                  // shared: pid==5 and [0x3100, 0x3110)
        lbl_guitar_rb2: return kUsbGuitarRb2;         // [0x3010, 0x3100)
        lbl_guitar: return kUsbGuitar;                // shared: pid==4 and [0x3000, 0x3010)
        lbl_mpa: return kUsbMidiKeyboardMpa;          // pid==0x3338
        lbl_midi_keyboard: return kUsbMidiKeyboard;  // shared: [0x3330, 0x3338) and (0x3338, 0x3430)
        lbl_mustang: return kUsbMidiGuitarMustang;
        lbl_squire: return kUsbMidiGuitarSquire;
    }
vid_check:
#ifdef MILO_DEBUG
    // only in this build does the game check for PS3 instruments
    // Sony Computer Entertainment vendor ID
    if (device->vid != 0x12BA) {
        return kUsbNone;
    } else {
        // PS3 Drums
        if (device->pid == 0x210)
            return kUsbDrums;
        // PS3 Guitar
        else if (device->pid == 0x200)
            return kUsbGuitar;
        else
            return kUsbNone;
    }
#endif
}

bool UsbWii::HasUnifiedLEDCalbertMsg(UsbType type) {
    switch (type) {
    case kUsbGuitarRb2:
    case kUsbUnused:
        return true;
    }
    return false;
}

int UsbWii::GetJoypadType(int num) const {
    switch (sDevices[num].type) {
    case kUsbDrums:
        return kJoypadWiiHxDrums;
    case kUsbDrumsRb2:
        return kJoypadWiiHxDrumsRb2;
    case kUsbGuitar:
        return kJoypadWiiHxGuitar;
    case kUsbGuitarRb2:
        return kJoypadWiiHxGuitarRb2;
    case kUsbUnused:
        return kJoypadWiiCoreGuitar;
    case kUsbMidiKeyboardMpa:
        return kJoypadWiiMidiBoxKeyboard;
    case kUsbMidiDrums:
        return kJoypadWiiMidiBoxDrums;
    case kUsbMidiGuitarMustang:
        return kJoypadWiiButtonGuitar;
    case kUsbMidiGuitarSquire:
        return kJoypadWiiRealGuitar22Fret;
    case kUsbMidiKeyboard:
        return kJoypadWiiKeytar;
    }
    return 0;
}

inline bool UsbWii::IsDeviceActive(int num) {
    bool stateValid = false;
    bool deviceValid = false;
    if (num < 4 && sDevices[num].type != kUsbNone) {
        deviceValid = true;
    }
    if (deviceValid && sDevices[num].state >= 2) {
        stateValid = true;
    }
    if (stateValid && (sDevices[num].flags & kUsbFlagActive) == 0) {
        return false;
    }
    return true;
}

bool UsbWii::IsActive(int num) const {
    bool deviceValid = false;
    bool stateValid = false;
    if ((unsigned int)num < 4 && sDevices[num].type != kUsbNone) {
        deviceValid = true;
    }
    if (deviceValid && (int)sDevices[num].state > 2) {
        stateValid = true;
    }
    if (stateValid && (sDevices[num].flags & kUsbFlagActive) == 0) {
        return false;
    }
    return stateValid;
}

void UsbWii::UsbOpenCloseCallback(long result, unsigned long unk) {
    sUSBOpenCloseResult = result;
}

bool UsbWii::OpenLib() {
    static int _x = MemFindHeap("main");
    MemPushHeap(_x);
    usbMem = _MemAlloc(0x3AE0, 0x20);
    MemPopHeap();
    if (usbMem == NULL) {
        return false;
    }
    sUSBOpenCloseResult = OPENCLOSE_START_MAGIC;
    int r;
    while (true) {
        r = HIDOpenAsync(usbMem, UsbOpenCloseCallback, 0);
        if (r == 0) {
            break;
        }
        if (r != -5) {
            return false;
        }
    }
    return WaitForUSBOpenCloseResult();
}

void UsbWii::SetLED(int num, int led) {
    bool deviceValid = false;
    bool stateValid = false;
    if ((unsigned int)num < 4 && sDevices[num].type != kUsbNone) {
        deviceValid = true;
    }
    if (deviceValid && (int)sDevices[num].state > 2) {
        stateValid = true;
    }
    if (!stateValid) {
        return;
    }
    sDevices[num].flags |= kUsbFlagHasLED;
    sDevices[num].ledNum = led;
}

void UsbWii::SetInactive(int num) {
    if ((unsigned int)num >= 4)
        return;
    sDevices[num].flags = (sDevices[num].flags & ~kUsbFlagActive) | kUsbFlagInactive;
}

void UsbWii::UsbReadInstrCallback(
    HIDDevice *device, long result, u8 *bytes, unsigned long length, unsigned long user
) {
    UsbDevice &dev = sDevices[user];
    // error during read
    if (result != 0) {
        if ((int)dev.state == kUsbStateDone) {
            dev.state = kUsbStateReading;
            if ((dev.flags & kUsbFlagInactive) == 0) {
                dev.inactivity = 0;
            }
        }
        // fatal error / device disconnected?
        if (result == -4) {
            ClearDevice(user);
            return;
        }
        return;
    }
    // D-PAD correction
    u8 dpad = dev.packet[2];
    if ((dpad & 8) != 0) {
        dev.packet[2] = 0;
    } else {
        switch (dpad) {
        case 0:
            dev.packet[2] = 1;
            break;
        case 2:
            dev.packet[2] = 8;
            break;
        case 6:
            dev.packet[2] = 4;
            break;
        case 4:
            dev.packet[2] = 2;
            break;
        }
    }
    // fill in the button mask
    dev.buttonMask = dev.packet[0] | (dev.packet[1] << 8) | (dev.packet[2] << 16);
    // fill in the stick values
    dev.lstickX = dev.packet[3];
    dev.lstickY = dev.packet[4];
    dev.rstickX = dev.packet[5];
    dev.rstickY = dev.packet[6];
    // fill in the pressure values
    dev.pressures[0] = dev.packet[11];
    dev.pressures[1] = dev.packet[12];
    dev.pressures[2] = dev.packet[13];
    dev.pressures[3] = dev.packet[14];
    // copy over the extra values - manually 8-wide unrolled with ctr=2
    // matches CW's mtctr/bdnz pattern from -O4,p partial unroll
    {
        u8 *base = (u8 *)&dev;
        int i = 0;
        int ctr = 2;
        do {
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
            base[i + 0x2c] = base[i + 0x45]; ++i;
        } while (--ctr);
    }
    // check if buttons are pressed for inactivity reading
    int buttonsPressed = dev.buttonMask;
    int type = dev.type;
    // CW emits cmpwi/beq + cmpwi/bne for chained ==, vs subic/cmplwi/bgt for ||.
    if (type == kUsbGuitar) goto mask_select;
    if (type != kUsbGuitarRb2) goto inactivity_check;
mask_select:
    buttonsPressed &= ~0x20;
inactivity_check:;
    if (buttonsPressed != 0) {
        if ((dev.flags & kUsbFlagInactive) == 0) {
            dev.flags |= kUsbFlagActive;
        } else {
            dev.inactivity = 0;
        }
    } else if ((dev.flags & kUsbFlagInactive) != 0) {
        dev.inactivity += 1;
    }
    // send another request if we're done reading
    if ((int)dev.state == kUsbStateDone) {
        dev.state = kUsbStateReading;
        RequestReadInstr(user, false);
        return;
    }
}

UsbWii::UsbWii() {
    usbMem = NULL;
    for (int i = 0; i < 4; i++)
        ClearDevice(i);
    OpenLib();
    HIDRegisterClient(&client, UsbAttachHandler);
    gsTheUsbWii = this;
}

UsbWii::~UsbWii() {
    gsTheUsbWii = NULL;
    sUSBOpenCloseResult = OPENCLOSE_START_MAGIC;
    HIDUnregisterClientAsync(&client, UsbDetachHandler, NULL);
    WaitForUSBOpenCloseResult();
    CloseLib();
}
