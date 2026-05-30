#include "types.h"

void *__MIXChannel;
s32 __MIXMaxVoices;
s32 __MIXSoundMode;

void __MIXSetPan(void *chan, s32 pan) {
    (void)chan; (void)pan;
}

s32 __MIXGetVolume(void *chan) {
    (void)chan;
    return 0;
}

s32 MIXInitSpecifyMem(void *buf, s32 size, s32 voices) {
    (void)buf; (void)size; (void)voices;
    return 0;
}

void MIXQuit(void) {
}

void MIXSetSoundMode(s32 mode) {
    (void)mode;
}

void MIXInitChannel(s32 chan, s32 voice) {
    (void)chan; (void)voice;
}

void MIXReleaseChannel(s32 chan) {
    (void)chan;
}

void MIXSetInput(s32 chan, s32 input) {
    (void)chan; (void)input;
}

void MIXSetAuxA(s32 chan, s32 val) {
    (void)chan; (void)val;
}

void MIXSetAuxB(s32 chan, s32 val) {
    (void)chan; (void)val;
}

void MIXSetPan(s32 chan, s32 pan) {
    (void)chan; (void)pan;
}

void MIXSetSPan(s32 chan, s32 pan) {
    (void)chan; (void)pan;
}

void MIXSetFader(s32 chan, s32 fader) {
    (void)chan; (void)fader;
}

void MIXUpdateSettings(void) {
}
