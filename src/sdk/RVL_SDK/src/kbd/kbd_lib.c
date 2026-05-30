#include "types.h"

u8 kbdKeyMapUS_International[0x3E8];
u8 kbdModifierMapUS_International[0x50];
u8 noRepeatKeys[0xE];
u8 kbdShutdownInfo[0x10];
s32 __KBDVersion = 0;
void *p_km;

void kbdAttachHandler(void *p0, void *p1) {
    (void)p0; (void)p1;
}

void kbdEventHandler(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void kbdRepeatHandler(void *p0) {
    (void)p0;
}

static void kbdProcKey(void *p0, void *p1) {
    (void)p0; (void)p1;
}

static void kbdProcMod(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 kbdSendKey(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

static void kbd_led_handler(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 KBDSetLedsAsync(void *p0, void *p1, void *p2) {
    (void)p0; (void)p1; (void)p2;
    return 0;
}

s32 KBDSetLeds(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

s32 KBDSetLedsRetry(void *p0, void *p1, void *p2) {
    (void)p0; (void)p1; (void)p2;
    return 0;
}

static void kbdLedAlarmHandler(void *p0) {
    (void)p0;
}

static void kbdLedRetryCallback(void *p0, void *p1) {
    (void)p0; (void)p1;
}

void kbdInitMap(void *p0) {
    (void)p0;
}

static void kbdInitMapIntl(void *p0) {
    (void)p0;
}

void kbdShutdown(void *p0) {
    (void)p0;
}

s32 KBDInit(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

s32 KBDGetKey(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

s32 KBDGetChannelStatus(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

s32 KBDResetChannel(void *p0) {
    (void)p0;
    return 0;
}

s32 KBDSetCountry(void *p0, void *p1) {
    (void)p0; (void)p1;
    return 0;
}

void KBDSetLockProcessing(void *p0, void *p1) {
    (void)p0; (void)p1;
}

void KBDSetModState(void *p0, void *p1) {
    (void)p0; (void)p1;
}

s32 KBDGetModState(void *p0) {
    (void)p0;
    return 0;
}

s32 KBDTranslateHidCode(void *p0, void *p1, void *p2) {
    (void)p0; (void)p1; (void)p2;
    return 0;
}
