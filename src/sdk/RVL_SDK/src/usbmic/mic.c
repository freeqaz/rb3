struct micOpenParam {};
struct micDesc {};
struct IsoTransfer {};

namespace usbmic {

struct Mic {};

void Mic_SetCurrFeatureUnit(Mic* m, unsigned char a, unsigned char b,
    unsigned char c, unsigned char d, unsigned short e, unsigned long f,
    bool g, void (*cb)(void*), void* arg) {
    (void)m; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    (void)g; (void)cb; (void)arg;
}

void Mic_Initialize(Mic* m) {
    (void)m;
}

void Mic_DeInitialize(Mic* m) {
    (void)m;
}

void Mic_GetDescription(Mic* m, micDesc* desc) {
    (void)m; (void)desc;
}

long Mic_Open(Mic* m, micOpenParam* param) {
    (void)m; (void)param;
    return 0;
}

void Mic_CloseFinalize(Mic* m) {
    (void)m;
}

void Mic_Close(Mic* m) {
    (void)m;
}

long Mic_Read(Mic* m, void* buf, unsigned long* size) {
    (void)m; (void)buf; (void)size;
    return 0;
}

void Mic_OnSetVolDone(long result, void* arg) {
    (void)result; (void)arg;
}

long Mic_SetVolume(Mic* m, unsigned short vol) {
    (void)m; (void)vol;
    return 0;
}

void Mic_OnSetMuteDone(long result, void* arg) {
    (void)result; (void)arg;
}

long Mic_SetMute(Mic* m, bool mute) {
    (void)m; (void)mute;
    return 0;
}

void Mic_IncOutstandingRequests(Mic* m) {
    (void)m;
}

void Mic_DecOutstandingRequests(Mic* m) {
    (void)m;
}

} // namespace usbmic
